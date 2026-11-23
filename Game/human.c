/* v7.0.0 PRO-mensen: zie human.h */
#include "human.h"
#include "models.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ SDF-bouwstenen */

static float sd_sph(V3 p, V3 c, float r) { return v3len(v3sub(p, c)) - r; }

static float sd_ell(V3 p, V3 c, V3 r) {   /* benadering (Inigo Quilez) */
    V3 q = v3sub(p, c);
    V3 a = v3(q.x / r.x, q.y / r.y, q.z / r.z), b = v3(q.x / (r.x * r.x), q.y / (r.y * r.y), q.z / (r.z * r.z));
    float k0 = v3len(a), k1 = v3len(b);
    if (k1 < 1e-6f) return -fminf(r.x, fminf(r.y, r.z));
    return k0 * (k0 - 1.0f) / k1;
}

static float sd_cap(V3 p, V3 a, V3 b, float ra, float rb) {   /* capsule met verlopende straal */
    V3 ba = v3sub(b, a), pa = v3sub(p, a);
    float t = clampf(v3dot(pa, ba) / fmaxf(v3dot(ba, ba), 1e-9f), 0, 1);
    return v3len(v3sub(pa, v3scale(ba, t))) - (ra + (rb - ra) * t);
}

static float sd_rbox(V3 p, V3 c, V3 h, float r) {
    V3 q = v3(fabsf(p.x - c.x) - h.x + r, fabsf(p.y - c.y) - h.y + r, fabsf(p.z - c.z) - h.z + r);
    V3 mq = v3(fmaxf(q.x, 0), fmaxf(q.y, 0), fmaxf(q.z, 0));
    return v3len(mq) + fminf(fmaxf(q.x, fmaxf(q.y, q.z)), 0.0f) - r;
}

static float sd_torus_y(V3 p, V3 c, float R, float r) {
    V3 q = v3sub(p, c);
    float a = sqrtf(q.x * q.x + q.z * q.z) - R;
    return sqrtf(a * a + q.y * q.y) - r;
}

static float smin(float a, float b, float k) {
    float h = clampf(0.5f + 0.5f * (b - a) / k, 0, 1);
    return b + (a - b) * h - k * h * (1.0f - h);
}
static float smax(float a, float b, float k) { return -smin(-a, -b, k); }

static V3 mx(V3 p) { return v3(fabsf(p.x), p.y, p.z); }   /* spiegelen in x (links = rechts) */

/* ------------------------------------------------------------------ uiterlijk per variant */

typedef struct {
    int v, fem, hair, beard, cap, sleeves, cop, player;
    float bulk;
    Col skin, hairc, shirt, pants, shoe, lip, iris, capc;
} HP;

static HP hp_of(int v) {
    static const int shirts[CHAR_VARIANTS][3] = {
        {196, 36, 40}, {60, 110, 190}, {230, 200, 70}, {70, 150, 90},
        {230, 230, 225}, {130, 70, 150}, {240, 130, 50}, {60, 60, 66}, {38, 56, 112}};
    static const int pants[CHAR_VARIANTS][3] = {
        {34, 36, 44}, {70, 80, 110}, {60, 50, 40}, {40, 40, 45},
        {90, 100, 130}, {50, 50, 60}, {120, 110, 90}, {150, 140, 120}, {26, 30, 52}};
    static const int irisC[4][3] = {{92, 62, 38}, {60, 96, 140}, {70, 110, 70}, {50, 36, 26}};
    /* kapsel: 0 pet, 1 kort, 2 lang, 3 opgeschoren, 4 krullen, 5 paardenstaart, 6 bob, 7 kaal */
    static const int hairStyle[CHAR_VARIANTS] = {0, 1, 2, 3, 4, 5, 6, 7, 0};
    static const int female[CHAR_VARIANTS] = {0, 0, 1, 0, 0, 1, 1, 0, 0};
    static const float bulk[CHAR_VARIANTS] = {0.2f, 0.4f, 0.0f, 0.6f, 0.0f, 0.1f, 0.3f, 0.9f, 0.4f};
    HP h;
    Col skin, hair;
    memset(&h, 0, sizeof(h));
    v = ((v % CHAR_VARIANTS) + CHAR_VARIANTS) % CHAR_VARIANTS;
    model_char_colors(v, &skin, &hair);
    h.v = v;
    h.skin = skin; h.hairc = hair;
    h.shirt = col(shirts[v][0], shirts[v][1], shirts[v][2]);
    h.pants = col(pants[v][0], pants[v][1], pants[v][2]);
    h.shoe = col(30, 30, 32);
    h.fem = female[v];
    h.hair = hairStyle[v];
    h.cap = h.hair == 0;
    h.beard = v == 7 || v == 3;
    h.bulk = bulk[v];
    h.cop = v == COP_VARIANT;
    h.player = v == 0;
    h.sleeves = v == COP_VARIANT || v == 7 || v == 5;
    h.lip = col((int)(skin.r * 0.78f + 30), (int)(skin.g * 0.5f + 8), (int)(skin.b * 0.5f + 10));
    if (h.fem) h.lip = col((int)(skin.r * 0.6f + 70), (int)(skin.g * 0.35f + 10), (int)(skin.b * 0.4f + 20));
    h.iris = col(irisC[(v * 7 + 1) % 4][0], irisC[(v * 7 + 1) % 4][1], irisC[(v * 7 + 1) % 4][2]);
    h.capc = h.cop ? col(24, 34, 74) : col(30, 30, 34);
    if (h.player) h.capc = col(200, 30, 40);
    return h;
}

