#include "nltile.h"
#include "json.h"
#include "models.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAXP 4096

/* ------------------------------------------------------------------ TileData */

static void feat_free(TFeat *f, int n) {
    int i;
    for (i = 0; i < n; i++) { free(f[i].x); free(f[i].y); }
    free(f);
}

void td_free(TileData *td) {
    int i;
    feat_free(td->b, td->nb); feat_free(td->r, td->nr); feat_free(td->w, td->nw); feat_free(td->t, td->nt);
    for (i = 0; i < td->nnames; i++) free(td->names[i]);
    free(td->names);
    memset(td, 0, sizeof(*td));
}

static TFeat *feat_add(TFeat **arr, int *n, int *cap, int npts) {
    TFeat *f;
    if (*n >= *cap) {
        int nc = *cap ? *cap * 2 : 64;
        TFeat *na = (TFeat *)realloc(*arr, (size_t)nc * sizeof(TFeat));
        if (!na) return NULL;
        *arr = na; *cap = nc;
    }
    f = &(*arr)[(*n)++];
    memset(f, 0, sizeof(*f));
    f->n = npts;
    f->x = (float *)malloc(sizeof(float) * (size_t)(npts > 0 ? npts : 1));
    f->y = (float *)malloc(sizeof(float) * (size_t)(npts > 0 ? npts : 1));
    f->name = -1;
    return f;
}

/* --- binair: zie tools/build_region_pack.py */
typedef struct { const unsigned char *p, *end; int bad; } Rd;
static unsigned rd_u8(Rd *r) { if (r->p + 1 > r->end) { r->bad = 1; return 0; } return *r->p++; }
static unsigned rd_u16(Rd *r) { unsigned v; if (r->p + 2 > r->end) { r->bad = 1; return 0; } v = r->p[0] | (r->p[1] << 8); r->p += 2; return v; }
static int rd_i16(Rd *r) { int v = (int)rd_u16(r); return v >= 32768 ? v - 65536 : v; }

static int rd_pts(Rd *r, TFeat *f) {
    int i;
    for (i = 0; i < f->n; i++) { f->x[i] = rd_i16(r) * 0.1f; f->y[i] = rd_i16(r) * 0.1f; }
    return !r->bad;
}

int td_decode(const unsigned char *raw, long len, TileData *td) {
    Rd r;
    int i, n, cb = 0, cr = 0, cw = 0, ct = 0;
    memset(td, 0, sizeof(*td));
    r.p = raw; r.end = raw + len; r.bad = 0;
    n = (int)rd_u16(&r);
    if (n == 0xFFF5) {   /* v5.5-formaat: daken, bouwjaar */
        rd_u8(&r);       /* versie */
        td->roofs = 1;
        n = (int)rd_u16(&r);
        for (i = 0; i < n && !r.bad; i++) {
            int kind = (int)rd_u8(&r);
            float h = rd_u16(&r) * 0.1f, h0 = rd_u16(&r) * 0.1f, az = rd_u16(&r) * 0.1f, sl = (float)rd_u8(&r);
            int year = (int)rd_u16(&r), grp = (int)rd_u16(&r);
            TFeat *f = feat_add(&td->b, &td->nb, &cb, (int)rd_u16(&r));
            if (!f) break;
            f->code = kind; f->h = h; f->h0 = h0; f->az = az; f->sl = sl; f->year = year; f->grp = grp;
            rd_pts(&r, f);
        }
    } else for (i = 0; i < n && !r.bad; i++) {   /* oud formaat (meegeleverd kaartpakket): alleen hoogte */
        float h = rd_u16(&r) * 0.1f;
        TFeat *f = feat_add(&td->b, &td->nb, &cb, (int)rd_u16(&r));
        if (!f) break;
        f->h = h;
        f->grp = i;
        rd_pts(&r, f);
    }
    n = (int)rd_u16(&r);
    for (i = 0; i < n && !r.bad; i++) {
        float w = rd_u8(&r) / 5.0f;
        int kind = (int)rd_u8(&r), name = (int)rd_u16(&r);
        TFeat *f = feat_add(&td->r, &td->nr, &cr, (int)rd_u16(&r));
        if (!f) break;
        f->w = w; f->code = kind; f->name = name == 0xFFFF ? -1 : name;
        rd_pts(&r, f);
    }
    n = (int)rd_u16(&r);
    for (i = 0; i < n && !r.bad; i++) {
        TFeat *f = feat_add(&td->w, &td->nw, &cw, (int)rd_u16(&r));
        if (!f) break;
        rd_pts(&r, f);
    }
    n = (int)rd_u16(&r);
    for (i = 0; i < n && !r.bad; i++) {
        int code = (int)rd_u8(&r);
        TFeat *f = feat_add(&td->t, &td->nt, &ct, (int)rd_u16(&r));
        if (!f) break;
        f->code = code;
        rd_pts(&r, f);
    }
    n = (int)rd_u16(&r);
    if (n > 0 && !r.bad) {
        td->names = (char **)calloc((size_t)n, sizeof(char *));
        for (i = 0; i < n && !r.bad; i++) {
            int l = (int)rd_u8(&r);
            char *s = (char *)malloc((size_t)l + 1);
            if (r.p + l > r.end) { r.bad = 1; free(s); break; }
            memcpy(s, r.p, (size_t)l); s[l] = 0; r.p += l;
            td->names[td->nnames++] = s;
        }
    }
    for (i = 0; i < td->nr; i++) if (td->r[i].name >= td->nnames) td->r[i].name = -1;
    return !r.bad;
}

typedef struct { unsigned char *p; long n, cap; } Wb;
static void wb_put(Wb *w, const void *d, long n) {
    if (w->n + n > w->cap) {
        long nc = w->cap ? w->cap * 2 : 65536;
        while (nc < w->n + n) nc *= 2;
        w->p = (unsigned char *)realloc(w->p, (size_t)nc);
        w->cap = nc;
    }
    memcpy(w->p + w->n, d, (size_t)n);
    w->n += n;
}
static void wb_u8(Wb *w, unsigned v) { unsigned char b = (unsigned char)v; wb_put(w, &b, 1); }
static void wb_u16(Wb *w, unsigned v) { unsigned char b[2]; b[0] = (unsigned char)(v & 255); b[1] = (unsigned char)((v >> 8) & 255); wb_put(w, b, 2); }
static int q16(float v) { int q = (int)(v * 10.0f + (v >= 0 ? 0.5f : -0.5f)); return q < -32767 ? -32767 : (q > 32767 ? 32767 : q); }
static void wb_pts(Wb *w, const TFeat *f) {
    int i, n = f->n > 4000 ? 4000 : f->n;
    wb_u16(w, (unsigned)n);
    for (i = 0; i < n; i++) { wb_u16(w, (unsigned)(q16(f->x[i]) & 0xFFFF)); wb_u16(w, (unsigned)(q16(f->y[i]) & 0xFFFF)); }
}

