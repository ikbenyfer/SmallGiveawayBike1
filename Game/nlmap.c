#include "nlmap.h"
#include "json.h"
#include "models.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define CRS "http%3A%2F%2Fwww.opengis.net%2Fdef%2Fcrs%2FEPSG%2F0%2F28992"
#define MAXP 4096

static World *W;
static double OX, OY;  /* RD van wereld-oorsprong (x0, y-noord) */

/* ------------------------------------------------------------------ url's */

int nl_normalize_postcode(const char *in, char *out) {
    int n = 0, i;
    for (i = 0; in[i] && n < 6; i++) {
        char c = (char)toupper((unsigned char)in[i]);
        if (c == ' ') continue;
        out[n++] = c;
    }
    out[n] = 0;
    if (n != 6) return 0;
    for (i = 0; i < 4; i++) if (!isdigit((unsigned char)out[i])) return 0;
    if (out[0] == '0') return 0;
    for (i = 4; i < 6; i++) if (out[i] < 'A' || out[i] > 'Z') return 0;
    return 1;
}

void nl_url_geocode(char *buf, int n, const char *pc) {
    snprintf(buf, n, "https://api.pdok.nl/bzk/locatieserver/search/v3_1/free?q=postcode:%s&fq=type:postcode&rows=1&fl=centroide_rd,weergavenaam", pc);
}

void nl_url_bag3d(char *buf, int n, double cx, double cy) {
    snprintf(buf, n, "https://data.3dbag.nl/api/BAG3D/wfs?service=WFS&version=2.0.0&request=GetFeature&typeNames=BAG3D:lod12"
             "&outputFormat=application/json&srsName=EPSG:28992&bbox=%.0f,%.0f,%.0f,%.0f,EPSG:28992"
             "&propertyName=geom,b3_h_70p,b3_h_maaiveld&count=3000",
             cx - NL_HALF, cy - NL_HALF, cx + NL_HALF, cy + NL_HALF);
}

void nl_url_bagpdok(char *buf, int n, double cx, double cy) {
    snprintf(buf, n, "https://api.pdok.nl/kadaster/bag/ogc/v2/collections/pand/items?bbox=%.0f,%.0f,%.0f,%.0f"
             "&bbox-crs=" CRS "&crs=" CRS "&limit=1000&f=json",
             cx - NL_HALF, cy - NL_HALF, cx + NL_HALF, cy + NL_HALF);
}

void nl_url_top10(char *buf, int n, const char *collection, double cx, double cy) {
    snprintf(buf, n, "https://api.pdok.nl/brt/top10nl/ogc/v1/collections/%s/items?bbox=%.0f,%.0f,%.0f,%.0f"
             "&bbox-crs=" CRS "&crs=" CRS "&limit=1000&f=json",
             collection, cx - NL_HALF, cy - NL_HALF, cx + NL_HALF, cy + NL_HALF);
}

int nl_parse_geocode(char *json, long len, NLGeo *out) {
    JArena a = {0};
    JV *root = json_parse(json, len, &a), *resp, *docs, *d;
    const char *pt, *nm;
    int ok = 0;
    memset(out, 0, sizeof(*out));
    resp = json_get(root, "response");
    docs = json_get(resp, "docs");
    d = docs ? docs->child : NULL;
    pt = json_str(json_get(d, "centroide_rd"));
    nm = json_str(json_get(d, "weergavenaam"));
    if (pt) {
        const char *p = strchr(pt, '(');
        if (p && sscanf(p + 1, "%lf %lf", &out->cx, &out->cy) == 2) ok = 1;
    }
    if (nm) snprintf(out->name, sizeof(out->name), "%s", nm);
    json_free(&a);
    return ok;
}

/* ------------------------------------------------------------------ geometrie-hulp */

static float wx_of(double rx) { return (float)(rx - OX); }
static float wz_of(double ry) { return (float)(OY - ry); }