/* ------------------------------------------------------------------ surface nets */

typedef float (*SdfFn)(V3 p, const HP *h, int *part);
typedef void (*ColFn)(V3 p, V3 n, const HP *h, int part, Col *c, int *mat);

typedef struct { V3 p, n; Col c; int mat; } SVert;

static float eval(SdfFn f, V3 p, const HP *h) { int part = 0; return f(p, h, &part); }

static void snets(MeshB *m, V3 lo, V3 hi, float cell, SdfFn f, ColFn cf, const HP *h) {
    int nx = (int)((hi.x - lo.x) / cell) + 2, ny = (int)((hi.y - lo.y) / cell) + 2, nz = (int)((hi.z - lo.z) / cell) + 2;
    size_t np = (size_t)nx * ny * nz;
    float *val = (float *)malloc(np * sizeof(float));
    int *vid = (int *)malloc(np * sizeof(int));
    SVert *sv = NULL;
    int nv = 0, cap = 0, i, j, k, a;
    if (!val || !vid) { free(val); free(vid); return; }
#define IDX(i, j, k) (((size_t)(k) * ny + (j)) * nx + (i))
    for (k = 0; k < nz; k++)
        for (j = 0; j < ny; j++)
            for (i = 0; i < nx; i++)
                val[IDX(i, j, k)] = eval(f, v3(lo.x + i * cell, lo.y + j * cell, lo.z + k * cell), h);
    /* per cel een punt op het oppervlak: gemiddelde van de randkruisingen */
    for (k = 0; k < nz - 1; k++)
        for (j = 0; j < ny - 1; j++)
            for (i = 0; i < nx - 1; i++) {
                static const int E[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
                float c8[8];
                int q, mask = 0, cnt = 0;
                V3 acc = v3(0, 0, 0);
                for (q = 0; q < 8; q++) {
                    c8[q] = val[IDX(i + (q & 1), j + ((q >> 1) & 1), k + ((q >> 2) & 1))];
                    if (c8[q] < 0) mask |= 1 << q;
                }
                vid[IDX(i, j, k)] = -1;
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
                    SVert *ns = (SVert *)realloc(sv, (size_t)nc * sizeof(SVert));
                    if (!ns) continue;
                    sv = ns; cap = nc;
                }
                acc = v3scale(acc, 1.0f / (float)cnt);
                sv[nv].p = v3(lo.x + (i + acc.x) * cell, lo.y + (j + acc.y) * cell, lo.z + (k + acc.z) * cell);
                vid[IDX(i, j, k)] = nv++;
            }
    /* normaal (gradient), kleur en een beetje ambient occlusion per punt */
    for (i = 0; i < nv; i++) {
        V3 p = sv[i].p, n;
        float e = cell * 0.5f, ao = 0, s;
        int part = 0, mat = MAT_DEFAULT;
        n = v3(eval(f, v3(p.x + e, p.y, p.z), h) - eval(f, v3(p.x - e, p.y, p.z), h),
               eval(f, v3(p.x, p.y + e, p.z), h) - eval(f, v3(p.x, p.y - e, p.z), h),
               eval(f, v3(p.x, p.y, p.z + e), h) - eval(f, v3(p.x, p.y, p.z - e), h));
        n = v3norm(n);
        f(p, h, &part);
        for (s = 1; s <= 4; s += 1) {   /* SDF-AO: hoeveel ruimte is er naast het oppervlak */
            float d = 0.006f * s;
            ao += (d - eval(f, v3add(p, v3scale(n, d)), h)) / (0.006f * s) * (1.0f / s);
        }
        ao = clampf(1.0f - ao * 0.32f, 0.5f, 1.0f);
        cf(p, n, h, part, &sv[i].c, &mat);
        sv[i].c = col_mul(sv[i].c, ao);
        sv[i].n = n;
        sv[i].mat = mat;
    }
    /* per rand met tekenwissel een vierhoek uit de vier cellen eromheen */
    for (k = 0; k < nz - 1; k++)
        for (j = 0; j < ny - 1; j++)
            for (i = 0; i < nx - 1; i++)
                for (a = 0; a < 3; a++) {
                    int c[4], q;
                    float v0 = val[IDX(i, j, k)], v1;
                    if (a == 0) { v1 = val[IDX(i + 1, j, k)]; if (j < 1 || k < 1) continue;
                        c[0] = vid[IDX(i, j - 1, k - 1)]; c[1] = vid[IDX(i, j, k - 1)]; c[2] = vid[IDX(i, j, k)]; c[3] = vid[IDX(i, j - 1, k)]; }
                    else if (a == 1) { v1 = val[IDX(i, j + 1, k)]; if (i < 1 || k < 1) continue;
                        c[0] = vid[IDX(i - 1, j, k - 1)]; c[1] = vid[IDX(i, j, k - 1)]; c[2] = vid[IDX(i, j, k)]; c[3] = vid[IDX(i - 1, j, k)]; }
                    else { v1 = val[IDX(i, j, k + 1)]; if (i < 1 || j < 1) continue;
                        c[0] = vid[IDX(i - 1, j - 1, k)]; c[1] = vid[IDX(i, j - 1, k)]; c[2] = vid[IDX(i, j, k)]; c[3] = vid[IDX(i - 1, j, k)]; }
                    if ((v0 < 0) == (v1 < 0)) continue;
                    for (q = 0; q < 4; q++) if (c[q] < 0) break;
                    if (q < 4) continue;
                    for (q = 0; q < 2; q++) {
                        const SVert *A = &sv[c[0]], *B = &sv[c[1 + q]], *C = &sv[c[2 + q]];
                        V3 fn = v3cross(v3sub(B->p, A->p), v3sub(C->p, A->p));
                        if (v3dot(fn, v3add(A->n, v3add(B->n, C->n))) < 0) { const SVert *t = B; B = C; C = t; }
                        mb_tri_nm(m, A->p, B->p, C->p, A->n, B->n, C->n, A->c, B->c, C->c, A->mat);
                    }
                }
#undef IDX
    free(val); free(vid); free(sv);
}