long td_encode(const TileData *td, unsigned char **out) {
    Wb w = {0};
    int i;
    wb_u16(&w, 0xFFF5);
    wb_u8(&w, 1);
    wb_u16(&w, (unsigned)td->nb);
    for (i = 0; i < td->nb; i++) {
        const TFeat *f = &td->b[i];
        wb_u8(&w, (unsigned)f->code);
        wb_u16(&w, (unsigned)clampf(f->h * 10.0f + 0.5f, 0, 65535));
        wb_u16(&w, (unsigned)clampf(f->h0 * 10.0f + 0.5f, 0, 65535));
        wb_u16(&w, (unsigned)clampf(f->az * 10.0f + 0.5f, 0, 3600));
        wb_u8(&w, (unsigned)clampf(f->sl + 0.5f, 0, 90));
        wb_u16(&w, (unsigned)clampf((float)f->year, 0, 65535));
        wb_u16(&w, (unsigned)(f->grp & 0xFFFF));
        wb_pts(&w, f);
    }
    wb_u16(&w, (unsigned)td->nr);
    for (i = 0; i < td->nr; i++) {
        wb_u8(&w, (unsigned)clampf(td->r[i].w * 5.0f + 0.5f, 0, 255));
        wb_u8(&w, (unsigned)td->r[i].code);
        wb_u16(&w, td->r[i].name < 0 ? 0xFFFF : (unsigned)td->r[i].name);
        wb_pts(&w, &td->r[i]);
    }
    wb_u16(&w, (unsigned)td->nw);
    for (i = 0; i < td->nw; i++) wb_pts(&w, &td->w[i]);
    wb_u16(&w, (unsigned)td->nt);
    for (i = 0; i < td->nt; i++) { wb_u8(&w, (unsigned)td->t[i].code); wb_pts(&w, &td->t[i]); }
    wb_u16(&w, (unsigned)td->nnames);
    for (i = 0; i < td->nnames; i++) {
        int l = (int)strlen(td->names[i]);
        if (l > 255) l = 255;
        wb_u8(&w, (unsigned)l);
        wb_put(&w, td->names[i], l);
    }
    *out = w.p;
    return w.n;
}

/* ------------------------------------------------------------------ JSON -> TileData */

static float g_px[MAXP * 2], g_py[MAXP * 2];

static int ring_pts(const JV *ring, double ox, double oy, float *xs, float *ys, int max) {
    const JV *p;
    int n = 0;
    if (!ring || ring->type != J_ARR) return 0;
    for (p = ring->child; p && n < max; p = p->next) {
        const JV *a = p->child;
        if (!a || !a->next) continue;
        xs[n] = (float)(a->num - ox);
        ys[n] = (float)(a->next->num - oy);
        if (n > 0 && fabsf(xs[n] - xs[n - 1]) < 0.05f && fabsf(ys[n] - ys[n - 1]) < 0.05f) continue;
        n++;
    }
    if (n > 1 && fabsf(xs[n - 1] - xs[0]) < 0.05f && fabsf(ys[n - 1] - ys[0]) < 0.05f) n--;
    return n;
}

static int clip_sq(float *x, float *y, int n, float lo, float hi) {
    static float tx[MAXP * 2], ty[MAXP * 2];
    int e;
    for (e = 0; e < 4 && n > 0; e++) {
        int i, m = 0;
        for (i = 0; i < n; i++) {
            int j = (i + 1) % n;
            float a = e == 0 ? x[i] - lo : e == 1 ? hi - x[i] : e == 2 ? y[i] - lo : hi - y[i];
            float b = e == 0 ? x[j] - lo : e == 1 ? hi - x[j] : e == 2 ? y[j] - lo : hi - y[j];
            if (a >= 0 && m < MAXP * 2) { tx[m] = x[i]; ty[m] = y[i]; m++; }
            if ((a >= 0) != (b >= 0) && m < MAXP * 2) {
                float t = a / (a - b);
                tx[m] = x[i] + (x[j] - x[i]) * t; ty[m] = y[i] + (y[j] - y[i]) * t; m++;
            }
        }
        n = m < MAXP ? m : MAXP;
        memcpy(x, tx, sizeof(float) * (size_t)n);
        memcpy(y, ty, sizeof(float) * (size_t)n);
    }
    return n;
}

static void feat_copy(TFeat *f, const float *x, const float *y) {
    memcpy(f->x, x, sizeof(float) * (size_t)f->n);
    memcpy(f->y, y, sizeof(float) * (size_t)f->n);
}

static int landuse(const char *t) {
    if (!t) return 0;
    if (strstr(t, "grasland")) return 1;
    if (strstr(t, "bos")) return 2;
    if (strstr(t, "akker")) return 3;
    if (strstr(t, "bebouwd")) return 4;
    if (strstr(t, "zand")) return 5;
    if (strstr(t, "kwekerij") || strstr(t, "boomgaard")) return 6;
    if (strstr(t, "dodenakker")) return 7;
    if (strstr(t, "spoor")) return 8;
    if (strstr(t, "heide")) return 9;
    return 0;
}

static int name_index(TileData *td, const char *s, int *cap) {
    int i;
    if (!s || !*s) return -1;
    for (i = 0; i < td->nnames; i++) if (strcmp(td->names[i], s) == 0) return i;
    if (td->nnames >= *cap) {
        int nc = *cap ? *cap * 2 : 32;
        char **nn = (char **)realloc(td->names, sizeof(char *) * (size_t)nc);
        if (!nn) return -1;
        td->names = nn; *cap = nc;
    }
    {
        size_t l = strlen(s);
        char *c = (char *)malloc(l + 1);
        memcpy(c, s, l + 1);
        td->names[td->nnames] = c;
    }
    return td->nnames++;
}

static JV *features_of(char *buf, long len, JArena *a) {
    if (!buf || len <= 0) return NULL;
    return json_get(json_parse(buf, len, a), "features");
}

static void each_poly(const JV *geom, double ox, double oy, void (*fn)(TileData *, float *, float *, int, const JV *, void *), TileData *td, const JV *props, void *ctx) {
    const char *t = json_str(json_get(geom, "type"));
    const JV *co = json_get(geom, "coordinates"), *p;
    int n;
    if (!t || !co) return;
    if (strcmp(t, "Polygon") == 0) {
        n = ring_pts(co->child, ox, oy, g_px, g_py, MAXP);
        if (n >= 3) fn(td, g_px, g_py, n, props, ctx);
    } else if (strcmp(t, "MultiPolygon") == 0) {
        for (p = co->child; p; p = p->next) {
            n = ring_pts(p->child, ox, oy, g_px, g_py, MAXP);
            if (n >= 3) fn(td, g_px, g_py, n, props, ctx);
        }
    }
}

typedef struct { int cb, cr, cw, ct, cn; int bagPdok, idx; } JCtx;