static int chunk_of(float v) {
    int c = (int)floorf(v / (NL_SIZE / CHN));
    return c < 0 ? 0 : (c >= CHN ? CHN - 1 : c);
}
static MeshB *mesh_at(float x, float z) { return &W->chunks[chunk_of(z) * CHN + chunk_of(x)].mb; }

/* ring uit GeoJSON-coordinatenlijst -> wereld-xz */
static int read_ring(const JV *ring, float *xs, float *zs, int max) {
    const JV *p;
    int n = 0;
    if (!ring || ring->type != J_ARR) return 0;
    for (p = ring->child; p && n < max; p = p->next) {
        const JV *a = p->child;
        if (!a || !a->next) continue;
        xs[n] = wx_of(a->num);
        zs[n] = wz_of(a->next->num);
        if (n > 0 && fabsf(xs[n] - xs[n - 1]) < 0.05f && fabsf(zs[n] - zs[n - 1]) < 0.05f) continue;
        n++;
    }
    if (n > 1 && fabsf(xs[n - 1] - xs[0]) < 0.05f && fabsf(zs[n - 1] - zs[0]) < 0.05f) n--;
    return n;
}

static float ring_area(const float *x, const float *z, int n) {
    float a = 0;
    int i;
    for (i = 0; i < n; i++) {
        int j = (i + 1) % n;
        a += x[i] * z[j] - x[j] * z[i];
    }
    return a * 0.5f;
}

/* Sutherland-Hodgman tegen het vierkant [lo, hi] */
static int clip_ring(float *x, float *z, int n, float lo, float hi) {
    static float tx[MAXP * 2], tz[MAXP * 2];
    int e;
    for (e = 0; e < 4 && n > 0; e++) {
        int i, m = 0;
        for (i = 0; i < n; i++) {
            int j = (i + 1) % n;
            float a = e == 0 ? x[i] - lo : e == 1 ? hi - x[i] : e == 2 ? z[i] - lo : hi - z[i];
            float b = e == 0 ? x[j] - lo : e == 1 ? hi - x[j] : e == 2 ? z[j] - lo : hi - z[j];
            if (a >= 0 && m < MAXP * 2) { tx[m] = x[i]; tz[m] = z[i]; m++; }
            if ((a >= 0) != (b >= 0) && m < MAXP * 2) {
                float t = a / (a - b);
                tx[m] = x[i] + (x[j] - x[i]) * t; tz[m] = z[i] + (z[j] - z[i]) * t; m++;
            }
        }
        n = m < MAXP ? m : MAXP;
        memcpy(x, tx, sizeof(float) * n);
        memcpy(z, tz, sizeof(float) * n);
    }
    return n;
}

/* oortjes-knippen; ring moet tegen de klok in (area > 0). Roept emit per driehoek aan. */
static int tri_idx[MAXP * 3];
static int triangulate(const float *x, const float *z, int n) {
    static int V[MAXP];
    int nv = n, cnt = 0, guard = 0, i;
    for (i = 0; i < n; i++) V[i] = i;
    while (nv > 2 && guard < nv * nv + 50) {
        int found = 0;
        for (i = 0; i < nv; i++) {
            int a = V[(i + nv - 1) % nv], b = V[i], c = V[(i + 1) % nv], k, inside = 0;
            float cr = (x[b] - x[a]) * (z[c] - z[a]) - (z[b] - z[a]) * (x[c] - x[a]);
            guard++;
            if (cr <= 1e-6f) continue;
            for (k = 0; k < nv && !inside; k++) {
                int p = V[k];
                float d1, d2, d3;
                if (p == a || p == b || p == c) continue;
                d1 = (x[b] - x[a]) * (z[p] - z[a]) - (z[b] - z[a]) * (x[p] - x[a]);
                d2 = (x[c] - x[b]) * (z[p] - z[b]) - (z[c] - z[b]) * (x[p] - x[b]);
                d3 = (x[a] - x[c]) * (z[p] - z[c]) - (z[a] - z[c]) * (x[p] - x[c]);
                if (d1 >= 0 && d2 >= 0 && d3 >= 0) inside = 1;
            }
            if (inside) continue;
            tri_idx[cnt * 3] = a; tri_idx[cnt * 3 + 1] = b; tri_idx[cnt * 3 + 2] = c;
            cnt++;
            memmove(&V[i], &V[i + 1], sizeof(int) * (nv - i - 1));
            nv--;
            found = 1;
            break;
        }
        if (!found) break;
    }
    if (nv > 2) { /* rest: waaier (gedegenereerde polygonen) */
        for (i = 1; i + 1 < nv; i++) {
            tri_idx[cnt * 3] = V[0]; tri_idx[cnt * 3 + 1] = V[i]; tri_idx[cnt * 3 + 2] = V[i + 1];
            cnt++;
        }
    }
    return cnt;
}

