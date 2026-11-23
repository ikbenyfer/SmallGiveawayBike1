#include "nlstream.h"
#include "../Game/nlmap.h"
#include <pthread.h>
#include <zlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NS_MAXT 25
#define NS_MAPCACHE 120
#define LOAD_R 1     /* 3x3 laden */
#define KEEP_R 2     /* 5x5 vasthouden */

enum { T_EMPTY = 0, T_NET, T_WORK, T_READY, T_NONE };
enum { J_BUILD_JSON = 1, J_BUILD_BLOB = 2, J_SAVE_JSON = 3 };

/* ------------------------------------------------------------------ pakket */

static struct {
    unsigned char *data; long len;
    double ox, oy; int ntx, nty; float tile;
    long idxOff;
    int npc; long pcOff;
    int nstart; long startOff;
    int ok;
} P;

static unsigned rd32(const unsigned char *p) { return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24); }
static double rdf64(const unsigned char *p) { double d; memcpy(&d, p, 8); return d; }
static float rdf32(const unsigned char *p) { float f; memcpy(&f, p, 4); return f; }

int ns_pack_load(void) {
    long off;
    if (P.ok) return 1;
    if (!plat_read_resource("nl_hb.pak", (char **)&P.data, &P.len) || P.len < 40) { plat_log("SGB-NL geen ingebouwd kaartpakket"); return 0; }
    if (memcmp(P.data, "SGBP", 4) != 0) return 0;
    P.ox = rdf64(P.data + 8); P.oy = rdf64(P.data + 16);
    P.ntx = (int)rd32(P.data + 24); P.nty = (int)rd32(P.data + 28); P.tile = rdf32(P.data + 32);
    P.idxOff = 36;
    off = P.idxOff + (long)P.ntx * P.nty * 12;
    P.npc = (int)rd32(P.data + off); P.pcOff = off + 4;
    off = P.pcOff + (long)P.npc * 14;
    P.nstart = (int)rd32(P.data + off); P.startOff = off + 4;
    P.ok = 1;
    plat_log("SGB-NL ingebouwd pakket: %d x %d tegels, %d postcodes, %d startpunten, %.1f MB", P.ntx, P.nty, P.npc, P.nstart, P.len / 1048576.0);
    return 1;
}

int ns_pack_postcode(const char *pc, double *rx, double *ry) {
    int i;
    if (!P.ok) return 0;
    for (i = 0; i < P.npc; i++) {
        const unsigned char *e = P.data + P.pcOff + (long)i * 14;
        if (memcmp(e, pc, 6) == 0) { *rx = (int)rd32(e + 6); *ry = (int)rd32(e + 10); return 1; }
    }
    return 0;
}

int ns_pack_nstarts(void) { return P.ok ? P.nstart : 0; }

const char *ns_pack_start(int i, double *rx, double *ry) {
    const unsigned char *e;
    if (!P.ok || i < 0 || i >= P.nstart) return NULL;
    e = P.data + P.startOff + (long)i * 56;
    *rx = (int)rd32(e + 48); *ry = (int)rd32(e + 52);
    return (const char *)e;
}

static int pack_tile(int tx, int ty, const unsigned char **blob, long *clen, long *rawlen) {
    int ix, iy;
    const unsigned char *e;
    if (!P.ok) return -1;
    ix = tx - (int)floor(P.ox / P.tile + 0.5); iy = ty - (int)floor(P.oy / P.tile + 0.5);
    if (ix < 0 || iy < 0 || ix >= P.ntx || iy >= P.nty) return -1;      /* buiten het pakket */
    e = P.data + P.idxOff + ((long)iy * P.ntx + ix) * 12;
    if (rd32(e) == 0) return 0;                                          /* binnen, maar geen regio-tegel */
    *blob = P.data + rd32(e); *clen = rd32(e + 4); *rawlen = rd32(e + 8);
    return 1;
}

int ns_pack_contains(double rx, double ry) {
    const unsigned char *b; long c, r;
    return pack_tile((int)floor(rx / TILE_M), (int)floor(ry / TILE_M), &b, &c, &r) == 1;
}