static unsigned str_hash(const char *s) {
    unsigned h = 2166136261u;
    while (s && *s) { h ^= (unsigned char)*s++; h *= 16777619u; }
    return h;
}

static void j_building(TileData *td, float *x, float *y, int n, const JV *p, void *vc) {
    JCtx *c = (JCtx *)vc;
    float cx = 0, cy = 0, h, mv;
    int i, kind = BK_FLAT, year;
    float h0 = 0, az = 0, sl = 0;
    const char *id;
    TFeat *f;
    for (i = 0; i < n; i++) { cx += x[i]; cy += y[i]; }
    cx /= n; cy /= n;
    if (cx < 0 || cy < 0 || cx >= TILE_M || cy >= TILE_M) return;   /* hoort bij de buurtegel */
    year = (int)json_num(json_get(p, "oorspronkelijkbouwjaar"), 0.0);
    if (!year) year = (int)json_num(json_get(p, "oorspronkelijkBouwjaar"), 0.0);
    if (year < 1000 || year > 2100) year = 0;
    id = json_str(json_get(p, "identificatie"));
    mv = (float)json_num(json_get(p, "b3_h_maaiveld"), 0.0);
    if (c->bagPdok) h = 7.0f + (float)((c->idx * 37) % 7);
    else if (json_get(p, "b3_azimut")) {
        /* 3D BAG LoD 2.2: een dakvlak van een pand */
        double a = json_num(json_get(p, "b3_azimut"), -1.0);
        kind = BK_FACE;
        h = (float)(json_num(json_get(p, "b3_h_max"), 9.0) - mv);
        h0 = (float)(json_num(json_get(p, "b3_h_min"), 9.0) - mv);
        sl = (float)json_num(json_get(p, "b3_hellingshoek"), 0.0);
        az = a < 0 ? 0.0f : (float)a;
        if (sl < 4.0f || a < 0) { sl = 0; h = (float)(json_num(json_get(p, "b3_h_70p"), h + mv) - mv); h0 = h; }
        h = clampf(h, 1.0f, 200.0f); h0 = clampf(h0, 0.5f, h);
    } else {
        /* 3D BAG LoD 1.2: een hoogte per pand, plus daktype en nokhoogte */
        const char *dt = json_str(json_get(p, "b3_dak_type"));
        float h70 = (float)(json_num(json_get(p, "b3_h_70p"), 9.0) - mv);
        float nok = (float)(json_num(json_get(p, "b3_h_nok"), -999.0) - mv);
        float hmin = (float)(json_num(json_get(p, "b3_h_min"), -999.0) - mv);
        h = h70;
        if (dt && strcmp(dt, "slanted") == 0 && nok > h70 && nok < h70 * 2.0f + 6.0f && h70 < 30.0f) {
            float eave = (h70 - 0.7f * nok) / 0.3f;   /* bij een schuin dak ligt 70% van de dakpunten onder h70 */
            if (hmin > 1.5f && hmin > eave) eave = hmin;
            eave = clampf(eave, 2.6f, nok - 1.2f);
            if (eave < nok - 1.0f) { kind = BK_HIP; h = nok; h0 = eave; }
        }
    }
    h = clampf(h, kind == BK_FACE ? 1.0f : 2.5f, 160.0f);
    f = feat_add(&td->b, &td->nb, &c->cb, n);
    if (!f) return;
    f->h = h; f->code = kind; f->h0 = h0; f->az = az; f->sl = sl; f->year = year;
    f->grp = id ? (int)(str_hash(id) & 0xFFFF) : (c->idx & 0xFFFF);
    feat_copy(f, x, y);
}

static void j_water(TileData *td, float *x, float *y, int n, const JV *p, void *vc) {
    JCtx *c = (JCtx *)vc;
    TFeat *f;
    (void)p;
    n = clip_sq(x, y, n, 0, TILE_M);
    if (n < 3) return;
    f = feat_add(&td->w, &td->nw, &c->cw, n);
    if (f) feat_copy(f, x, y);
}

static void j_terrain(TileData *td, float *x, float *y, int n, const JV *p, void *vc) {
    JCtx *c = (JCtx *)vc;
    TFeat *f;
    n = clip_sq(x, y, n, 0, TILE_M);
    if (n < 3) return;
    f = feat_add(&td->t, &td->nt, &c->ct, n);
    if (!f) return;
    f->code = landuse(json_str(json_get(p, "typelandgebruik")));
    feat_copy(f, x, y);
}

static void j_road_line(TileData *td, const JV *line, double ox, double oy, const JV *p, JCtx *c) {
    const char *br = json_str(json_get(p, "verhardingsbreedteklasse"));
    const char *g = json_str(json_get(p, "hoofdverkeersgebruik"));
    const char *tw = json_str(json_get(p, "typeweg"));
    float w = 4.5f;
    int kind = ROAD_CAR, n, i, start = -1, name = -2;
    if (br) {
        if (strstr(br, "< 2")) w = 1.8f;
        else if (strstr(br, "2 - 4")) w = 3.4f;
        else if (strstr(br, "4 - 7")) w = 5.8f;
        else if (strstr(br, "> 7")) w = 8.5f;
    }
    if (g && strstr(g, "fiets")) kind = ROAD_BIKE;
    else if (g && strstr(g, "voetgang")) kind = ROAD_FOOT;
    else if (tw && (strstr(tw, "hoofdweg") || strstr(tw, "autosnelweg") || strstr(tw, "regionale"))) kind = ROAD_MAIN;
    n = ring_pts(line, ox, oy, g_px, g_py, MAXP);
    /* lijnstukken waarvan het midden in deze tegel ligt, aaneengesloten */
    for (i = 0; i <= n - 1; i++) {
        int in = 0;
        if (i < n - 1) {
            float mx = (g_px[i] + g_px[i + 1]) * 0.5f, my = (g_py[i] + g_py[i + 1]) * 0.5f;
            in = mx >= 0 && my >= 0 && mx < TILE_M && my < TILE_M;
        }
        if (in && start < 0) start = i;
        if ((!in || i == n - 1) && start >= 0) {
            int end = in ? i + 1 : i;   /* laatste punt inclusief */
            int cnt = end - start + 1;
            if (cnt >= 2) {
                TFeat *f = feat_add(&td->r, &td->nr, &c->cr, cnt);
                if (f) {
                    if (name == -2) name = name_index(td, json_str(json_get(p, "naam")), &c->cn);
                    f->w = w; f->code = kind; f->name = name;
                    feat_copy(f, g_px + start, g_py + start);
                }
            }
            start = -1;
        }
    }
}