static void flat_poly(float *x, float *z, int n, float y, Col c) {
    int t, nt;
    if (n > 300) {   /* grote terreinvlakken vereenvoudigen (rekentijd op de A5) */
        int step = (n + 299) / 300, m = 0, i;
        for (i = 0; i < n; i += step) { x[m] = x[i]; z[m] = z[i]; m++; }
        n = m;
    }
    nt = triangulate(x, z, n);
    for (t = 0; t < nt; t++) {
        int a = tri_idx[t * 3], b = tri_idx[t * 3 + 1], d = tri_idx[t * 3 + 2];
        MeshB *m = mesh_at((x[a] + x[b] + x[d]) / 3, (z[a] + z[b] + z[d]) / 3);
        mb_tri(m, v3(x[a], y, z[a]), v3(x[b], y, z[b]), v3(x[d], y, z[d]), v3(0, 1, 0), c);
    }
}

/* ------------------------------------------------------------------ minimap */

static void mm_poly(const float *x, const float *z, int n, int r, int g, int b) {
    int y, i, mm = W->mm;
    float sc = mm / W->mmSpan;
    for (y = 0; y < mm; y++) {
        float wz = W->mmOrg + (y + 0.5f) / sc;
        float xs[64];
        int nx = 0, k;
        for (i = 0; i < n && nx < 64; i++) {
            int j = (i + 1) % n;
            if ((z[i] <= wz) != (z[j] <= wz)) xs[nx++] = x[i] + (wz - z[i]) / (z[j] - z[i]) * (x[j] - x[i]);
        }
        for (i = 1; i < nx; i++) { float v = xs[i]; k = i - 1; while (k >= 0 && xs[k] > v) { xs[k + 1] = xs[k]; k--; } xs[k + 1] = v; }
        for (i = 0; i + 1 < nx; i += 2) {
            int x0 = (int)((xs[i] - W->mmOrg) * sc + 0.5f), x1 = (int)((xs[i + 1] - W->mmOrg) * sc - 0.5f), px;
            if (x0 < 0) x0 = 0; if (x1 >= mm) x1 = mm - 1;
            for (px = x0; px <= x1; px++) {
                uint8_t *p = &W->minimap[(y * mm + px) * 4];
                p[0] = (uint8_t)r; p[1] = (uint8_t)g; p[2] = (uint8_t)b; p[3] = 235;
            }
        }
    }
}