/* ------------------------------------------------------------------ werker-thread */

typedef struct {
    int kind, tx, ty, gen;
    float wx0, wzS;
    char *buf[5]; long len[5];
    int bagPdok;
    unsigned char *blob; long blobLen, rawLen;  /* blob: zlib */
    TileBuilt *out;
    int ok;
    long saved;
} Job;

#define QN 64
static pthread_t th;
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static Job *qin[QN], *qout[QN];
static int nin, nout, thStarted;

/* v5.5: hoge kwaliteit = echte dakvlakken (LoD 2.2), anders LoD 1.2 met schilddaken */
static int g_hq;
void ns_set_detail(int hq) { g_hq = hq ? 1 : 0; }
static void tile_name(char *name, int n, int tx, int ty) { snprintf(name, n, "nl/t55/%d_%d_%c.t", tx, ty, g_hq ? 'h' : 'm'); }

static void save_tile(int tx, int ty, const TileData *td, long *saved) {
    unsigned char *raw = NULL, *z;
    long rl = td_encode(td, &raw);
    uLongf zl = compressBound((uLong)rl);
    char name[64];
    z = (unsigned char *)malloc(zl + 4);
    if (z && compress2(z + 4, &zl, raw, (uLong)rl, 6) == Z_OK) {
        z[0] = (unsigned char)(rl & 255); z[1] = (unsigned char)((rl >> 8) & 255); z[2] = (unsigned char)((rl >> 16) & 255); z[3] = (unsigned char)((rl >> 24) & 255);
        tile_name(name, sizeof(name), tx, ty);
        plat_write_file(name, (const char *)z, (long)zl + 4);
        if (saved) *saved = (long)zl + 4;
    }
    free(z);
    free(raw);
}

static void run_job(Job *j) {
    TileData td;
    memset(&td, 0, sizeof(td));
    if (j->kind == J_BUILD_BLOB) {
        unsigned char *raw = (unsigned char *)malloc((size_t)j->rawLen + 1);
        uLongf rl = (uLongf)j->rawLen;
        j->ok = raw && uncompress(raw, &rl, j->blob, (uLong)j->blobLen) == Z_OK && td_decode(raw, (long)rl, &td);
        free(raw);
    } else {
        double tminx = j->tx * (double)TILE_M, tminy = j->ty * (double)TILE_M;
        j->ok = td_from_json(tminx, tminy, j->buf[0], j->len[0], j->bagPdok, j->buf[1], j->len[1], j->buf[2], j->len[2], j->buf[3], j->len[3], &td);
        if (j->ok) save_tile(j->tx, j->ty, &td, &j->saved);
    }
    if (j->ok && j->kind != J_SAVE_JSON) {
        j->out = (TileBuilt *)calloc(1, sizeof(TileBuilt));
        tb_build(&td, j->wx0, j->wzS, j->out);
    }
    td_free(&td);
}

static void *worker(void *arg) {
    (void)arg;
    for (;;) {
        Job *j;
        pthread_mutex_lock(&mu);
        while (nin == 0) pthread_cond_wait(&cv, &mu);
        j = qin[0];
        memmove(qin, qin + 1, sizeof(Job *) * (size_t)(nin - 1));
        nin--;
        pthread_mutex_unlock(&mu);
        run_job(j);
        pthread_mutex_lock(&mu);
        if (nout < QN) qout[nout++] = j;
        pthread_mutex_unlock(&mu);
    }
    return NULL;
}

static int submit(Job *j) {
    int ok = 0;
    if (!thStarted) { pthread_create(&th, NULL, worker, NULL); thStarted = 1; }
    pthread_mutex_lock(&mu);
    if (nin < QN) { qin[nin++] = j; ok = 1; pthread_cond_signal(&cv); }
    pthread_mutex_unlock(&mu);
    return ok;
}