int td_from_json(double tminx, double tminy, char *bag, long bagLen, int bagPdok,
                 char *roads, long roadsLen, char *water, long waterLen, char *terrain, long terrainLen, TileData *td) {
    JArena a;
    JV *fs, *f;
    JCtx c;
    memset(td, 0, sizeof(*td));
    memset(&c, 0, sizeof(c));
    memset(&a, 0, sizeof(a));
    c.bagPdok = bagPdok;
    td->heightsEstimated = bagPdok;
    td->roofs = 1;
    if ((fs = features_of(terrain, terrainLen, &a)) != NULL)
        for (f = fs->child; f; f = f->next) each_poly(json_get(f, "geometry"), tminx, tminy, j_terrain, td, json_get(f, "properties"), &c);
    json_free(&a);
    if ((fs = features_of(water, waterLen, &a)) != NULL)
        for (f = fs->child; f; f = f->next) each_poly(json_get(f, "geometry"), tminx, tminy, j_water, td, json_get(f, "properties"), &c);
    json_free(&a);
    if ((fs = features_of(roads, roadsLen, &a)) != NULL)
        for (f = fs->child; f; f = f->next) {
            const JV *g = json_get(f, "geometry"), *p = json_get(f, "properties"), *co = json_get(g, "coordinates"), *l;
            const char *t = json_str(json_get(g, "type"));
            if (!t || !co) continue;
            if (strcmp(t, "LineString") == 0) j_road_line(td, co, tminx, tminy, p, &c);
            else if (strcmp(t, "MultiLineString") == 0) for (l = co->child; l; l = l->next) j_road_line(td, l, tminx, tminy, p, &c);
        }
    json_free(&a);
    if ((fs = features_of(bag, bagLen, &a)) != NULL)
        for (f = fs->child; f; f = f->next) {
            each_poly(json_get(f, "geometry"), tminx, tminy, j_building, td, json_get(f, "properties"), &c);
            c.idx++;
        }
    json_free(&a);
    return 1;
}

/* ------------------------------------------------------------------ bouwen */

typedef struct {
    TileBuilt *tb;
    float wx0, wzN;    /* wereld: westrand, noordrand */
    int capsegs, caprsegs, capnames;
    int idx;
} BCtx;

static MeshB *sub_at(BCtx *b, float x, float z) {
    int sx = x >= b->wx0 + TILE_M * 0.5f, sz = z >= b->wzN + TILE_M * 0.5f;
    return &b->tb->sub[sz * 2 + sx];
}

static int tri_idx[MAXP * 3];
static int triangulate(const float *x, const float *z, int n) {
    static int V[MAXP];
    int nv = n, cnt = 0, guard = 0, i;
    for (i = 0; i < n; i++) V[i] = i;
    while (nv > 2 && guard < nv * nv + 50) {
        int found = 0;
        for (i = 0; i < nv; i++) {
            int a = V[(i + nv - 1) % nv], bb = V[i], c = V[(i + 1) % nv], k, inside = 0;
            float cr = (x[bb] - x[a]) * (z[c] - z[a]) - (z[bb] - z[a]) * (x[c] - x[a]);
            guard++;
            if (cr <= 1e-6f) continue;
            for (k = 0; k < nv && !inside; k++) {
                int p = V[k];
                float d1, d2, d3;
                if (p == a || p == bb || p == c) continue;
                d1 = (x[bb] - x[a]) * (z[p] - z[a]) - (z[bb] - z[a]) * (x[p] - x[a]);
                d2 = (x[c] - x[bb]) * (z[p] - z[bb]) - (z[c] - z[bb]) * (x[p] - x[bb]);
                d3 = (x[a] - x[c]) * (z[p] - z[c]) - (z[a] - z[c]) * (x[p] - x[c]);
                if (d1 >= 0 && d2 >= 0 && d3 >= 0) inside = 1;
            }
            if (inside) continue;
            tri_idx[cnt * 3] = a; tri_idx[cnt * 3 + 1] = bb; tri_idx[cnt * 3 + 2] = c;
            cnt++;
            memmove(&V[i], &V[i + 1], sizeof(int) * (size_t)(nv - i - 1));
            nv--;
            found = 1;
            break;
        }
        if (!found) break;
    }
    for (i = 1; nv > 2 && i + 1 < nv; i++) {
        tri_idx[cnt * 3] = V[0]; tri_idx[cnt * 3 + 1] = V[i]; tri_idx[cnt * 3 + 2] = V[i + 1];
        cnt++;
    }
    return cnt;
}

/* wereldcoordinaten van een feature, tegen de klok in (area > 0 in x,z) */
static float wx_[MAXP], wz_[MAXP];
static int to_world(BCtx *b, const TFeat *f, int closed) {
    int i, n = f->n > MAXP ? MAXP : f->n;
    float area = 0;
    for (i = 0; i < n; i++) { wx_[i] = b->wx0 + f->x[i]; wz_[i] = b->wzN + TILE_M - f->y[i]; }
    if (!closed) return n;
    for (i = 0; i < n; i++) { int j = (i + 1) % n; area += wx_[i] * wz_[j] - wx_[j] * wz_[i]; }
    if (area < 0) {
        for (i = 0; i < n / 2; i++) {
            float t = wx_[i]; wx_[i] = wx_[n - 1 - i]; wx_[n - 1 - i] = t;
            t = wz_[i]; wz_[i] = wz_[n - 1 - i]; wz_[n - 1 - i] = t;
        }
    }
    return n;
}

static void raster_poly(BCtx *b, const float *x, const float *z, int n, int r, int g, int bl) {
    int y, i, R = TILE_RASTER;
    float sc = R / TILE_M;
    uint8_t *img = b->tb->raster;
    for (y = 0; y < R; y++) {
        float wz = b->wzN + (y + 0.5f) / sc;
        float xs[96];
        int nx = 0, k;
        for (i = 0; i < n && nx < 96; i++) {
            int j = (i + 1) % n;
            if ((z[i] <= wz) != (z[j] <= wz)) xs[nx++] = x[i] + (wz - z[i]) / (z[j] - z[i]) * (x[j] - x[i]);
        }
        for (i = 1; i < nx; i++) { float v = xs[i]; k = i - 1; while (k >= 0 && xs[k] > v) { xs[k + 1] = xs[k]; k--; } xs[k + 1] = v; }
        for (i = 0; i + 1 < nx; i += 2) {
            int x0 = (int)((xs[i] - b->wx0) * sc + 0.5f), x1 = (int)((xs[i + 1] - b->wx0) * sc - 0.5f), px;
            if (x0 < 0) x0 = 0;
            if (x1 >= R) x1 = R - 1;
            for (px = x0; px <= x1; px++) {
                uint8_t *p = &img[(y * R + px) * 4];
                p[0] = (uint8_t)r; p[1] = (uint8_t)g; p[2] = (uint8_t)bl; p[3] = 255;
            }
        }
    }
}

