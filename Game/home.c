#include "home.h"
#include <math.h>
#include <string.h>

/* ------------------------------------------------------------------ maten */

static const float EXT[HOME_LEVELS][5] = {   /* x0, z0, x1, z1, hoogte */
    {414.0f, 190.0f, 422.0f, 199.0f, 3.0f},
    {413.0f, 187.0f, 425.0f, 203.0f, 5.8f},
    {412.0f, 183.0f, 428.0f, 206.0f, 6.2f},
};
static const float ROOM_W[HOME_LEVELS] = {10.0f, 14.0f, 18.0f}, ROOM_D[HOME_LEVELS] = {8.0f, 10.0f, 13.0f};
#define GAR_X0 (-612.0f)
#define GAR_X0Z(z) (z)

static int clampl(int l, int n) { return l < 0 ? 0 : (l >= n ? n - 1 : l); }

int home_upgrade_price(int house, int level) {
    static const int hp[HOME_LEVELS - 1] = {25000, 75000};
    static const int gp[GARAGE_LEVELS - 1] = {10000, 20000, 35000};
    if (house) return level >= 0 && level < HOME_LEVELS - 1 ? hp[level] : -1;
    return level >= 0 && level < GARAGE_LEVELS - 1 ? gp[level] : -1;
}

void home_door(int lvl, float *x, float *z) {
    lvl = clampl(lvl, HOME_LEVELS);
    *x = EXT[lvl][0] - 1.3f;
    *z = (EXT[lvl][1] + EXT[lvl][3]) * 0.5f;
}

void home_room(int lvl, float *x0, float *z0, float *x1, float *z1) {
    lvl = clampl(lvl, HOME_LEVELS);
    *x0 = HOME_INT_X - ROOM_W[lvl] * 0.5f; *x1 = HOME_INT_X + ROOM_W[lvl] * 0.5f;
    *z0 = HOME_INT_Z - ROOM_D[lvl] * 0.5f; *z1 = HOME_INT_Z + ROOM_D[lvl] * 0.5f;
}

void home_garage_room(int glvl, float *x0, float *z0, float *x1, float *z1) {
    int cap = clampl(glvl, GARAGE_LEVELS) + 1;
    *x0 = GAR_X0; *x1 = GAR_X0 + 8.0f + 4.2f * cap;
    *z0 = HOME_GAR_Z - 7.0f; *z1 = HOME_GAR_Z + 7.0f;
}

void home_garage_spot(int glvl, int k, float *x, float *z, float *h) {
    float x0, z0, x1, z1;
    home_garage_room(glvl, &x0, &z0, &x1, &z1);
    *x = x0 + 4.0f + 4.2f * k; *z = z0 + 3.0f; *h = 0.0f;
}

int home_markers(int lvl, int glvl, V3 *pos, int *kind, float *rad, int max) {
    float x0, z0, x1, z1, dx, dz;
    int n = 0;
#define ADD(K, X, Z, R) do { if (n < max) { pos[n] = v3((X), 0, (Z)); kind[n] = (K); rad[n] = (R); n++; } } while (0)
    home_door(lvl, &dx, &dz);
    ADD(HM_FRONTDOOR, dx, dz, 1.2f);
    ADD(HM_RAMP, HOME_RAMP_X, HOME_RAMP_Z, 2.6f);
    home_room(lvl, &x0, &z0, &x1, &z1);
    ADD(HM_EXIT, x0 + 1.0f, (z0 + z1) * 0.5f, 0.9f);
    ADD(HM_LAPTOP, x1 - 1.7f, z1 - 1.8f, 1.0f);
    ADD(HM_BED, x1 - 3.7f, z0 + 1.2f, 1.0f);
    ADD(HM_TOGARAGE, x0 + 1.6f, z0 + 1.0f, 0.9f);
    home_garage_room(glvl, &x0, &z0, &x1, &z1);
    ADD(HM_GAR_EXIT, x1 - 1.8f, z1 - 3.5f, 2.3f);
    ADD(HM_GAR_HOUSE, x0 + 1.0f, z1 - 1.5f, 0.9f);
#undef ADD
    return n;
}

/* ------------------------------------------------------------------ botsingen */

static int house_boxes(int lvl, HBox *b, int max) {
    int n = 0;
#define B(X0, Z0, X1, Z1) do { if (n < max) { b[n].x0 = (X0); b[n].z0 = (Z0); b[n].x1 = (X1); b[n].z1 = (Z1); n++; } } while (0)
    lvl = clampl(lvl, HOME_LEVELS);
    B(EXT[lvl][0], EXT[lvl][1], EXT[lvl][2], EXT[lvl][3]);
    B(412.0f, 207.5f, 420.0f, 208.2f);     /* garage-ingang: zijmuren en achterkant */
    B(412.0f, 215.8f, 420.0f, 216.5f);
    B(419.3f, 207.5f, 420.0f, 216.5f);
    B(405.0f, 176.0f, 433.0f, 176.8f);     /* heggen */
    B(405.0f, 221.2f, 433.0f, 222.0f);
#undef B
    return n;
}