static Job *take_result(int kindSave) {
    Job *j = NULL;
    int i;
    pthread_mutex_lock(&mu);
    for (i = 0; i < nout; i++) {
        if ((qout[i]->kind == J_SAVE_JSON) == (kindSave != 0)) {
            j = qout[i];
            memmove(qout + i, qout + i + 1, sizeof(Job *) * (size_t)(nout - i - 1));
            nout--;
            break;
        }
    }
    pthread_mutex_unlock(&mu);
    return j;
}

static void job_free(Job *j) {
    int i;
    if (!j) return;
    for (i = 0; i < 5; i++) free(j->buf[i]);
    free(j->blob);
    if (j->out) { tb_free(j->out); free(j->out); }
    free(j);
}

/* ------------------------------------------------------------------ netwerk per tegel */

typedef struct { int req[5]; int done[5]; char *buf[5]; long len[5]; int bagPdok; } Net;

static void net_start(Net *n, int tx, int ty) {
    char url[800];
    double x0 = tx * (double)TILE_M, y0 = ty * (double)TILE_M, cx = x0 + TILE_M * 0.5, cy = y0 + TILE_M * 0.5;
    int i;
    memset(n, 0, sizeof(*n));
    (void)cx; (void)cy;
    for (i = 0; i < 4; i++) {
        if (i == 0 && g_hq) nl_url_bag3d_lod22_bbox(url, sizeof(url), x0, y0, x0 + TILE_M, y0 + TILE_M);
        else if (i == 0) nl_url_bag3d_bbox(url, sizeof(url), x0, y0, x0 + TILE_M, y0 + TILE_M);
        else nl_url_top10_bbox(url, sizeof(url), i == 1 ? "wegdeel_hartlijn" : (i == 2 ? "waterdeel_vlak" : "terrein_vlak"), x0, y0, x0 + TILE_M, y0 + TILE_M);
        n->req[i] = plat_http_start(url);
        if (n->req[i] <= 0) n->done[i] = -1;
    }
}

/* 1 = alles binnen, 0 = bezig */
static int net_poll(Net *n, int tx, int ty) {
    int i, busy = 0;
    for (i = 0; i < 5; i++) {
        int r;
        if (n->done[i] || n->req[i] <= 0) continue;
        r = plat_http_poll(n->req[i], &n->buf[i], &n->len[i]);
        if (r != 0) n->done[i] = r > 0 ? 1 : -1;
    }
    if (n->done[0] == -1 && !n->req[4]) {   /* 3D BAG mislukt: PDOK BAG zonder hoogte */
        char url[800];
        double x0 = tx * (double)TILE_M, y0 = ty * (double)TILE_M;
        nl_url_bagpdok_bbox(url, sizeof(url), x0, y0, x0 + TILE_M, y0 + TILE_M);
        n->req[4] = plat_http_start(url);
        n->bagPdok = 1;
        if (n->req[4] <= 0) n->done[4] = -1;
    }
    for (i = 0; i < 5; i++) if (n->req[i] > 0 && !n->done[i]) busy = 1;
    return !busy;
}

static int net_any(const Net *n) { int i; for (i = 0; i < 5; i++) if (n->done[i] == 1) return 1; return 0; }

static void net_to_job(Net *n, Job *j) {
    int b = n->bagPdok ? 4 : 0, i;
    j->buf[0] = n->done[b] == 1 ? n->buf[b] : NULL; j->len[0] = n->len[b];
    if (b == 4) { free(n->buf[0]); } else { free(n->buf[4]); }
    for (i = 1; i < 4; i++) { j->buf[i] = n->done[i] == 1 ? n->buf[i] : NULL; j->len[i] = n->len[i]; if (n->done[i] != 1) free(n->buf[i]); }
    if (n->done[b] != 1) free(n->buf[b]);
    j->bagPdok = n->bagPdok;
    memset(n->buf, 0, sizeof(n->buf));
}

static void net_free(Net *n) { int i; for (i = 0; i < 5; i++) { free(n->buf[i]); n->buf[i] = NULL; } }

/* ------------------------------------------------------------------ streaming-wereld */

typedef struct {
    int used, tx, ty, state, src;   /* src: 1 pakket, 2 cache, 3 net */
    int netFailed;                  /* v5.5: geen internet -> dan het meegeleverde pakket gebruiken */
    int slot[4];
    TileBuilt *tb;
    Net net;
    double retryAt;
} STile;

