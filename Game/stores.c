#include "stores.h"
#include "world.h"
#include <math.h>

/* ------------------------------------------------------------------ maten en gegevens */

typedef struct { float x0, z0, x1, z1; } SBox;

static const char *NAMES[STORE_COUNT] = {"BLOCK ARMS", "GIVEAWAY BURGER", "BLOCK GYM", "DRIP", "BLING", "GIVEAWAY MART"};
static const int COLS[STORE_COUNT][3] = {{220, 30, 30}, {240, 150, 30}, {40, 200, 220}, {230, 60, 160}, {250, 215, 60}, {60, 200, 90}};

static int sclamp(int s) { return s < 0 ? 0 : (s >= STORE_COUNT ? STORE_COUNT - 1 : s); }

int store_poi(int s) {
    static const int p[STORE_COUNT] = {POI_GUNSHOP, POI_BURGER, POI_GYM, POI_DRIP, POI_BLING, POI_MART};
    return p[sclamp(s)];
}
const char *store_name(int s) { return NAMES[sclamp(s)]; }
void store_color(int s, float *r, float *g, float *b) {
    s = sclamp(s);
    *r = COLS[s][0] / 255.0f; *g = COLS[s][1] / 255.0f; *b = COLS[s][2] / 255.0f;
}

static void dims(int s, float *w, float *d) {
    if (s == STORE_MART) { *w = 20.0f; *d = 16.0f; }
    else { *w = 14.0f; *d = 10.0f; }
}

void store_room(int s, float *x0, float *z0, float *x1, float *z1) {
    float w, d, cx;
    s = sclamp(s);
    dims(s, &w, &d);
    cx = STORE_X0 + s * STORE_DX;
    *x0 = cx - w * 0.5f; *x1 = cx + w * 0.5f;
    *z0 = STORE_Z - d * 0.5f; *z1 = STORE_Z + d * 0.5f;
}

int store_at(float x, float z) {
    int s;
    float x0, z0, x1, z1;
    if (z > STORE_ZONE_Z || x < -300.0f) return 0;
    s = (int)floorf((x - STORE_X0) / STORE_DX + 0.5f);
    if (s < 0 || s >= STORE_COUNT) return 0;
    store_room(s, &x0, &z0, &x1, &z1);
    if (x < x0 - 2 || x > x1 + 2 || z < z0 - 2 || z > z1 + 2) return 0;
    return s + 1;
}

void store_entry(int s, float *x, float *z, float *h) {
    float x0, z0, x1, z1;
    store_room(s, &x0, &z0, &x1, &z1);
    *x = (x0 + x1) * 0.5f; *z = z0 + 1.9f; *h = 0.0f;
}

void store_clerk(int s, float *x, float *z, float *h) {
    float x0, z0, x1, z1, cx;
    store_room(s, &x0, &z0, &x1, &z1);
    cx = (x0 + x1) * 0.5f;
    if (s == STORE_MART) { *x = cx + 5.0f; *z = z0 + 4.7f; *h = PI_F; }
    else { *x = cx; *z = z1 - 0.9f; *h = PI_F; }
}

int store_markers(int s, V3 *pos, int *kind, float *rad, int max) {
    float x0, z0, x1, z1, cx;
    int n = 0;
#define ADD(K, X, Z, R) do { if (n < max) { pos[n] = v3((X), 0, (Z)); kind[n] = (K); rad[n] = (R); n++; } } while (0)
    store_room(s, &x0, &z0, &x1, &z1);
    cx = (x0 + x1) * 0.5f;
    ADD(SM_EXIT, cx, z0 + 0.75f, 0.85f);
    if (s == STORE_MART) ADD(SM_ROB, cx + 5.0f, z0 + 2.3f, 0.95f);
    else ADD(SM_COUNTER, cx, z1 - 3.2f, 0.95f);
#undef ADD
    return n;
}