/* ------------------------------------------------------------------ romp + hoofd */

enum { PT_BODY = 0, PT_HAIR, PT_EYE, PT_CAP };

static float torso_sdf(V3 p, const HP *h) {
    float fe = (float)h->fem, b = h->bulk, d;
    V3 q = mx(p);
    d = sd_ell(p, v3(0, 0.985f, -0.005f), v3(0.165f + 0.022f * fe + 0.015f * b, 0.12f, 0.112f + 0.01f * b));   /* bekken */
    d = smin(d, sd_ell(q, v3(0.075f, 0.95f, -0.055f), v3(0.085f + 0.01f * fe, 0.09f, 0.072f)), 0.04f);         /* billen */
    d = smin(d, sd_ell(p, v3(0, 1.17f, 0.006f + 0.028f * b), v3(0.148f + 0.025f * b - 0.018f * fe, 0.13f, 0.097f + 0.035f * b)), 0.06f);   /* buik */
    d = smin(d, sd_ell(p, v3(0, 1.355f, 0.0f), v3(0.19f - 0.03f * fe + 0.012f * b, 0.14f, 0.112f + 0.012f * b)), 0.07f);   /* borstkas */
    d = smin(d, sd_ell(q, v3(0.075f, 1.39f, 0.055f), v3(0.08f, 0.055f, 0.05f)), 0.04f);   /* borstspieren */
    if (h->fem) d = smin(d, sd_sph(q, v3(0.07f, 1.33f, 0.072f), 0.058f), 0.03f);
    d = smin(d, sd_cap(p, v3(-0.30f + 0.03f * fe, 1.462f, -0.005f), v3(0.30f - 0.03f * fe, 1.462f, -0.005f), 0.064f, 0.064f), 0.06f);   /* schouders */
    d = smin(d, sd_cap(p, v3(-0.16f, 1.495f, -0.02f), v3(0.16f, 1.495f, -0.02f), 0.052f, 0.052f), 0.05f);    /* monnikskapspier */
    d = smin(d, sd_ell(q, v3(0.095f, 1.33f, -0.07f), v3(0.08f, 0.13f, 0.05f)), 0.05f);   /* rugspieren */
    d = smin(d, sd_cap(p, v3(0, 1.46f, -0.008f), v3(0, 1.70f, 0.01f), 0.056f - 0.007f * fe, 0.05f - 0.004f * fe), 0.035f);   /* nek */
    d = smin(d, sd_sph(q, v3(HIP_X, 0.905f, 0), 0.094f + 0.006f * fe), 0.05f);   /* aanzet van de benen */
    if (p.y > 1.12f && p.y < 1.5f)   /* plooien in het shirt */
        d += 0.0022f * sinf(p.y * 75.0f + 3.0f * sinf(p.x * 22.0f)) * clampf((1.48f - p.y) * 8.0f, 0, 1);
    if (p.y < 1.08f && p.y > 0.86f)
        d += 0.0016f * sinf(p.x * 60.0f + p.y * 40.0f);
    d = smin(d, sd_torus_y(p, v3(0, 1.505f, 0.004f), 0.064f - 0.005f * fe, 0.011f), 0.01f);   /* kraag */
    return d;
}