typedef struct { int tx, ty; Tex tex; double used; } MapTile;

static struct {
    int on, gen;
    Renderer *r;
    World *w;
    double ox, oy;
    STile t[NS_MAXT];
    int ptx, pty, mmtx, mmty, dirty;
    int slotUsed[RW_SLOTS];
    MapTile map[NS_MAPCACHE]; int nmap;
    int nPack, nCache, nNet;
    uint8_t *mm;
} S;

void ns_to_rd(float x, float z, double *rx, double *ry) { *rx = S.ox + x; *ry = S.oy - z; }
void ns_from_rd(double rx, double ry, float *x, float *z) { *x = (float)(rx - S.ox); *z = (float)(S.oy - ry); }
int ns_active(void) { return S.on; }
void ns_origin(double *ox, double *oy) { *ox = S.ox; *oy = S.oy; }

static int slot_alloc(void) {
    int i;
    for (i = 0; i < RW_SLOTS; i++) if (!S.slotUsed[i]) { S.slotUsed[i] = 1; return i; }
    return -1;
}

static void tile_release(STile *t) {
    int k;
    for (k = 0; k < 4; k++) if (t->slot[k] >= 0) { r_slot_clear(S.r, 1, t->slot[k]); S.slotUsed[t->slot[k]] = 0; t->slot[k] = -1; }
    if (t->tb) { tb_free(t->tb); free(t->tb); t->tb = NULL; }
    net_free(&t->net);
    t->used = 0;
    S.dirty = 1;
}

static STile *tile_find(int tx, int ty) {
    int i;
    for (i = 0; i < NS_MAXT; i++) if (S.t[i].used && S.t[i].tx == tx && S.t[i].ty == ty) return &S.t[i];
    return NULL;
}

static void map_add(int tx, int ty, const uint8_t *raster) {
    int i, best = 0;
    for (i = 0; i < S.nmap; i++) if (S.map[i].tx == tx && S.map[i].ty == ty) { S.map[i].used = plat_time(); return; }
    if (S.nmap < NS_MAPCACHE) best = S.nmap++;
    else for (i = 1; i < S.nmap; i++) if (S.map[i].used < S.map[best].used) best = i;
    S.map[best].tx = tx; S.map[best].ty = ty; S.map[best].used = plat_time();
    r_tex_upload(&S.map[best].tex, TILE_RASTER, TILE_RASTER, raster);
}

int ns_map_count(void) { return S.nmap; }
const Tex *ns_map_tile(int i, double *rx0, double *ry0) {
    if (i < 0 || i >= S.nmap) return NULL;
    *rx0 = S.map[i].tx * (double)TILE_M; *ry0 = S.map[i].ty * (double)TILE_M;
    return &S.map[i].tex;
}

void ns_begin(Renderer *r, World *w, double ox, double oy) {
    int i;
    ns_end();
    S.on = 1;
    S.gen++;
    S.r = r; S.w = w;
    S.ox = ox; S.oy = oy;
    S.ptx = S.pty = S.mmtx = S.mmty = 0x7fffffff;
    for (i = 0; i < NS_MAXT; i++) { memset(&S.t[i], 0, sizeof(STile)); S.t[i].slot[0] = S.t[i].slot[1] = S.t[i].slot[2] = S.t[i].slot[3] = -1; }
    memset(S.slotUsed, 0, sizeof(S.slotUsed));
    S.nPack = S.nCache = S.nNet = 0;
    world_free(w);
    world_init(w);
    w->real = 1;
    w->realSize = 0;      /* geen kaartrand */
    mb_flat(&w->ground, -12000.0f, -12000.0f, 12000.0f, 12000.0f, -0.02f, col(140, 150, 122));
    w->mm = 384;
    w->mmSpan = TILE_M * 3;
    w->minimap = (uint8_t *)calloc(384 * 384, 4);
    r_upload_world(r, w, 1);
    for (i = 0; i < RW_SLOTS; i++) r_slot_clear(r, 1, i);
    world_build_grid(w, -100.0f, 200.0f);
    S.dirty = 1;
}