/* meubels waar je niet doorheen kunt */
static int boxes(int s, SBox *b, int max) {
    float x0, z0, x1, z1, cx;
    int n = 0, k;
#define B(X0, Z0, X1, Z1) do { if (n < max) { b[n].x0 = (X0); b[n].z0 = (Z0); b[n].x1 = (X1); b[n].z1 = (Z1); n++; } } while (0)
    store_room(s, &x0, &z0, &x1, &z1);
    cx = (x0 + x1) * 0.5f;
    switch (s) {
    case STORE_MART:
        B(cx + 3.2f, z0 + 3.2f, cx + 6.8f, z0 + 4.0f);            /* kassa */
        B(cx + 6.8f, z0 + 3.2f, cx + 7.6f, z0 + 5.6f);
        for (k = 0; k < 3; k++) {                                  /* schappen */
            float sz = z0 + 6.6f + k * 2.6f;
            B(x0 + 2.5f, sz - 0.4f, cx - 1.3f, sz + 0.4f);
            B(cx + 1.3f, sz - 0.4f, x1 - 2.5f, sz + 0.4f);
        }
        B(x0, z1 - 0.9f, x1, z1);                                  /* koelingen */
        B(x0, z0 + 5.0f, x0 + 0.7f, z1);                           /* wandschap */
        B(x0 + 1.0f, z0 + 2.4f, x0 + 5.0f, z0 + 3.8f);             /* groente en fruit */
        break;
    case STORE_GUN:
        B(cx - 3.0f, z1 - 2.4f, cx + 3.0f, z1 - 1.6f);
        B(x0, z0 + 2.0f, x0 + 0.7f, z1 - 1.0f);                    /* kogelrek */
        B(x1 - 2.2f, z0 + 3.0f, x1 - 1.2f, z0 + 6.0f);             /* vitrine */
        break;
    case STORE_BURGER:
        B(cx - 3.5f, z1 - 2.4f, cx + 3.5f, z1 - 1.6f);
        for (k = 0; k < 3; k++) B(x0 + 1.4f + k * 4.0f, z0 + 3.0f, x0 + 2.6f + k * 4.0f, z0 + 4.2f);
        break;
    case STORE_GYM:
        B(cx - 2.5f, z1 - 2.4f, cx + 2.5f, z1 - 1.6f);
        B(x0 + 0.2f, z0 + 2.2f, x0 + 2.2f, z0 + 4.2f);             /* loopbanden */
        B(x0 + 0.2f, z0 + 5.0f, x0 + 2.2f, z0 + 7.0f);
        B(x1 - 2.6f, z0 + 2.6f, x1 - 0.8f, z0 + 3.4f);             /* bankdrukken */
        B(x1 - 0.8f, z0 + 4.4f, x1, z0 + 7.6f);                    /* halterrek */
        B(cx + 2.5f, z0 + 6.5f, cx + 3.1f, z0 + 7.1f);             /* bokszak */
        break;
    case STORE_DRIP:
        B(cx - 2.5f, z1 - 2.4f, cx + 2.5f, z1 - 1.6f);
        B(x0 + 1.5f, z0 + 3.0f, x0 + 4.5f, z0 + 3.5f);             /* kledingrekken */
        B(x1 - 4.5f, z0 + 3.0f, x1 - 1.5f, z0 + 3.5f);
        B(x0 + 1.5f, z0 + 5.6f, x0 + 4.5f, z0 + 6.1f);
        B(x1 - 4.5f, z0 + 5.6f, x1 - 1.5f, z0 + 6.1f);
        break;
    default:   /* BLING */
        B(cx - 2.5f, z1 - 2.4f, cx + 2.5f, z1 - 1.6f);
        for (k = 0; k < 2; k++) {
            B(x0 + 1.5f, z0 + 2.8f + k * 3.0f, x0 + 3.5f, z0 + 3.6f + k * 3.0f);
            B(x1 - 3.5f, z0 + 2.8f + k * 3.0f, x1 - 1.5f, z0 + 3.6f + k * 3.0f);
        }
        break;
    }
#undef B
    return n;
}

int store_push(float *x, float *z, float r) {
    SBox b[24];
    float x0, z0, x1, z1, nx, nz;
    int s = store_at(*x, *z), n, i, hit = 0;
    if (!s) return 0;
    s--;
    store_room(s, &x0, &z0, &x1, &z1);
    nx = clampf(*x, x0 + r, x1 - r); nz = clampf(*z, z0 + r, z1 - r);
    if (nx != *x || nz != *z) { *x = nx; *z = nz; hit = 1; }
    n = boxes(s, b, 24);
    for (i = 0; i < n; i++) {
        float qx = clampf(*x, b[i].x0, b[i].x1), qz = clampf(*z, b[i].z0, b[i].z1);
        float dx = *x - qx, dz = *z - qz, d2 = dx * dx + dz * dz;
        if (d2 >= r * r) continue;
        if (d2 > 1e-8f) {
            float d = sqrtf(d2);
            *x += dx / d * (r - d); *z += dz / d * (r - d);
        } else {
            float l = *x - b[i].x0, rr = b[i].x1 - *x, t = *z - b[i].z0, bo = b[i].z1 - *z;
            float m = fminf(fminf(l, rr), fminf(t, bo));
            if (m == l) *x = b[i].x0 - r; else if (m == rr) *x = b[i].x1 + r;
            else if (m == t) *z = b[i].z0 - r; else *z = b[i].z1 + r;
        }
        hit = 1;
    }
    return hit;
}

/* ------------------------------------------------------------------ bouwhulpjes */

static void blob(MeshB *m, V3 c, V3 h, float rv, float rh, int su, int sv, Col c0) {
    SQ s = sq(c, h, rv, rh, c0);
    s.su = su; s.sv = sv;
    mb_sq(m, &s);
}
static void qx(MeshB *m, float x, float z0, float z1, float y0, float y1, float nx, Col c) {
    mb_quad(m, v3(x, y0, z0), v3(x, y0, z1), v3(x, y1, z1), v3(x, y1, z0), v3(nx, 0, 0), c);
}
static void qz(MeshB *m, float z, float x0, float x1, float y0, float y1, float nz, Col c) {
    mb_quad(m, v3(x0, y0, z), v3(x1, y0, z), v3(x1, y1, z), v3(x0, y1, z), v3(0, 0, nz), c);
}

static Col prod(int i) {
    static const int pc[10][3] = {{220, 50, 50}, {250, 200, 40}, {60, 140, 230}, {240, 120, 30}, {80, 190, 80},
                                  {240, 240, 236}, {150, 70, 170}, {40, 170, 170}, {200, 160, 110}, {230, 90, 140}};
    i = ((i * 7 + 3) % 10 + 10) % 10;
    return col(pc[i][0], pc[i][1], pc[i][2]);
}