static void mm_line(float x0, float z0, float x1, float z1, float wdt, int r, int g, int b) {
    int mm = W->mm, steps, s;
    float sc = mm / W->mmSpan, rad = fmaxf(0.6f, wdt * 0.5f * sc);
    float len = sqrtf((x1 - x0) * (x1 - x0) + (z1 - z0) * (z1 - z0)) * sc;
    steps = (int)(len * 2) + 1;
    for (s = 0; s <= steps; s++) {
        float t = (float)s / steps;
        float px = (x0 + (x1 - x0) * t - W->mmOrg) * sc, pz = (z0 + (z1 - z0) * t - W->mmOrg) * sc;
        int ix, iz, ri = (int)ceilf(rad);
        for (iz = (int)pz - ri; iz <= (int)pz + ri; iz++)
            for (ix = (int)px - ri; ix <= (int)px + ri; ix++) {
                float dx = ix + 0.5f - px, dz = iz + 0.5f - pz;
                if (ix < 0 || iz < 0 || ix >= mm || iz >= mm || dx * dx + dz * dz > rad * rad + 0.25f) continue;
                {
                    uint8_t *p = &W->minimap[(iz * mm + ix) * 4];
                    p[0] = (uint8_t)r; p[1] = (uint8_t)g; p[2] = (uint8_t)b; p[3] = 235;
                }
            }
    }
}

/* ------------------------------------------------------------------ lagen */

static float px_[MAXP], pz_[MAXP];

static void each_polygon(const JV *geom, void (*fn)(float *, float *, int, const JV *), const JV *props) {
    const char *t = json_str(json_get(geom, "type"));
    const JV *co = json_get(geom, "coordinates"), *p;
    if (!t || !co) return;
    if (strcmp(t, "Polygon") == 0) {
        int n = read_ring(co->child, px_, pz_, MAXP);
        if (n >= 3) fn(px_, pz_, n, props);
    } else if (strcmp(t, "MultiPolygon") == 0) {
        for (p = co->child; p; p = p->next) {
            int n = read_ring(p->child, px_, pz_, MAXP);
            if (n >= 3) fn(px_, pz_, n, props);
        }
    }
}

static NLStats *ST;

static Col terrain_col(const char *t) {
    if (!t) return col(150, 156, 128);
    if (strstr(t, "grasland")) return col(112, 162, 82);
    if (strstr(t, "bos")) return col(62, 112, 56);
    if (strstr(t, "akker")) return col(156, 150, 96);
    if (strstr(t, "bebouwd")) return col(168, 164, 154);
    if (strstr(t, "zand")) return col(214, 202, 152);
    if (strstr(t, "kwekerij") || strstr(t, "boomgaard")) return col(98, 140, 72);
    if (strstr(t, "dodenakker")) return col(118, 150, 102);
    if (strstr(t, "spoor")) return col(122, 112, 104);
    return col(150, 156, 128);
}

static void do_terrain(float *x, float *z, int n, const JV *props) {
    Col c = terrain_col(json_str(json_get(props, "typelandgebruik")));
    n = clip_ring(x, z, n, -20.0f, NL_SIZE + 20.0f);
    if (n < 3) return;
    if (ring_area(x, z, n) < 0) { int i; for (i = 0; i < n / 2; i++) { float t = x[i]; x[i] = x[n - 1 - i]; x[n - 1 - i] = t; t = z[i]; z[i] = z[n - 1 - i]; z[n - 1 - i] = t; } }
    mm_poly(x, z, n, c.r * 0.8f, c.g * 0.8f, c.b * 0.8f);
    flat_poly(x, z, n, 0.01f, c);
    ST->terrain++;
}

static void do_water(float *x, float *z, int n, const JV *props) {
    (void)props;
    n = clip_ring(x, z, n, -20.0f, NL_SIZE + 20.0f);
    if (n < 3) return;
    if (ring_area(x, z, n) < 0) { int i; for (i = 0; i < n / 2; i++) { float t = x[i]; x[i] = x[n - 1 - i]; x[n - 1 - i] = t; t = z[i]; z[i] = z[n - 1 - i]; z[n - 1 - i] = t; } }
    mm_poly(x, z, n, 70, 120, 175);
    flat_poly(x, z, n, 0.03f, col_e(58, 104, 150, 30));
    ST->water++;
}

static float BH; /* hoogte voor huidig gebouw */
static int BIDX;

