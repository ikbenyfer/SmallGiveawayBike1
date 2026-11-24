/* v7.1.0 PRO-modellen: zie pro_models.h */
#include "pro_models.h"
#include "models.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ SDF-bouwstenen */

static float sd_sph(V3 p, V3 c, float r) { return v3len(v3sub(p, c)) - r; }

static float sd_ell(V3 p, V3 c, V3 r) {
    V3 q = v3sub(p, c);
    V3 a = v3(q.x / r.x, q.y / r.y, q.z / r.z), b = v3(q.x / (r.x * r.x), q.y / (r.y * r.y), q.z / (r.z * r.z));
    float k0 = v3len(a), k1 = v3len(b);
    if (k1 < 1e-6f) return -fminf(r.x, fminf(r.y, r.z));
    return k0 * (k0 - 1.0f) / k1;
}

static float sd_cap(V3 p, V3 a, V3 b, float ra, float rb) {
    V3 ba = v3sub(b, a), pa = v3sub(p, a);
    float t = clampf(v3dot(pa, ba) / fmaxf(v3dot(ba, ba), 1e-9f), 0, 1);
    return v3len(v3sub(pa, v3scale(ba, t))) - (ra + (rb - ra) * t);
}

static float sd_rbox(V3 p, V3 c, V3 h, float r) {
    V3 q = v3(fabsf(p.x - c.x) - h.x + r, fabsf(p.y - c.y) - h.y + r, fabsf(p.z - c.z) - h.z + r);
    V3 mq = v3(fmaxf(q.x, 0), fmaxf(q.y, 0), fmaxf(q.z, 0));
    return v3len(mq) + fminf(fmaxf(q.x, fmaxf(q.y, q.z)), 0.0f) - r;
}

static float smin(float a, float b, float k) {
    float h = clampf(0.5f + 0.5f * (b - a) / k, 0, 1);
    return b + (a - b) * h - k * h * (1.0f - h);
}
static float smax(float a, float b, float k) { return -smin(-a, -b, k); }

static float hash3(float x, float y, float z) {
    float h = sinf(x * 127.1f + y * 311.7f + z * 74.7f) * 43758.5453f;
    return h - floorf(h);
}
static float vnoise(V3 p) {   /* waardenruis in 3D */
    float ix = floorf(p.x), iy = floorf(p.y), iz = floorf(p.z), fx = p.x - ix, fy = p.y - iy, fz = p.z - iz;
    float ux = fx * fx * (3 - 2 * fx), uy = fy * fy * (3 - 2 * fy), uz = fz * fz * (3 - 2 * fz);
    float a = hash3(ix, iy, iz), b = hash3(ix + 1, iy, iz), c = hash3(ix, iy + 1, iz), d = hash3(ix + 1, iy + 1, iz);
    float e = hash3(ix, iy, iz + 1), f = hash3(ix + 1, iy, iz + 1), g = hash3(ix, iy + 1, iz + 1), h = hash3(ix + 1, iy + 1, iz + 1);
    float x0 = a + (b - a) * ux, x1 = c + (d - c) * ux, x2 = e + (f - e) * ux, x3 = g + (h - g) * ux;
    float y0 = x0 + (x1 - x0) * uy, y1 = x2 + (x3 - x2) * uy;
    return y0 + (y1 - y0) * uz;
}

static Col cmix(Col a, Col b, float t) {
    t = clampf(t, 0, 1);
    return col_e((int)(a.r + (b.r - a.r) * t), (int)(a.g + (b.g - a.g) * t), (int)(a.b + (b.b - a.b) * t), (int)(a.a + (b.a - a.a) * t));
}

/* ------------------------------------------------------------------ surface nets (algemeen) */

typedef float (*SdfV)(V3 p, const void *u);
typedef void (*ColV)(V3 p, V3 n, const void *u, Col *c, int *mat);
typedef struct { V3 p, n; Col c; int mat; } SV;