static void raster_line(BCtx *b, float x0, float z0, float x1, float z1, float wdt, int r, int g, int bl) {
    int R = TILE_RASTER, steps, s;
    float sc = R / TILE_M, rad = fmaxf(0.6f, wdt * 0.5f * sc);
    float len = sqrtf((x1 - x0) * (x1 - x0) + (z1 - z0) * (z1 - z0)) * sc;
    steps = (int)(len * 2) + 1;
    for (s = 0; s <= steps; s++) {
        float t = (float)s / steps;
        float px = (x0 + (x1 - x0) * t - b->wx0) * sc, pz = (z0 + (z1 - z0) * t - b->wzN) * sc;
        int ix, iz, ri = (int)ceilf(rad);
        for (iz = (int)pz - ri; iz <= (int)pz + ri; iz++)
            for (ix = (int)px - ri; ix <= (int)px + ri; ix++) {
                float dx = ix + 0.5f - px, dz = iz + 0.5f - pz;
                uint8_t *p;
                if (ix < 0 || iz < 0 || ix >= R || iz >= R || dx * dx + dz * dz > rad * rad + 0.25f) continue;
                p = &b->tb->raster[(iz * R + ix) * 4];
                p[0] = (uint8_t)r; p[1] = (uint8_t)g; p[2] = (uint8_t)bl; p[3] = 255;
            }
    }
}

static void flat_poly(BCtx *b, float *x, float *z, int n, float y, Col c) {
    int t, nt;
    if (n > 300) {
        int step = (n + 299) / 300, m = 0, i;
        for (i = 0; i < n; i += step) { x[m] = x[i]; z[m] = z[i]; m++; }
        n = m;
    }
    nt = triangulate(x, z, n);
    for (t = 0; t < nt; t++) {
        int a = tri_idx[t * 3], bb = tri_idx[t * 3 + 1], d = tri_idx[t * 3 + 2];
        MeshB *m = sub_at(b, (x[a] + x[bb] + x[d]) / 3, (z[a] + z[bb] + z[d]) / 3);
        mb_tri(m, v3(x[a], y, z[a]), v3(x[bb], y, z[bb]), v3(x[d], y, z[d]), v3(0, 1, 0), c);
    }
}

static Col terrain_col(int code) {
    switch (code) {
    case 1: return col(112, 162, 82);
    case 2: return col(62, 112, 56);
    case 3: return col(156, 150, 96);
    case 4: return col(168, 164, 154);
    case 5: return col(214, 202, 152);
    case 6: return col(98, 140, 72);
    case 7: return col(118, 150, 102);
    case 8: return col(122, 112, 104);
    case 9: return col(140, 118, 120);
    default: return col(150, 156, 128);
    }
}

static void add_seg(BCtx *b, float x0, float z0, float x1, float z1, float h) {
    TileBuilt *t = b->tb;
    if (t->nsegs >= b->capsegs) {
        int nc = b->capsegs ? b->capsegs * 2 : 1024;
        WSeg *ns = (WSeg *)realloc(t->segs, (size_t)nc * sizeof(WSeg));
        if (!ns) return;
        t->segs = ns; b->capsegs = nc;
    }
    t->segs[t->nsegs].x0 = x0; t->segs[t->nsegs].z0 = z0; t->segs[t->nsegs].x1 = x1; t->segs[t->nsegs].z1 = z1; t->segs[t->nsegs].h = h;
    t->nsegs++;
}

/* gevelkleur naar bouwjaar (zoals Nederlandse straten er echt uitzien) */
static Col facade_col(int year, unsigned hsh) {
    static const int y1[][3] = {{118, 56, 44}, {134, 64, 50}, {102, 50, 42}, {146, 80, 58}, {126, 70, 56}};      /* voor 1930: donkere baksteen */
    static const int y2[][3] = {{150, 90, 62}, {164, 102, 70}, {134, 76, 56}, {176, 118, 82}, {142, 98, 74}};     /* 1930-1960: oranje baksteen */
    static const int y3[][3] = {{196, 168, 118}, {182, 158, 120}, {168, 150, 122}, {204, 192, 160}, {150, 132, 110}}; /* 1960-1985: gele steen, beton */
    static const int y4[][3] = {{146, 84, 68}, {208, 204, 194}, {118, 108, 102}, {230, 226, 218}, {88, 86, 92}};    /* na 1985: gemengd/modern */
    const int (*t)[3] = year <= 0 ? (hsh & 1 ? y2 : y3) : (year < 1930 ? y1 : (year < 1960 ? y2 : (year < 1985 ? y3 : y4)));
    int k = (int)(hsh % 5u);
    return col(t[k][0], t[k][1], t[k][2]);
}

static Col roof_col(int sloped, unsigned hsh) {
    static const int sl[][3] = {{150, 68, 50}, {124, 56, 44}, {64, 64, 70}, {90, 62, 54}, {110, 46, 40}};
    static const int fl[][3] = {{84, 84, 88}, {70, 72, 76}, {98, 96, 92}, {60, 62, 64}};
    if (sloped) { int k = (int)((hsh >> 3) % 5u); return col(sl[k][0], sl[k][1], sl[k][2]); }
    { int k = (int)((hsh >> 3) % 4u); return col(fl[k][0], fl[k][1], fl[k][2]); }
}

static unsigned grp_hash(const TFeat *f) { unsigned h = (unsigned)f->grp * 2654435761u; return h ^ (h >> 15); }

static int g_nlBrick;   /* v6.0: gevel van baksteen (oudere panden) */