static float face_sdf(V3 p, const HP *h) {
    float fe = (float)h->fem, d;
    V3 q = mx(p);
    d = sd_ell(p, v3(0, 1.832f, -0.008f), v3(0.088f - 0.004f * fe, 0.108f, 0.103f));                  /* schedel */
    d = smin(d, sd_ell(p, v3(0, 1.765f, 0.026f), v3(0.073f - 0.006f * fe, 0.074f, 0.082f)), 0.04f);  /* gezicht */
    d = smin(d, sd_ell(p, v3(0, 1.727f, 0.044f), v3(0.061f - 0.009f * fe, 0.04f, 0.06f)), 0.03f);    /* kaak */
    d = smin(d, sd_sph(p, v3(0, 1.704f, 0.077f), 0.025f - 0.003f * fe), 0.02f);                     /* kin */
    d = smin(d, sd_sph(q, v3(0.05f, 1.79f, 0.071f), 0.026f), 0.02f);                                 /* jukbeenderen */
    d = smin(d, sd_cap(p, v3(-0.05f, 1.839f, 0.087f), v3(0.05f, 1.839f, 0.087f), 0.013f - 0.003f * fe, 0.013f - 0.003f * fe), 0.016f);   /* wenkbrauwboog */
    d = smin(d, sd_cap(p, v3(0, 1.832f, 0.097f), v3(0, 1.789f, 0.12f), 0.008f, 0.012f), 0.01f);    /* neusrug */
    d = smin(d, sd_sph(p, v3(0, 1.783f, 0.119f), 0.0155f), 0.008f);                                  /* neuspunt */
    d = smin(d, sd_sph(q, v3(0.0145f, 1.777f, 0.106f), 0.011f), 0.008f);                             /* neusvleugels */
    d = smax(d, -sd_sph(q, v3(0.009f, 1.771f, 0.115f), 0.0045f), 0.002f);                            /* neusgaten */
    d = smin(d, sd_ell(p, v3(0, 1.7475f, 0.1035f), v3(0.025f, 0.0085f, 0.013f)), 0.006f);           /* bovenlip */
    d = smin(d, sd_ell(p, v3(0, 1.7345f, 0.1005f), v3(0.022f, 0.0095f, 0.013f)), 0.006f);           /* onderlip */
    d = smax(d, -sd_rbox(p, v3(0, 1.7412f, 0.116f), v3(0.024f, 0.0011f, 0.012f), 0.0008f), 0.0015f);   /* mondspleet */
    d = smax(d, -sd_sph(q, v3(0.033f, 1.81f, 0.104f), 0.0195f), 0.011f);                              /* oogkassen */
    d = smin(d, sd_ell(q, v3(0.033f, 1.8195f, 0.0925f), v3(0.0185f, 0.0085f, 0.0135f)), 0.004f);       /* bovenoogleden */
    d = smin(d, sd_ell(q, v3(0.033f, 1.8005f, 0.094f), v3(0.016f, 0.0045f, 0.011f)), 0.004f);          /* onderoogleden */
    d = smin(d, sd_ell(q, v3(0.088f, 1.795f, -0.008f), v3(0.012f, 0.031f, 0.02f)), 0.01f);           /* oren */
    d = smax(d, -sd_sph(q, v3(0.098f, 1.797f, -0.004f), 0.0105f), 0.004f);                            /* oorschelp */
    return d;
}

static float eye_sdf(V3 p) { return sd_sph(mx(p), v3(0.033f, 1.81f, 0.0905f), 0.0128f); }