static int interior_boxes(int lvl, HBox *b, int max) {
    float x0, z0, x1, z1;
    int n = 0;
#define B(X0, Z0, X1, Z1) do { if (n < max) { b[n].x0 = (X0); b[n].z0 = (Z0); b[n].x1 = (X1); b[n].z1 = (Z1); n++; } } while (0)
    home_room(lvl, &x0, &z0, &x1, &z1);
    B(x1 - 3.2f, z0, x1 - 0.2f, z0 + 2.2f);           /* bed */
    B(x0 + 3.0f, z1 - 1.1f, x0 + 5.6f, z1 - 0.2f);    /* bank */
    B(x0 + 3.4f, z0, x0 + 5.6f, z0 + 0.7f);           /* tv-meubel */
    B(x1 - 1.0f, z1 - 2.8f, x1 - 0.1f, z1 - 0.9f);    /* bureau */
    if (lvl >= 1) {
        B(x0 + 6.5f, z0, x0 + 9.0f, z0 + 0.8f);       /* keuken */
        B(x0 + 9.2f, z0, x0 + 10.0f, z0 + 0.9f);      /* koelkast */
    }
    if (lvl >= 2) {
        B(x0 + 9.0f, z0 + 5.0f, x0 + 11.6f, z0 + 7.0f);   /* eettafel */
        B(x0 + 0.1f, z1 - 5.0f, x0 + 0.6f, z1 - 2.0f);    /* boekenkast */
        B(x0 + 7.0f, z1 - 1.1f, x0 + 9.6f, z1 - 0.2f);    /* tweede bank */
    }
#undef B
    return n;
}

static int push_boxes(const HBox *b, int n, float *x, float *z, float r) {
    int i, hit = 0;
    for (i = 0; i < n; i++) {
        float qx = clampf(*x, b[i].x0, b[i].x1), qz = clampf(*z, b[i].z0, b[i].z1);
        float dx = *x - qx, dz = *z - qz, d2 = dx * dx + dz * dz;
        if (d2 >= r * r) continue;
        if (d2 > 1e-8f) {
            float d = sqrtf(d2);
            *x += dx / d * (r - d); *z += dz / d * (r - d);
        } else {   /* midden in de doos: kortste weg eruit */
            float l = *x - b[i].x0, rr = b[i].x1 - *x, t = *z - b[i].z0, bo = b[i].z1 - *z;
            float m = fminf(fminf(l, rr), fminf(t, bo));
            if (m == l) *x = b[i].x0 - r; else if (m == rr) *x = b[i].x1 + r;
            else if (m == t) *z = b[i].z0 - r; else *z = b[i].z1 + r;
        }
        hit = 1;
    }
    return hit;
}

int home_room_at(int lvl, int glvl, float x, float z, float *x0, float *z0, float *x1, float *z1) {
    if (x > HOME_ZONE_X) return 0;
    if (z > (HOME_INT_Z + HOME_GAR_Z) * 0.5f) { home_room(lvl, x0, z0, x1, z1); return 1; }
    home_garage_room(glvl, x0, z0, x1, z1);
    return 2;
}

int home_push(int lvl, int glvl, float *x, float *z, float r) {
    HBox b[24];
    int n, hit = 0;
    if (*x > HOME_ZONE_X) {
        if (*x < HOME_PLOT_X0 - 3 || *z < HOME_PLOT_Z0 - 3 || *z > HOME_PLOT_Z1 + 3) return 0;
        n = house_boxes(lvl, b, 24);
        return push_boxes(b, n, x, z, r);
    }
    {
        float x0, z0, x1, z1;
        int room = home_room_at(lvl, glvl, *x, *z, &x0, &z0, &x1, &z1);
        float nx = clampf(*x, x0 + r, x1 - r), nz = clampf(*z, z0 + r, z1 - r);
        if (nx != *x || nz != *z) { *x = nx; *z = nz; hit = 1; }
        if (room == 1) {
            n = interior_boxes(lvl, b, 24);
            hit |= push_boxes(b, n, x, z, r);
        }
    }
    return hit;
}

/* ------------------------------------------------------------------ bouwhulpjes */

static void blob(MeshB *m, V3 c, V3 h, float rv, float rh, int su, int sv, Col c0) {
    SQ s = sq(c, h, rv, rh, c0);
    s.su = su; s.sv = sv;
    mb_sq(m, &s);
}

/* vlak op een muur (naar normaal n): x/z-vlak van (a0,y0) tot (a1,y1) */
static void wall_quad_x(MeshB *m, float x, float z0, float z1, float y0, float y1, float nx, Col c) {
    mb_quad(m, v3(x, y0, z0), v3(x, y0, z1), v3(x, y1, z1), v3(x, y1, z0), v3(nx, 0, 0), c);
}
static void wall_quad_z(MeshB *m, float z, float x0, float x1, float y0, float y1, float nz, Col c) {
    mb_quad(m, v3(x0, y0, z), v3(x1, y0, z), v3(x1, y1, z), v3(x0, y1, z), v3(0, 0, nz), c);
}