/* gevel van (x0,z0) naar (x1,z1): van onderkant ya/yb tot bovenkant ta/tb, met ramen als dat kan */
static void wall_strip(BCtx *b, float x0, float z0, float x1, float z1, float ya, float yb, float ta, float tb,
                       Col wall, int windows, unsigned hsh) {
    float ex = x1 - x0, ez = z1 - z0, L = sqrtf(ex * ex + ez * ez), top;
    V3 nrm, dir;
    MeshB *m;
    int floors, fl, wn, q;
    if (L < 0.05f || (ta - ya < 0.05f && tb - yb < 0.05f)) return;
    nrm = v3(ez / L, 0, -ex / L);
    dir = v3(ex / L, 0, ez / L);
    m = sub_at(b, (x0 + x1) * 0.5f, (z0 + z1) * 0.5f);
    g_meshMat = g_nlBrick ? MAT_BRICK : MAT_DEFAULT;
    mb_quad4(m, v3(x0, ya, z0), v3(x1, yb, z1), v3(x1, tb, z1), v3(x0, ta, z0), nrm,
             col_mul(wall, ya < 0.1f ? 0.72f : 0.95f), col_mul(wall, yb < 0.1f ? 0.72f : 0.95f), wall, wall);
    g_meshMat = MAT_DEFAULT;
    if (!windows || L < 2.6f) return;
    top = fminf(ta, tb);
    floors = (int)((top - 0.4f) / 2.9f);
    if (floors > 8) floors = 8;
    wn = (int)(L / 2.8f);
    if (wn > 6) wn = 6;
    if (wn < 1) wn = 1;
    for (fl = 0; fl < floors; fl++) {
        float y = fl * 2.9f + (fl == 0 ? 0.8f : 0.9f), wh = fl == 0 ? 1.6f : 1.4f;
        if (y + wh > top - 0.35f) break;
        for (q = 0; q < wn; q++) {
            float tc = L * (q + 0.5f) / wn, hw = fminf(0.62f, L / wn * 0.32f);
            int lit = ((hsh >> (q + fl * 3)) & 7) == 0;
            Col c = lit ? col_e(255, 214, 140, 255) : col(44, 56, 72);
            Col fr = col(232, 230, 222);
            V3 a = v3(x0 + dir.x * (tc - hw) + nrm.x * 0.03f, y, z0 + dir.z * (tc - hw) + nrm.z * 0.03f);
            V3 c2 = v3(x0 + dir.x * (tc + hw) + nrm.x * 0.03f, y, z0 + dir.z * (tc + hw) + nrm.z * 0.03f);
            V3 fa = v3(a.x - dir.x * 0.07f, y - 0.07f, a.z - dir.z * 0.07f), fc = v3(c2.x + dir.x * 0.07f, y - 0.07f, c2.z + dir.z * 0.07f);
            if (fl == 0 && q == wn / 2 && ((hsh >> 5) & 1) && L > 4.0f) {   /* voordeur */
                mb_quad(m, v3(a.x, 0.0f, a.z), v3(c2.x, 0.0f, c2.z), v3(c2.x, 2.2f, c2.z), v3(a.x, 2.2f, a.z), nrm, col_mul(wall, 0.45f));
                continue;
            }
            mb_quad(m, fa, fc, v3(fc.x, y + wh + 0.07f, fc.z), v3(fa.x, y + wh + 0.07f, fa.z), nrm, fr);   /* kozijn */
            a.x += nrm.x * 0.01f; a.z += nrm.z * 0.01f; c2.x += nrm.x * 0.01f; c2.z += nrm.z * 0.01f;
            g_meshMat = MAT_GLASS;
            mb_quad(m, a, c2, v3(c2.x, y + wh, c2.z), v3(a.x, y + wh, a.z), nrm, c);
            g_meshMat = MAT_DEFAULT;
        }
    }
}

static void roof_poly(BCtx *b, const float *x, const float *z, const float *y, int n, V3 nrm, Col c) {
    int t, cnt = triangulate(x, z, n);
    g_meshMat = nrm.y < 0.97f ? MAT_ROOF : MAT_DEFAULT;
    MeshB *m;
    float cx = 0, cz = 0;
    for (t = 0; t < n; t++) { cx += x[t]; cz += z[t]; }
    m = sub_at(b, cx / n, cz / n);
    for (t = 0; t < cnt; t++) {
        int a = tri_idx[t * 3], bb = tri_idx[t * 3 + 1], cc = tri_idx[t * 3 + 2];
        mb_tri(m, v3(x[a], y[a], z[a]), v3(x[bb], y[bb], z[bb]), v3(x[cc], y[cc], z[cc]), nrm, c);
    }
    g_meshMat = MAT_DEFAULT;
}

static float ry_[MAXP];

/* plat dak (doos) of schilddak tot de echte nokhoogte */
static void building(BCtx *b, const TFeat *f) {
    unsigned hsh = grp_hash(f);
    int n = to_world(b, f, 1), i, hip = f->code == BK_HIP;
    float eave = hip ? f->h0 : f->h;
    Col wall = facade_col(f->year, hsh), roof = roof_col(hip, hsh);
    if (n < 3) return;
    g_nlBrick = f->year < 1985 && !(f->year == 0 && f->h > 25);
    if (f->year == 0 && f->h > 25) wall = col(150 + (int)(hsh % 40), 150 + (int)(hsh % 40), 156 + (int)(hsh % 40));
    for (i = 0; i < n; i++) {
        int j = (i + 1) % n;
        wall_strip(b, wx_[i], wz_[i], wx_[j], wz_[j], 0, 0, eave, eave, wall, 1, hsh + (unsigned)i * 7u);
        add_seg(b, wx_[i], wz_[i], wx_[j], wz_[j], eave);
    }
    raster_poly(b, wx_, wz_, n, 70, 66, 74);
    if (!hip) {
        for (i = 0; i < n; i++) ry_[i] = eave;
        roof_poly(b, wx_, wz_, ry_, n, v3(0, 1, 0), roof);
        /* dakrand */
        for (i = 0; i < n; i++) {
            int j = (i + 1) % n;
            float ex = wx_[j] - wx_[i], ez = wz_[j] - wz_[i], L = sqrtf(ex * ex + ez * ez);
            V3 nr;
            if (L < 0.5f) continue;
            nr = v3(ez / L, 0, -ex / L);
            mb_quad(sub_at(b, wx_[i], wz_[i]), v3(wx_[i] + nr.x * 0.08f, eave - 0.25f, wz_[i] + nr.z * 0.08f), v3(wx_[j] + nr.x * 0.08f, eave - 0.25f, wz_[j] + nr.z * 0.08f),
                    v3(wx_[j] + nr.x * 0.08f, eave + 0.15f, wz_[j] + nr.z * 0.08f), v3(wx_[i] + nr.x * 0.08f, eave + 0.15f, wz_[i] + nr.z * 0.08f), nr, col_mul(wall, 0.7f));
        }
    } else {
        /* schilddak: rand op goothoogte, kleinere kopie van de plattegrond op de nok */
        static float ix[MAXP], iz[MAXP];
        float cx = 0, cz = 0, k = 0.28f, rise = f->h - eave;
        for (i = 0; i < n; i++) { cx += wx_[i]; cz += wz_[i]; }
        cx /= n; cz /= n;
        for (i = 0; i < n; i++) { ix[i] = cx + (wx_[i] - cx) * k; iz[i] = cz + (wz_[i] - cz) * k; }
        for (i = 0; i < n; i++) {
            int j = (i + 1) % n;
            float ex = wx_[j] - wx_[i], ez = wz_[j] - wz_[i], L = sqrtf(ex * ex + ez * ez), run;
            V3 nr, rn;
            if (L < 0.02f) continue;
            nr = v3(ez / L, 0, -ex / L);
            run = fabsf((wx_[i] - ix[i]) * nr.x + (wz_[i] - iz[i]) * nr.z) + 0.01f;
            rn = v3norm(v3(nr.x * rise / run, 1.0f, nr.z * rise / run));
            g_meshMat = MAT_ROOF;
            mb_quad4(sub_at(b, (wx_[i] + wx_[j]) * 0.5f, (wz_[i] + wz_[j]) * 0.5f),
                     v3(wx_[i] + nr.x * 0.25f, eave - 0.1f, wz_[i] + nr.z * 0.25f), v3(wx_[j] + nr.x * 0.25f, eave - 0.1f, wz_[j] + nr.z * 0.25f),
                     v3(ix[j], f->h, iz[j]), v3(ix[i], f->h, iz[i]), rn, col_mul(roof, 0.9f), col_mul(roof, 0.9f), roof, roof);
            g_meshMat = MAT_DEFAULT;
        }
        for (i = 0; i < n; i++) ry_[i] = f->h;
        roof_poly(b, ix, iz, ry_, n, v3(0, 1, 0), roof);
    }
    if (f->h > b->tb->maxH) b->tb->maxH = f->h;
    b->tb->nb++;
}