static float hair_sdf(V3 p, const HP *h) {
    float d = 1e3f, cut;
    V3 q = mx(p);
    int st = h->hair;
    if (st != 7 && st != 0) {
        float th = st == 3 ? 0.004f : (st == 4 ? 0.016f : 0.009f);
        float low = st == 6 ? 1.725f : (st == 2 ? 1.70f : 1.765f);
        V3 n = v3norm(v3(0, 1.0f, -0.62f));
        d = sd_ell(p, v3(0, 1.835f, -0.012f), v3(0.094f + th, 0.114f + th, 0.108f + th));
        cut = v3dot(v3sub(p, v3(0, 1.872f - (h->fem ? 0.006f : 0.0f), 0.082f)), n);   /* haarlijn voor */
        d = smax(d, -cut, 0.012f);
        d = smax(d, low - p.y, 0.01f);   /* onderkant */
        if (st != 6 && st != 2) d = smax(d, -sd_sph(q, v3(0.092f, 1.79f, 0.01f), 0.028f), 0.01f);   /* vrij boven de oren */
        if (st == 4) d += 0.0045f * sinf(p.x * 150.0f) * sinf(p.y * 150.0f) * sinf(p.z * 150.0f);   /* krullen */
        if (st == 2) {   /* lang haar tot op de schouders */
            float back = sd_ell(p, v3(0, 1.73f, -0.055f), v3(0.105f, 0.12f, 0.07f));
            back = smax(back, p.z - 0.02f, 0.02f);
            back = smax(back, 1.648f - p.y, 0.01f);
            d = smin(d, back, 0.03f);
            d = smin(d, smax(sd_cap(q, v3(0.085f, 1.83f, 0.02f), v3(0.095f, 1.66f, 0.0f), 0.022f, 0.018f), 1.648f - p.y, 0.01f), 0.02f);
        }
        if (st == 5) d = smin(d, sd_cap(p, v3(0, 1.86f, -0.115f), v3(0, 1.70f, -0.15f), 0.026f, 0.016f), 0.02f);   /* paardenstaart */
        if (st == 6) d = smin(d, smax(sd_ell(p, v3(0, 1.77f, -0.02f), v3(0.11f, 0.07f, 0.11f)), p.z - 0.05f, 0.02f), 0.03f);   /* bob */
        if (st != 4) d += 0.0012f * sinf(p.x * 400.0f + p.z * 90.0f);   /* lokken */
    }
    if (h->cap) {   /* korte haren onder de pet aan de achterkant */
        float sh = sd_ell(p, v3(0, 1.80f, -0.03f), v3(0.096f, 0.085f, 0.09f));
        sh = smax(sh, 1.76f - p.y, 0.01f);
        sh = smax(sh, p.z - 0.0f, 0.02f);
        d = fminf(d, sh);
    }
    if (h->beard) {   /* baard */
        float bd = sd_ell(p, v3(0, 1.728f, 0.05f), v3(0.068f, 0.05f, 0.068f));
        bd = smax(bd, p.y - 1.772f, 0.01f);
        bd = smax(bd, -0.01f - p.z, 0.02f);
        bd = smax(bd, -sd_ell(p, v3(0, 1.741f, 0.11f), v3(0.03f, 0.018f, 0.03f)), 0.006f);   /* mond vrij */
        bd += 0.0015f * sinf(p.x * 300.0f) * sinf(p.y * 300.0f);
        d = fminf(d, bd);
    }
    return d;
}

static float cap_sdf(V3 p, const HP *h) {
    float d;
    if (!h->cap) return 1e3f;
    d = sd_ell(p, v3(0, 1.865f, -0.006f), v3(0.104f, 0.082f, 0.117f));
    d = smax(d, 1.842f - p.y, 0.008f);
    d = fminf(d, smax(sd_ell(p, v3(0, 1.848f, 0.118f), v3(0.088f, 0.007f, 0.075f)), 0.06f - p.z, 0.01f));   /* klep */
    d = fminf(d, sd_sph(p, v3(0, 1.947f, -0.006f), 0.009f));   /* knoopje */
    return d;
}

static float body_sdf(V3 p, const HP *h, int *part) {
    float b, d, e, hr, cp;
    b = smin(torso_sdf(p, h), p.y > 1.62f ? face_sdf(p, h) : 1e3f, 0.02f);
    d = b; *part = PT_BODY;
    if (p.y > 1.6f) {
        e = eye_sdf(p);
        if (e < d) { d = e; *part = PT_EYE; }
        hr = hair_sdf(p, h);
        if (hr < d) { d = hr; *part = PT_HAIR; }
        cp = cap_sdf(p, h);
        if (cp < d) { d = cp; *part = PT_CAP; }
    }
    return d;
}