/* muren, vloer, plafond, deur en lampen */
static void shell(MeshB *m, int s, Col wall, Col floorc, Col ceil, Col accent, int checker) {
    float x0, z0, x1, z1, cx, H = STORE_H;
    int k, a, b;
    store_room(s, &x0, &z0, &x1, &z1);
    cx = (x0 + x1) * 0.5f;
    if (checker) {
        for (a = 0; a < (int)(x1 - x0); a++)
            for (b = 0; b < (int)(z1 - z0); b++)
                mb_flat(m, x0 + a, z0 + b, x0 + a + 1, z0 + b + 1, 0.01f, ((a + b) & 1) ? floorc : col(236, 232, 224));
    } else {
        mb_flat(m, x0, z0, x1, z1, 0.01f, floorc);
        for (k = 1; k < (int)(x1 - x0) / 2; k++) mb_flat(m, x0 + k * 2.0f - 0.015f, z0, x0 + k * 2.0f + 0.015f, z1, 0.012f, col_mul(floorc, 0.82f));
    }
    mb_quad(m, v3(x0, H, z0), v3(x1, H, z0), v3(x1, H, z1), v3(x0, H, z1), v3(0, -1, 0), ceil);
    m->ao = 0.18f;
    mb_box(m, v3(x0 - 0.3f, 0, z0 - 0.3f), v3(x0, H, z1 + 0.3f), wall, 0);
    mb_box(m, v3(x1, 0, z0 - 0.3f), v3(x1 + 0.3f, H, z1 + 0.3f), wall, 0);
    mb_box(m, v3(x0, 0, z0 - 0.3f), v3(x1, H, z0), wall, 0);
    mb_box(m, v3(x0, 0, z1), v3(x1, H, z1 + 0.3f), wall, 0);
    m->ao = 0;
    /* gekleurde band langs de muren */
    qx(m, x0 + 0.01f, z0, z1, 2.55f, 2.75f, 1, accent);
    qx(m, x1 - 0.01f, z0, z1, 2.55f, 2.75f, -1, accent);
    qz(m, z1 - 0.01f, x0, x1, 2.55f, 2.75f, -1, accent);
    qz(m, z0 + 0.01f, x0, x1, 2.55f, 2.75f, 1, accent);
    /* glazen voordeur met daglicht, etalageramen, deurmat */
    qz(m, z0 + 0.012f, cx - 0.9f, cx + 0.9f, 0, 2.3f, 1, col_e(200, 228, 250, 210));
    mb_box(m, v3(cx - 0.98f, 0, z0), v3(cx - 0.9f, 2.38f, z0 + 0.08f), col(60, 60, 64), 0);
    mb_box(m, v3(cx + 0.9f, 0, z0), v3(cx + 0.98f, 2.38f, z0 + 0.08f), col(60, 60, 64), 0);
    mb_box(m, v3(cx - 0.98f, 2.3f, z0), v3(cx + 0.98f, 2.38f, z0 + 0.08f), col(60, 60, 64), 0);
    for (k = -1; k <= 1; k += 2) qz(m, z0 + 0.012f, cx + k * 1.4f - (k > 0 ? 0 : 2.6f), cx + k * 1.4f + (k > 0 ? 2.6f : 0), 0.6f, 2.3f, 1, col_e(200, 228, 250, 170));
    mb_flat(m, cx - 0.9f, z0 + 0.05f, cx + 0.9f, z0 + 1.1f, 0.02f, col(50, 50, 54));
    qz(m, z0 + 0.014f, cx - 0.5f, cx + 0.5f, 2.42f, 2.62f, 1, col_e(40, 220, 90, 255));   /* EXIT-bordje */
    /* tl-lampen */
    for (a = 0; a < (int)((x1 - x0) / 5.0f); a++)
        for (b = 0; b < (int)((z1 - z0) / 4.0f); b++) {
            float lx = x0 + 2.5f + a * 5.0f, lz = z0 + 2.0f + b * 4.0f;
            mb_box(m, v3(lx - 0.8f, H - 0.06f, lz - 0.15f), v3(lx + 0.8f, H - 0.01f, lz + 0.15f), col_e(250, 250, 240, 255), 0);
        }
}

/* toonbank met verkoperplek erachter; top = kleur bovenblad */
static void counter(MeshB *m, float ax0, float az0, float ax1, float az1, Col front, Col top) {
    mb_box(m, v3(ax0, 0, az0), v3(ax1, 1.0f, az1), front, 0);
    mb_box(m, v3(ax0 - 0.05f, 1.0f, az0 - 0.05f), v3(ax1 + 0.05f, 1.07f, az1 + 0.05f), top, 0);
}

static void reg(MeshB *m, float x, float y, float z) {   /* kassa */
    mb_box(m, v3(x - 0.25f, y, z - 0.2f), v3(x + 0.25f, y + 0.18f, z + 0.2f), col(40, 40, 44), 0);
    mb_quad(m, v3(x - 0.18f, y + 0.2f, z - 0.05f), v3(x + 0.18f, y + 0.2f, z - 0.05f), v3(x + 0.18f, y + 0.42f, z + 0.05f),
            v3(x - 0.18f, y + 0.42f, z + 0.05f), v3(0, 0.4f, -1), col_e(60, 220, 120, 230));
}