static void tree(MeshB *m, float x, float z) {
    mb_capsule(m, v3(x, 0, z), v3(x, 2.4f, z), 0.18f, 0.13f, 8, col(110, 76, 48));
    blob(m, v3(x, 3.2f, z), v3(1.5f, 1.3f, 1.5f), 0.9f, 0.9f, 12, 8, col(70, 140, 70));
    blob(m, v3(x + 0.6f, 3.9f, z - 0.3f), v3(0.9f, 0.8f, 0.9f), 0.9f, 0.9f, 10, 6, col(84, 156, 80));
}

static void plant(MeshB *m, float x, float z, float s) {
    mb_lathe(m, v3(x, 0, z), 1, (const float[]){0, 0.18f * s, 0.24f * s, 0.24f * s, 0},
             (const float[]){0, 0, 0.45f * s, 0.5f * s, 0.5f * s},
             (const Col[]){col(150, 80, 50), col(150, 80, 50), col(170, 92, 58), col(170, 92, 58), col(60, 40, 30)}, 5, 12);
    blob(m, v3(x, 0.85f * s, z), v3(0.35f * s, 0.45f * s, 0.35f * s), 0.8f, 0.8f, 10, 6, col(60, 140, 70));
    blob(m, v3(x + 0.12f * s, 1.15f * s, z + 0.05f), v3(0.22f * s, 0.3f * s, 0.22f * s), 0.8f, 0.8f, 8, 5, col(76, 160, 80));
}

/* ------------------------------------------------------------------ buiten */