/* ---- echte dakvlakken (3D BAG LoD 2.2): vlak voor vlak, met muren alleen waar je ze kunt zien ---- */
typedef struct { int k[4]; float ya, yb; int face, next, sw; } FEdge;

static int qc(float v) { return (int)floorf(v * 10.0f + 0.5f); }

static void build_faces(BCtx *b, const TileData *td) {
    int nf = 0, i, j, np = 0, ne = 0, hsz = 1, *head, *off, *cnt;
    float *px, *pz, *py;
    FEdge *e;
    for (i = 0; i < td->nb; i++) if (td->b[i].code == BK_FACE) { nf++; np += td->b[i].n > MAXP ? MAXP : td->b[i].n; }
    if (!nf) return;
    px = (float *)malloc(sizeof(float) * (size_t)np); pz = (float *)malloc(sizeof(float) * (size_t)np); py = (float *)malloc(sizeof(float) * (size_t)np);
    off = (int *)malloc(sizeof(int) * (size_t)td->nb); cnt = (int *)calloc((size_t)td->nb, sizeof(int));
    e = (FEdge *)malloc(sizeof(FEdge) * (size_t)np);
    while (hsz < np * 2) hsz <<= 1;
    head = (int *)malloc(sizeof(int) * (size_t)hsz);
    for (i = 0; i < hsz; i++) head[i] = -1;
    np = 0;
    /* 1: hoeken in wereldcoordinaten + hoogte op elk punt van het (vlakke, schuine) dakvlak */
    for (i = 0; i < td->nb; i++) {
        const TFeat *f = &td->b[i];
        int n;
        float dx, dz, smin = 1e9f, smax = -1e9f;
        if (f->code != BK_FACE) continue;
        n = to_world(b, f, 1);
        off[i] = np; cnt[i] = n;
        dx = sinf(f->az * PI_F / 180.0f); dz = -cosf(f->az * PI_F / 180.0f);   /* richting waarin het vlak afloopt */
        for (j = 0; j < n; j++) { float s2 = wx_[j] * dx + wz_[j] * dz; if (s2 < smin) smin = s2; if (s2 > smax) smax = s2; }
        for (j = 0; j < n; j++) {
            px[np + j] = wx_[j]; pz[np + j] = wz_[j];
            if (f->sl <= 0 || smax - smin < 0.05f) py[np + j] = f->h;
            else py[np + j] = f->h - (wx_[j] * dx + wz_[j] * dz - smin) / (smax - smin) * (f->h - f->h0);
        }
        np += n;
    }
    /* 2: randen in een hashtabel, zodat gedeelde randen (tussen vlakken en tussen rijtjeshuizen) gevonden worden */
    for (i = 0; i < td->nb; i++) {
        int n = cnt[i], o = off[i];
        if (td->b[i].code != BK_FACE) continue;
        for (j = 0; j < n; j++) {
            int a = o + j, c = o + (j + 1) % n, k0[2], k1[2], sw;
            unsigned hh;
            FEdge *ed = &e[ne];
            k0[0] = qc(px[a]); k0[1] = qc(pz[a]); k1[0] = qc(px[c]); k1[1] = qc(pz[c]);
            sw = k0[0] > k1[0] || (k0[0] == k1[0] && k0[1] > k1[1]);
            ed->k[0] = sw ? k1[0] : k0[0]; ed->k[1] = sw ? k1[1] : k0[1]; ed->k[2] = sw ? k0[0] : k1[0]; ed->k[3] = sw ? k0[1] : k1[1];
            ed->ya = sw ? py[c] : py[a]; ed->yb = sw ? py[a] : py[c];
            ed->face = i; ed->sw = sw;
            hh = ((unsigned)ed->k[0] * 73856093u) ^ ((unsigned)ed->k[1] * 19349663u) ^ ((unsigned)ed->k[2] * 83492791u) ^ ((unsigned)ed->k[3] * 2654435761u);
            hh &= (unsigned)(hsz - 1);
            ed->next = head[hh]; head[hh] = ne;
            ne++;
        }
    }
    /* 3: dak en muren per vlak */
    ne = 0;
    for (i = 0; i < td->nb; i++) {
        const TFeat *f = &td->b[i];
        int n = cnt[i], o = off[i];
        unsigned hsh = grp_hash(f);
        Col wall, roof;
        V3 nrm;
        if (f->code != BK_FACE) continue;
        wall = facade_col(f->year, hsh); roof = roof_col(f->sl > 0, hsh);
        g_nlBrick = f->year < 1985;
        if (f->sl > 0) {
            float s = sinf(f->sl * PI_F / 180.0f), c = cosf(f->sl * PI_F / 180.0f);
            nrm = v3(s * sinf(f->az * PI_F / 180.0f), c, -s * cosf(f->az * PI_F / 180.0f));
        } else nrm = v3(0, 1, 0);
        roof_poly(b, px + o, pz + o, py + o, n, nrm, f->sl > 0 ? roof : col_mul(roof, 0.95f));
        raster_poly(b, px + o, pz + o, n, 70, 66, 74);
        for (j = 0; j < n; j++, ne++) {
            FEdge *ed = &e[ne], *m = NULL;
            int a = o + j, c = o + (j + 1) % n, k;
            unsigned hh = ((unsigned)ed->k[0] * 73856093u) ^ ((unsigned)ed->k[1] * 19349663u) ^ ((unsigned)ed->k[2] * 83492791u) ^ ((unsigned)ed->k[3] * 2654435761u);
            for (k = head[hh & (unsigned)(hsz - 1)]; k >= 0; k = e[k].next)
                if (k != ne && e[k].face != i && !memcmp(e[k].k, ed->k, sizeof(ed->k))) { m = &e[k]; break; }
            if (!m) {   /* buitenmuur: van de grond tot de dakrand, met ramen */
                wall_strip(b, px[a], pz[a], px[c], pz[c], 0, 0, py[a], py[c], wall, 1, hsh + (unsigned)j * 7u);
                add_seg(b, px[a], pz[a], px[c], pz[c], fminf(py[a], py[c]));
            } else {
                /* gedeelde rand: alleen het stuk dat boven het buurvlak uitsteekt */
                float oa = ed->sw ? m->yb : m->ya, ob = ed->sw ? m->ya : m->yb;
                if (py[a] > oa + 0.3f || py[c] > ob + 0.3f)
                    wall_strip(b, px[a], pz[a], px[c], pz[c], fminf(oa, py[a]), fminf(ob, py[c]), py[a], py[c], wall, 0, hsh);
            }
        }
        if (f->h > b->tb->maxH) b->tb->maxH = f->h;
        b->tb->nb++;
    }
    free(px); free(pz); free(py); free(off); free(cnt); free(e); free(head);
}