/* schap met producten aan twee kanten, langs x */
static void shelf_x(MeshB *m, float ax0, float ax1, float zc, int seed) {
    float y;
    int k, lvl;
    mb_box(m, v3(ax0, 0, zc - 0.08f), v3(ax1, 1.75f, zc + 0.08f), col(200, 200, 204), 0);
    for (lvl = 0; lvl < 4; lvl++) {
        y = 0.12f + lvl * 0.42f;
        mb_box(m, v3(ax0, y - 0.04f, zc - 0.4f), v3(ax1, y, zc + 0.4f), col(225, 225, 228), 0);
        for (k = 0; k < (int)((ax1 - ax0) / 0.42f); k++) {
            float px = ax0 + 0.06f + k * 0.42f, h = 0.2f + 0.1f * ((k + lvl + seed) % 3);
            Col c = prod(k + lvl * 3 + seed);
            mb_box(m, v3(px, y, zc - 0.38f), v3(px + 0.34f, y + h, zc - 0.1f), c, BOX_NOBOTTOM);
            mb_box(m, v3(px, y, zc + 0.1f), v3(px + 0.34f, y + h, zc + 0.38f), prod(k + lvl * 5 + seed + 4), BOX_NOBOTTOM);
        }
    }
    mb_box(m, v3(ax0, 1.75f, zc - 0.4f), v3(ax1, 1.8f, zc + 0.4f), col(200, 200, 204), 0);
}

/* ------------------------------------------------------------------ interieurs */

static void build_mart(MeshB *m, int s) {
    float x0, z0, x1, z1, cx;
    int k, j;
    store_room(s, &x0, &z0, &x1, &z1);
    cx = (x0 + x1) * 0.5f;
    shell(m, s, col(236, 240, 232), col(190, 196, 188), col(240, 240, 240), col_e(60, 200, 90, 180), 0);
    /* groot bord achterin */
    qz(m, z1 - 0.95f, cx - 3.0f, cx + 3.0f, 2.3f, 2.9f, -1, col_e(60, 200, 90, 230));
    qz(m, z1 - 0.96f, cx - 2.6f, cx + 2.6f, 2.45f, 2.75f, -1, col_e(250, 250, 250, 255));
    /* kassa met band */
    counter(m, cx + 3.2f, z0 + 3.2f, cx + 6.8f, z0 + 4.0f, col(60, 140, 80), col(70, 70, 76));
    counter(m, cx + 6.8f, z0 + 3.2f, cx + 7.6f, z0 + 5.6f, col(60, 140, 80), col(70, 70, 76));
    mb_box(m, v3(cx + 3.3f, 1.07f, z0 + 3.35f), v3(cx + 5.6f, 1.1f, z0 + 3.85f), col(30, 30, 34), 0);
    reg(m, cx + 5.0f, 1.07f, z0 + 3.6f);
    /* sigaretten- en snoepwand achter de kassa */
    mb_box(m, v3(cx + 3.4f, 0, z0 + 5.6f), v3(cx + 6.6f, 2.4f, z0 + 5.9f), col(90, 70, 50), 0);
    for (j = 0; j < 4; j++)
        for (k = 0; k < 10; k++)
            mb_box(m, v3(cx + 3.5f + k * 0.31f, 0.9f + j * 0.36f, z0 + 5.45f), v3(cx + 3.75f + k * 0.31f, 1.15f + j * 0.36f, z0 + 5.6f), prod(k + j * 4), 0);
    /* schappen */
    for (k = 0; k < 3; k++) {
        float sz = z0 + 6.6f + k * 2.6f;
        shelf_x(m, x0 + 2.5f, cx - 1.3f, sz, k * 3);
        shelf_x(m, cx + 1.3f, x1 - 2.5f, sz, k * 3 + 5);
        /* bordje boven het pad */
        mb_box(m, v3(cx - 0.6f, 2.55f, sz - 0.02f), v3(cx + 0.6f, 2.85f, sz + 0.02f), col_e(60, 200, 90, 200), 0);
    }
    /* koelingen met licht */
    mb_box(m, v3(x0, 0, z1 - 0.9f), v3(x1, 2.3f, z1), col(210, 214, 218), 0);
    for (k = 0; k < (int)((x1 - x0) / 1.2f); k++) {
        float fx = x0 + 0.1f + k * 1.2f;
        qz(m, z1 - 0.91f, fx, fx + 1.05f, 0.15f, 2.1f, -1, col_e(200, 230, 250, 140));
        for (j = 0; j < 4; j++)
            mb_box(m, v3(fx + 0.1f, 0.25f + j * 0.48f, z1 - 0.88f), v3(fx + 0.95f, 0.5f + j * 0.48f, z1 - 0.89f + 0.0f + 0.001f), prod(k * 2 + j), 0);
    }
    /* wandschap links */
    mb_box(m, v3(x0, 0, z0 + 5.0f), v3(x0 + 0.7f, 2.0f, z1 - 0.9f), col(200, 200, 204), 0);
    for (j = 0; j < 4; j++)
        for (k = 0; k < (int)((z1 - 0.9f - z0 - 5.0f) / 0.42f); k++)
            mb_box(m, v3(x0 + 0.7f, 0.15f + j * 0.45f, z0 + 5.05f + k * 0.42f), v3(x0 + 0.72f, 0.4f + j * 0.45f, z0 + 5.4f + k * 0.42f), prod(k + j * 2 + 1), 0);
    /* groente en fruit in bakken */
    mb_box(m, v3(x0 + 1.0f, 0, z0 + 2.4f), v3(x0 + 5.0f, 0.8f, z0 + 3.8f), col(130, 90, 55), 0);
    for (k = 0; k < 4; k++) {
        static const int fc[4][3] = {{230, 50, 40}, {250, 200, 40}, {90, 180, 60}, {240, 130, 30}};
        for (j = 0; j < 6; j++)
            blob(m, v3(x0 + 1.25f + k * 1.0f + (j % 3) * 0.25f, 0.9f, z0 + 2.75f + (j / 3) * 0.6f), v3(0.11f, 0.1f, 0.11f), 1, 1, 6, 4, col(fc[k][0], fc[k][1], fc[k][2]));
    }
    /* winkelmandjes en karretjes bij de deur */
    for (k = 0; k < 3; k++) mb_box(m, v3(cx - 3.6f, 0.02f + k * 0.18f, z0 + 0.6f), v3(cx - 3.0f, 0.18f + k * 0.18f, z0 + 1.0f), col(200, 40, 40), BOX_NOTOP);
    for (k = 0; k < 2; k++) {
        float tx = cx - 6.0f + k * 0.4f;
        mb_box(m, v3(tx, 0.35f, z0 + 0.5f), v3(tx + 0.6f, 0.95f, z0 + 1.4f), col(170, 170, 176), BOX_NOTOP);
    }
    /* beveiligingscamera in de hoek */
    blob(m, v3(cx + 7.5f, 3.1f, z0 + 0.5f), v3(0.15f, 0.1f, 0.22f), 0.6f, 0.6f, 8, 4, col(230, 230, 230));
}