static float body_lower(V3 p, const HP *h, int *part) { return fmaxf(body_sdf(p, h, part), p.y - 1.66f); }
static float body_upper(V3 p, const HP *h, int *part) { return fmaxf(body_sdf(p, h, part), 1.64f - p.y); }

static Col cmix(Col a, Col b, float t) {
    t = clampf(t, 0, 1);
    return col((int)(a.r + (b.r - a.r) * t), (int)(a.g + (b.g - a.g) * t), (int)(a.b + (b.b - a.b) * t));
}

static void body_col(V3 p, V3 n, const HP *h, int part, Col *c, int *mat) {
    V3 q = mx(p);
    if (part == PT_HAIR) {
        *c = h->hairc;
        if (h->beard && p.y < 1.78f) *c = col_mul(h->hairc, 0.9f);
        *mat = MAT_HAIR; return;
    }
    if (part == PT_CAP) {
        *c = h->capc;
        if (h->cop && p.z > 0.09f && p.y > 1.875f && p.y < 1.905f && q.x < 0.022f) { *c = col_e(240, 200, 70, 90); *mat = MAT_SHINY; return; }
        *mat = MAT_CLOTH; return;
    }
    if (part == PT_EYE) {
        V3 d = v3norm(v3sub(q, v3(0.033f, 1.81f, 0.0905f)));
        if (d.z > 0.965f) *c = col(12, 10, 10);
        else if (d.z > 0.86f) *c = cmix(col_mul(h->iris, 0.55f), h->iris, (d.z - 0.86f) * 12.0f);
        else *c = cmix(col(236, 226, 222), col(246, 244, 240), d.z);
        *mat = MAT_SHINY; return;
    }
    {
        float neck = 1.507f;
        if (!h->cop && !h->sleeves && p.z > 0) neck -= 0.035f * clampf(1.0f - q.x / 0.05f, 0, 1);   /* V-hals */
        if (p.y >= neck || (p.y > 1.49f && p.z < -0.0f && p.y > 1.515f)) {   /* huid */
            Col sk = h->skin;
            /* lippen, wangen, wenkbrauwen */
            if (sd_ell(p, v3(0, 1.7475f, 0.1035f), v3(0.025f, 0.0085f, 0.013f)) < 0.002f || sd_ell(p, v3(0, 1.7345f, 0.1005f), v3(0.022f, 0.0095f, 0.013f)) < 0.002f) {
                *c = h->lip; *mat = MAT_SKIN; return;
            }
            if (q.x > 0.012f && q.x < 0.058f && p.z > 0.072f && p.y > 1.836f && p.y < 1.851f - 0.12f * (q.x - 0.035f) * (q.x - 0.035f) * 30.0f) {
                *c = col_mul(h->hairc, 0.75f); *mat = MAT_HAIR; return;
            }
            if (p.y > 1.8f && p.y < 1.83f && q.x > 0.016f && q.x < 0.05f && p.z > 0.098f && fabsf(p.y - 1.8195f) < 0.006f) {   /* wimpers */
                *c = col_mul(h->hairc, 0.4f); *mat = MAT_HAIR; return;
            }
            sk = cmix(sk, col((int)(sk.r * 1.05f), (int)(sk.g * 0.86f), (int)(sk.b * 0.84f)), 0.6f * clampf(1.0f - sd_sph(q, v3(0.052f, 1.775f, 0.085f), 0.0f) / 0.03f, 0, 1));   /* blosjes */
            if (h->beard == 0 && !h->fem && p.y < 1.75f && p.z > 0.04f) sk = cmix(sk, col_mul(sk, 0.88f), 0.5f);   /* stoppels */
            *c = sk; *mat = MAT_SKIN; return;
        }
        if (p.y > 1.115f) {   /* shirt */
            *c = h->shirt; *mat = MAT_CLOTH;
            if (h->player && p.z > 0.09f && fabsf(p.x) < 0.11f && fabsf(p.y - 1.33f) < 0.08f) {
                *c = col(235, 235, 230);
                if (sd_sph(v3(p.x, p.y, 0), v3(-0.012f, 1.345f, 0), 0.016f) < 0 || sd_sph(v3(p.x, p.y, 0), v3(0.012f, 1.345f, 0), 0.016f) < 0 ||
                    (p.y < 1.345f && p.y > 1.31f + 0.9f * fabsf(p.x))) *c = col(200, 30, 40);
            }
            if (h->cop) {
                if (p.z > 0.09f && fabsf(p.x - 0.11f) < 0.022f && fabsf(p.y - 1.40f) < 0.026f) { *c = col_e(240, 200, 70, 90); *mat = MAT_SHINY; }
                if (p.z > 0.09f && fabsf(p.x) < 0.007f && fmodf(p.y * 12.5f, 1.0f) < 0.16f) { *c = col(200, 190, 120); *mat = MAT_SHINY; }
            }
            return;
        }
        if (p.y > 1.087f) {   /* riem en gesp */
            *c = col(30, 26, 24); *mat = MAT_DEFAULT;
            if (p.z > 0.09f && fabsf(p.x) < 0.024f) { *c = col(190, 170, 90); *mat = MAT_SHINY; }
            return;
        }
        *c = h->pants; *mat = MAT_CLOTH;
        if (p.z > 0.09f && fabsf(p.x) < 0.004f && p.y > 0.96f) *c = col_mul(h->pants, 0.7f);   /* gulp */
        (void)n;
    }
}