void ns_end(void) {
    int i;
    if (!S.on) return;
    for (i = 0; i < NS_MAXT; i++) if (S.t[i].used) tile_release(&S.t[i]);
    S.on = 0;
    S.gen++;   /* resultaten van de werker voor de oude wereld worden weggegooid */
}

static void world_rebuild(void) {
    World *w = S.w;
    int i, k, ns = 0, nr = 0, nl = 0;
    float minx = 1e9f, minz = 1e9f, maxx = -1e9f, maxz = -1e9f;
    free(w->segs); free(w->rsegs); free(w->names);
    free(w->gstart); free(w->gitems); free(w->stamp); free(w->sstart); free(w->sitems); free(w->sstamp);
    w->segs = NULL; w->rsegs = NULL; w->names = NULL; w->gstart = w->gitems = w->stamp = NULL; w->sstart = w->sitems = w->sstamp = NULL;
    for (i = 0; i < NS_MAXT; i++) if (S.t[i].used && S.t[i].tb) { ns += S.t[i].tb->nsegs; nr += S.t[i].tb->nrsegs; nl += S.t[i].tb->nameslen; }
    w->segs = (WSeg *)malloc(sizeof(WSeg) * (size_t)(ns + 1));
    w->rsegs = (RSeg *)malloc(sizeof(RSeg) * (size_t)(nr + 1));
    w->names = (char *)malloc((size_t)nl + 1);
    w->nsegs = w->nrsegs = 0; w->nameslen = 0;
    w->capsegs = ns + 1; w->caprsegs = nr + 1; w->namescap = nl + 1;
    for (i = 0; i < NS_MAXT; i++) {
        TileBuilt *tb;
        float wx0, wzS;
        if (!S.t[i].used || !S.t[i].tb) continue;
        tb = S.t[i].tb;
        memcpy(w->segs + w->nsegs, tb->segs, sizeof(WSeg) * (size_t)tb->nsegs);
        w->nsegs += tb->nsegs;
        for (k = 0; k < tb->nrsegs; k++) {
            RSeg r = tb->rsegs[k];
            if (r.name >= 0) r.name += w->nameslen;
            w->rsegs[w->nrsegs++] = r;
        }
        if (tb->nameslen) memcpy(w->names + w->nameslen, tb->names, (size_t)tb->nameslen);
        w->nameslen += tb->nameslen;
        wx0 = (float)(S.t[i].tx * (double)TILE_M - S.ox);
        wzS = (float)(S.oy - S.t[i].ty * (double)TILE_M);
        if (wx0 < minx) minx = wx0;
        if (wx0 + TILE_M > maxx) maxx = wx0 + TILE_M;
        if (wzS - TILE_M < minz) minz = wzS - TILE_M;
        if (wzS > maxz) maxz = wzS;
    }
    if (minx > maxx) { minx = minz = -100; maxx = maxz = 100; }
    {
        float org = fminf(minx, minz) - 20.0f, size = fmaxf(maxx, maxz) - org + 20.0f;
        world_build_grid(w, org, size);
    }
}

static void minimap_compose(void) {
    World *w = S.w;
    int dx, dy, y;
    uint8_t *mm = w->minimap;
    for (y = 0; y < 384 * 384; y++) { mm[y * 4] = 128; mm[y * 4 + 1] = 136; mm[y * 4 + 2] = 116; mm[y * 4 + 3] = 235; }
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++) {
            STile *t = tile_find(S.ptx + dx, S.pty + dy);
            int col0 = (dx + 1) * TILE_RASTER, row0 = (1 - dy) * TILE_RASTER;   /* noord boven */
            if (!t || !t->tb || !t->tb->raster) continue;
            for (y = 0; y < TILE_RASTER; y++)
                memcpy(mm + ((size_t)(row0 + y) * 384 + col0) * 4, t->tb->raster + (size_t)y * TILE_RASTER * 4, TILE_RASTER * 4);
        }
    w->mmOrg = (float)((S.ptx - 1) * (double)TILE_M - S.ox);
    w->mmOrgZ = (float)(S.oy - (S.pty + 2) * (double)TILE_M);
    w->mmSpan = TILE_M * 3;
    r_tex_upload(&S.r->rw[1].minimap, 384, 384, mm);
    S.mmtx = S.ptx; S.mmty = S.pty;
}