/* filter: 0 alles, 1 alleen lak (MAT_PAINT), 2 alles behalve lak. bottom = ook de onderkant tekenen */
static void sn_mesh(MeshB *m, V3 lo, V3 hi, float cell, SdfV f, ColV cf, const void *u, int filter, int bottom, float aoK) {
    int nx = (int)((hi.x - lo.x) / cell) + 2, ny = (int)((hi.y - lo.y) / cell) + 2, nz = (int)((hi.z - lo.z) / cell) + 2;
    size_t np = (size_t)nx * ny * nz;
    float *val = (float *)malloc(np * sizeof(float));
    int *vid = (int *)malloc(np * sizeof(int));
    SV *sv = NULL;
    int nv = 0, cap = 0, i, j, k, a;
    if (!val || !vid) { free(val); free(vid); return; }
#define IX(i, j, k) (((size_t)(k) * ny + (j)) * nx + (i))
    for (k = 0; k < nz; k++)
        for (j = 0; j < ny; j++)
            for (i = 0; i < nx; i++)
                val[IX(i, j, k)] = f(v3(lo.x + i * cell, lo.y + j * cell, lo.z + k * cell), u);
    for (k = 0; k < nz - 1; k++)
        for (j = 0; j < ny - 1; j++)
            for (i = 0; i < nx - 1; i++) {
                static const int E[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
                float c8[8];
                int q, mask = 0, cnt = 0;
                V3 acc = v3(0, 0, 0);
                for (q = 0; q < 8; q++) {
                    c8[q] = val[IX(i + (q & 1), j + ((q >> 1) & 1), k + ((q >> 2) & 1))];
                    if (c8[q] < 0) mask |= 1 << q;
                }
                vid[IX(i, j, k)] = -1;
                if (mask == 0 || mask == 255) continue;
                for (q = 0; q < 12; q++) {
                    int e0 = E[q][0], e1 = E[q][1];
                    float v0 = c8[e0], v1 = c8[e1], t;
                    if ((v0 < 0) == (v1 < 0)) continue;
                    t = v0 / (v0 - v1);
                    acc = v3add(acc, v3((e0 & 1) + ((e1 & 1) - (e0 & 1)) * t,
                                        ((e0 >> 1) & 1) + (((e1 >> 1) & 1) - ((e0 >> 1) & 1)) * t,
                                        ((e0 >> 2) & 1) + (((e1 >> 2) & 1) - ((e0 >> 2) & 1)) * t));
                    cnt++;
                }
                if (nv >= cap) {
                    int nc = cap ? cap * 2 : 4096;
                    SV *ns = (SV *)realloc(sv, (size_t)nc * sizeof(SV));
                    if (!ns) continue;
                    sv = ns; cap = nc;
                }
                acc = v3scale(acc, 1.0f / (float)cnt);
                sv[nv].p = v3(lo.x + (i + acc.x) * cell, lo.y + (j + acc.y) * cell, lo.z + (k + acc.z) * cell);
                vid[IX(i, j, k)] = nv++;
            }
    for (i = 0; i < nv; i++) {
        V3 p = sv[i].p, n;
        float e = cell * 0.5f, ao = 0, s;
        int mat = MAT_DEFAULT;
        n = v3(f(v3(p.x + e, p.y, p.z), u) - f(v3(p.x - e, p.y, p.z), u),
               f(v3(p.x, p.y + e, p.z), u) - f(v3(p.x, p.y - e, p.z), u),
               f(v3(p.x, p.y, p.z + e), u) - f(v3(p.x, p.y, p.z - e), u));
        n = v3norm(n);
        for (s = 1; s <= 4; s += 1) {
            float d = aoK * s;
            ao += (d - f(v3add(p, v3scale(n, d)), u)) / d * (1.0f / s);
        }
        ao = clampf(1.0f - ao * 0.3f, 0.45f, 1.0f);
        cf(p, n, u, &sv[i].c, &mat);
        sv[i].c.r = (uint8_t)(sv[i].c.r * ao); sv[i].c.g = (uint8_t)(sv[i].c.g * ao); sv[i].c.b = (uint8_t)(sv[i].c.b * ao);
        sv[i].n = n;
        sv[i].mat = mat;
    }
    for (k = 0; k < nz - 1; k++)
        for (j = 0; j < ny - 1; j++)
            for (i = 0; i < nx - 1; i++)
                for (a = 0; a < 3; a++) {
                    int c[4], q;
                    float v0 = val[IX(i, j, k)], v1;
                    if (a == 0) { v1 = val[IX(i + 1, j, k)]; if (j < 1 || k < 1) continue;
                        c[0] = vid[IX(i, j - 1, k - 1)]; c[1] = vid[IX(i, j, k - 1)]; c[2] = vid[IX(i, j, k)]; c[3] = vid[IX(i, j - 1, k)]; }
                    else if (a == 1) { v1 = val[IX(i, j + 1, k)]; if (i < 1 || k < 1) continue;
                        c[0] = vid[IX(i - 1, j, k - 1)]; c[1] = vid[IX(i, j, k - 1)]; c[2] = vid[IX(i, j, k)]; c[3] = vid[IX(i - 1, j, k)]; }
                    else { v1 = val[IX(i, j, k + 1)]; if (i < 1 || j < 1) continue;
                        c[0] = vid[IX(i - 1, j - 1, k)]; c[1] = vid[IX(i, j - 1, k)]; c[2] = vid[IX(i, j, k)]; c[3] = vid[IX(i - 1, j, k)]; }
                    if ((v0 < 0) == (v1 < 0)) continue;
                    for (q = 0; q < 4; q++) if (c[q] < 0) break;
                    if (q < 4) continue;
                    for (q = 0; q < 2; q++) {
                        const SV *A = &sv[c[0]], *B = &sv[c[1 + q]], *C = &sv[c[2 + q]];
                        V3 fn = v3cross(v3sub(B->p, A->p), v3sub(C->p, A->p));
                        int pa = A->mat == MAT_PAINT;
                        if (filter == 1 && !pa) continue;
                        if (filter == 2 && pa) continue;
                        if (!bottom && A->n.y < -0.85f && B->n.y < -0.85f && C->n.y < -0.85f) continue;
                        if (v3dot(fn, v3add(A->n, v3add(B->n, C->n))) < 0) { const SV *t = B; B = C; C = t; }
                        mb_tri_nm(m, A->p, B->p, C->p, A->n, B->n, C->n, A->c, B->c, C->c, A->mat);
                    }
                }
#undef IX
    free(val); free(vid); free(sv);
}

/* ------------------------------------------------------------------ auto's */

typedef struct {
    float W, L, yb, yt, roof, rW, cz, cL, wsF, wsR, nose, tail;
    float wx, wzf, wzr, wr, wrr, yL;
    int scoop, wing, haunch, police, taxi, traffic;
    Col paint;
} CarP;

static CarP car_params(int model, int traffic, int variant) {
    CarP c;
    memset(&c, 0, sizeof(c));
    /* standaard: sedan/hatchback (verkeer en CITY HATCH) */
    c.W = 0.92f; c.L = 2.03f; c.yb = 0.31f; c.yt = 0.95f; c.roof = 1.49f; c.rW = 0.7f;
    c.cz = -0.22f; c.cL = 0.62f; c.wsF = 0.52f; c.wsR = 0.3f; c.nose = 0.16f; c.tail = 0.12f;
    c.wx = 0.84f; c.wzf = 1.3f; c.wzr = -1.3f; c.wr = 0.34f; c.wrr = 0.34f; c.yL = 0.74f;
    c.traffic = traffic;
    c.paint = traffic ? car_color(variant) : col(236, 236, 236);
    if (traffic) { c.police = variant == POLICE_VARIANT; c.taxi = variant == 2; return c; }
    switch (model) {
    case 1:   /* RHINO V8 */
        c.W = 0.95f; c.L = 2.3f; c.yb = 0.31f; c.yt = 0.92f; c.roof = 1.43f; c.rW = 0.74f;
        c.cz = -0.5f; c.cL = 0.56f; c.wsF = 0.6f; c.wsR = 0.38f; c.nose = 0.1f; c.tail = 0.1f;
        c.wx = 0.9f; c.wzf = 1.45f; c.wzr = -1.45f; c.wr = 0.35f; c.wrr = 0.38f; c.yL = 0.7f; c.scoop = 1;
        break;
    case 2:   /* VORTEX GT: laag, fastback */
        c.W = 0.96f; c.L = 2.18f; c.yb = 0.24f; c.yt = 0.74f; c.roof = 1.12f; c.rW = 0.62f;
        c.cz = -0.32f; c.cL = 0.38f; c.wsF = 0.72f; c.wsR = 0.85f; c.nose = 0.3f; c.tail = 0.25f;
        c.wx = 0.9f; c.wzf = 1.4f; c.wzr = -1.35f; c.wr = 0.34f; c.wrr = 0.35f; c.yL = 0.55f; c.haunch = 1;
        break;
    case 3:   /* BOLT X: wig met vleugel */
        c.W = 1.0f; c.L = 2.28f; c.yb = 0.21f; c.yt = 0.66f; c.roof = 1.02f; c.rW = 0.56f;
        c.cz = 0.05f; c.cL = 0.34f; c.wsF = 0.85f; c.wsR = 0.95f; c.nose = 0.42f; c.tail = 0.15f;
        c.wx = 0.95f; c.wzf = 1.45f; c.wzr = -1.45f; c.wr = 0.35f; c.wrr = 0.37f; c.yL = 0.46f; c.wing = 1; c.haunch = 1;
        break;
    default:  /* CITY HATCH: korte, steile achterkant */
        c.cz = -0.3f; c.cL = 0.68f; c.wsR = 0.1f; c.tail = 0.05f;
        break;
    }
    return c;
}

/* cabine-vorm op hoogte p.y: halve breedte en voor/achtergrens */
static void cab_shape(const CarP *c, float y, float *hw, float *zf, float *zr) {
    float t = clampf((y - c->yt) / (c->roof - c->yt), 0, 1);
    *hw = c->W * 0.93f + (c->rW - c->W * 0.93f) * t;
    *zf = c->cz + c->cL + c->wsF * (1.0f - t);
    *zr = c->cz - c->cL - c->wsR * (1.0f - t);
}

static float car_sdf(V3 p, const void *u) {
    const CarP *c = (const CarP *)u;
    float W = c->W, L = c->L, d, cab, hw, zf, zr, ax = fabsf(p.x);
    int k;
    /* onderste carrosserie */
    d = sd_rbox(p, v3(0, (c->yb + c->yt) * 0.5f, 0), v3(W, (c->yt - c->yb) * 0.5f, L), 0.15f);
    d = smax(d, (p.y - c->yt + c->nose * fmaxf(0.0f, p.z - (L - 1.0f))) * 0.95f, 0.08f);          /* motorkap loopt af */
    d = smax(d, (p.y - c->yt + c->tail * fmaxf(0.0f, -p.z - (L - 0.6f))) * 0.97f, 0.06f);         /* kofferbak */
    d = smax(d, ax - W + 0.035f * clampf((c->yb + 0.1f - p.y) / 0.1f, 0, 1), 0.02f);              /* dorpel iets naar binnen */
    /* cabine met schuine ruiten */
    cab_shape(c, p.y, &hw, &zf, &zr);
    cab = fmaxf(fmaxf(ax - hw, fmaxf(p.z - zf, zr - p.z)), fmaxf(p.y - c->roof, c->yt - 0.08f - p.y));
    d = smin(d, cab * 0.78f, 0.07f);
    /* spatborden (lage auto's: anders snijdt de wielkast door de motorkap), daarna de wielkasten */
    for (k = 0; k < 2; k++) {
        float wz = k ? c->wzr : c->wzf, wr = k ? c->wrr : c->wr;
        if (c->haunch || wr * 2.0f + 0.16f > c->yt) d = smin(d, sd_ell(v3(ax, p.y, p.z), v3(W - 0.1f, wr + 0.17f, wz), v3(0.18f, 0.27f, 0.6f)), 0.08f);
    }
    for (k = 0; k < 2; k++) {
        float wz = k ? c->wzr : c->wzf, wr = k ? c->wrr : c->wr;
        float cy = sqrtf((p.y - wr) * (p.y - wr) + (p.z - wz) * (p.z - wz)) - (wr + 0.075f);
        d = smax(d, -fmaxf(cy, (W - 0.42f) - ax), 0.025f);
    }
    /* bumpers */
    d = smin(d, sd_rbox(p, v3(0, c->yb + 0.11f, L - 0.02f), v3(W - 0.03f, 0.1f, 0.08f), 0.05f), 0.03f);
    d = smin(d, sd_rbox(p, v3(0, c->yb + 0.11f, -L + 0.02f), v3(W - 0.03f, 0.1f, 0.08f), 0.05f), 0.03f);
    /* spiegels */
    {
        float zfb, hwb, zrb;
        cab_shape(c, c->yt + 0.02f, &hwb, &zfb, &zrb);
        d = fminf(d, sd_ell(v3(ax, p.y, p.z), v3(W + 0.07f, c->yt + 0.1f, zfb - 0.12f), v3(0.075f, 0.05f, 0.055f)));
        d = fminf(d, sd_cap(v3(ax, p.y, p.z), v3(W - 0.08f, c->yt + 0.05f, zfb - 0.1f), v3(W + 0.03f, c->yt + 0.09f, zfb - 0.12f), 0.015f, 0.015f));
    }
    if (c->scoop) d = smin(d, sd_rbox(p, v3(0, c->yt + 0.02f, 1.05f), v3(0.3f, 0.06f, 0.42f), 0.04f), 0.04f);
    if (c->wing) {
        d = fminf(d, sd_rbox(p, v3(0, 1.06f, -2.0f), v3(0.98f, 0.022f, 0.19f), 0.015f));
        d = fminf(d, sd_rbox(v3(ax, p.y, p.z), v3(0.62f, 0.88f, -2.0f), v3(0.025f, 0.17f, 0.09f), 0.01f));
    }
    if (c->police) d = fminf(d, sd_rbox(p, v3(0, c->roof + 0.05f, c->cz), v3(0.55f, 0.045f, 0.13f), 0.03f));
    if (c->taxi) d = fminf(d, sd_rbox(p, v3(0, c->roof + 0.08f, c->cz + 0.05f), v3(0.22f, 0.07f, 0.11f), 0.03f));
    return d;
}

static void car_col(V3 p, V3 n, const void *u, Col *out, int *mat) {
    const CarP *c = (const CarP *)u;
    float W = c->W, L = c->L, ax = fabsf(p.x), hw, zf, zr, an = fabsf(n.x);
    Col paint = c->paint, black = col(22, 22, 24), chrome = col(205, 207, 212), glass = col(26, 36, 48);
    int k;
    *mat = MAT_PAINT; *out = paint;
    cab_shape(c, p.y, &hw, &zf, &zr);
    /* zwaailichtbalk / taxibord */
    if (c->police && p.y > c->roof + 0.005f && fabsf(p.z - c->cz) < 0.16f && ax < 0.58f) {
        *out = p.x < 0 ? col_e(230, 30, 30, 160) : col_e(40, 70, 240, 160); *mat = MAT_SHINY; return;
    }
    if (c->taxi && p.y > c->roof + 0.01f && ax < 0.25f) { *out = col_e(255, 220, 90, 170); *mat = MAT_SHINY; return; }
    /* spiegels */
    if (ax > W - 0.02f && p.y > c->yt + 0.03f && p.y < c->yt + 0.17f && p.z < c->cz + c->cL + c->wsF) {
        if (n.z < -0.55f) { *out = chrome; *mat = MAT_SHINY; }
        return;
    }
    /* ruiten en stijlen */
    if (p.y > c->yt + 0.035f && p.y < c->roof - 0.035f && !(c->wing && p.z < -1.7f)) {
        float bz = c->cz + 0.04f;
        int pillar = 0;
        if (n.y > 0.82f) return;   /* dak */
        if (an > 0.35f) {   /* zijkant */
            if (p.z > zf - 0.07f || p.z < zr + (c->wsR > 0.5f ? 0.1f : 0.16f) || fabsf(p.z - bz) < 0.045f) pillar = 1;
            if (fabsf(p.z - bz) < 0.045f && !pillar) pillar = 1;
        } else if (ax > hw - 0.065f) pillar = 1;
        if (p.y < c->yt + 0.06f || p.y > c->roof - 0.06f) pillar = 2;   /* rubberrand */
        if (pillar == 1) { if (fabsf(p.z - bz) < 0.045f && an > 0.35f) { *out = black; *mat = MAT_DEFAULT; } return; }
        if (pillar == 2) { *out = black; *mat = MAT_DEFAULT; return; }
        *out = glass; *mat = MAT_GLASS; return;
    }
    /* wielkast binnenkant */
    for (k = 0; k < 2; k++) {
        float wz = k ? c->wzr : c->wzf, wr = k ? c->wrr : c->wr;
        float cy = sqrtf((p.y - wr) * (p.y - wr) + (p.z - wz) * (p.z - wz));
        /* binnenkant: normaal wijst naar de wielas */
        if (cy < wr + 0.1f && ax < W - 0.025f && (n.y * (p.y - wr) + n.z * (p.z - wz)) < -0.3f * cy) { *out = col(18, 18, 20); *mat = MAT_RUBBER; return; }
    }
    /* koplampen, richtingaanwijzers, achterlichten */
    if (p.z > L - 0.3f && n.z > 0.2f && fabsf(p.y - c->yL) < 0.055f && ax > W - 0.44f && ax < W - 0.05f) {
        if (ax > W - 0.12f) *out = col_e(255, 170, 40, 120);
        else *out = cmix(col_e(250, 246, 232, 255), chrome, fabsf(p.y - c->yL) > 0.042f ? 1.0f : 0.0f);
        *mat = MAT_SHINY; return;
    }
    if (p.z < -L + 0.3f && n.z < -0.2f && fabsf(p.y - c->yL - 0.03f) < 0.05f && ax > W - 0.42f && ax < W - 0.04f) {
        *out = ax > W - 0.13f ? col_e(255, 160, 40, 110) : col_e(205, 16, 18, 150); *mat = MAT_SHINY; return;
    }
    /* grille */
    if (c->nose >= 0.25f) {   /* sportwagen: smalle luchtinlaat onderin in plaats van een grote grille */
        if (p.z > L - 0.25f && n.z > 0.3f && ax < W * 0.62f && p.y > c->yb + 0.06f && p.y < c->yb + 0.14f) {
            *out = fmodf((p.x + 4.0f) * 30.0f, 1.0f) < 0.25f ? col(60, 60, 64) : col(10, 10, 12); *mat = MAT_SHINY; return;
        }
    } else if (p.z > L - 0.25f && n.z > 0.3f && ax < W * 0.42f && p.y < c->yL + 0.035f && p.y > c->yL - 0.13f) {
        *out = fmodf((p.y + 4.0f) * 55.0f, 1.0f) < 0.3f ? col(70, 70, 74) : col(10, 10, 12); *mat = MAT_SHINY; return;
    }
    /* kentekens */
    if (fabsf(n.z) > 0.5f && ax < 0.21f && fabsf(p.y - (c->yb + 0.13f)) < 0.05f && fabsf(p.z) > L - 0.15f) {
        *out = col(232, 226, 150); *mat = MAT_DEFAULT;
        if (fabsf(p.y - (c->yb + 0.13f)) < 0.025f && fmodf((p.x + 4.0f) * 22.0f, 1.0f) < 0.45f) *out = col(30, 30, 34);
        return;
    }
    /* bumpers en dorpels: donker kunststof */
    if ((fabsf(p.z) > L - 0.12f && p.y < c->yb + 0.2f) || p.y < c->yb + 0.05f) { *out = col(30, 30, 33); *mat = MAT_RUBBER; return; }
    /* deurgrepen */
    if (an > 0.6f && fabsf(p.y - (c->yt - 0.07f)) < 0.013f) {
        float bz = c->cz + 0.04f;
        if (fabsf(p.z - (bz + 0.55f)) < 0.065f || fabsf(p.z - (bz - 0.32f)) < 0.065f) { *out = chrome; *mat = MAT_SHINY; return; }
    }
    /* politie: zwart-wit */
    if (c->police && an > 0.4f && fabsf(p.z - c->cz) < 0.95f && p.y < c->yt + 0.02f) *out = col(236, 236, 240);
    if (c->police && p.y > c->yt - 0.02f && n.y > 0.6f && fabsf(p.z - c->cz) < 0.9f) *out = col(236, 236, 240);
    /* deurnaden (in de lak) */
    if (an > 0.55f && p.y > c->yb + 0.08f && p.y < c->yt + 0.03f) {
        float bz = c->cz + 0.04f;
        if (fabsf(p.z - (bz + 0.9f)) < 0.006f || fabsf(p.z - bz) < 0.006f || fabsf(p.z - (bz - 0.85f)) < 0.006f) *out = col_mul(*out, 0.3f);
    }
    if (n.y > 0.6f && fabsf(p.z - (L - 1.02f)) < 0.006f) *out = col_mul(*out, 0.35f);   /* motorkapnaad */
    if (c->wing && p.y > 0.95f && p.z < -1.7f) *out = c->traffic ? *out : col(236, 236, 236);
}

static void car_build(MeshB *m, const CarP *c, int filter) {
    V3 lo = v3(-c->W - 0.18f, c->yb - 0.04f, -c->L - 0.12f), hi = v3(c->W + 0.18f, (c->wing ? 1.12f : c->roof + 0.2f), c->L + 0.12f);
    sn_mesh(m, lo, hi, 0.026f, car_sdf, car_col, c, filter, 0, 0.05f);
}

void pro_car_traffic(MeshB *m, int v) {
    CarP c = car_params(0, 1, v);
    car_build(m, &c, 0);
    g_meshMat = MAT_SHINY;
    model_wheels4(m, c.wx, c.wzf, c.wzr, c.wr, c.wrr, col(192, 194, 200));
    g_meshMat = MAT_DEFAULT;
}

void pro_car_body(MeshB *m, int model) {
    CarP c = car_params(model % CAR_MODELS, 0, 0);
    car_build(m, &c, 1);
}

void pro_car_detail(MeshB *m, int model) {
    CarP c = car_params(model % CAR_MODELS, 0, 0);
    car_build(m, &c, 2);
    g_meshMat = MAT_SHINY;
    model_wheels4(m, c.wx, c.wzf, c.wzr, c.wr, c.wrr, col(195, 197, 204));
    if (model % CAR_MODELS == 1) {   /* dubbele uitlaat */
        int k;
        for (k = -1; k <= 1; k += 2) mb_rtube(m, v3(0.35f * k, 0.3f, -2.2f), v3(0.35f * k, 0.3f, -2.45f), 0.05f, 0.05f, 14, col(200, 200, 206));
    }
    g_meshMat = MAT_DEFAULT;
}

/* ------------------------------------------------------------------ bomen */

typedef struct { int kind; float s; } TreeP;

static float tree_sdf(V3 p, const void *u) {
    const TreeP *t = (const TreeP *)u;
    float d, can = 1e3f, n;
    int i;
    if (t->kind == 1) {   /* den: stam + gelaagde kegels */
        d = sd_cap(p, v3(0, 0, 0), v3(0, 5.2f, 0), 0.2f, 0.05f);
        for (i = 0; i < 5; i++) {
            float y0 = 1.3f + i * 0.85f, r0 = 1.75f - i * 0.3f, hh = 1.3f;
            float q = sqrtf(p.x * p.x + p.z * p.z), yy = p.y - y0;
            float cone = fmaxf(q - r0 * clampf(1.0f - yy / hh, 0, 1), fmaxf(-yy, yy - hh)) * 0.75f;
            can = smin(can, cone, 0.15f);
        }
        n = vnoise(v3(p.x * 5.0f, p.y * 5.0f, p.z * 5.0f)) - 0.5f;
        can += n * 0.12f + 0.04f * sinf(atan2f(p.z, p.x) * 14.0f + p.y * 3.0f);
        return smin(d, can, 0.08f);
    }
    /* loofboom: stam met wortelvoet, takken en een bolle kruin van bladerclusters */
    d = sd_cap(p, v3(0, -0.1f, 0), v3(0.05f, 2.3f, 0.02f), 0.21f, 0.13f);
    d = smin(d, sd_sph(v3(p.x, p.y + 0.25f, p.z), v3(0, 0, 0), 0.42f), 0.25f);   /* wortelvoet */
    {
        static const float br[5][3] = {{0.9f, 3.2f, 0.2f}, {-0.8f, 3.4f, 0.5f}, {0.2f, 3.6f, -0.9f}, {-0.3f, 3.0f, -0.6f}, {0.5f, 3.9f, 0.6f}};
        for (i = 0; i < 5; i++) d = smin(d, sd_cap(p, v3(0.04f, 2.0f + i * 0.12f, 0.02f), v3(br[i][0], br[i][1], br[i][2]), 0.09f, 0.035f), 0.08f);
    }
    if (t->kind == 2) {   /* populier: smal en hoog */
        can = sd_ell(p, v3(0, 4.3f, 0), v3(1.0f, 2.6f, 1.0f));
    } else {
        static const float cl[8][4] = {{0, 3.9f, 0, 1.35f}, {1.0f, 3.5f, 0.3f, 1.0f}, {-0.95f, 3.6f, 0.4f, 1.05f}, {0.2f, 3.6f, -1.0f, 1.0f},
                                       {-0.4f, 3.3f, -0.7f, 0.9f}, {0.5f, 4.5f, 0.5f, 0.95f}, {-0.5f, 4.6f, -0.2f, 0.9f}, {0.7f, 3.1f, 0.9f, 0.8f}};
        for (i = 0; i < 8; i++) can = smin(can, sd_sph(p, v3(cl[i][0], cl[i][1], cl[i][2]), cl[i][3]), 0.35f);
    }
    n = vnoise(v3(p.x * 3.2f, p.y * 3.2f, p.z * 3.2f)) - 0.5f;
    can += n * 0.32f + (vnoise(v3(p.x * 9.0f, p.y * 9.0f, p.z * 9.0f)) - 0.5f) * 0.1f;   /* blaadjes-reliëf */
    return smin(d, can, 0.05f);
}

static void tree_col(V3 p, V3 n, const void *u, Col *c, int *mat) {
    const TreeP *t = (const TreeP *)u;
    float r = sqrtf(p.x * p.x + p.z * p.z);
    int trunk = t->kind == 1 ? (r < 0.24f && p.y < 1.4f) : (p.y < 2.45f && r < 0.5f);
    (void)n;
    if (trunk || (t->kind != 1 && r < 0.12f && p.y < 3.6f)) {   /* bast */
        float b = vnoise(v3(p.x * 18.0f, p.y * 4.0f, p.z * 18.0f));
        *c = col((int)(78 + 40 * b), (int)(58 + 28 * b), (int)(40 + 18 * b)); *mat = MAT_DEFAULT; return;
    }
    {
        float v = vnoise(v3(p.x * 2.0f, p.y * 2.0f, p.z * 2.0f)), w = vnoise(v3(p.x * 11.0f, p.y * 11.0f, p.z * 11.0f));
        float top = clampf((p.y - 2.5f) / 3.0f, 0, 1);
        if (t->kind == 1) *c = col((int)(28 + 22 * v), (int)(70 + 35 * v + 20 * w), (int)(38 + 15 * v));
        else *c = col((int)(55 + 45 * v + 25 * top), (int)(105 + 50 * v + 30 * w + 20 * top), (int)(35 + 20 * v));
        *mat = MAT_GRASS;
    }
}

void pro_tree(MeshB *m, int kind, int hi) {
    TreeP t;
    float cell = hi ? 0.12f : 0.32f;
    t.kind = kind % PRO_TREE_KINDS; t.s = 1.0f;
    sn_mesh(m, v3(-2.7f, -0.35f, -2.7f), v3(2.7f, t.kind == 2 ? 7.2f : 6.3f, 2.7f), cell, tree_sdf, tree_col, &t, 0, 0, hi ? 0.12f : 0.3f);
}