static void add_seg(float x0, float z0, float x1, float z1, float h) {
    if (W->nsegs >= W->capsegs) {
        int nc = W->capsegs ? W->capsegs * 2 : 4096;
        WSeg *ns = (WSeg *)realloc(W->segs, (size_t)nc * sizeof(WSeg));
        if (!ns) return;
        W->segs = ns; W->capsegs = nc;
    }
    W->segs[W->nsegs].x0 = x0; W->segs[W->nsegs].z0 = z0;
    W->segs[W->nsegs].x1 = x1; W->segs[W->nsegs].z1 = z1;
    W->segs[W->nsegs].h = h;
    W->nsegs++;
}

static void do_building(float *x, float *z, int n, const JV *props) {
    static const int walls[][3] = {{150, 72, 58}, {124, 62, 50}, {172, 98, 72}, {112, 84, 74}, {214, 208, 194},
                                   {188, 184, 174}, {140, 140, 146}, {160, 120, 92}};
    float h = BH, area, cx = 0, cz = 0;
    Col wall, roof;
    int i, k = BIDX % 8, floors;
    (void)props;
    for (i = 0; i < n; i++) { cx += x[i]; cz += z[i]; }
    cx /= n; cz /= n;
    if (cx < -30 || cz < -30 || cx > NL_SIZE + 30 || cz > NL_SIZE + 30) return;
    area = ring_area(x, z, n);
    if (fabsf(area) < 4.0f) return;
    if (area < 0) {
        for (i = 0; i < n / 2; i++) { float t = x[i]; x[i] = x[n - 1 - i]; x[n - 1 - i] = t; t = z[i]; z[i] = z[n - 1 - i]; z[n - 1 - i] = t; }
    }
    if (h > 25) k = 6 + (BIDX & 1);
    wall = col(walls[k][0], walls[k][1], walls[k][2]);
    roof = h < 12 ? col(96, 70, 62) : col(84, 84, 88);
    floors = (int)(h / 3.0f);
    if (floors > 10) floors = 10;
    for (i = 0; i < n; i++) {
        int j = (i + 1) % n, f, wn, q;
        float ex = x[j] - x[i], ez = z[j] - z[i], L = sqrtf(ex * ex + ez * ez);
        V3 nrm, dir;
        MeshB *m;
        if (L < 0.05f) continue;
        nrm = v3(ez / L, 0, -ex / L);       /* ring is tegen de klok in: buitenkant */
        dir = v3(ex / L, 0, ez / L);
        m = mesh_at((x[i] + x[j]) * 0.5f, (z[i] + z[j]) * 0.5f);
        mb_quad4(m, v3(x[i], 0, z[i]), v3(x[j], 0, z[j]), v3(x[j], h, z[j]), v3(x[i], h, z[i]), nrm,
                 col_mul(wall, 0.7f), col_mul(wall, 0.7f), wall, wall);
        add_seg(x[i], z[i], x[j], z[j], h);
        if (L < 3.5f) continue;
        wn = (int)(L / 3.2f);
        if (wn > 6) wn = 6;
        for (f = 0; f < floors; f++) {
            float y = f * 3.0f + 1.0f;
            for (q = 0; q < wn; q++) {
                float tc = L * (q + 0.5f) / wn;
                int lit = ((BIDX * 7 + f * 3 + q * 5) % 11) < 3;
                Col c = lit ? col_e(255, 214, 140, 255) : col(46, 58, 76);
                V3 a = v3(x[i] + dir.x * (tc - 0.6f) + nrm.x * 0.04f, y, z[i] + dir.z * (tc - 0.6f) + nrm.z * 0.04f);
                V3 b = v3(x[i] + dir.x * (tc + 0.6f) + nrm.x * 0.04f, y, z[i] + dir.z * (tc + 0.6f) + nrm.z * 0.04f);
                if (y + 1.5f > h - 0.3f) break;
                mb_quad(m, a, b, v3(b.x, y + 1.5f, b.z), v3(a.x, y + 1.5f, a.z), nrm, c);
            }
        }
    }
    mm_poly(x, z, n, 70, 66, 74);
    flat_poly(x, z, n, h, roof);
    if (h > ST->maxH) ST->maxH = h;
    ST->buildings++;
}