static void start_source(STile *t) {
    const unsigned char *blob; long clen, rawlen;
    int pk = pack_tile(t->tx, t->ty, &blob, &clen, &rawlen);
    Job *j;
    char name[64];
    char *cdata = NULL; long clen2 = 0;
    /* v5.5: eerst de cache (nieuwe gebouwdata), dan internet; het meegeleverde pakket (oude data) alleen offline */
    tile_name(name, sizeof(name), t->tx, t->ty);
    if (plat_read_file(name, &cdata, &clen2) && clen2 > 4) {
        j = (Job *)calloc(1, sizeof(Job));
        j->kind = J_BUILD_BLOB;
        j->rawLen = (long)rd32((unsigned char *)cdata);
        j->blob = (unsigned char *)malloc((size_t)clen2 - 4);
        memcpy(j->blob, cdata + 4, (size_t)clen2 - 4);
        j->blobLen = clen2 - 4;
        free(cdata);
        t->src = 2;
    } else if (pk == 1 && t->netFailed) {
        free(cdata);
        j = (Job *)calloc(1, sizeof(Job));
        j->kind = J_BUILD_BLOB; j->blob = (unsigned char *)malloc((size_t)clen); memcpy(j->blob, blob, (size_t)clen);
        j->blobLen = clen; j->rawLen = rawlen;
        t->src = 1;
    } else {
        free(cdata);
        if (plat_time() < t->retryAt) return;
        net_start(&t->net, t->tx, t->ty);
        t->state = T_NET;
        t->src = 3;
        return;
    }
    j->tx = t->tx; j->ty = t->ty; j->gen = S.gen;
    j->wx0 = (float)(t->tx * (double)TILE_M - S.ox);
    j->wzS = (float)(S.oy - t->ty * (double)TILE_M);
    if (submit(j)) t->state = T_WORK; else job_free(j);
}

static int net_inflight(void) {
    int i, n = 0;
    for (i = 0; i < NS_MAXT; i++) if (S.t[i].used && S.t[i].state == T_NET) n++;
    return n;
}