static void build_gun(MeshB *m, int s) {
    float x0, z0, x1, z1, cx;
    int k, j;
    store_room(s, &x0, &z0, &x1, &z1);
    cx = (x0 + x1) * 0.5f;
    shell(m, s, col(70, 70, 76), col(100, 100, 104), col(50, 50, 54), col_e(220, 30, 30, 200), 0);
    /* toonbank: glazen vitrine met wapens */
    counter(m, cx - 3.0f, z1 - 2.4f, cx + 3.0f, z1 - 1.6f, col(40, 40, 44), col_e(170, 200, 220, 80));
    for (k = 0; k < 5; k++) mb_box(m, v3(cx - 2.6f + k * 1.1f, 1.0f, z1 - 2.1f), v3(cx - 1.9f + k * 1.1f, 1.05f, z1 - 1.95f), col(20, 20, 22), 0);
    reg(m, cx + 2.4f, 1.07f, z1 - 2.0f);
    /* wapenwand achter de verkoper */
    mb_box(m, v3(cx - 4.5f, 0.8f, z1 - 0.06f), v3(cx + 4.5f, 2.9f, z1), col(120, 85, 55), 0);
    for (j = 0; j < 3; j++)
        for (k = 0; k < 6; k++) {
            float gx = cx - 3.9f + k * 1.5f, gy = 1.2f + j * 0.6f;
            mb_box(m, v3(gx, gy, z1 - 0.14f), v3(gx + 1.1f, gy + 0.12f, z1 - 0.06f), col(25, 25, 28), 0);
            mb_box(m, v3(gx + (j == 1 ? 0.0f : 0.75f), gy - 0.2f, z1 - 0.13f), v3(gx + (j == 1 ? 0.2f : 0.9f), gy, z1 - 0.07f), col(90, 60, 40), 0);
        }
    /* kogelrek links */
    mb_box(m, v3(x0, 0, z0 + 2.0f), v3(x0 + 0.7f, 2.2f, z1 - 1.0f), col(60, 60, 66), 0);
    for (j = 0; j < 4; j++)
        for (k = 0; k < 14; k++)
            mb_box(m, v3(x0 + 0.7f, 0.15f + j * 0.5f, z0 + 2.1f + k * 0.48f), v3(x0 + 0.72f, 0.4f + j * 0.5f, z0 + 2.5f + k * 0.48f),
                   (k + j) % 3 == 0 ? col(60, 110, 60) : ((k + j) % 3 == 1 ? col(170, 140, 50) : col(150, 40, 40)), 0);
    /* vitrine met vesten rechts */
    mb_box(m, v3(x1 - 2.2f, 0, z0 + 3.0f), v3(x1 - 1.2f, 0.9f, z0 + 6.0f), col(40, 40, 44), 0);
    mb_box(m, v3(x1 - 2.2f, 0.9f, z0 + 3.0f), v3(x1 - 1.2f, 1.6f, z0 + 6.0f), col_e(170, 200, 220, 60), 0);
    for (k = 0; k < 3; k++) blob(m, v3(x1 - 1.7f, 1.2f, z0 + 3.6f + k * 0.9f), v3(0.28f, 0.25f, 0.1f), 0.4f, 0.4f, 8, 4, col(50, 60, 40));
    /* schietschijven aan de muur */
    for (k = 0; k < 3; k++) {
        float tz = z0 + 2.5f + k * 2.2f;
        qx(m, x1 - 0.01f, tz - 0.5f, tz + 0.5f, 1.2f, 2.3f, -1, col(240, 240, 236));
        blob(m, v3(x1 - 0.02f, 1.75f, tz), v3(0.01f, 0.32f, 0.32f), 1, 1, 12, 4, col(200, 30, 30));
        blob(m, v3(x1 - 0.03f, 1.75f, tz), v3(0.01f, 0.16f, 0.16f), 1, 1, 12, 4, col(240, 240, 236));
    }
    qz(m, z1 - 0.01f, cx - 1.4f, cx + 1.4f, 3.0f, 3.3f, -1, col_e(220, 30, 30, 255));
}