void home_build_exterior(MeshB *m, int lvl) {
    float x0, z0, x1, z1, h, zc, dz;
    Col cream = col(236, 226, 206), roof = col(150, 56, 46), wood = col(96, 60, 36), win = col_e(255, 226, 160, 150);
    Col frame = col(240, 240, 236), grass = col(92, 150, 76), stone = col(170, 168, 160);
    int k;
    lvl = clampl(lvl, HOME_LEVELS);
    x0 = EXT[lvl][0]; z0 = EXT[lvl][1]; x1 = EXT[lvl][2]; z1 = EXT[lvl][3]; h = EXT[lvl][4];
    zc = (z0 + z1) * 0.5f;
    /* tuin, pad, oprit, heggen, brievenbus, bomen */
    mb_flat(m, HOME_PLOT_X0, HOME_PLOT_Z0, HOME_PLOT_X1, HOME_PLOT_Z1, 0.008f, grass);
    mb_flat(m, 403.5f, zc - 0.8f, x0, zc + 0.8f, 0.02f, stone);
    mb_flat(m, 403.5f, 208.5f, 412.0f, 215.5f, 0.02f, col(120, 120, 124));
    mb_box(m, v3(405.0f, 0, 176.0f), v3(433.0f, 1.0f, 176.8f), col(60, 120, 60), BOX_NOBOTTOM);
    mb_box(m, v3(405.0f, 0, 221.2f), v3(433.0f, 1.0f, 222.0f), col(60, 120, 60), BOX_NOBOTTOM);
    mb_box(m, v3(405.6f, 0, zc + 1.4f), v3(405.75f, 1.0f, zc + 1.55f), col(60, 60, 64), 0);
    blob(m, v3(405.68f, 1.12f, zc + 1.48f), v3(0.16f, 0.13f, 0.24f), 0.5f, 0.5f, 10, 5, col(200, 40, 40));
    tree(m, 429.5f, 179.5f);
    if (lvl < 2) tree(m, 428.5f, 205.0f);
    /* huis */
    m->ao = 0.2f;
    if (lvl < 2) {
        mb_box(m, v3(x0, 0, z0), v3(x1, h, z1), cream, BOX_NOBOTTOM);
        m->ao = 0;
        mb_roof(m, x0 - 0.45f, z0 - 0.45f, x1 + 0.45f, z1 + 0.45f, h, lvl ? 2.6f : 2.0f, 0, roof, cream);
        mb_box(m, v3(x1 - 2.2f, h, z0 + 1.2f), v3(x1 - 1.4f, h + 2.4f, z0 + 2.0f), col(140, 70, 60), 0);   /* schoorsteen */
    } else {   /* villa: plat dak, grote ramen, aanbouw */
        mb_box(m, v3(x0, 0, z0), v3(x1, h, z1), col(244, 244, 240), BOX_NOBOTTOM);
        m->ao = 0;
        mb_box(m, v3(x0 - 0.3f, h, z0 - 0.3f), v3(x1 + 0.3f, h + 0.35f, z1 + 0.3f), col(60, 60, 66), 0);
        mb_box(m, v3(x0 - 2.6f, 3.0f, zc - 3.5f), v3(x0, 3.2f, zc + 3.5f), col(60, 60, 66), 0);          /* luifel */
        for (k = -1; k <= 1; k += 2) mb_capsule(m, v3(x0 - 2.3f, 0, zc + 3.1f * k), v3(x0 - 2.3f, 3.0f, zc + 3.1f * k), 0.16f, 0.16f, 10, frame);
        for (k = 0; k < 4; k++) {   /* glazen gevel */
            float a = z0 + 1.2f + k * 5.2f;
            if (fabsf(a + 1.6f - zc) < 2.0f) continue;
            wall_quad_x(m, x0 - 0.02f, a, a + 3.2f, 0.4f, 2.7f, -1, col_e(170, 210, 240, 110));
            wall_quad_x(m, x0 - 0.02f, a, a + 3.2f, 3.4f, 5.7f, -1, col_e(170, 210, 240, 110));
        }
        mb_box(m, v3(x0 - 1.4f, 3.25f, z0 + 1.0f), v3(x0, 3.4f, z0 + 5.6f), col(80, 80, 86), 0);   /* balkon */
        mb_box(m, v3(x0 - 1.4f, 3.4f, z0 + 1.0f), v3(x0 - 1.3f, 4.4f, z0 + 5.6f), col_e(170, 210, 240, 60), 0);
    }
    /* deur en kozijn */
    dz = 0.6f;
    wall_quad_x(m, x0 - 0.02f, zc - dz, zc + dz, 0, 2.2f, -1, wood);
    mb_box(m, v3(x0 - 0.1f, 0, zc - dz - 0.12f), v3(x0, 2.32f, zc - dz), frame, 0);
    mb_box(m, v3(x0 - 0.1f, 0, zc + dz), v3(x0, 2.32f, zc + dz + 0.12f), frame, 0);
    mb_box(m, v3(x0 - 0.1f, 2.2f, zc - dz - 0.12f), v3(x0, 2.32f, zc + dz + 0.12f), frame, 0);
    blob(m, v3(x0 - 0.06f, 1.05f, zc + dz - 0.15f), v3(0.04f, 0.04f, 0.04f), 1, 1, 6, 4, col(210, 180, 80));
    mb_box(m, v3(x0 - 0.9f, 0, zc - 1.0f), v3(x0, 0.15f, zc + 1.0f), stone, BOX_NOBOTTOM);   /* stoep */
    if (lvl < 2) {
        int floors = lvl ? 2 : 1, f;
        for (f = 0; f < floors; f++) {
            float y0 = 0.9f + f * 2.8f, y1 = y0 + 1.3f;
            for (k = -1; k <= 1; k += 2) {
                float a = zc + k * (dz + 1.4f);
                wall_quad_x(m, x0 - 0.02f, a - 0.6f, a + 0.6f, y0, y1, -1, win);
                mb_box(m, v3(x0 - 0.12f, y0 - 0.1f, a - 0.7f), v3(x0, y0, a + 0.7f), frame, 0);
            }
            if (f == 1) wall_quad_x(m, x0 - 0.02f, zc - 0.6f, zc + 0.6f, y0, y1, -1, win);
            for (k = 0; k < 2; k++) {
                float a = x0 + (x1 - x0) * (k ? 0.7f : 0.3f);
                wall_quad_z(m, z0 - 0.02f, a - 0.6f, a + 0.6f, y0, y1, -1, win);
                wall_quad_z(m, z1 + 0.02f, a - 0.6f, a + 0.6f, y0, y1, 1, win);
            }
        }
        if (lvl == 1) {   /* veranda */
            mb_box(m, v3(x0 - 1.8f, 2.6f, zc - 1.6f), v3(x0, 2.75f, zc + 1.6f), roof, 0);
            for (k = -1; k <= 1; k += 2) mb_capsule(m, v3(x0 - 1.6f, 0, zc + 1.4f * k), v3(x0 - 1.6f, 2.6f, zc + 1.4f * k), 0.09f, 0.09f, 8, frame);
        }
    }
    plant(m, x0 - 0.5f, zc - 1.4f, 1.0f);
    plant(m, x0 - 0.5f, zc + 1.4f, 1.0f);
    /* ingang van de ondergrondse garage */
    m->ao = 0.15f;
    mb_box(m, v3(412.0f, 0, 207.5f), v3(420.0f, 2.8f, 216.5f), col(150, 150, 154), BOX_NOBOTTOM);
    m->ao = 0;
    wall_quad_x(m, 411.98f, 208.4f, 215.6f, 0.0f, 2.4f, -1, col(14, 14, 16));
    for (k = 0; k < 6; k++)   /* geel-zwarte balk boven de ingang */
        wall_quad_x(m, 411.96f, 208.4f + k * 1.2f, 209.0f + k * 1.2f, 2.42f, 2.7f, -1, col_e(250, 200, 30, 90));
    mb_flat(m, 405.0f, 208.6f, 412.0f, 215.4f, 0.025f, col(60, 60, 64));
    for (k = 0; k < 5; k++) mb_flat(m, 405.4f + k * 1.4f, 211.8f, 406.2f + k * 1.4f, 212.2f, 0.03f, col_e(240, 240, 240, 40));
}