static int add_name(const char *s) {
    int l, at;
    if (!s || !*s) return -1;
    l = (int)strlen(s) + 1;
    if (W->nameslen + l > W->namescap) {
        int nc = W->namescap ? W->namescap * 2 : 8192;
        char *nn;
        while (nc < W->nameslen + l) nc *= 2;
        nn = (char *)realloc(W->names, (size_t)nc);
        if (!nn) return -1;
        W->names = nn; W->namescap = nc;
    }
    at = W->nameslen;
    memcpy(W->names + at, s, (size_t)l);
    W->nameslen += l;
    return at;
}

static void road_line(const JV *line, const JV *props) {
    const char *br = json_str(json_get(props, "verhardingsbreedteklasse"));
    const char *gebruik = json_str(json_get(props, "hoofdverkeersgebruik"));
    const char *typeweg = json_str(json_get(props, "typeweg"));
    const char *naam = json_str(json_get(props, "naam"));
    float w = 4.5f;
    Col c = col(64, 64, 70);
    int n, i, nameAt;
    if (br) {
        if (strstr(br, "< 2")) w = 1.8f;
        else if (strstr(br, "2 - 4")) w = 3.4f;
        else if (strstr(br, "4 - 7")) w = 5.8f;
        else if (strstr(br, "> 7")) w = 8.5f;
    }
    if (gebruik && strstr(gebruik, "fiets")) c = col(176, 82, 70);          /* rood fietspad */
    else if (gebruik && strstr(gebruik, "voetgang")) c = col(186, 178, 164);
    else if (typeweg && (strstr(typeweg, "hoofdweg") || strstr(typeweg, "autosnelweg"))) c = col(54, 54, 60);
    n = read_ring(line, px_, pz_, MAXP);
    if (n < 2) return;
    nameAt = add_name(naam);
    for (i = 0; i + 1 < n; i++) {
        float x0 = px_[i], z0 = pz_[i], x1 = px_[i + 1], z1 = pz_[i + 1];
        float ex = x1 - x0, ez = z1 - z0, L = sqrtf(ex * ex + ez * ez), hw = w * 0.5f;
        float nx, nz;
        MeshB *m;
        if (L < 0.05f) continue;
        if ((x0 < -40 && x1 < -40) || (z0 < -40 && z1 < -40) || (x0 > NL_SIZE + 40 && x1 > NL_SIZE + 40) || (z0 > NL_SIZE + 40 && z1 > NL_SIZE + 40)) continue;
        nx = -ez / L * hw; nz = ex / L * hw;
        m = mesh_at((x0 + x1) * 0.5f, (z0 + z1) * 0.5f);
        mb_quad(m, v3(x0 + nx, 0.05f, z0 + nz), v3(x1 + nx, 0.05f, z1 + nz), v3(x1 - nx, 0.05f, z1 - nz), v3(x0 - nx, 0.05f, z0 - nz), v3(0, 1, 0), c);
        if (i + 1 < n - 1 || i == 0) mb_disc(m, v3(x1, 0.05f, z1), hw, 8, c);
        if (w >= 5.0f && L > 6.0f) {   /* middenstreep */
            float t;
            for (t = 2.0f; t + 2.0f < L; t += 6.0f) {
                float ax = x0 + ex / L * t, az = z0 + ez / L * t, bx = x0 + ex / L * (t + 2.5f), bz = z0 + ez / L * (t + 2.5f);
                float sx = -ez / L * 0.08f, sz = ex / L * 0.08f;
                mb_quad(m, v3(ax + sx, 0.06f, az + sz), v3(bx + sx, 0.06f, bz + sz), v3(bx - sx, 0.06f, bz - sz), v3(ax - sx, 0.06f, az - sz), v3(0, 1, 0), col(225, 225, 220));
            }
        }
        mm_line(x0, z0, x1, z1, w, c.r * 0.9f + 40, c.g * 0.9f + 40, c.b * 0.9f + 40);
        if (W->nrsegs >= W->caprsegs) {
            int nc = W->caprsegs ? W->caprsegs * 2 : 2048;
            RSeg *ns = (RSeg *)realloc(W->rsegs, (size_t)nc * sizeof(RSeg));
            if (!ns) continue;
            W->rsegs = ns; W->caprsegs = nc;
        }
        W->rsegs[W->nrsegs].x0 = x0; W->rsegs[W->nrsegs].z0 = z0;
        W->rsegs[W->nrsegs].x1 = x1; W->rsegs[W->nrsegs].z1 = z1;
        W->rsegs[W->nrsegs].name = nameAt;
        W->nrsegs++;
    }
    ST->roads++;
}