static void build_burger(MeshB *m, int s) {
    float x0, z0, x1, z1, cx;
    int k, j;
    store_room(s, &x0, &z0, &x1, &z1);
    cx = (x0 + x1) * 0.5f;
    shell(m, s, col(250, 236, 200), col(200, 40, 40), col(250, 245, 230), col_e(240, 150, 30, 200), 1);
    counter(m, cx - 3.5f, z1 - 2.4f, cx + 3.5f, z1 - 1.6f, col(220, 60, 40), col(240, 240, 236));
    reg(m, cx - 2.6f, 1.07f, z1 - 2.0f);
    reg(m, cx + 2.6f, 1.07f, z1 - 2.0f);
    /* dienblad met burger en friet */
    mb_box(m, v3(cx - 0.4f, 1.07f, z1 - 2.25f), v3(cx + 0.4f, 1.1f, z1 - 1.8f), col(200, 50, 40), 0);
    blob(m, v3(cx - 0.15f, 1.17f, z1 - 2.02f), v3(0.13f, 0.07f, 0.13f), 0.5f, 0.9f, 10, 5, col(210, 150, 70));
    for (k = 0; k < 4; k++) mb_box(m, v3(cx + 0.12f + k * 0.04f, 1.1f, z1 - 2.08f), v3(cx + 0.14f + k * 0.04f, 1.28f, z1 - 2.06f), col(250, 210, 60), 0);
    /* menuborden (licht) */
    for (k = 0; k < 3; k++) {
        float mx = cx - 3.3f + k * 2.3f;
        qz(m, z1 - 0.01f, mx, mx + 2.0f, 1.9f, 2.5f, -1, col_e(30, 30, 30, 200));
        for (j = 0; j < 3; j++) qz(m, z1 - 0.012f, mx + 0.15f, mx + 1.2f + 0.2f * j, 2.0f + j * 0.16f, 2.08f + j * 0.16f, -1, col_e(250, 200, 60, 255));
    }
    /* keuken achter de toonbank */
    mb_box(m, v3(cx - 4.5f, 0, z1 - 0.7f), v3(cx - 1.0f, 0.95f, z1), col(190, 190, 196), 0);
    mb_box(m, v3(cx + 1.0f, 0, z1 - 0.7f), v3(cx + 4.5f, 0.95f, z1), col(190, 190, 196), 0);
    /* tafels met krukken */
    for (k = 0; k < 3; k++) {
        float tx = x0 + 2.0f + k * 4.0f, tz = z0 + 3.6f;
        mb_capsule(m, v3(tx, 0, tz), v3(tx, 0.75f, tz), 0.06f, 0.06f, 8, col(80, 80, 84));
        mb_box(m, v3(tx - 0.6f, 0.75f, tz - 0.6f), v3(tx + 0.6f, 0.8f, tz + 0.6f), col(240, 240, 236), 0);
        for (j = 0; j < 4; j++) {
            float sx = tx + (j < 2 ? (j ? 0.95f : -0.95f) : 0), sz2 = tz + (j >= 2 ? (j == 3 ? 0.95f : -0.95f) : 0);
            mb_capsule(m, v3(sx, 0, sz2), v3(sx, 0.45f, sz2), 0.04f, 0.04f, 6, col(80, 80, 84));
            blob(m, v3(sx, 0.48f, sz2), v3(0.2f, 0.05f, 0.2f), 0.6f, 0.9f, 10, 3, col(220, 50, 40));
        }
    }
}