/* ------------------------------------------------------------------ binnen */

static void sofa(MeshB *m, float x0, float z0, float x1, float z1, Col c, int backAtZ1) {
    float zb = backAtZ1 ? z1 - 0.25f : z0, ze = backAtZ1 ? z1 : z0 + 0.25f;
    float zs0 = backAtZ1 ? z0 : z0 + 0.25f, zs1 = backAtZ1 ? z1 - 0.25f : z1;
    int k, n = (int)((x1 - x0) / 0.85f);
    mb_box(m, v3(x0, 0.05f, z0), v3(x1, 0.42f, z1), col_mul(c, 0.8f), 0);
    blob(m, v3((x0 + x1) * 0.5f, 0.75f, (zb + ze) * 0.5f), v3((x1 - x0) * 0.5f, 0.38f, 0.14f), 0.4f, 0.35f, 16, 6, c);
    for (k = 0; k < 2; k++) blob(m, v3(k ? x1 - 0.12f : x0 + 0.12f, 0.6f, (z0 + z1) * 0.5f), v3(0.13f, 0.2f, (z1 - z0) * 0.5f), 0.4f, 0.4f, 8, 5, c);
    if (n < 1) n = 1;
    for (k = 0; k < n; k++) {
        float a = x0 + 0.25f + (x1 - x0 - 0.5f) * (k + 0.5f) / n;
        blob(m, v3(a, 0.5f, (zs0 + zs1) * 0.5f), v3((x1 - x0 - 0.5f) * 0.5f / n - 0.02f, 0.09f, (zs1 - zs0) * 0.5f - 0.03f), 0.45f, 0.4f, 10, 5, col_mul(c, 1.08f));
    }
}

static void table(MeshB *m, float x0, float z0, float x1, float z1, float h, Col top, Col leg) {
    int a, b;
    mb_box(m, v3(x0, h - 0.06f, z0), v3(x1, h, z1), top, 0);
    for (a = 0; a < 2; a++)
        for (b = 0; b < 2; b++)
            mb_capsule(m, v3(a ? x1 - 0.1f : x0 + 0.1f, 0, b ? z1 - 0.1f : z0 + 0.1f), v3(a ? x1 - 0.1f : x0 + 0.1f, h - 0.06f, b ? z1 - 0.1f : z0 + 0.1f), 0.035f, 0.03f, 6, leg);
}