void human_body(MeshB *m, int v) {
    HP h = hp_of(v);
    snets(m, v3(-0.41f, 0.84f, -0.21f), v3(0.41f, 1.67f, 0.21f), 0.006f, body_lower, body_col, &h);
    snets(m, v3(-0.145f, 1.63f, -0.2f), v3(0.145f, 2.0f, 0.21f), 0.0028f, body_upper, body_col, &h);
}

/* ------------------------------------------------------------------ benen */

static float thigh_sdf(V3 p, const HP *h, int *part) {
    float fe = (float)h->fem, d;
    *part = PT_BODY;
    d = sd_cap(p, v3(0, 0.03f, 0), v3(0, -KNEE_Y, 0.005f), 0.1f + 0.008f * fe + 0.01f * h->bulk, 0.072f);
    d = smin(d, sd_ell(p, v3(0, -0.16f, 0.022f), v3(0.087f + 0.006f * fe, 0.17f, 0.084f)), 0.04f);   /* dijspier */
    d = smin(d, sd_sph(p, v3(0, -KNEE_Y, 0.01f), 0.07f), 0.03f);
    d += 0.0018f * sinf(p.y * 55.0f + p.x * 20.0f);   /* stof */
    return d;
}

static void leg_col(V3 p, V3 n, const HP *h, int part, Col *c, int *mat) {
    (void)n; (void)part;
    *c = h->pants; *mat = MAT_CLOTH;
    if (p.y < -0.40f) {   /* schoen */
        *c = h->shoe; *mat = MAT_DEFAULT;
        if (p.y < -0.463f) { *c = col(205, 205, 200); *mat = MAT_RUBBER; }
        else if (p.z > 0.02f && p.z < 0.12f && p.y > -0.425f && fmodf(p.z * 60.0f, 1.0f) < 0.3f && fabsf(p.x) < 0.022f) *c = col(220, 220, 215);   /* veters */
    }
}

static float shin_sdf(V3 p, const HP *h, int *part) {
    float d, shoe;
    *part = PT_BODY;
    d = sd_sph(p, v3(0, 0, 0.004f), 0.072f);
    d = smin(d, sd_cap(p, v3(0, 0, 0.004f), v3(0, -0.38f, -0.004f), 0.07f, 0.046f), 0.03f);
    d = smin(d, sd_ell(p, v3(0, -0.13f, -0.026f), v3(0.06f, 0.11f, 0.064f)), 0.03f);   /* kuit */
    d += 0.0015f * sinf(p.y * 60.0f + p.z * 25.0f);
    shoe = sd_ell(p, v3(0, -0.428f, -0.018f), v3(0.048f, 0.05f, 0.056f));
    shoe = smin(shoe, sd_ell(p, v3(0, -0.44f, 0.086f), v3(0.049f, 0.034f, 0.088f)), 0.03f);
    shoe = smin(shoe, sd_rbox(p, v3(0, -0.4705f, 0.034f), v3(0.051f, 0.0095f, 0.132f), 0.008f), 0.008f);
    d = smin(d, shoe, 0.02f);
    return fmaxf(d, -0.48f - p.y);
}

void human_thigh(MeshB *m, int v) {
    HP h = hp_of(v);
    snets(m, v3(-0.13f, -KNEE_Y - 0.09f, -0.12f), v3(0.13f, 0.14f, 0.13f), 0.005f, thigh_sdf, leg_col, &h);
}

void human_shin(MeshB *m, int v) {
    HP h = hp_of(v);
    snets(m, v3(-0.085f, -0.49f, -0.115f), v3(0.085f, 0.08f, 0.25f), 0.0038f, shin_sdf, leg_col, &h);
}