void ns_update(float px, float pz) {
    double rx, ry;
    int ptx, pty, dx, dy, i;
    Job *j;
    if (!S.on) return;
    ns_to_rd(px, pz, &rx, &ry);
    ptx = (int)floor(rx / TILE_M); pty = (int)floor(ry / TILE_M);
    S.ptx = ptx; S.pty = pty;
    /* te ver weg: loslaten */
    for (i = 0; i < NS_MAXT; i++) {
        STile *t = &S.t[i];
        if (!t->used) continue;
        if (abs(t->tx - ptx) > KEEP_R || abs(t->ty - pty) > KEEP_R) {
            if (t->state == T_WORK) continue;   /* wacht tot de werker klaar is */
            tile_release(t);
        }
    }
    /* nodig: 3x3, dichtstbijzijnde eerst */
    for (i = 0; i <= LOAD_R; i++)
        for (dy = -i; dy <= i; dy++)
            for (dx = -i; dx <= i; dx++) {
                STile *t;
                int k;
                if (abs(dx) != i && abs(dy) != i) continue;
                if (tile_find(ptx + dx, pty + dy)) continue;
                for (k = 0; k < NS_MAXT && S.t[k].used; k++) {}
                if (k >= NS_MAXT) continue;
                t = &S.t[k];
                memset(t, 0, sizeof(*t));
                t->used = 1; t->tx = ptx + dx; t->ty = pty + dy; t->state = T_EMPTY;
                t->slot[0] = t->slot[1] = t->slot[2] = t->slot[3] = -1;
            }
    /* bronnen starten */
    for (i = 0; i < NS_MAXT; i++) {
        STile *t = &S.t[i];
        if (!t->used || t->state != T_EMPTY) continue;
        if (net_inflight() >= 2) {
            const unsigned char *b; long c, r;
            if (pack_tile(t->tx, t->ty, &b, &c, &r) != 1) continue;
        }
        start_source(t);
    }
    /* netwerk */
    for (i = 0; i < NS_MAXT; i++) {
        STile *t = &S.t[i];
        if (!t->used || t->state != T_NET) continue;
        if (!net_poll(&t->net, t->tx, t->ty)) continue;
        if (!net_any(&t->net)) {
            const unsigned char *pb; long pc, pr;
            net_free(&t->net);
            t->netFailed = 1;
            if (pack_tile(t->tx, t->ty, &pb, &pc, &pr) == 1) { t->state = T_EMPTY; S.dirty = 1; continue; }   /* offline: pakket */
            t->state = T_EMPTY;
            t->retryAt = plat_time() + 15.0;
            t->state = T_NONE;
            S.dirty = 1;
            continue;
        }
        j = (Job *)calloc(1, sizeof(Job));
        j->kind = J_BUILD_JSON; j->tx = t->tx; j->ty = t->ty; j->gen = S.gen;
        j->wx0 = (float)(t->tx * (double)TILE_M - S.ox);
        j->wzS = (float)(S.oy - t->ty * (double)TILE_M);
        net_to_job(&t->net, j);
        if (submit(j)) t->state = T_WORK; else { job_free(j); t->state = T_EMPTY; }
    }
    /* mislukte tegels later opnieuw */
    for (i = 0; i < NS_MAXT; i++) if (S.t[i].used && S.t[i].state == T_NONE && S.t[i].src == 3 && plat_time() > S.t[i].retryAt) S.t[i].state = T_EMPTY;
    /* resultaten van de werker: hooguit 1 per frame uploaden */
    j = take_result(0);
    if (j) {
        STile *t = j->gen == S.gen ? tile_find(j->tx, j->ty) : NULL;
        if (t && t->state == T_WORK) {
            if (j->ok && j->out) {
                int k;
                for (k = 0; k < 4; k++) {
                    int s = slot_alloc();
                    t->slot[k] = s;
                    if (s >= 0) r_slot_upload(S.r, 1, s, &j->out->sub[k]);
                    mb_free(&j->out->sub[k]);
                }
                t->tb = j->out;
                j->out = NULL;
                t->state = T_READY;
                if (t->src == 1) S.nPack++; else if (t->src == 2) S.nCache++; else S.nNet++;
                map_add(t->tx, t->ty, t->tb->raster);
            } else {
                t->state = T_NONE;
                t->retryAt = plat_time() + 15.0;
            }
            S.dirty = 1;
        } else if (t && t->state != T_WORK) {
            /* verouderd */
        }
        job_free(j);
    }
    if (S.dirty) { world_rebuild(); S.dirty = 0; S.mmtx = 0x7fffffff; }
    if (S.mmtx != ptx || S.mmty != pty) minimap_compose();
}

int ns_ready_at(float x, float z) {
    double rx, ry;
    STile *t;
    ns_to_rd(x, z, &rx, &ry);
    t = tile_find((int)floor(rx / TILE_M), (int)floor(ry / TILE_M));
    return t && (t->state == T_READY || t->state == T_NONE);
}

int ns_offline_hole(float x, float z) {
    double rx, ry;
    STile *t;
    ns_to_rd(x, z, &rx, &ry);
    t = tile_find((int)floor(rx / TILE_M), (int)floor(ry / TILE_M));
    return t && t->state == T_NONE;
}

int ns_busy(void) {
    int i, n = 0;
    for (i = 0; i < NS_MAXT; i++) if (S.t[i].used && (S.t[i].state == T_NET || S.t[i].state == T_WORK || S.t[i].state == T_EMPTY)) n++;
    return n;
}