static int add_name(BCtx *b, const char *s) {
    TileBuilt *t = b->tb;
    int l, at;
    if (!s || !*s) return -1;
    l = (int)strlen(s) + 1;
    if (t->nameslen + l > b->capnames) {
        int nc = b->capnames ? b->capnames * 2 : 1024;
        char *nn;
        while (nc < t->nameslen + l) nc *= 2;
        nn = (char *)realloc(t->names, (size_t)nc);
        if (!nn) return -1;
        t->names = nn; b->capnames = nc;
    }
    at = t->nameslen;
    memcpy(t->names + at, s, (size_t)l);
    t->nameslen += l;
    return at;
}

static void road(BCtx *b, const TFeat *f, int nameAt) {
    float w = f->w > 0.5f ? f->w : 4.5f;
    Col c = col(64, 64, 70);
    int n = to_world(b, f, 0), i;
    if (f->code == ROAD_BIKE) c = col(176, 82, 70);
    else if (f->code == ROAD_FOOT) c = col(186, 178, 164);
    else if (f->code == ROAD_MAIN) c = col(54, 54, 60);
    for (i = 0; i + 1 < n; i++) {
        float x0 = wx_[i], z0 = wz_[i], x1 = wx_[i + 1], z1 = wz_[i + 1];
        float ex = x1 - x0, ez = z1 - z0, L = sqrtf(ex * ex + ez * ez), hw = w * 0.5f, nx, nz;
        MeshB *m;
        if (L < 0.05f) continue;
        nx = -ez / L * hw; nz = ex / L * hw;
        m = sub_at(b, (x0 + x1) * 0.5f, (z0 + z1) * 0.5f);
        mb_quad(m, v3(x0 + nx, 0.05f, z0 + nz), v3(x1 + nx, 0.05f, z1 + nz), v3(x1 - nx, 0.05f, z1 - nz), v3(x0 - nx, 0.05f, z0 - nz), v3(0, 1, 0), c);
        mb_disc(m, v3(x1, 0.05f, z1), hw, 8, c);
        if (w >= 5.0f && L > 6.0f) {
            float t;
            for (t = 2.0f; t + 2.0f < L; t += 6.0f) {
                float ax = x0 + ex / L * t, az = z0 + ez / L * t, bx = x0 + ex / L * (t + 2.5f), bz = z0 + ez / L * (t + 2.5f);
                float sx = -ez / L * 0.08f, sz = ex / L * 0.08f;
                mb_quad(m, v3(ax + sx, 0.06f, az + sz), v3(bx + sx, 0.06f, bz + sz), v3(bx - sx, 0.06f, bz - sz), v3(ax - sx, 0.06f, az - sz), v3(0, 1, 0), col(225, 225, 220));
            }
        }
        raster_line(b, x0, z0, x1, z1, w, (int)(c.r * 0.9f + 40), (int)(c.g * 0.9f + 40), (int)(c.b * 0.9f + 40));
        {
            TileBuilt *t = b->tb;
            if (t->nrsegs >= b->caprsegs) {
                int nc = b->caprsegs ? b->caprsegs * 2 : 512;
                RSeg *ns = (RSeg *)realloc(t->rsegs, (size_t)nc * sizeof(RSeg));
                if (!ns) continue;
                t->rsegs = ns; b->caprsegs = nc;
            }
            t->rsegs[t->nrsegs].x0 = x0; t->rsegs[t->nrsegs].z0 = z0;
            t->rsegs[t->nrsegs].x1 = x1; t->rsegs[t->nrsegs].z1 = z1;
            t->rsegs[t->nrsegs].name = nameAt;
            t->nrsegs++;
        }
    }
    b->tb->nr++;
}

void tb_build(const TileData *td, float wx0, float wzS, TileBuilt *out) {
    BCtx b;
    int i, *nameAt;
    memset(out, 0, sizeof(*out));
    memset(&b, 0, sizeof(b));
    for (i = 0; i < 4; i++) mb_init(&out->sub[i]);
    b.tb = out;
    b.wx0 = wx0;
    b.wzN = wzS - TILE_M;
    out->raster = (uint8_t *)malloc(TILE_RASTER * TILE_RASTER * 4);
    for (i = 0; i < TILE_RASTER * TILE_RASTER; i++) {
        out->raster[i * 4] = 128; out->raster[i * 4 + 1] = 136; out->raster[i * 4 + 2] = 116; out->raster[i * 4 + 3] = 255;
    }
    for (i = 0; i < td->nt; i++) {
        int n = to_world(&b, &td->t[i], 1);
        Col c = terrain_col(td->t[i].code);
        if (n < 3) continue;
        raster_poly(&b, wx_, wz_, n, (int)(c.r * 0.8f), (int)(c.g * 0.8f), (int)(c.b * 0.8f));
        g_meshMat = c.g > c.r + 10 ? MAT_GRASS : MAT_DEFAULT;
        flat_poly(&b, wx_, wz_, n, 0.01f, c);
        g_meshMat = MAT_DEFAULT;
        out->nt++;
    }
    for (i = 0; i < td->nw; i++) {
        int n = to_world(&b, &td->w[i], 1);
        if (n < 3) continue;
        raster_poly(&b, wx_, wz_, n, 70, 120, 175);
        g_meshMat = MAT_WATER;
        flat_poly(&b, wx_, wz_, n, 0.03f, col_e(58, 104, 150, 30));
        g_meshMat = MAT_DEFAULT;
        out->nw++;
    }
    nameAt = (int *)malloc(sizeof(int) * (size_t)(td->nnames > 0 ? td->nnames : 1));
    for (i = 0; i < td->nnames; i++) nameAt[i] = add_name(&b, td->names[i]);
    g_meshMat = MAT_ASPHALT;
    for (i = 0; i < td->nr; i++) road(&b, &td->r[i], td->r[i].name >= 0 && td->r[i].name < td->nnames ? nameAt[td->r[i].name] : -1);
    g_meshMat = MAT_DEFAULT;
    free(nameAt);
    for (i = 0; i < td->nb; i++) { b.idx = i; if (td->b[i].code != BK_FACE) building(&b, &td->b[i]); }
    build_faces(&b, td);
    for (i = 0; i < 4; i++) out->verts += out->sub[i].n;
}

void tb_free(TileBuilt *tb) {
    int i;
    for (i = 0; i < 4; i++) mb_free(&tb->sub[i]);
    free(tb->segs); free(tb->rsegs); free(tb->names); free(tb->raster);
    memset(tb, 0, sizeof(*tb));
}