static JV *features(char *buf, long len, JArena *a) {
    JV *root;
    if (!buf || len <= 0) return NULL;
    root = json_parse(buf, len, a);
    return json_get(root, "features");
}

/* ------------------------------------------------------------------ bouwen */

int nl_build_world(World *w, double cx, double cy,
                   char *bag, long bagLen, int bagPdok,
                   char *roads, long roadsLen, char *water, long waterLen, char *terrain, long terrainLen,
                   NLStats *st) {
    JArena a;
    JV *fs, *f;
    int i;
    memset(st, 0, sizeof(*st));
    ST = st;
    W = w;
    world_free(w);
    world_init(w);
    w->real = 1;
    w->realSize = NL_SIZE;
    OX = cx - NL_HALF;
    OY = cy + NL_HALF;
    w->mm = 256;
    w->mmOrg = -20.0f;
    w->mmSpan = NL_SIZE + 40.0f;
    w->minimap = (uint8_t *)malloc(256 * 256 * 4);
    for (i = 0; i < 256 * 256; i++) {
        w->minimap[i * 4] = 128; w->minimap[i * 4 + 1] = 136; w->minimap[i * 4 + 2] = 116; w->minimap[i * 4 + 3] = 235;
    }
    /* grond */
    mb_flat(&w->ground, -900.0f, -900.0f, NL_SIZE + 900.0f, NL_SIZE + 900.0f, -0.02f, col(140, 150, 122));

    memset(&a, 0, sizeof(a));
    if ((fs = features(terrain, terrainLen, &a)) != NULL)
        for (f = fs->child; f; f = f->next) each_polygon(json_get(f, "geometry"), do_terrain, json_get(f, "properties"));
    json_free(&a);
    if ((fs = features(water, waterLen, &a)) != NULL)
        for (f = fs->child; f; f = f->next) each_polygon(json_get(f, "geometry"), do_water, json_get(f, "properties"));
    json_free(&a);
    if ((fs = features(roads, roadsLen, &a)) != NULL)
        for (f = fs->child; f; f = f->next) {
            const JV *g = json_get(f, "geometry"), *p = json_get(f, "properties");
            const char *t = json_str(json_get(g, "type"));
            const JV *co = json_get(g, "coordinates"), *l;
            if (!t || !co) continue;
            if (strcmp(t, "LineString") == 0) road_line(co, p);
            else if (strcmp(t, "MultiLineString") == 0) for (l = co->child; l; l = l->next) road_line(l, p);
        }
    json_free(&a);
    st->heightsReal = !bagPdok;
    if ((fs = features(bag, bagLen, &a)) != NULL) {
        BIDX = 0;
        for (f = fs->child; f; f = f->next) {
            const JV *p = json_get(f, "properties");
            float h;
            if (bagPdok) h = 7.0f + (float)((BIDX * 37) % 7);   /* geschat: geen hoogte in PDOK BAG */
            else h = (float)(json_num(json_get(p, "b3_h_70p"), 9.0) - json_num(json_get(p, "b3_h_maaiveld"), 0.0));
            if (h < 2.5f) h = 2.5f;
            if (h > 160.0f) h = 160.0f;
            BH = h;
            each_polygon(json_get(f, "geometry"), do_building, p);
            BIDX++;
        }
    }
    json_free(&a);

    /* startpunt: wegstuk zo dicht mogelijk bij het midden */
    {
        float bd = 1e18f, bx = NL_HALF, bz = NL_HALF, bh = 0;
        for (i = 0; i < w->nrsegs; i++) {
            RSeg *r = &w->rsegs[i];
            float mx = (r->x0 + r->x1) * 0.5f, mz = (r->z0 + r->z1) * 0.5f;
            float d = (mx - NL_HALF) * (mx - NL_HALF) + (mz - NL_HALF) * (mz - NL_HALF);
            float L2 = (r->x1 - r->x0) * (r->x1 - r->x0) + (r->z1 - r->z0) * (r->z1 - r->z0);
            if (L2 < 9.0f) continue;
            if (d < bd) { bd = d; bx = mx; bz = mz; bh = atan2f(r->x1 - r->x0, r->z1 - r->z0); }
        }
        w->poi[POI_SPAWN] = v3(bx, 0, bz);
        w->spawn_heading = bh;
        for (i = 0; i < POI_COUNT; i++) if (i != POI_SPAWN) w->poi[i] = w->poi[POI_SPAWN];
        {
            V3 f = fwd_of(bh), r = right_of(bh);
            w->npark = 0;
            w->park[0].x = bx + r.x * 1.3f + f.x * 0.6f; w->park[0].z = bz + r.z * 1.3f + f.z * 0.6f;
            w->park[0].h = bh; w->park[0].type = VEH_BIKE; w->park[0].variant = 0;
            w->park[1].x = bx + f.x * 7.0f; w->park[1].z = bz + f.z * 7.0f;
            w->park[1].h = bh; w->park[1].type = VEH_CAR; w->park[1].variant = 1;
            w->npark = 2;
        }
    }
    w->ncash = 0; w->ngifts = 0; w->nhouses = 0;
    world_build_grid(w, -60.0f, NL_SIZE + 120.0f);
    st->segs = w->nsegs;

    /* chunk-grenzen */
    w->verts_total = w->ground.n;
    for (i = 0; i < CHN * CHN; i++) {
        Chunk *ch = &w->chunks[i];
        int k;
        ch->x0 = ch->z0 = 1e9f; ch->x1 = ch->z1 = -1e9f; ch->y0 = 0; ch->y1 = 1;
        for (k = 0; k < ch->mb.n; k++) {
            Vtx *v = &ch->mb.v[k];
            if (v->x < ch->x0) ch->x0 = v->x; if (v->x > ch->x1) ch->x1 = v->x;
            if (v->z < ch->z0) ch->z0 = v->z; if (v->z > ch->z1) ch->z1 = v->z;
            if (v->y > ch->y1) ch->y1 = v->y;
        }
        w->verts_total += ch->mb.n;
    }
    st->verts = w->verts_total;
    return st->buildings > 0 || st->roads > 0;
}

const char *nl_street_name(const World *w, float x, float z) {
    int i, best = -1;
    float bd = 14.0f * 14.0f;
    for (i = 0; i < w->nrsegs; i++) {
        const RSeg *r = &w->rsegs[i];
        float ex = r->x1 - r->x0, ez = r->z1 - r->z0, l2 = ex * ex + ez * ez, t, dx, dz, d2;
        if (r->name < 0) continue;
        t = l2 > 1e-6f ? clampf(((x - r->x0) * ex + (z - r->z0) * ez) / l2, 0, 1) : 0;
        dx = x - (r->x0 + ex * t); dz = z - (r->z0 + ez * t); d2 = dx * dx + dz * dz;
        if (d2 < bd) { bd = d2; best = r->name; }
    }
    return best >= 0 ? w->names + best : NULL;
}