int ns_find_road(float x, float z, float *ox, float *oz, float *heading) {
    World *w = S.w;
    int i, best = -1;
    float bd = 1e18f, bx = x, bz = z;
    for (i = 0; i < w->nrsegs; i++) {
        const RSeg *r = &w->rsegs[i];
        float ex = r->x1 - r->x0, ez = r->z1 - r->z0, l2 = ex * ex + ez * ez, t, qx, qz, d2;
        if (l2 < 4.0f) continue;
        t = clampf(((x - r->x0) * ex + (z - r->z0) * ez) / l2, 0.1f, 0.9f);
        qx = r->x0 + ex * t; qz = r->z0 + ez * t;
        d2 = (qx - x) * (qx - x) + (qz - z) * (qz - z);
        if (d2 < bd) { bd = d2; best = i; bx = qx; bz = qz; }
    }
    if (best < 0) return 0;
    *ox = bx; *oz = bz;
    *heading = atan2f(w->rsegs[best].x1 - w->rsegs[best].x0, w->rsegs[best].z1 - w->rsegs[best].z0);
    return 1;
}

void ns_stats(int *tiles, int *buildings, int *verts, int *fromPack, int *fromCache, int *fromNet) {
    int i;
    *tiles = *buildings = *verts = 0;
    for (i = 0; i < NS_MAXT; i++)
        if (S.t[i].used && S.t[i].state == T_READY && S.t[i].tb) {
            int k;
            (*tiles)++;
            *buildings += S.t[i].tb->nb;
            for (k = 0; k < 4; k++) if (S.t[i].slot[k] >= 0) *verts += S.r->rw[1].chunk[S.t[i].slot[k]].count;
        }
    *fromPack = S.nPack; *fromCache = S.nCache; *fromNet = S.nNet;
}

/* ------------------------------------------------------------------ downloaden voor offline */

static struct {
    int on, total, done, failed, cur, active;
    int tx[64], ty[64];
    Net net;
    long bytes;
    int waitingJob;
} D;

void ns_dl_start(double rx, double ry, int radius) {
    int cx = (int)floor(rx / TILE_M), cy = (int)floor(ry / TILE_M), dx, dy;
    memset(&D, 0, sizeof(D));
    D.on = 1;
    for (dy = -radius; dy <= radius; dy++)
        for (dx = -radius; dx <= radius; dx++)
            if (D.total < 64) { D.tx[D.total] = cx + dx; D.ty[D.total] = cy + dy; D.total++; }
}

int ns_dl_busy(void) { return D.on; }
void ns_dl_progress(int *done, int *total, long *bytes, int *failed) { *done = D.done; *total = D.total; *bytes = D.bytes; *failed = D.failed; }

void ns_dl_tick(void) {
    Job *j = take_result(1);
    if (j) {
        if (j->ok) D.bytes += j->saved; else D.failed++;
        D.done++;
        D.waitingJob = 0;
        job_free(j);
    }
    if (!D.on || D.waitingJob) return;
    if (D.active) {
        if (!net_poll(&D.net, D.tx[D.cur], D.ty[D.cur])) return;
        D.active = 0;
        if (!net_any(&D.net)) { net_free(&D.net); D.failed++; D.done++; D.cur++; }
        else {
            j = (Job *)calloc(1, sizeof(Job));
            j->kind = J_SAVE_JSON; j->tx = D.tx[D.cur]; j->ty = D.ty[D.cur];
            net_to_job(&D.net, j);
            if (submit(j)) D.waitingJob = 1; else { job_free(j); D.failed++; D.done++; }
            D.cur++;
        }
        return;
    }
    while (D.cur < D.total) {
        char name[64], *d = NULL;
        long l = 0;
        const unsigned char *b; long c, r;
        if (pack_tile(D.tx[D.cur], D.ty[D.cur], &b, &c, &r) == 1) { D.done++; D.cur++; continue; }   /* al ingebouwd */
        tile_name(name, sizeof(name), D.tx[D.cur], D.ty[D.cur]);
        if (plat_read_file(name, &d, &l)) { D.bytes += l; free(d); D.done++; D.cur++; continue; }
        net_start(&D.net, D.tx[D.cur], D.ty[D.cur]);
        D.active = 1;
        return;
    }
    if (D.done >= D.total && !D.waitingJob) D.on = 0;
}