/* ------------------------------------------------------------------ armen */

static float uarm_sdf(V3 p, const HP *h, int *part) {
    float fe = (float)h->fem, d;
    *part = PT_BODY;
    d = sd_sph(p, v3(0, -0.005f, 0), 0.068f - 0.006f * fe);   /* deltaspier */
    d = smin(d, sd_cap(p, v3(0, 0, 0), v3(0, -ELBOW_Y, 0), 0.06f - 0.006f * fe, 0.046f), 0.03f);
    d = smin(d, sd_ell(p, v3(0, -0.13f, 0.018f), v3(0.051f - 0.006f * fe, 0.1f, 0.049f)), 0.03f);   /* biceps */
    d = smin(d, sd_ell(p, v3(0, -0.12f, -0.02f), v3(0.049f - 0.005f * fe, 0.1f, 0.045f)), 0.03f);   /* triceps */
    if (!h->sleeves && p.y > -0.15f && p.y < -0.125f) d -= 0.004f;   /* mouwrand */
    if (h->sleeves || p.y > -0.14f) d += 0.0015f * sinf(p.y * 70.0f + p.x * 30.0f);
    return d;
}

static void arm_col(V3 p, V3 n, const HP *h, int part, Col *c, int *mat) {
    (void)n; (void)part;
    if (h->sleeves || p.y > -0.14f) { *c = h->shirt; *mat = MAT_CLOTH; }
    else { *c = h->skin; *mat = MAT_SKIN; }
}

static float farm_sdf(V3 p, const HP *h, int *part) {
    float fe = (float)h->fem, d, x;
    int f;
    *part = PT_BODY;
    d = sd_sph(p, v3(0, 0, 0), 0.049f - 0.005f * fe);
    d = smin(d, sd_cap(p, v3(0, 0, 0), v3(0, -0.245f, 0.003f), 0.048f - 0.005f * fe, 0.032f - 0.003f * fe), 0.03f);
    d = smin(d, sd_ell(p, v3(0, -0.07f, 0.008f), v3(0.049f - 0.004f * fe, 0.08f, 0.045f)), 0.03f);   /* onderarmspieren */
    d = smin(d, sd_rbox(p, v3(0, -0.296f, 0.004f), v3(0.035f, 0.042f, 0.0125f), 0.01f), 0.016f);    /* handpalm */
    for (f = 0; f < 4; f++) {   /* vingers met twee kootjes, licht gebogen */
        float len = (f == 0 || f == 3) ? 0.052f : 0.064f;
        x = -0.026f + f * 0.0175f;
        d = smin(d, sd_cap(p, v3(x, -0.334f, 0.005f), v3(x, -0.334f - len * 0.55f, 0.012f), 0.0092f - 0.001f * (f == 3), 0.0085f), 0.006f);
        d = smin(d, sd_cap(p, v3(x, -0.334f - len * 0.55f, 0.012f), v3(x, -0.334f - len, 0.024f), 0.0085f, 0.0072f), 0.004f);
    }
    d = smin(d, sd_cap(p, v3(0.0f, -0.272f, 0.018f), v3(0.006f, -0.312f, 0.042f), 0.013f, 0.0105f), 0.01f);   /* duim */
    d = smin(d, sd_cap(p, v3(0.006f, -0.312f, 0.042f), v3(0.008f, -0.336f, 0.05f), 0.0105f, 0.0088f), 0.004f);
    if (h->sleeves && p.y > -0.2f) d += 0.0015f * sinf(p.y * 70.0f + p.x * 30.0f);
    return d;
}

static void farm_col(V3 p, V3 n, const HP *h, int part, Col *c, int *mat) {
    (void)part;
    if (h->sleeves && p.y > -0.205f) { *c = h->shirt; *mat = MAT_CLOTH; if (p.y < -0.19f) *c = col_mul(h->shirt, 0.85f); return; }
    *c = h->skin; *mat = MAT_SKIN;
    if (p.y < -0.36f && n.z > 0.5f) *c = cmix(h->skin, col(235, 205, 200), 0.5f);   /* nagels */
}

void human_uarm(MeshB *m, int v) {
    HP h = hp_of(v);
    snets(m, v3(-0.09f, -ELBOW_Y - 0.06f, -0.09f), v3(0.09f, 0.085f, 0.09f), 0.0042f, uarm_sdf, arm_col, &h);
}

void human_farm(MeshB *m, int v) {
    HP h = hp_of(v);
    snets(m, v3(-0.065f, -0.44f, -0.06f), v3(0.065f, 0.06f, 0.08f), 0.0024f, farm_sdf, farm_col, &h);
}