void home_build_interior(MeshB *m, int lvl) {
    float x0, z0, x1, z1, zc, H = 3.0f;
    Col wall = col(232, 222, 204), floorc = col(156, 110, 70), white = col(240, 240, 236), dark = col(40, 40, 44);
    Col winc = col_e(200, 225, 245, 200), wood = col(110, 72, 42);
    int k;
    lvl = clampl(lvl, HOME_LEVELS);
    home_room(lvl, &x0, &z0, &x1, &z1);
    zc = (z0 + z1) * 0.5f;
    /* vloer met planken, kleed, plafond, muren */
    mb_flat(m, x0, z0, x1, z1, 0.01f, floorc);
    for (k = 1; k < (int)(z1 - z0) * 2; k++) mb_flat(m, x0, z0 + k * 0.5f - 0.01f, x1, z0 + k * 0.5f + 0.01f, 0.012f, col_mul(floorc, 0.85f));
    mb_flat(m, x0 + 2.6f, zc - 1.6f, x0 + 6.2f, zc + 1.4f, 0.016f, col(150, 40, 50));
    mb_flat(m, x0 + 2.8f, zc - 1.4f, x0 + 6.0f, zc + 1.2f, 0.018f, col(190, 170, 120));
    mb_quad(m, v3(x0, H, z0), v3(x1, H, z0), v3(x1, H, z1), v3(x0, H, z1), v3(0, -1, 0), white);
    mb_box(m, v3(x0 - 0.3f, 0, z0 - 0.3f), v3(x0, H, z1 + 0.3f), wall, 0);
    mb_box(m, v3(x1, 0, z0 - 0.3f), v3(x1 + 0.3f, H, z1 + 0.3f), wall, 0);
    mb_box(m, v3(x0, 0, z0 - 0.3f), v3(x1, H, z0), wall, 0);
    mb_box(m, v3(x0, 0, z1), v3(x1, H, z1 + 0.3f), wall, 0);
    for (k = 0; k < 4; k++) {   /* plinten */
        if (k < 2) mb_box(m, v3(k ? x1 - 0.03f : x0, 0, z0), v3(k ? x1 : x0 + 0.03f, 0.12f, z1), white, 0);
        else mb_box(m, v3(x0, 0, k == 3 ? z1 - 0.03f : z0), v3(x1, 0.12f, k == 3 ? z1 : z0 + 0.03f), white, 0);
    }
    /* lampen aan het plafond */
    for (k = 0; k < 1 + lvl; k++) {
        float lx = x0 + (x1 - x0) * (k + 1) / (2 + lvl);
        blob(m, v3(lx, H - 0.1f, zc), v3(0.3f, 0.08f, 0.3f), 0.5f, 0.8f, 12, 4, col_e(255, 240, 200, 255));
    }
    /* ramen (daglicht) in de oost- en zuidmuur */
    for (k = 0; k < 2 + lvl; k++) {
        float a = z0 + (z1 - z0) * (k + 0.5f) / (2 + lvl);
        if (a > z1 - 3.0f) continue;   /* niet achter het bureau */
        wall_quad_x(m, x1 - 0.01f, a - 0.6f, a + 0.6f, 1.0f, 2.3f, -1, winc);
        mb_box(m, v3(x1 - 0.08f, 0.92f, a - 0.68f), v3(x1, 1.0f, a + 0.68f), white, 0);
    }
    for (k = 0; k < 1 + lvl; k++) {
        float a = x0 + 6.5f + k * 3.0f;
        if (a + 0.7f > x1 - 0.5f) break;
        wall_quad_z(m, z1 - 0.01f, a - 0.7f, a + 0.7f, 1.0f, 2.3f, -1, winc);
    }
    /* voordeur (uitgang) */
    wall_quad_x(m, x0 + 0.01f, zc - 0.6f, zc + 0.6f, 0, 2.2f, 1, wood);
    mb_box(m, v3(x0, 2.2f, zc - 0.72f), v3(x0 + 0.08f, 2.32f, zc + 0.72f), white, 0);
    blob(m, v3(x0 + 0.06f, 1.05f, zc + 0.45f), v3(0.04f, 0.04f, 0.04f), 1, 1, 6, 4, col(210, 180, 80));
    mb_flat(m, x0 + 0.1f, zc - 0.6f, x0 + 0.9f, zc + 0.6f, 0.02f, col(70, 60, 50));   /* deurmat */
    /* deur naar de garage (trap omlaag) met bordje */
    wall_quad_z(m, z0 + 0.01f, x0 + 1.0f, x0 + 2.2f, 0, 2.2f, 1, col(70, 72, 80));
    wall_quad_z(m, z0 + 0.012f, x0 + 1.2f, x0 + 2.0f, 2.3f, 2.6f, 1, col_e(250, 200, 30, 200));
    /* bed */
    {
        float bx0 = x1 - 3.2f, bx1 = x1 - 0.2f, bz0 = z0 + 0.05f, bz1 = z0 + 2.2f;
        Col sheet = lvl == 2 ? col(70, 80, 140) : col(90, 130, 180);
        mb_box(m, v3(bx0, 0.05f, bz0), v3(bx1, 0.45f, bz1), wood, 0);
        mb_box(m, v3(bx1 - 0.1f, 0.05f, bz0), v3(bx1, 1.1f, bz1), wood, 0);
        blob(m, v3((bx0 + bx1) * 0.5f, 0.55f, (bz0 + bz1) * 0.5f), v3((bx1 - bx0) * 0.5f - 0.08f, 0.12f, (bz1 - bz0) * 0.5f - 0.06f), 0.35f, 0.25f, 14, 4, white);
        blob(m, v3((bx0 + bx1) * 0.5f - 0.3f, 0.66f, (bz0 + bz1) * 0.5f), v3((bx1 - bx0) * 0.35f, 0.07f, (bz1 - bz0) * 0.5f - 0.02f), 0.35f, 0.25f, 14, 4, sheet);
        for (k = 0; k < 2; k++) blob(m, v3(bx1 - 0.45f, 0.72f, bz0 + 0.55f + k * 1.0f), v3(0.22f, 0.08f, 0.4f), 0.6f, 0.6f, 10, 4, white);
        mb_box(m, v3(bx0 - 0.6f, 0.05f, bz0), v3(bx0 - 0.1f, 0.55f, bz0 + 0.5f), wood, 0);   /* nachtkastje */
        blob(m, v3(bx0 - 0.35f, 0.75f, bz0 + 0.25f), v3(0.1f, 0.18f, 0.1f), 0.8f, 0.8f, 8, 5, col_e(255, 230, 170, 200));
    }
    /* bank en tv */
    sofa(m, x0 + 3.0f, z1 - 1.1f, x0 + 5.6f, z1 - 0.2f, lvl == 2 ? col(60, 60, 66) : col(120, 70, 50), 1);
    mb_box(m, v3(x0 + 3.4f, 0.05f, z0 + 0.05f), v3(x0 + 5.6f, 0.55f, z0 + 0.6f), wood, 0);
    mb_box(m, v3(x0 + 3.5f - 0.2f * lvl, 0.75f, z0 + 0.12f), v3(x0 + 5.5f + 0.2f * lvl, 1.85f + 0.2f * lvl, z0 + 0.2f), dark, 0);
    wall_quad_z(m, z0 + 0.205f, x0 + 3.58f - 0.2f * lvl, x0 + 5.42f + 0.2f * lvl, 0.82f, 1.78f + 0.2f * lvl, 1, col_e(60, 120, 200, 220));
    table(m, x0 + 3.8f, zc - 0.4f, x0 + 5.0f, zc + 0.3f, 0.4f, wood, dark);   /* salontafel */
    /* bureau met laptop */
    table(m, x1 - 1.0f, z1 - 2.8f, x1 - 0.15f, z1 - 0.9f, 0.76f, white, dark);
    mb_box(m, v3(x1 - 0.75f, 0.76f, z1 - 2.1f), v3(x1 - 0.4f, 0.79f, z1 - 1.55f), col(180, 180, 186), 0);
    mb_quad(m, v3(x1 - 0.42f, 0.79f, z1 - 2.1f), v3(x1 - 0.42f, 0.79f, z1 - 1.55f), v3(x1 - 0.32f, 1.12f, z1 - 1.55f), v3(x1 - 0.32f, 1.12f, z1 - 2.1f), v3(-0.95f, 0.3f, 0), col_e(80, 160, 255, 255));
    blob(m, v3(x1 - 1.45f, 0.45f, z1 - 1.85f), v3(0.24f, 0.05f, 0.24f), 0.5f, 0.8f, 10, 3, dark);   /* stoel */
    mb_capsule(m, v3(x1 - 1.45f, 0.0f, z1 - 1.85f), v3(x1 - 1.45f, 0.42f, z1 - 1.85f), 0.04f, 0.04f, 6, dark);
    blob(m, v3(x1 - 1.66f, 0.8f, z1 - 1.85f), v3(0.04f, 0.32f, 0.22f), 0.5f, 0.6f, 8, 4, dark);
    plant(m, x0 + 0.6f, z1 - 0.6f, 1.2f);
    plant(m, x1 - 0.5f, z1 - 0.5f, 0.9f);
    if (lvl >= 1) {   /* keuken en koelkast */
        mb_box(m, v3(x0 + 6.5f, 0, z0 + 0.05f), v3(x0 + 9.0f, 0.9f, z0 + 0.8f), white, 0);
        mb_box(m, v3(x0 + 6.45f, 0.9f, z0 + 0.05f), v3(x0 + 9.05f, 0.95f, z0 + 0.85f), col(60, 60, 64), 0);
        mb_box(m, v3(x0 + 6.6f, 1.6f, z0 + 0.05f), v3(x0 + 8.9f, 2.4f, z0 + 0.4f), white, 0);
        mb_box(m, v3(x0 + 9.2f, 0, z0 + 0.05f), v3(x0 + 10.0f, 2.0f, z0 + 0.9f), col(200, 200, 206), 0);
        mb_lathe(m, v3(x0 + 7.4f, 0.95f, z0 + 0.45f), 1, (const float[]){0, 0.18f, 0.2f, 0.17f, 0}, (const float[]){0, 0, 0.12f, 0.14f, 0.14f},
                 (const Col[]){white, white, white, white, white}, 5, 12);   /* schaal */
        for (k = 0; k < 3; k++) blob(m, v3(x0 + 7.35f + k * 0.08f, 1.1f, z0 + 0.45f), v3(0.06f, 0.06f, 0.06f), 1, 1, 8, 5, col(220, 60 + k * 60, 40));
    }
    if (lvl >= 2) {   /* eettafel, boekenkast, tweede bank, schilderij */
        int c;
        table(m, x0 + 9.0f, z0 + 5.0f, x0 + 11.6f, z0 + 7.0f, 0.76f, wood, dark);
        for (c = 0; c < 4; c++) {
            float cx = x0 + 9.6f + (c & 1) * 1.4f, cz = c < 2 ? z0 + 4.6f : z0 + 7.4f;
            blob(m, v3(cx, 0.46f, cz), v3(0.22f, 0.04f, 0.22f), 0.5f, 0.6f, 8, 3, dark);
            mb_capsule(m, v3(cx, 0, cz), v3(cx, 0.44f, cz), 0.03f, 0.03f, 6, dark);
        }
        mb_box(m, v3(x0 + 0.05f, 0, z1 - 5.0f), v3(x0 + 0.6f, 2.4f, z1 - 2.0f), wood, 0);
        for (c = 0; c < 4; c++) for (k = 0; k < 6; k++)
            mb_box(m, v3(x0 + 0.6f, 0.3f + c * 0.55f, z1 - 4.9f + k * 0.48f), v3(x0 + 0.64f, 0.7f + c * 0.55f, z1 - 4.55f + k * 0.48f), col(60 + 40 * ((c + k) % 4), 70 + 30 * (k % 3), 90 + 50 * (c % 2)), 0);
        sofa(m, x0 + 7.0f, z1 - 1.1f, x0 + 9.6f, z1 - 0.2f, col(60, 60, 66), 1);
        wall_quad_z(m, z0 + 0.012f, x0 + 11.0f, x0 + 13.0f, 1.2f, 2.4f, 1, col(220, 140, 60));
        wall_quad_z(m, z0 + 0.014f, x0 + 11.2f, x0 + 12.8f, 1.4f, 2.2f, 1, col(60, 120, 170));
    }
}