static void build_gym(MeshB *m, int s) {
    float x0, z0, x1, z1, cx;
    int k;
    store_room(s, &x0, &z0, &x1, &z1);
    cx = (x0 + x1) * 0.5f;
    shell(m, s, col(200, 210, 214), col(50, 52, 56), col(220, 220, 224), col_e(40, 200, 220, 200), 0);
    counter(m, cx - 2.5f, z1 - 2.4f, cx + 2.5f, z1 - 1.6f, col(30, 120, 140), col(230, 230, 232));
    reg(m, cx + 1.8f, 1.07f, z1 - 2.0f);
    /* shakes op de toonbank */
    for (k = 0; k < 3; k++) mb_capsule(m, v3(cx - 1.2f + k * 0.3f, 1.07f, z1 - 2.0f), v3(cx - 1.2f + k * 0.3f, 1.3f, z1 - 2.0f), 0.05f, 0.06f, 8, col(240, 140 + 40 * k, 200 - 60 * k));
    /* spiegelwand rechts */
    qx(m, x1 - 0.01f, z0 + 1.5f, z1 - 1.5f, 0.3f, 2.4f, -1, col_e(190, 215, 230, 120));
    /* loopbanden */
    for (k = 0; k < 2; k++) {
        float tz = z0 + 2.2f + k * 2.8f;
        mb_box(m, v3(x0 + 0.2f, 0, tz), v3(x0 + 2.2f, 0.2f, tz + 2.0f), col(30, 30, 34), 0);
        mb_flat(m, x0 + 0.3f, tz + 0.2f, x0 + 2.1f, tz + 1.8f, 0.21f, col(60, 60, 64));
        mb_capsule(m, v3(x0 + 2.0f, 0.2f, tz + 0.2f), v3(x0 + 2.0f, 1.3f, tz + 0.2f), 0.04f, 0.04f, 6, col(160, 160, 166));
        mb_capsule(m, v3(x0 + 2.0f, 0.2f, tz + 1.8f), v3(x0 + 2.0f, 1.3f, tz + 1.8f), 0.04f, 0.04f, 6, col(160, 160, 166));
        mb_box(m, v3(x0 + 1.9f, 1.25f, tz + 0.2f), v3(x0 + 2.15f, 1.45f, tz + 1.8f), col(40, 40, 44), 0);
        qx(m, x0 + 1.89f, tz + 0.6f, tz + 1.4f, 1.3f, 1.42f, -1, col_e(40, 200, 220, 255));
    }
    /* bankdrukken */
    mb_box(m, v3(x1 - 2.6f, 0.4f, z0 + 2.8f), v3(x1 - 0.8f, 0.5f, z0 + 3.2f), col(30, 30, 34), 0);
    mb_capsule(m, v3(x1 - 2.4f, 0, z0 + 3.0f), v3(x1 - 2.4f, 0.4f, z0 + 3.0f), 0.05f, 0.05f, 6, col(120, 120, 126));
    mb_capsule(m, v3(x1 - 1.0f, 0, z0 + 3.0f), v3(x1 - 1.0f, 0.4f, z0 + 3.0f), 0.05f, 0.05f, 6, col(120, 120, 126));
    for (k = -1; k <= 1; k += 2) mb_capsule(m, v3(x1 - 1.1f, 0, z0 + 3.0f + k * 0.5f), v3(x1 - 1.1f, 1.1f, z0 + 3.0f + k * 0.5f), 0.04f, 0.04f, 6, col(150, 150, 156));
    mb_capsule(m, v3(x1 - 1.1f, 1.1f, z0 + 2.0f), v3(x1 - 1.1f, 1.1f, z0 + 4.0f), 0.025f, 0.025f, 6, col(200, 200, 206));
    for (k = -1; k <= 1; k += 2) mb_cyl(m, v3(x1 - 1.1f, 1.1f, z0 + 3.0f + k * 0.8f), 0.28f, 0.08f, 14, 2, col(30, 30, 34), col(40, 40, 44), 1);
    /* halterrek */
    mb_box(m, v3(x1 - 0.8f, 0, z0 + 4.4f), v3(x1, 0.7f, z0 + 7.6f), col(40, 40, 44), 0);
    for (k = 0; k < 7; k++) {
        float hz = z0 + 4.6f + k * 0.42f, rr = 0.07f + 0.01f * k;
        mb_capsule(m, v3(x1 - 0.65f, 0.78f, hz), v3(x1 - 0.15f, 0.78f, hz), 0.02f, 0.02f, 6, col(180, 180, 186));
        mb_cyl(m, v3(x1 - 0.62f, 0.78f, hz), rr, 0.08f, 10, 0, col(30, 30, 34), col(30, 30, 34), 1);
        mb_cyl(m, v3(x1 - 0.18f, 0.78f, hz), rr, 0.08f, 10, 0, col(30, 30, 34), col(30, 30, 34), 1);
    }
    /* matten en een bokszak */
    mb_flat(m, cx - 2.0f, z0 + 3.0f, cx + 2.0f, z0 + 6.0f, 0.02f, col(40, 120, 160));
    mb_capsule(m, v3(cx + 2.8f, 0.6f, z0 + 6.8f), v3(cx + 2.8f, 1.9f, z0 + 6.8f), 0.28f, 0.28f, 12, col(170, 30, 30));
    mb_capsule(m, v3(cx + 2.8f, 1.9f, z0 + 6.8f), v3(cx + 2.8f, STORE_H, z0 + 6.8f), 0.015f, 0.015f, 4, col(120, 120, 120));
}