/* ------------------------------------------------------------------ ondergrondse garage */

void home_build_garage(MeshB *m, int glvl) {
    float x0, z0, x1, z1, H = 3.2f;
    Col concrete = col(130, 130, 134), floorc = col(84, 84, 88), yellow = col_e(240, 200, 40, 60), white = col(220, 220, 220);
    int k, cap = clampl(glvl, GARAGE_LEVELS) + 1;
    home_garage_room(glvl, &x0, &z0, &x1, &z1);
    mb_flat(m, x0, z0, x1, z1, 0.01f, floorc);
    mb_quad(m, v3(x0, H, z0), v3(x1, H, z0), v3(x1, H, z1), v3(x0, H, z1), v3(0, -1, 0), col(110, 110, 114));
    m->ao = 0.3f;
    mb_box(m, v3(x0 - 0.3f, 0, z0 - 0.3f), v3(x0, H, z1 + 0.3f), concrete, 0);
    mb_box(m, v3(x1, 0, z0 - 0.3f), v3(x1 + 0.3f, H, z1 + 0.3f), concrete, 0);
    mb_box(m, v3(x0, 0, z0 - 0.3f), v3(x1, H, z0), concrete, 0);
    mb_box(m, v3(x0, 0, z1), v3(x1, H, z1 + 0.3f), concrete, 0);
    m->ao = 0;
    /* gele strepen tussen de plekken, nummers als stippen */
    for (k = 0; k <= cap; k++) {
        float lx = x0 + 1.9f + 4.2f * k;
        mb_flat(m, lx - 0.06f, z0, lx + 0.06f, z0 + 5.6f, 0.02f, yellow);
    }
    for (k = 0; k < cap; k++) {
        int d;
        for (d = 0; d <= k; d++) blob(m, v3(x0 + 4.0f + 4.2f * k - 0.3f * k * 0.5f + 0.3f * d, 1.6f, z0 + 0.02f), v3(0.1f, 0.1f, 0.01f), 1, 1, 8, 4, white);
        mb_box(m, v3(x0 + 1.75f + 4.2f * k, 0, z0), v3(x0 + 2.05f + 4.2f * k, H, z0 + 0.3f), col(150, 150, 154), 0);   /* zuiltjes */
    }
    mb_box(m, v3(x0 + 1.75f + 4.2f * cap, 0, z0), v3(x0 + 2.05f + 4.2f * cap, H, z0 + 0.3f), col(150, 150, 154), 0);
    for (k = 0; k < cap + 1; k++)   /* tl-buizen */
        mb_box(m, v3(x0 + 1.5f + 4.2f * k, H - 0.1f, (z0 + z1) * 0.5f - 0.06f), v3(x0 + 3.5f + 4.2f * k, H - 0.03f, (z0 + z1) * 0.5f + 0.06f), col_e(240, 250, 255, 255), 0);
    /* hellingbaan naar buiten + groen EXIT-bord */
    mb_quad(m, v3(x1 - 4.0f, 0.015f, z1 - 6.0f), v3(x1, 1.2f, z1 - 6.0f), v3(x1, 1.2f, z1 - 1.0f), v3(x1 - 4.0f, 0.015f, z1 - 1.0f), v3(-0.29f, 0.96f, 0), col(100, 100, 104));
    for (k = 0; k < 6; k++) mb_flat(m, x1 - 3.6f + k * 0.6f, z1 - 3.6f, x1 - 3.3f + k * 0.6f, z1 - 3.4f, 0.03f + k * 0.18f, white);
    mb_box(m, v3(x1 - 0.12f, 2.3f, z1 - 4.2f), v3(x1, 2.8f, z1 - 2.8f), col_e(40, 220, 90, 255), 0);
    /* deur naar het huis */
    wall_quad_x(m, x0 + 0.01f, z1 - 2.1f, z1 - 0.9f, 0, 2.2f, 1, col(150, 100, 60));
    mb_box(m, v3(x0, 2.3f, z1 - 2.0f), v3(x0 + 0.1f, 2.6f, z1 - 1.0f), col_e(250, 200, 30, 220), 0);
    /* rijstrook-pijlen */
    for (k = 0; k < cap; k++) mb_flat(m, x0 + 3.2f + 4.2f * k, z1 - 3.7f, x0 + 4.8f + 4.2f * k, z1 - 3.3f, 0.02f, white);
}