static void build_drip(MeshB *m, int s) {
    float x0, z0, x1, z1, cx;
    int k, j, r;
    store_room(s, &x0, &z0, &x1, &z1);
    cx = (x0 + x1) * 0.5f;
    shell(m, s, col(245, 240, 244), col(230, 226, 222), col(250, 250, 250), col_e(230, 60, 160, 220), 0);
    counter(m, cx - 2.5f, z1 - 2.4f, cx + 2.5f, z1 - 1.6f, col(30, 30, 34), col(230, 60, 160));
    reg(m, cx + 1.8f, 1.07f, z1 - 2.0f);
    /* kledingrekken met shirts */
    for (r = 0; r < 4; r++) {
        float rx0 = (r & 1) ? x1 - 4.5f : x0 + 1.5f, rz = z0 + 3.25f + (r / 2) * 2.6f;
        mb_capsule(m, v3(rx0, 0, rz), v3(rx0, 1.6f, rz), 0.03f, 0.03f, 6, col(180, 180, 186));
        mb_capsule(m, v3(rx0 + 3.0f, 0, rz), v3(rx0 + 3.0f, 1.6f, rz), 0.03f, 0.03f, 6, col(180, 180, 186));
        mb_capsule(m, v3(rx0, 1.6f, rz), v3(rx0 + 3.0f, 1.6f, rz), 0.02f, 0.02f, 6, col(200, 200, 206));
        for (k = 0; k < 9; k++) {
            Col c = prod(k + r * 3);
            blob(m, v3(rx0 + 0.25f + k * 0.31f, 1.2f, rz), v3(0.05f, 0.36f, 0.24f), 0.3f, 0.3f, 6, 4, c);
        }
    }
    /* schoenenwand achter de toonbank */
    for (j = 0; j < 3; j++) {
        mb_box(m, v3(cx - 4.5f, 1.2f + j * 0.5f, z1 - 0.35f), v3(cx + 4.5f, 1.24f + j * 0.5f, z1), col(240, 240, 240), 0);
        for (k = 0; k < 12; k++)
            blob(m, v3(cx - 4.1f + k * 0.75f, 1.32f + j * 0.5f, z1 - 0.18f), v3(0.14f, 0.08f, 0.1f), 0.5f, 0.6f, 8, 4, prod(k * 3 + j));
    }
    /* paspoppen */
    for (k = -1; k <= 1; k += 2) {
        float mx = cx + k * 1.6f, mz = z0 + 1.6f;
        mb_capsule(m, v3(mx, 0, mz), v3(mx, 0.9f, mz), 0.03f, 0.03f, 6, col(60, 60, 64));
        blob(m, v3(mx, 1.25f, mz), v3(0.24f, 0.36f, 0.14f), 0.5f, 0.5f, 10, 6, k < 0 ? col(230, 60, 160) : col(40, 40, 44));
        blob(m, v3(mx, 1.75f, mz), v3(0.1f, 0.13f, 0.1f), 1, 1, 8, 5, col(240, 240, 240));
    }
    /* spiegels */
    for (k = 0; k < 2; k++) qx(m, k ? x1 - 0.01f : x0 + 0.01f, z1 - 4.0f, z1 - 2.8f, 0.2f, 2.2f, k ? -1.0f : 1.0f, col_e(190, 215, 230, 130));
}

static void build_bling(MeshB *m, int s) {
    float x0, z0, x1, z1, cx;
    int k, j;
    Col gold = col_e(250, 205, 70, 60);
    store_room(s, &x0, &z0, &x1, &z1);
    cx = (x0 + x1) * 0.5f;
    shell(m, s, col(40, 34, 40), col(30, 28, 32), col(60, 50, 40), col_e(250, 215, 60, 220), 0);
    mb_flat(m, cx - 1.0f, z0 + 0.2f, cx + 1.0f, z1 - 2.5f, 0.02f, col(150, 20, 40));   /* rode loper */
    counter(m, cx - 2.5f, z1 - 2.4f, cx + 2.5f, z1 - 1.6f, col(20, 20, 24), gold);
    reg(m, cx + 1.8f, 1.07f, z1 - 2.0f);
    /* glazen vitrines met goud */
    for (k = 0; k < 4; k++) {
        float vx0 = (k & 1) ? x1 - 3.5f : x0 + 1.5f, vz0 = z0 + 2.8f + (k / 2) * 3.0f;
        mb_box(m, v3(vx0, 0, vz0), v3(vx0 + 2.0f, 0.9f, vz0 + 0.8f), col(20, 20, 24), 0);
        mb_box(m, v3(vx0, 0.9f, vz0), v3(vx0 + 2.0f, 1.3f, vz0 + 0.8f), col_e(170, 200, 220, 70), 0);
        for (j = 0; j < 5; j++) blob(m, v3(vx0 + 0.25f + j * 0.38f, 0.98f, vz0 + 0.4f), v3(0.09f, 0.04f, 0.09f), 1, 1, 8, 3, j % 2 ? gold : col_e(170, 230, 255, 120));
    }
    /* kroonluchter */
    blob(m, v3(cx, STORE_H - 0.5f, (z0 + z1) * 0.5f - 1.0f), v3(0.6f, 0.3f, 0.6f), 0.5f, 0.6f, 12, 6, col_e(255, 240, 200, 255));
    for (k = 0; k < 8; k++) {
        float a = k * PI_F / 4.0f;
        blob(m, v3(cx + cosf(a) * 0.7f, STORE_H - 0.75f, (z0 + z1) * 0.5f - 1.0f + sinf(a) * 0.7f), v3(0.06f, 0.1f, 0.06f), 1, 1, 6, 4, col_e(220, 240, 255, 220));
    }
    /* gouden lijsten aan de muur */
    for (k = 0; k < 3; k++) {
        qz(m, z1 - 0.01f, cx - 4.5f + k * 3.5f, cx - 3.3f + k * 3.5f, 1.5f, 2.4f, -1, gold);
        qz(m, z1 - 0.012f, cx - 4.4f + k * 3.5f, cx - 3.4f + k * 3.5f, 1.6f, 2.3f, -1, col(30 + 60 * k, 40, 80));
    }
    /* fluwelen koord bij de deur */
    for (k = -1; k <= 1; k += 2) mb_capsule(m, v3(cx + k * 1.2f, 0, z0 + 1.6f), v3(cx + k * 1.2f, 0.9f, z0 + 1.6f), 0.04f, 0.04f, 8, gold);
}

void store_build(MeshB *m, int s) {
    switch (sclamp(s)) {
    case STORE_GUN: build_gun(m, s); break;
    case STORE_BURGER: build_burger(m, s); break;
    case STORE_GYM: build_gym(m, s); break;
    case STORE_DRIP: build_drip(m, s); break;
    case STORE_BLING: build_bling(m, s); break;
    default: build_mart(m, s); break;
    }
}
