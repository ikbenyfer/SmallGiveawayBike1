/* Stadsgenerator. Alles is vast (zelfde zaadjes), dus de stad is elke keer gelijk. */
#include "world.h"
#include "models.h"
#include <stdlib.h>
#include <string.h>

static const char *LAYOUT[NB] = {
    "HHHPHHH",
    "HHHPHHH",
    "SLSASDS",
    "ADSCDAL",
    "DSAPDGA",
    "LADSADS",
    "PDSLDAP",
};

static World *W;
static MeshB *cur;

static void at(float x, float z) {
    int ci = (int)floorf(x / PITCH), cj = (int)floorf(z / PITCH);
    if (ci < 0) ci = 0; if (ci >= CHN) ci = CHN - 1;
    if (cj < 0) cj = 0; if (cj >= CHN) cj = CHN - 1;
    cur = &W->chunks[cj * CHN + ci].mb;
}

static void add_box(float x0, float z0, float x1, float z1, float y1, int kind) {
    if (W->nboxes >= W->capboxes) {
        int nc = W->capboxes ? W->capboxes * 2 : 512;
        Box *nb = (Box *)realloc(W->boxes, (size_t)nc * sizeof(Box));
        if (!nb) return;
        W->boxes = nb; W->capboxes = nc;
    }
    Box *b = &W->boxes[W->nboxes++];
    b->x0 = x0 < x1 ? x0 : x1; b->x1 = x0 < x1 ? x1 : x0;
    b->z0 = z0 < z1 ? z0 : z1; b->z1 = z0 < z1 ? z1 : z0;
    b->y1 = y1; b->kind = kind;
}

static void add_park(float x, float z, float h, int type, int variant) {
    if (W->npark >= (int)(sizeof(W->park) / sizeof(W->park[0]))) return;
    ParkSpot *p = &W->park[W->npark++];
    p->x = x; p->z = z; p->h = h; p->type = type; p->variant = variant;
}

/* ------------------------------------------------------------------ onderdelen */

static const int WALLS[][3] = {
    {150, 72, 58}, {192, 172, 140}, {140, 140, 146}, {92, 106, 126}, {210, 200, 176},
    {118, 92, 84}, {170, 120, 90}, {120, 130, 110}};
#define NWALLS ((int)(sizeof(WALLS) / sizeof(WALLS[0])))

static Col wall_col(Rng *r) { int k = rng_int(r, NWALLS); return col(WALLS[k][0], WALLS[k][1], WALLS[k][2]); }

/* punt op een gevel: A..B langs de grond, t meter vanaf A, hoogte y, uit de muur d */
static V3 fpt(V3 A, V3 dir, V3 n, float t, float y, float d) {
    return v3(A.x + dir.x * t + n.x * d, y, A.z + dir.z * t + n.z * d);
}

static void face_quad(V3 A, V3 dir, V3 n, float t0, float t1, float y0, float y1, float d, Col c) {
    mb_quad(cur, fpt(A, dir, n, t0, y0, d), fpt(A, dir, n, t1, y0, d), fpt(A, dir, n, t1, y1, d), fpt(A, dir, n, t0, y1, d), n, c);
}

/* face k: 0 = noord (-z), 1 = oost (+x), 2 = zuid (+z), 3 = west (-x) */
static void face_info(float x0, float z0, float x1, float z1, int k, V3 *A, V3 *dir, V3 *n, float *L) {
    switch (k) {
    case 0: *A = v3(x0, 0, z0); *dir = v3(1, 0, 0); *n = v3(0, 0, -1); *L = x1 - x0; break;
    case 1: *A = v3(x1, 0, z0); *dir = v3(0, 0, 1); *n = v3(1, 0, 0); *L = z1 - z0; break;
    case 2: *A = v3(x1, 0, z1); *dir = v3(-1, 0, 0); *n = v3(0, 0, 1); *L = x1 - x0; break;
    default: *A = v3(x0, 0, z1); *dir = v3(0, 0, -1); *n = v3(-1, 0, 0); *L = z1 - z0; break;
    }
}

static void protrude_box(V3 A, V3 dir, V3 n, float t0, float t1, float y0, float y1, float d0, float d1, Col c) {
    V3 p0 = fpt(A, dir, n, t0, y0, d0), p1 = fpt(A, dir, n, t1, y1, d1);
    V3 mn = v3(fminf(p0.x, p1.x), y0, fminf(p0.z, p1.z)), mx = v3(fmaxf(p0.x, p1.x), y1, fmaxf(p0.z, p1.z));
    mb_box(cur, mn, mx, c, 0);
}

static void tree(float x, float z, Rng *r) {
    float s = rng_range(r, 0.8f, 1.25f);
    Col trunk = col(105, 75, 50);
    mb_box(cur, v3(x - 0.18f, CURB, z - 0.18f), v3(x + 0.18f, CURB + 2.0f * s, z + 0.18f), trunk, BOX_NOBOTTOM);
    if (rng_f(r) < 0.5f) {
        Col g = col(40 + rng_int(r, 20), 110 + rng_int(r, 30), 50);
        mb_cone(cur, v3(x, CURB + 1.4f * s, z), 1.9f * s, 2.6f * s, 7, g);
        mb_cone(cur, v3(x, CURB + 2.8f * s, z), 1.4f * s, 2.2f * s, 7, col_mul(g, 1.1f));
    } else {
        Col g = col(60 + rng_int(r, 30), 130 + rng_int(r, 30), 55);
        mb_box(cur, v3(x - 1.5f * s, CURB + 1.8f * s, z - 1.5f * s), v3(x + 1.5f * s, CURB + 4.0f * s, z + 1.5f * s), g, 0);
        mb_box(cur, v3(x - 1.0f * s, CURB + 4.0f * s, z - 1.0f * s), v3(x + 1.0f * s, CURB + 4.8f * s, z + 1.0f * s), col_mul(g, 1.08f), 0);
    }
    add_box(x - 0.3f, z - 0.3f, x + 0.3f, z + 0.3f, 6.0f, BOX_TREE);
}

static void lamp(float x, float z, float dx, float dz) {
    Col pole = col(60, 62, 66);
    mb_box(cur, v3(x - 0.07f, CURB, z - 0.07f), v3(x + 0.07f, CURB + 4.6f, z + 0.07f), pole, BOX_NOBOTTOM);
    {
        float ax = x + dx * 1.1f, az = z + dz * 1.1f;
        V3 mn = v3(fminf(x, ax) - 0.05f, CURB + 4.5f, fminf(z, az) - 0.05f);
        V3 mx = v3(fmaxf(x, ax) + 0.05f, CURB + 4.6f, fmaxf(z, az) + 0.05f);
        mb_box(cur, mn, mx, pole, 0);
        mb_box(cur, v3(ax - 0.22f, CURB + 4.3f, az - 0.22f), v3(ax + 0.22f, CURB + 4.5f, az + 0.22f), col_e(255, 232, 170, 255), 0);
    }
    add_box(x - 0.12f, z - 0.12f, x + 0.12f, z + 0.12f, 4.6f, BOX_PROP);
}

static void bench(float x, float z, int along_x) {
    Col wood = col(140, 100, 60), iron = col(50, 50, 55);
    if (along_x) {
        mb_box(cur, v3(x - 0.9f, CURB + 0.40f, z - 0.25f), v3(x + 0.9f, CURB + 0.48f, z + 0.25f), wood, 0);
        mb_box(cur, v3(x - 0.9f, CURB + 0.48f, z + 0.18f), v3(x + 0.9f, CURB + 0.9f, z + 0.25f), wood, 0);
        mb_box(cur, v3(x - 0.85f, CURB, z - 0.2f), v3(x - 0.75f, CURB + 0.4f, z + 0.2f), iron, 0);
        mb_box(cur, v3(x + 0.75f, CURB, z - 0.2f), v3(x + 0.85f, CURB + 0.4f, z + 0.2f), iron, 0);
    } else {
        mb_box(cur, v3(x - 0.25f, CURB + 0.40f, z - 0.9f), v3(x + 0.25f, CURB + 0.48f, z + 0.9f), wood, 0);
        mb_box(cur, v3(x + 0.18f, CURB + 0.48f, z - 0.9f), v3(x + 0.25f, CURB + 0.9f, z + 0.9f), wood, 0);
        mb_box(cur, v3(x - 0.2f, CURB, z - 0.85f), v3(x + 0.2f, CURB + 0.4f, z - 0.75f), iron, 0);
        mb_box(cur, v3(x - 0.2f, CURB, z + 0.75f), v3(x + 0.2f, CURB + 0.4f, z + 0.85f), iron, 0);
    }
}

/* Gebouw met ramen. door: welke gevel (0..3) de deur/winkel heeft; -1 = geen. */
static void building(float x0, float z0, float x1, float z1, int floors, Col wall, int door, int shop, Rng *r) {
    float fh = 3.2f;
    float h = CURB + floors * fh;
    Col roof = col_mul(wall, 0.55f);
    int k;
    cur->ao = 0.35f;
    mb_box2(cur, v3(x0, CURB, z0), v3(x1, h, z1), roof, wall, BOX_NOBOTTOM);
    cur->ao = 0;
    /* dakrand */
    {
        Col pc = col_mul(wall, 0.78f);
        float t = 0.3f, ph = 0.55f;
        mb_box(cur, v3(x0, h, z0), v3(x1, h + ph, z0 + t), pc, BOX_NOBOTTOM);
        mb_box(cur, v3(x0, h, z1 - t), v3(x1, h + ph, z1), pc, BOX_NOBOTTOM);
        mb_box(cur, v3(x0, h, z0 + t), v3(x0 + t, h + ph, z1 - t), pc, BOX_NOBOTTOM);
        mb_box(cur, v3(x1 - t, h, z0 + t), v3(x1, h + ph, z1 - t), pc, BOX_NOBOTTOM);
        if (rng_f(r) < 0.7f && x1 - x0 > 6 && z1 - z0 > 6) {
            float ax = rng_range(r, x0 + 2, x1 - 4), az = rng_range(r, z0 + 2, z1 - 4);
            mb_box(cur, v3(ax, h, az), v3(ax + 2.0f, h + 1.2f, az + 1.6f), col(150, 152, 156), BOX_NOBOTTOM);
        }
        if (floors >= 6 && rng_f(r) < 0.4f) {
            float ax = (x0 + x1) * 0.5f, az = (z0 + z1) * 0.5f;
            mb_cyl(cur, v3(ax, h + 1.5f, az), 1.3f, 2.4f, 8, 1, col(120, 90, 70), col(90, 70, 55), 1);
            mb_box(cur, v3(ax - 1.0f, h, az - 1.0f), v3(ax + 1.0f, h + 1.5f, az + 1.0f), col(70, 70, 75), BOX_NOBOTTOM);
        }
    }
    for (k = 0; k < 4; k++) {
        V3 A, dir, n; float L;
        int cnt, f, wdw;
        face_info(x0, z0, x1, z1, k, &A, &dir, &n, &L);
        cnt = (int)((L - 1.0f) / 3.0f);
        if (cnt < 1) continue;
        {
            float sp = L / cnt;
            int startf = 0;
            if (k == door) {
                startf = 1;
                if (shop) {
                    Col aw = col(60 + rng_int(r, 190), 60 + rng_int(r, 150), 60 + rng_int(r, 120));
                    face_quad(A, dir, n, 1.0f, L - 1.0f, CURB + 0.25f, CURB + 2.7f, 0.04f, col_e(200, 220, 230, 150));
                    face_quad(A, dir, n, L * 0.5f - 0.8f, L * 0.5f + 0.8f, CURB, CURB + 2.5f, 0.06f, col(60, 45, 35));
                    protrude_box(A, dir, n, 0.6f, L - 0.6f, CURB + 2.85f, CURB + 3.05f, 0.0f, 1.4f, aw);
                    protrude_box(A, dir, n, 1.5f, L - 1.5f, CURB + 3.2f, CURB + 4.0f, 0.0f, 0.25f, col_e(aw.r, aw.g, aw.b, 170));
                } else {
                    face_quad(A, dir, n, L * 0.5f - 0.8f, L * 0.5f + 0.8f, CURB, CURB + 2.4f, 0.05f, col(70, 50, 40));
                    protrude_box(A, dir, n, L * 0.5f - 1.2f, L * 0.5f + 1.2f, CURB + 2.6f, CURB + 2.75f, 0.0f, 0.9f, col_mul(wall, 0.6f));
                    for (wdw = 0; wdw < cnt; wdw++) {
                        float tc = (wdw + 0.5f) * sp;
                        if (fabsf(tc - L * 0.5f) < 1.8f) continue;
                        face_quad(A, dir, n, tc - 0.65f, tc + 0.65f, CURB + 0.9f, CURB + 2.4f, 0.04f, col(45, 58, 78));
                    }
                }
            }
            for (f = startf; f < floors; f++) {
                float y = CURB + f * fh + 0.9f;
                for (wdw = 0; wdw < cnt; wdw++) {
                    float tc = (wdw + 0.5f) * sp;
                    int lit = rng_f(r) < 0.32f;
                    Col c = lit ? col_e(255, 214, 140, 255) : col(45 + rng_int(r, 10), 58, 78);
                    face_quad(A, dir, n, tc - 0.65f, tc + 0.65f, y, y + 1.6f, 0.04f, c);
                }
            }
        }
    }
    add_box(x0, z0, x1, z1, h + 0.55f, BOX_BUILDING);
}

static void house(float x0, float z0, float x1, float z1, int door, Rng *r) {
    static const int pastel[][3] = {{230, 220, 190}, {200, 220, 230}, {235, 200, 190}, {210, 230, 200}, {240, 235, 225}, {220, 200, 150}};
    static const int roofs[][3] = {{150, 60, 50}, {70, 70, 82}, {90, 110, 140}, {120, 80, 60}};
    int p = rng_int(r, 6), q = rng_int(r, 4);
    Col wall = col(pastel[p][0], pastel[p][1], pastel[p][2]);
    Col roof = col(roofs[q][0], roofs[q][1], roofs[q][2]);
    float h = CURB + 3.0f;
    int k;
    cur->ao = 0.3f;
    mb_box2(cur, v3(x0, CURB, z0), v3(x1, h, z1), wall, wall, BOX_NOBOTTOM | BOX_NOTOP);
    cur->ao = 0;
    mb_roof(cur, x0 - 0.4f, z0 - 0.4f, x1 + 0.4f, z1 + 0.4f, h, 2.0f, 1, roof, wall);
    mb_box(cur, v3(x1 - 2.0f, h + 0.5f, z0 + 1.2f), v3(x1 - 1.3f, h + 2.6f, z0 + 1.9f), col(130, 80, 70), BOX_NOBOTTOM);
    for (k = 0; k < 4; k++) {
        V3 A, dir, n; float L;
        face_info(x0, z0, x1, z1, k, &A, &dir, &n, &L);
        if (k == door) {
            face_quad(A, dir, n, L * 0.5f - 0.55f, L * 0.5f + 0.55f, CURB, CURB + 2.2f, 0.04f, col(110, 60, 40));
            face_quad(A, dir, n, 0.8f, 2.4f, CURB + 0.9f, CURB + 2.2f, 0.04f, col_e(255, 214, 140, rng_f(r) < 0.5f ? 200 : 0));
            face_quad(A, dir, n, L - 2.4f, L - 0.8f, CURB + 0.9f, CURB + 2.2f, 0.04f, col(50, 65, 85));
        } else {
            face_quad(A, dir, n, L * 0.5f - 0.8f, L * 0.5f + 0.8f, CURB + 0.9f, CURB + 2.2f, 0.04f,
                      rng_f(r) < 0.3f ? col_e(255, 214, 140, 220) : col(50, 65, 85));
        }
    }
    add_box(x0, z0, x1, z1, h + 2.0f, BOX_BUILDING);
}

static void fence_line(float x0, float z0, float x1, float z1, Col c) {
    float t = 0.06f;
    if (fabsf(x1 - x0) > fabsf(z1 - z0)) {
        mb_box(cur, v3(fminf(x0, x1), CURB, z0 - t), v3(fmaxf(x0, x1), CURB + 0.9f, z0 + t), c, BOX_NOBOTTOM);
        add_box(x0, z0 - 0.1f, x1, z0 + 0.1f, CURB + 0.9f, BOX_FENCE);
    } else {
        mb_box(cur, v3(x0 - t, CURB, fminf(z0, z1)), v3(x0 + t, CURB + 0.9f, fmaxf(z0, z1)), c, BOX_NOBOTTOM);
        add_box(x0 - 0.1f, z0, x0 + 0.1f, z1, CURB + 0.9f, BOX_FENCE);
    }
}

static void dumpster(float x, float z) {
    mb_box(cur, v3(x - 0.9f, CURB, z - 0.6f), v3(x + 0.9f, CURB + 1.3f, z + 0.6f), col(40, 100, 60), BOX_NOBOTTOM);
    mb_box(cur, v3(x - 0.95f, CURB + 1.3f, z - 0.65f), v3(x + 0.95f, CURB + 1.4f, z + 0.65f), col(30, 70, 45), 0);
    add_box(x - 0.9f, z - 0.6f, x + 0.9f, z + 0.6f, CURB + 1.4f, BOX_PROP);
}

/* ------------------------------------------------------------------ bloktypes */

static void gen_downtown(float ix0, float iz0, float ix1, float iz1, Rng *r) {
    if (rng_f(r) < 0.3f) {
        int fl = 9 + rng_int(r, 4);
        building(ix0 + 3, iz0 + 3, ix1 - 3, iz1 - 3, fl, wall_col(r), 2, 0, r);
        bench(ix0 + 8, iz1 - 1.3f, 1);
        tree(ix0 + 1.5f, iz0 + 1.5f, r);
        tree(ix1 - 1.5f, iz1 - 1.5f, r);
    } else {
        int qx, qz;
        float hw = (ix1 - ix0) * 0.5f, hd = (iz1 - iz0) * 0.5f;
        for (qz = 0; qz < 2; qz++)
            for (qx = 0; qx < 2; qx++) {
                float x0 = ix0 + qx * hw + (qx ? 1.8f : 0.0f), x1 = ix0 + (qx + 1) * hw - (qx ? 0.0f : 1.8f);
                float z0 = iz0 + qz * hd + (qz ? 1.8f : 0.0f), z1 = iz0 + (qz + 1) * hd - (qz ? 0.0f : 1.8f);
                building(x0, z0, x1, z1, 3 + rng_int(r, 6), wall_col(r), qz ? 2 : 0, rng_f(r) < 0.3f, r);
            }
        /* steegjes tussen de gebouwen: wat afval */
        dumpster((ix0 + ix1) * 0.5f + 0.0f, iz0 + 4.0f);
    }
}

static void gen_shops(float ix0, float iz0, float ix1, float iz1, Rng *r, int bi, int bj) {
    int s, side;
    float w = (ix1 - ix0) / 3.0f;
    for (side = 0; side < 2; side++)
        for (s = 0; s < 3; s++) {
            float x0 = ix0 + s * w + 0.4f, x1 = ix0 + (s + 1) * w - 0.4f;
            float z0 = side ? iz1 - 13.0f : iz0, z1 = side ? iz1 : iz0 + 13.0f;
            building(x0, z0, x1, z1, 2 + rng_int(r, 2), wall_col(r), side ? 2 : 0, 1, r);
            if (bi == 2 && bj == 2 && side == 0 && s == 1) {
                /* BLOCK ARMS: donkere gevel, rood bord met een geweer-silhouet */
                W->poi[POI_GUNSHOP] = v3((x0 + x1) * 0.5f, 0, iz0 - SIDEWALK * 0.5f - 0.3f);
                mb_box(cur, v3(x0 + 0.5f, CURB + 4.1f, z0 - 0.45f), v3(x1 - 0.5f, CURB + 5.3f, z0 - 0.1f), col_e(170, 20, 24, 230), 0);
                mb_box(cur, v3((x0 + x1) * 0.5f - 1.6f, CURB + 4.55f, z0 - 0.5f), v3((x0 + x1) * 0.5f + 1.4f, CURB + 4.8f, z0 - 0.45f), col_e(240, 240, 240, 255), 0);
                mb_box(cur, v3((x0 + x1) * 0.5f + 0.6f, CURB + 4.3f, z0 - 0.5f), v3((x0 + x1) * 0.5f + 1.0f, CURB + 4.6f, z0 - 0.45f), col_e(240, 240, 240, 255), 0);
                mb_box(cur, v3(x0 + 0.3f, CURB, z0 - 0.06f), v3(x1 - 0.3f, CURB + 3.0f, z0 - 0.02f), col(28, 28, 32), 0);
                mb_quad(cur, v3((x0 + x1) * 0.5f - 1.0f, CURB, z0 - 0.08f), v3((x0 + x1) * 0.5f + 1.0f, CURB, z0 - 0.08f),
                        v3((x0 + x1) * 0.5f + 1.0f, CURB + 2.5f, z0 - 0.08f), v3((x0 + x1) * 0.5f - 1.0f, CURB + 2.5f, z0 - 0.08f), v3(0, 0, -1), col_e(200, 60, 50, 120));
            }
            if (bi == 2 && bj == 3 && side == 0 && s == 1) {
                /* pizzeria: rood-wit, voor de deur */
                W->poi[POI_PIZZA] = v3((x0 + x1) * 0.5f, 0, iz0 - SIDEWALK * 0.5f - 0.3f);
                mb_box(cur, v3(x0 + 1, CURB + 4.2f, z0 - 0.4f), v3(x1 - 1, CURB + 5.2f, z0 - 0.1f), col_e(230, 40, 40, 220), 0);
            }
            if (bi == 4 && bj == 2 && side == 0 && s == 1) {
                /* v4.0 GIVEAWAY MART: groene lichtbak, grote etalageramen, glazen schuifdeur */
                float cx = (x0 + x1) * 0.5f;
                W->poi[POI_MART] = v3(cx, 0, iz0 - SIDEWALK * 0.5f - 0.3f);
                mb_box(cur, v3(x0 + 0.4f, CURB + 3.9f, z0 - 0.5f), v3(x1 - 0.4f, CURB + 5.3f, z0 - 0.1f), col_e(60, 200, 90, 230), 0);
                mb_box(cur, v3(cx - 2.6f, CURB + 4.35f, z0 - 0.55f), v3(cx + 2.6f, CURB + 4.85f, z0 - 0.5f), col_e(250, 250, 250, 255), 0);
                mb_box(cur, v3(x0 + 0.4f, CURB + 2.9f, z0 - 1.6f), v3(x1 - 0.4f, CURB + 3.05f, z0), col(40, 130, 60), 0);
                mb_box(cur, v3(x0 + 0.3f, CURB, z0 - 0.06f), v3(x1 - 0.3f, CURB + 2.8f, z0 - 0.02f), col_e(190, 220, 240, 120), 0);
                mb_quad(cur, v3(cx - 1.0f, CURB, z0 - 0.08f), v3(cx + 1.0f, CURB, z0 - 0.08f),
                        v3(cx + 1.0f, CURB + 2.4f, z0 - 0.08f), v3(cx - 1.0f, CURB + 2.4f, z0 - 0.08f), v3(0, 0, -1), col_e(230, 245, 255, 170));
            }
            if (bi == 1 && bj == 4 && side == 1 && s == 1) {
                W->poi[POI_BIKESHOP] = v3((x0 + x1) * 0.5f, 0, iz1 + SIDEWALK * 0.5f + 0.3f);
            }
            {   /* v3.2: nieuwe winkels met een lichtbak en luifel in hun eigen kleur */
                static const int sh[4][5] = {   /* blok i, blok j, kant, plek, poi */
                    {0, 2, 0, 1, POI_BURGER}, {6, 2, 0, 0, POI_GYM}, {3, 5, 0, 1, POI_DRIP}, {6, 5, 1, 1, POI_BLING}};
                static const int sc[4][3] = {{240, 150, 30}, {40, 200, 220}, {230, 60, 160}, {250, 215, 60}};
                int q;
                for (q = 0; q < 4; q++) {
                    float cx = (x0 + x1) * 0.5f, fz, dz;
                    Col c, cd;
                    if (bi != sh[q][0] || bj != sh[q][1] || side != sh[q][2] || s != sh[q][3]) continue;
                    c = col_e(sc[q][0], sc[q][1], sc[q][2], 230);
                    cd = col((int)(sc[q][0] * 0.6f), (int)(sc[q][1] * 0.6f), (int)(sc[q][2] * 0.6f));
                    fz = side ? z1 : z0; dz = side ? 1.0f : -1.0f;
                    W->poi[sh[q][4]] = v3(cx, 0, fz + dz * (SIDEWALK * 0.5f + 0.3f));
                    mb_box(cur, v3(x0 + 0.6f, CURB + 4.0f, fz + (side ? 0.1f : -0.45f)), v3(x1 - 0.6f, CURB + 5.2f, fz + (side ? 0.45f : -0.1f)), c, 0);
                    mb_box(cur, v3(x0 + 0.4f, CURB + 2.9f, fz + (side ? 0.0f : -1.5f)), v3(x1 - 0.4f, CURB + 3.05f, fz + (side ? 1.5f : 0.0f)), cd, 0);
                    mb_quad(cur, v3(cx - 1.1f, CURB, fz + dz * 0.06f), v3(cx + 1.1f, CURB, fz + dz * 0.06f),
                            v3(cx + 1.1f, CURB + 2.5f, fz + dz * 0.06f), v3(cx - 1.1f, CURB + 2.5f, fz + dz * 0.06f), v3(0, 0, dz),
                            col_e(sc[q][0] / 2 + 100, sc[q][1] / 2 + 100, sc[q][2] / 2 + 100, 140));
                }
            }
        }
    /* binnenplaats */
    mb_flat(cur, ix0, iz0 + 13.0f, ix1, iz1 - 13.0f, CURB + 0.02f, col(120, 118, 112));
    dumpster(ix0 + 4, (iz0 + iz1) * 0.5f);
    tree(ix1 - 5, (iz0 + iz1) * 0.5f, r);
}

static void gen_alley(float ix0, float iz0, float ix1, float iz1, Rng *r) {
    float cz = (iz0 + iz1) * 0.5f;
    float half = (ix1 - ix0) * 0.5f;
    int k;
    mb_flat(cur, ix0, cz - 3.2f, ix1, cz + 3.2f, CURB + 0.02f, col(58, 58, 62));
    building(ix0, iz0, ix0 + half - 0.5f, cz - 3.2f, 3 + rng_int(r, 3), wall_col(r), 0, rng_f(r) < 0.5f, r);
    building(ix0 + half + 0.5f, iz0, ix1, cz - 3.2f, 3 + rng_int(r, 3), wall_col(r), 0, 0, r);
    building(ix0, cz + 3.2f, ix0 + half - 0.5f, iz1, 3 + rng_int(r, 3), wall_col(r), 2, 0, r);
    building(ix0 + half + 0.5f, cz + 3.2f, ix1, iz1, 3 + rng_int(r, 3), wall_col(r), 2, rng_f(r) < 0.5f, r);
    dumpster(ix0 + 7, cz - 2.2f);
    dumpster(ix1 - 9, cz + 2.2f);
    /* kratjes, plassen, graffiti, lampen aan de muur */
    mb_box(cur, v3(ix0 + 15, CURB, cz + 1.8f), v3(ix0 + 16, CURB + 1, cz + 2.8f), col(150, 110, 70), 0);
    mb_box(cur, v3(ix0 + 15.2f, CURB + 1, cz + 2.0f), v3(ix0 + 16, CURB + 1.8f, cz + 2.8f), col(140, 100, 60), 0);
    add_box(ix0 + 15, cz + 1.8f, ix0 + 16, cz + 2.8f, CURB + 1.8f, BOX_PROP);
    mb_flat(cur, ix0 + 20, cz - 1.5f, ix0 + 23, cz + 0.5f, CURB + 0.03f, col_e(40, 50, 70, 30));
    for (k = 0; k < 5; k++) {
        static const int gc[5][3] = {{230, 60, 120}, {60, 200, 230}, {250, 200, 40}, {120, 220, 80}, {240, 90, 40}};
        float gx = ix0 + 5 + k * 7.5f + rng_range(r, 0, 2);
        float zz = (k & 1) ? cz + 3.17f : cz - 3.17f;
        float y0 = CURB + rng_range(r, 0.4f, 0.9f), w = rng_range(r, 1.5f, 3.5f), hh = rng_range(r, 0.7f, 1.5f);
        Col c = col_e(gc[k][0], gc[k][1], gc[k][2], 40);
        V3 n = (k & 1) ? v3(0, 0, -1) : v3(0, 0, 1);
        mb_quad(cur, v3(gx, y0, zz), v3(gx + w, y0, zz), v3(gx + w, y0 + hh, zz), v3(gx, y0 + hh, zz), n, c);
        mb_quad(cur, v3(gx + 0.3f, y0 + 0.2f, zz + n.z * 0.01f), v3(gx + w * 0.6f, y0 + 0.2f, zz + n.z * 0.01f),
                v3(gx + w * 0.6f, y0 + hh * 0.5f, zz + n.z * 0.01f), v3(gx + 0.3f, y0 + hh * 0.5f, zz + n.z * 0.01f), n, col(250, 250, 250));
    }
    for (k = 0; k < 3; k++) {
        float lx = ix0 + 6 + k * 15;
        mb_box(cur, v3(lx - 0.25f, CURB + 3.4f, cz - 3.2f), v3(lx + 0.25f, CURB + 3.7f, cz - 2.8f), col_e(255, 200, 120, 255), 0);
    }
}

static void gen_houses(float ix0, float iz0, float ix1, float iz1, Rng *r, int bi, int bj) {
    int a, b;
    float lw = (ix1 - ix0) / 3.0f, ld = (iz1 - iz0) / 2.0f;
    Col grass = col(92, 150, 72);
    mb_flat(cur, ix0, iz0, ix1, iz1, CURB + 0.02f, grass);
    for (b = 0; b < 2; b++)
        for (a = 0; a < 3; a++) {
            float lx0 = ix0 + a * lw, lx1 = lx0 + lw, lz0 = iz0 + b * ld, lz1 = lz0 + ld;
            float hx0 = lx0 + 2.4f, hx1 = lx1 - 2.4f;
            float hz0 = b ? lz1 - 11.0f : lz0 + 3.5f, hz1 = b ? lz1 - 3.5f : lz0 + 11.0f;
            Col fc = (a + b) & 1 ? col(235, 235, 230) : col(150, 110, 75);
            house(hx0, hz0, hx1, hz1, b ? 2 : 0, r);
            /* tuinpad en hek */
            if (b == 0) {
                mb_flat(cur, (hx0 + hx1) * 0.5f - 0.6f, lz0, (hx0 + hx1) * 0.5f + 0.6f, hz0, CURB + 0.04f, col(180, 170, 150));
                fence_line(lx0 + 0.2f, lz1 - 0.2f, lx1 - 0.2f, lz1 - 0.2f, fc);
            } else {
                mb_flat(cur, (hx0 + hx1) * 0.5f - 0.6f, hz1, (hx0 + hx1) * 0.5f + 0.6f, lz1, CURB + 0.04f, col(180, 170, 150));
            }
            fence_line(lx0 + 0.2f, b ? lz0 + 0.2f : lz0 + 2.0f, lx0 + 0.2f, b ? lz1 - 2.0f : lz1 - 0.2f, fc);
            if (b == 0 ? (lz1 - hz1 > 4) : (hz0 - lz0 > 4)) tree((lx0 + lx1) * 0.5f + 2.5f, b ? lz0 + 3.0f : lz1 - 3.0f, r);
            if (W->nhouses < 16 && ((a + b + bi + bj) % 2 == 0)) {
                W->houses[W->nhouses++] = v3((hx0 + hx1) * 0.5f, 0, b ? iz1 + 1.0f : iz0 - 1.0f);
            }
            if (rng_f(r) < 0.18f) {
                add_park(lx0 + 1.4f, b ? lz1 - 2.0f : lz0 + 2.5f, 0.3f, VEH_BIKE, 1 + rng_int(r, 3));
            }
        }
}

static void gen_park(float ix0, float iz0, float ix1, float iz1, Rng *r, int stage) {
    float cx = (ix0 + ix1) * 0.5f, cz = (iz0 + iz1) * 0.5f;
    int k;
    mb_flat(cur, ix0, iz0, ix1, iz1, CURB + 0.02f, col(70, 145, 62));
    mb_flat(cur, ix0, cz - 1.5f, ix1, cz + 1.5f, CURB + 0.04f, col(190, 175, 140));
    mb_flat(cur, cx - 1.5f, iz0, cx + 1.5f, cz - 1.5f, CURB + 0.04f, col(190, 175, 140));
    mb_flat(cur, cx - 1.5f, cz + 1.5f, cx + 1.5f, iz1, CURB + 0.04f, col(190, 175, 140));
    /* vijver */
    mb_disc(cur, v3(cx - 10, CURB + 0.05f, cz + 10), 6.0f, 14, col_e(60, 110, 170, 40));
    mb_disc(cur, v3(cx - 10, CURB + 0.045f, cz + 10), 6.6f, 14, col(150, 140, 120));
    for (k = 0; k < 4; k++) bench(cx + (k < 2 ? -6.0f : 6.0f), cz + ((k & 1) ? 2.3f : -2.3f), 1);
    if (stage) {
        mb_box(cur, v3(cx + 4, CURB, iz0 + 3), v3(cx + 16, CURB + 1.0f, iz0 + 10), col(140, 100, 62), BOX_NOBOTTOM);
        mb_box(cur, v3(cx + 4, CURB + 1.0f, iz0 + 3), v3(cx + 16, CURB + 5.5f, iz0 + 3.6f), col(40, 40, 46), BOX_NOBOTTOM);
        mb_box(cur, v3(cx + 5, CURB + 3.4f, iz0 + 3.6f), v3(cx + 15, CURB + 5.0f, iz0 + 3.75f), col_e(210, 30, 40, 200), 0);
        mb_box(cur, v3(cx + 7, CURB + 3.9f, iz0 + 3.75f), v3(cx + 13, CURB + 4.5f, iz0 + 3.8f), col_e(250, 250, 245, 220), 0);
        add_box(cx + 4, iz0 + 3, cx + 16, iz0 + 10, CURB + 1.0f, BOX_PROP);
        W->poi[POI_STAGE] = v3(cx + 10, 0, iz0 + 12.5f);
    }
    for (k = 0; k < 16; k++) {
        float x = rng_range(r, ix0 + 2, ix1 - 2), z = rng_range(r, iz0 + 2, iz1 - 2);
        if (fabsf(x - cx) < 3.5f || fabsf(z - cz) < 3.5f) continue;
        if ((x - (cx - 10)) * (x - (cx - 10)) + (z - (cz + 10)) * (z - (cz + 10)) < 64) continue;
        if (stage && x > cx + 2 && z < iz0 + 14) continue;
        tree(x, z, r);
    }
    add_park(cx + 2.4f, cz + 6.0f, 0.0f, VEH_BIKE, 1 + rng_int(r, 3));
}

static void gen_plaza(float ix0, float iz0, float ix1, float iz1, Rng *r) {
    float cx = (ix0 + ix1) * 0.5f, cz = (iz0 + iz1) * 0.5f;
    int k;
    mb_flat(cur, ix0, iz0, ix1, iz1, CURB + 0.02f, col(186, 176, 160));
    mb_flat(cur, cx - 9, cz - 9, cx + 9, cz + 9, CURB + 0.03f, col(160, 146, 128));
    /* monument: de gouden Giveaway Bike op een sokkel */
    mb_box(cur, v3(cx - 2.2f, CURB, cz - 3.4f), v3(cx + 2.2f, CURB + 1.6f, cz + 3.4f), col(150, 150, 156), BOX_NOBOTTOM);
    mb_box(cur, v3(cx - 2.5f, CURB, cz - 3.7f), v3(cx + 2.5f, CURB + 0.4f, cz + 3.7f), col(120, 120, 126), BOX_NOBOTTOM);
    {
        M4 xf = m4_mul(m4_translate(cx, CURB + 1.6f, cz), m4_scale(2.4f, 2.4f, 2.4f));
        int start = cur->n, i;
        mb_set_xf(cur, xf);
        model_bike(cur, 2);
        mb_clear_xf(cur);
        for (i = start; i < cur->n; i++) {           /* goud maken */
            Vtx *v = &cur->v[i];
            int l = (v->r + v->g + v->b) / 3;
            v->r = (uint8_t)clampf(l * 1.1f + 60, 0, 255); v->g = (uint8_t)clampf(l * 0.9f + 40, 0, 255); v->b = (uint8_t)clampf(l * 0.3f, 0, 255);
            v->a = 40;
        }
    }
    add_box(cx - 2.5f, cz - 3.7f, cx + 2.5f, cz + 3.7f, CURB + 4.0f, BOX_PROP);
    for (k = 0; k < 4; k++) {
        float px = cx + ((k & 1) ? 13.0f : -13.0f), pz = cz + ((k & 2) ? 13.0f : -13.0f);
        mb_box(cur, v3(px - 1.4f, CURB, pz - 1.4f), v3(px + 1.4f, CURB + 0.6f, pz + 1.4f), col(130, 125, 118), BOX_NOBOTTOM);
        add_box(px - 1.4f, pz - 1.4f, px + 1.4f, pz + 1.4f, CURB + 0.6f, BOX_PROP);
        tree(px, pz, r);
        bench(px + ((k & 1) ? -3.5f : 3.5f), pz, 0);
    }
    W->poi[POI_PLAZA] = v3(cx, 0, cz + 8.5f);
    W->poi[POI_SPAWN] = v3(cx - 4, 0, cz + 15);
    W->spawn_heading = PI_F;
    W->poi[POI_GIVEAWAY_BIKE] = v3(cx + 3.0f, 0, cz + 15.0f);
    add_park(cx + 3.0f, cz + 15.0f, PI_F * 0.5f, VEH_BIKE, 0);
}

static void display_car(int model, float x, float z, float h, Col paint) {
    int start = cur->n, i;
    mb_set_xf(cur, m4_trs_y(x, CURB + 0.3f, z, h));
    model_car_body(cur, model);
    for (i = start; i < cur->n; i++) {
        Vtx *v = &cur->v[i];
        v->r = (uint8_t)(v->r * paint.r / 255); v->g = (uint8_t)(v->g * paint.g / 255); v->b = (uint8_t)(v->b * paint.b / 255);
    }
    model_car_detail(cur, model);
    mb_clear_xf(cur);
}

/* GIVEAWAY MOTORS: toonzaal met etalage-auto's, eigen auto's staan aan de zuidkant */
static void gen_dealer(float ix0, float iz0, float ix1, float iz1, Rng *r) {
    float cx = (ix0 + ix1) * 0.5f, k;
    int i;
    mb_flat(cur, ix0, iz0, ix1, iz1, CURB + 0.02f, col(74, 74, 80));
    building(ix0 + 2, iz0 + 1, ix1 - 2, iz0 + 15, 2, col(220, 222, 226), 2, 1, r);
    mb_box(cur, v3(cx - 9, CURB + 6.6f, iz0 + 14.6f), v3(cx + 9, CURB + 8.2f, iz0 + 15.2f), col_e(250, 140, 30, 230), 0);
    mb_box(cur, v3(cx - 7, CURB + 7.0f, iz0 + 15.2f), v3(cx + 7, CURB + 7.8f, iz0 + 15.3f), col_e(255, 255, 255, 255), 0);
    for (i = 0; i < 3; i++) {
        float x = ix0 + 9 + i * 13.0f, z = iz0 + 22.0f;
        static const int models[3] = {1, 2, 3};
        mb_cyl(cur, v3(x, CURB, z), 3.0f, 0.3f, 14, 1, col(200, 200, 205), col(230, 230, 235), 1);
        display_car(models[i], x, z, 0.6f + i * 0.3f, paint_color(model_default_paint(models[i])));
        add_box(x - 3.0f, z - 3.0f, x + 3.0f, z + 3.0f, CURB + 1.6f, BOX_PROP);
    }
    for (k = ix0 + 2.0f; k < ix1 - 2.0f; k += 9.5f) mb_flat(cur, k - 0.07f, iz1 - 8.0f, k + 0.07f, iz1 - 0.5f, CURB + 0.04f, col(230, 230, 225));
    for (i = 0; i < 4; i++) W->dealerSpot[i] = v3(ix0 + 6.75f + i * 9.5f, 0, iz1 - 4.2f);
    W->dealerSpotH = PI_F;
    W->poi[POI_DEALER] = v3(cx, 0, iz0 + 17.0f);
    lamp(ix0 + 1.5f, iz1 - 10, 1, 0);
    lamp(ix1 - 1.5f, iz1 - 10, -1, 0);
}

static void gen_parking(float ix0, float iz0, float ix1, float iz1, Rng *r, int bi, int bj) {
    float x;
    int n = 0;
    Col line = col(230, 230, 225);
    mb_flat(cur, ix0, iz0, ix1, iz1, CURB + 0.02f, col(68, 68, 72));
    for (x = ix0 + 1.0f; x < ix1 - 1.0f; x += 3.0f) {
        mb_flat(cur, x - 0.07f, iz0 + 0.5f, x + 0.07f, iz0 + 6.0f, CURB + 0.04f, line);
        mb_flat(cur, x - 0.07f, iz1 - 6.0f, x + 0.07f, iz1 - 0.5f, CURB + 0.04f, line);
        if (x + 3.0f < ix1 - 1.0f) {
            float sx = x + 1.5f;
            int top = rng_f(r) < 0.28f, bot = rng_f(r) < 0.28f;
            if (bi == 6 && bj == 3 && n == 0) {
                W->poi[POI_REDCAR] = v3(sx, 0, iz0 + 3.2f);
                add_park(sx, iz0 + 3.2f, 0.0f, VEH_CAR, 0);
                n++;
                continue;
            }
            if (top) { add_park(sx, iz0 + 3.2f, 0.0f, VEH_CAR, 1 + rng_int(r, 5)); n++; }
            if (bot) { add_park(sx, iz1 - 3.2f, PI_F, VEH_CAR, 1 + rng_int(r, 5)); n++; }
        }
    }
    lamp((ix0 + ix1) * 0.5f - 8, (iz0 + iz1) * 0.5f, 1, 0);
    lamp((ix0 + ix1) * 0.5f + 8, (iz0 + iz1) * 0.5f, -1, 0);
    /* hokje */
    mb_box(cur, v3(ix1 - 4, CURB, (iz0 + iz1) * 0.5f - 1.2f), v3(ix1 - 1.5f, CURB + 2.6f, (iz0 + iz1) * 0.5f + 1.2f), col(200, 190, 120), BOX_NOBOTTOM);
    add_box(ix1 - 4, (iz0 + iz1) * 0.5f - 1.2f, ix1 - 1.5f, (iz0 + iz1) * 0.5f + 1.2f, CURB + 2.6f, BOX_BUILDING);
}

static void gen_garage(float ix0, float iz0, float ix1, float iz1, Rng *r) {
    float cx = (ix0 + ix1) * 0.5f, cz = (iz0 + iz1) * 0.5f;
    int k;
    (void)r;
    mb_flat(cur, ix0, iz0, ix1, iz1, CURB + 0.02f, col(150, 150, 148));
    {
        float x0 = ix0 + 1, x1 = cx + 6, z0 = iz0 + 1, z1 = cz - 3;
        float h = CURB + 6.0f;
        Col wall = col(200, 200, 205);
        cur->ao = 0.3f;
        mb_box2(cur, v3(x0, CURB, z0), v3(x1, h, z1), col(90, 90, 95), wall, BOX_NOBOTTOM);
        cur->ao = 0;
        for (k = 0; k < 3; k++) {
            float dx0 = x0 + 2 + k * 8.5f;
            mb_quad(cur, v3(dx0, CURB, z1 + 0.04f), v3(dx0 + 6.5f, CURB, z1 + 0.04f), v3(dx0 + 6.5f, CURB + 4.2f, z1 + 0.04f), v3(dx0, CURB + 4.2f, z1 + 0.04f), v3(0, 0, 1), col(35, 35, 40));
        }
        mb_box(cur, v3(x0 + 4, h - 1.4f, z1), v3(x1 - 4, h - 0.3f, z1 + 0.3f), col_e(240, 120, 30, 220), 0);
        add_box(x0, z0, x1, z1, h, BOX_BUILDING);
        W->poi[POI_GARAGE] = v3(x0 + 11, 0, z1 + 4.0f);
        /* BLOCK CUSTOMS: de linker garagedeur, blauw bord */
        W->poi[POI_CUSTOMS] = v3(x0 + 5.25f, 0, z1 + 3.5f);
        mb_box(cur, v3(x0 + 2.2f, CURB + 4.4f, z1 + 0.05f), v3(x0 + 8.3f, CURB + 5.3f, z1 + 0.3f), col_e(40, 120, 255, 230), 0);
        mb_box(cur, v3(x0 + 3.0f, CURB + 4.7f, z1 + 0.3f), v3(x0 + 7.5f, CURB + 5.0f, z1 + 0.35f), col_e(255, 255, 255, 255), 0);
    }
    /* tankstation */
    {
        float x0 = cx + 2, x1 = ix1 - 2, z0 = cz + 3, z1 = iz1 - 5;
        mb_box(cur, v3(x0, CURB + 4.2f, z0), v3(x1, CURB + 4.8f, z1), col_e(230, 230, 235, 60), 0);
        mb_box(cur, v3(x0, CURB + 4.5f, z0 - 0.05f), v3(x1, CURB + 4.8f, z0), col_e(220, 40, 40, 160), 0);
        for (k = 0; k < 4; k++) {
            float px = (k & 1) ? x1 - 1 : x0 + 1, pz = (k & 2) ? z1 - 1 : z0 + 1;
            mb_box(cur, v3(px - 0.2f, CURB, pz - 0.2f), v3(px + 0.2f, CURB + 4.2f, pz + 0.2f), col(200, 200, 200), BOX_NOBOTTOM);
            add_box(px - 0.2f, pz - 0.2f, px + 0.2f, pz + 0.2f, CURB + 4.2f, BOX_PROP);
        }
        for (k = 0; k < 2; k++) {
            float px = x0 + (x1 - x0) * (0.33f + 0.34f * k), pz = (z0 + z1) * 0.5f;
            mb_box(cur, v3(px - 0.4f, CURB, pz - 0.6f), v3(px + 0.4f, CURB + 1.6f, pz + 0.6f), col(220, 50, 50), BOX_NOBOTTOM);
            mb_box(cur, v3(px - 0.42f, CURB + 1.0f, pz - 0.5f), v3(px + 0.42f, CURB + 1.4f, pz + 0.5f), col_e(250, 250, 240, 120), 0);
            add_box(px - 0.4f, pz - 0.6f, px + 0.4f, pz + 0.6f, CURB + 1.6f, BOX_PROP);
        }
    }
    add_park(ix0 + 6, cz + 6, PI_F * 0.5f, VEH_CAR, 2);
}

/* De Giveaway Bank: klassieke gevel met zuilen naar het plein, kluisdeur aan de achterkant */
static void gen_bank(float ix0, float iz0, float ix1, float iz1, Rng *r) {
    float bx0 = ix0 + 7.0f, bx1 = ix1 - 2.0f, bz0 = iz0 + 3.0f, bz1 = iz1 - 12.0f;
    float h = CURB + 11.0f, cz = (bz0 + bz1) * 0.5f;
    Col stone = col(212, 204, 186), dark = col(150, 144, 130);
    int k;
    (void)r;
    mb_flat(cur, ix0, iz0, ix1, iz1, CURB + 0.02f, col(178, 172, 160));
    cur->ao = 0.3f;
    mb_box2(cur, v3(bx0, CURB, bz0), v3(bx1, h, bz1), col(120, 116, 110), stone, BOX_NOBOTTOM);
    cur->ao = 0;
    mb_box(cur, v3(bx0 - 0.3f, h, bz0 - 0.3f), v3(bx1 + 0.3f, h + 0.8f, bz1 + 0.3f), dark, BOX_NOBOTTOM);
    /* portiek met trappen, zuilen en fronton */
    for (k = 0; k < 3; k++) mb_box(cur, v3(bx0 - 6.0f + k * 0.6f, CURB, cz - 9.0f), v3(bx0, CURB + 0.25f * (k + 1), cz + 9.0f), col(190, 184, 170), BOX_NOBOTTOM);
    for (k = 0; k < 6; k++) {
        float zc = cz - 7.5f + k * 3.0f;
        mb_cyl(cur, v3(bx0 - 3.2f, CURB + 0.75f, zc), 0.55f, 7.6f, 10, 1, stone, stone, 1);
        add_box(bx0 - 3.8f, zc - 0.6f, bx0 - 2.6f, zc + 0.6f, h, BOX_PROP);
    }
    mb_box(cur, v3(bx0 - 4.2f, CURB + 8.35f, cz - 9.0f), v3(bx0, CURB + 9.2f, cz + 9.0f), dark, 0);
    mb_roof(cur, bx0 - 4.2f, cz - 9.0f, bx0, cz + 9.0f, CURB + 9.2f, 2.6f, 0, stone, stone);
    mb_box(cur, v3(bx0 - 4.3f, CURB + 9.6f, cz - 2.8f), v3(bx0 - 4.2f, CURB + 10.6f, cz + 2.8f), col_e(255, 205, 80, 255), 0);   /* BANK-bord */
    mb_quad(cur, v3(bx0 - 0.04f, CURB, cz - 1.6f), v3(bx0 - 0.04f, CURB, cz + 1.6f), v3(bx0 - 0.04f, CURB + 4.0f, cz + 1.6f), v3(bx0 - 0.04f, CURB + 4.0f, cz - 1.6f), v3(-1, 0, 0), col(70, 50, 30));
    for (k = 0; k < 4; k++) {
        float zc = bz0 + 4.0f + k * ((bz1 - bz0 - 8.0f) / 3.0f);
        if (fabsf(zc - cz) < 3.0f) continue;
        mb_quad(cur, v3(bx0 - 0.04f, CURB + 5.0f, zc - 1.0f), v3(bx0 - 0.04f, CURB + 5.0f, zc + 1.0f), v3(bx0 - 0.04f, CURB + 7.5f, zc + 1.0f), v3(bx0 - 0.04f, CURB + 7.5f, zc - 1.0f), v3(-1, 0, 0), col_e(255, 220, 150, 200));
    }
    add_box(bx0, bz0, bx1, bz1, h + 0.8f, BOX_BUILDING);
    /* kluisdeur aan de zuidkant (binnenplaats) */
    {
        float vx = (bx0 + bx1) * 0.5f;
        mb_cyl(cur, v3(vx, CURB + 1.6f, bz1 + 0.08f), 1.5f, 0.16f, 16, 2, col(150, 152, 160), col(120, 122, 130), 1);
        mb_cyl(cur, v3(vx, CURB + 1.6f, bz1 + 0.2f), 0.5f, 0.12f, 8, 2, col(200, 170, 60), col(200, 170, 60), 1);
        mb_box(cur, v3(vx - 0.9f, CURB + 1.55f, bz1 + 0.24f), v3(vx + 0.9f, CURB + 1.65f, bz1 + 0.3f), col(90, 90, 96), 0);
        mb_box(cur, v3(vx - 0.05f, CURB + 0.7f, bz1 + 0.24f), v3(vx + 0.05f, CURB + 2.5f, bz1 + 0.3f), col(90, 90, 96), 0);
        mb_box(cur, v3(vx + 2.0f, CURB + 2.6f, bz1), v3(vx + 2.4f, CURB + 2.9f, bz1 + 0.25f), col_e(255, 40, 40, 255), 0);  /* alarmlamp */
        W->poi[POI_VAULT] = v3(vx, 0, bz1 + 2.2f);
    }
    W->poi[POI_BANK] = v3(ix0 + 1.6f, 0, cz);
    dumpster(ix1 - 4.0f, iz1 - 3.0f);
    add_park(ix0 + 8.0f, iz1 - 5.0f, PI_F * 0.5f, VEH_CAR, 5);
}

static void gen_block(int i, int j) {
    char t = W->type[j][i];
    float bx0 = block_x0(i), bz0 = block_x0(j), bx1 = bx0 + BLOCKW, bz1 = bz0 + BLOCKW;
    float ix0 = bx0 + SIDEWALK, iz0 = bz0 + SIDEWALK, ix1 = bx1 - SIDEWALK, iz1 = bz1 - SIDEWALK;
    Rng r;
    int k;
    r.s = 1234567u + (uint32_t)i * 7919u + (uint32_t)j * 104729u;
    at((bx0 + bx1) * 0.5f, (bz0 + bz1) * 0.5f);
    /* stoeptegel-plaat met stoeprand */
    mb_box2(cur, v3(bx0, 0, bz0), v3(bx1, CURB, bz1), col(158, 156, 150), col(118, 116, 110), BOX_NOBOTTOM);
    /* lantaarns langs de stoep */
    for (k = 0; k < 2; k++) {
        float t2 = 11.0f + k * 22.0f;
        lamp(bx0 + t2, bz0 + 0.5f, 0, -1);
        lamp(bx0 + t2, bz1 - 0.5f, 0, 1);
        lamp(bx0 + 0.5f, bz0 + t2, -1, 0);
        lamp(bx1 - 0.5f, bz0 + t2, 1, 0);
    }
    switch (t) {
    case 'D':
        if (i == 4 && j == 3) gen_bank(ix0, iz0, ix1, iz1, &r);
        else gen_downtown(ix0, iz0, ix1, iz1, &r);
        break;
    case 'S': gen_shops(ix0, iz0, ix1, iz1, &r, i, j); break;
    case 'A': gen_alley(ix0, iz0, ix1, iz1, &r); break;
    case 'H': gen_houses(ix0, iz0, ix1, iz1, &r, i, j); break;
    case 'P': gen_park(ix0, iz0, ix1, iz1, &r, i == 3 && j == 4); break;
    case 'C': gen_plaza(ix0, iz0, ix1, iz1, &r); break;
    case 'L':
        if (i == 1 && j == 2) gen_dealer(ix0, iz0, ix1, iz1, &r);
        else gen_parking(ix0, iz0, ix1, iz1, &r, i, j);
        break;
    case 'G': gen_garage(ix0, iz0, ix1, iz1, &r); break;
    default: break;
    }
}

static void gen_roads(void) {
    int k, s;
    Col asph = col(56, 56, 62), dash = col_e(230, 200, 60, 30), white = col(220, 220, 215);
    for (k = 0; k <= NB; k++) {
        float r0 = k * PITCH, r1 = r0 + ROADW, rc = r0 + ROADW * 0.5f;
        for (s = 0; s <= NB; s++) {
            float q0 = s * PITCH;
            /* kruispunt */
            at(q0 + 6, r0 + 6);
            mb_flat(cur, q0, r0, q0 + ROADW, r1, 0.0f, asph);
            if (s == NB) continue;
            {
                float a0 = q0 + ROADW, a1 = (s + 1) * PITCH, d;
                /* weg langs x */
                at((a0 + a1) * 0.5f, rc);
                mb_flat(cur, a0, r0, a1, r1, 0.0f, asph);
                for (d = a0 + 4.0f; d < a1 - 4.0f; d += 6.0f) mb_flat(cur, d, rc - 0.09f, d + 3.0f, rc + 0.09f, 0.03f, dash);
                for (d = r0 + 0.9f; d < r1 - 0.8f; d += 1.25f) {
                    mb_flat(cur, a0 + 0.6f, d, a0 + 3.0f, d + 0.6f, 0.03f, white);
                    mb_flat(cur, a1 - 3.0f, d, a1 - 0.6f, d + 0.6f, 0.03f, white);
                }
                /* weg langs z */
                at(rc, (a0 + a1) * 0.5f);
                mb_flat(cur, r0, a0, r1, a1, 0.0f, asph);
                for (d = a0 + 4.0f; d < a1 - 4.0f; d += 6.0f) mb_flat(cur, rc - 0.09f, d, rc + 0.09f, d + 3.0f, 0.03f, dash);
                for (d = r0 + 0.9f; d < r1 - 0.8f; d += 1.25f) {
                    mb_flat(cur, d, a0 + 0.6f, d + 0.6f, a0 + 3.0f, 0.03f, white);
                    mb_flat(cur, d, a1 - 3.0f, d + 0.6f, a1 - 0.6f, 0.03f, white);
                }
            }
        }
    }
}

static void gen_outer(void) {
    Rng r;
    int k;
    float lo = -30.0f, hi = MAPW + 30.0f;
    r.s = 424242u;
    /* bomenrij rondom */
    for (k = 0; k < 120; k++) {
        float t = rng_f(&r) * 4.0f, d = rng_range(&r, 4.0f, 26.0f);
        float x, z;
        int side = (int)t;
        float u = (t - side) * (MAPW + 20) - 10;
        if (side == 0) { x = u; z = -d; }
        else if (side == 1) { x = MAPW + d; z = u; }
        else if (side == 2) { x = u; z = MAPW + d; }
        else { x = -d; z = u; }
        if (x > 401.0f && x < 437.0f && z > 172.0f && z < 226.0f) continue;   /* perceel van het eigen huis */
        if (x > 401.0f && x < 437.0f && z > 116.0f && z < 146.0f) continue;   /* BLOCK SPRAY */
        at(x, z);
        tree(x, z, &r);
    }
    /* hek */
    {
        Col fc = col(120, 90, 60);
        float p;
        for (p = lo; p < hi; p += 10.0f) {
            float q = fminf(p + 10.0f, hi);
            at(p + 5, lo);
            mb_box(cur, v3(p, 0, lo - 0.1f), v3(q, 1.6f, lo + 0.1f), fc, BOX_NOBOTTOM);
            at(p + 5, hi);
            mb_box(cur, v3(p, 0, hi - 0.1f), v3(q, 1.6f, hi + 0.1f), fc, BOX_NOBOTTOM);
            at(lo, p + 5);
            mb_box(cur, v3(lo - 0.1f, 0, p), v3(lo + 0.1f, 1.6f, q), fc, BOX_NOBOTTOM);
            at(hi, p + 5);
            mb_box(cur, v3(hi - 0.1f, 0, p), v3(hi + 0.1f, 1.6f, q), fc, BOX_NOBOTTOM);
        }
        add_box(lo - 2, lo - 2, hi + 2, lo, 3.0f, BOX_WALL);
        add_box(lo - 2, hi, hi + 2, hi + 2, 3.0f, BOX_WALL);
        add_box(lo - 2, lo, lo, hi, 3.0f, BOX_WALL);
        add_box(hi, lo, hi + 2, hi, 3.0f, BOX_WALL);
    }
    /* BLOCK SPRAY: overkapte spuitstraat aan de oostrand (inrijden vanaf de weg) */
    {
        float bx0 = 405.0f, bx1 = 419.0f, bz0 = 124.0f, bz1 = 138.0f;
        Col blue = col(40, 60, 140), grey = col(170, 170, 176);
        int q;
        at(412, 131);
        mb_flat(cur, 403.5f, bz0 + 2.0f, bx1, bz1 - 2.0f, 0.02f, col(80, 80, 86));
        mb_box(cur, v3(bx0, 3.4f, bz0), v3(bx1, 3.8f, bz1), blue, 0);
        mb_box(cur, v3(bx1 - 0.4f, 0, bz0), v3(bx1, 3.4f, bz1), grey, BOX_NOBOTTOM);
        mb_box(cur, v3(bx0 + 3.0f, 0, bz0), v3(bx1, 3.4f, bz0 + 0.3f), grey, BOX_NOBOTTOM);
        mb_box(cur, v3(bx0 + 3.0f, 0, bz1 - 0.3f), v3(bx1, 3.4f, bz1), grey, BOX_NOBOTTOM);
        add_box(bx1 - 0.4f, bz0, bx1, bz1, 3.4f, BOX_BUILDING);
        add_box(bx0 + 3.0f, bz0, bx1, bz0 + 0.3f, 3.4f, BOX_BUILDING);
        add_box(bx0 + 3.0f, bz1 - 0.3f, bx1, bz1, 3.4f, BOX_BUILDING);
        for (q = 0; q < 2; q++) {
            float pz = q ? bz1 - 0.2f : bz0 + 0.2f;
            mb_box(cur, v3(bx0, 0, pz - 0.2f), v3(bx0 + 0.4f, 3.4f, pz + 0.2f), grey, BOX_NOBOTTOM);
            add_box(bx0, pz - 0.2f, bx0 + 0.4f, pz + 0.2f, 3.4f, BOX_PROP);
        }
        mb_box(cur, v3(bx0 - 0.1f, 3.8f, bz0 + 2.0f), v3(bx0 + 0.3f, 4.8f, bz1 - 2.0f), col_e(250, 60, 140, 230), 0);
        for (q = 0; q < 5; q++)   /* spuitkoppen aan het plafond */
            mb_box(cur, v3(bx0 + 4.0f + q * 2.6f, 3.0f, bz0 + 6.0f), v3(bx0 + 4.4f + q * 2.6f, 3.4f, bz1 - 6.0f), col_e(120, 200, 255, 80), 0);
        W->poi[POI_SPRAY] = v3(412.0f, 0, 131.0f);
    }
    /* grondplaat + heuvels aan de horizon (altijd getekend) */
    {
        MeshB *g = &W->ground;
        float big = 900.0f, c = MAPW * 0.5f;
        mb_flat(g, c - big, c - big, c + big, c + big, -0.02f, col(78, 120, 64));
        for (k = 0; k < 26; k++) {
            float a = k * (2 * PI_F / 26) + rng_range(&r, -0.1f, 0.1f);
            float d = rng_range(&r, 330.0f, 420.0f);
            float hx = c + cosf(a) * d, hz = c + sinf(a) * d;
            mb_cone(g, v3(hx, -1, hz), rng_range(&r, 50, 90), rng_range(&r, 25, 60), 6, col(56 + rng_int(&r, 20), 92 + rng_int(&r, 20), 60));
        }
    }
}

/* ------------------------------------------------------------------ raster + minimap */

static void build_grid(void) { world_build_grid(W, -BORDER - 10.0f, MAPW + 2 * BORDER + 20.0f); }

void world_build_grid(World *w, float org, float size) {
    int i, cx, cz, total = 0;
    int *counts;
    w->gcell = 8.0f;
    w->gorg = org;
    w->gn = (int)ceilf(size / w->gcell);
    counts = (int *)calloc((size_t)w->gn * w->gn + 1, sizeof(int));
    w->gstart = (int *)calloc((size_t)w->gn * w->gn + 1, sizeof(int));
    for (i = 0; i < w->nboxes; i++) {
        Box *b = &w->boxes[i];
        int ax = (int)floorf((b->x0 - w->gorg) / w->gcell), bx = (int)floorf((b->x1 - w->gorg) / w->gcell);
        int az = (int)floorf((b->z0 - w->gorg) / w->gcell), bz = (int)floorf((b->z1 - w->gorg) / w->gcell);
        if (ax < 0) ax = 0; if (az < 0) az = 0; if (bx >= w->gn) bx = w->gn - 1; if (bz >= w->gn) bz = w->gn - 1;
        for (cz = az; cz <= bz; cz++) for (cx = ax; cx <= bx; cx++) { counts[cz * w->gn + cx]++; total++; }
    }
    w->gitems = (int *)malloc((size_t)(total + 1) * sizeof(int));
    {
        int acc = 0;
        for (i = 0; i < w->gn * w->gn; i++) { w->gstart[i] = acc; acc += counts[i]; counts[i] = 0; }
        w->gstart[w->gn * w->gn] = acc;
    }
    for (i = 0; i < w->nboxes; i++) {
        Box *b = &w->boxes[i];
        int ax = (int)floorf((b->x0 - w->gorg) / w->gcell), bx = (int)floorf((b->x1 - w->gorg) / w->gcell);
        int az = (int)floorf((b->z0 - w->gorg) / w->gcell), bz = (int)floorf((b->z1 - w->gorg) / w->gcell);
        if (ax < 0) ax = 0; if (az < 0) az = 0; if (bx >= w->gn) bx = w->gn - 1; if (bz >= w->gn) bz = w->gn - 1;
        for (cz = az; cz <= bz; cz++) for (cx = ax; cx <= bx; cx++) {
            int c = cz * w->gn + cx;
            w->gitems[w->gstart[c] + counts[c]++] = i;
        }
    }
    free(counts);
    w->stamp = (int *)calloc((size_t)w->nboxes + 1, sizeof(int));
    w->curstamp = 0;
    if (w->nsegs > 0) {
        int *cnt = (int *)calloc((size_t)w->gn * w->gn + 1, sizeof(int));
        int tot = 0, pass;
        w->sstart = (int *)calloc((size_t)w->gn * w->gn + 1, sizeof(int));
        for (pass = 0; pass < 2; pass++) {
            for (i = 0; i < w->nsegs; i++) {
                WSeg *s = &w->segs[i];
                int ax = (int)floorf((fminf(s->x0, s->x1) - w->gorg) / w->gcell), bx = (int)floorf((fmaxf(s->x0, s->x1) - w->gorg) / w->gcell);
                int az = (int)floorf((fminf(s->z0, s->z1) - w->gorg) / w->gcell), bz = (int)floorf((fmaxf(s->z0, s->z1) - w->gorg) / w->gcell);
                if (ax < 0) ax = 0; if (az < 0) az = 0; if (bx >= w->gn) bx = w->gn - 1; if (bz >= w->gn) bz = w->gn - 1;
                for (cz = az; cz <= bz; cz++) for (cx = ax; cx <= bx; cx++) {
                    int c = cz * w->gn + cx;
                    if (pass == 0) { cnt[c]++; tot++; }
                    else w->sitems[w->sstart[c] + cnt[c]++] = i;
                }
            }
            if (pass == 0) {
                int acc = 0;
                w->sitems = (int *)malloc((size_t)(tot + 1) * sizeof(int));
                for (i = 0; i < w->gn * w->gn; i++) { w->sstart[i] = acc; acc += cnt[i]; cnt[i] = 0; }
                w->sstart[w->gn * w->gn] = acc;
            }
        }
        free(cnt);
        w->sstamp = (int *)calloc((size_t)w->nsegs + 1, sizeof(int));
    }
}

static void mm_put(World *w, int x, int y, int r, int g, int b) {
    uint8_t *p;
    if (x < 0 || y < 0 || x >= w->mm || y >= w->mm) return;
    p = &w->minimap[(y * w->mm + x) * 4];
    p[0] = (uint8_t)r; p[1] = (uint8_t)g; p[2] = (uint8_t)b; p[3] = 235;
}

static void build_minimap(void) {
    World *w = W;
    int x, y, i;
    float span = MAPW + 2 * BORDER, px = span / 256.0f;
    w->mm = 256;
    w->mmOrg = -BORDER;
    w->mmOrgZ = -BORDER;
    w->mmSpan = span;
    w->minimap = (uint8_t *)malloc(256 * 256 * 4);
    for (y = 0; y < 256; y++)
        for (x = 0; x < 256; x++) {
            float wx = -BORDER + (x + 0.5f) * px, wz = -BORDER + (y + 0.5f) * px;
            int r = 50, g = 86, b = 52;
            if (wx > -30 && wz > -30 && wx < MAPW + 30 && wz < MAPW + 30) { r = 70; g = 112; b = 64; }
            if (wx >= 0 && wz >= 0 && wx <= MAPW && wz <= MAPW) {
                int bi = (int)floorf((wx - ROADW) / PITCH), bj = (int)floorf((wz - ROADW) / PITCH);
                float lx = wx - (bi >= 0 ? block_x0(bi) : 0), lz = wz - (bj >= 0 ? block_x0(bj) : 0);
                if (bi >= 0 && bj >= 0 && bi < NB && bj < NB && lx <= BLOCKW && lz <= BLOCKW) {
                    char t = w->type[bj][bi];
                    if (lx < SIDEWALK || lz < SIDEWALK || lx > BLOCKW - SIDEWALK || lz > BLOCKW - SIDEWALK) { r = 165; g = 162; b = 155; }
                    else switch (t) {
                        case 'H': r = 112; g = 158; b = 92; break;
                        case 'P': r = 76; g = 160; b = 74; break;
                        case 'L': r = 104; g = 104; b = 110; break;
                        case 'C': r = 205; g = 192; b = 170; break;
                        case 'G': r = 150; g = 150; b = 150; break;
                        case 'A': r = 95; g = 92; b = 94; break;
                        default: r = 135; g = 128; b = 122; break;
                    }
                } else { r = 58; g = 58; b = 66; }
            }
            mm_put(w, x, y, r, g, b);
        }
    for (i = 0; i < w->nboxes; i++) {
        Box *bx = &w->boxes[i];
        int ax = (int)((bx->x0 + BORDER) / px), az = (int)((bx->z0 + BORDER) / px);
        int bxx = (int)((bx->x1 + BORDER) / px), bz = (int)((bx->z1 + BORDER) / px);
        int r = 0, g = 0, b = 0, ok = 1;
        if (bx->kind == BOX_BUILDING) { r = 52; g = 50; b = 60; }
        else if (bx->kind == BOX_TREE) { r = 40; g = 104; b = 44; }
        else ok = 0;
        if (!ok) continue;
        for (y = az; y <= bz; y++) for (x = ax; x <= bxx; x++) mm_put(w, x, y, r, g, b);
    }
}

/* ------------------------------------------------------------------ publiek */

void world_init(World *w) {
    int j, i;
    memset(w, 0, sizeof(*w));
    for (j = 0; j < NB; j++) for (i = 0; i < NB; i++) w->type[j][i] = LAYOUT[j][i];
    for (i = 0; i < CHN * CHN; i++) mb_init(&w->chunks[i].mb);
    mb_init(&w->ground);
}

int world_build_steps(void) { return 3 + NB * NB; }

int world_build_step(World *w, int step) {
    W = w;
    if (step == 0) { gen_roads(); return 0; }
    if (step >= 1 && step <= NB * NB) {
        int k = step - 1;
        gen_block(k % NB, k / NB);
        return 0;
    }
    if (step == NB * NB + 1) { gen_outer(); return 0; }
    if (step == NB * NB + 2) {
        int c, i;
        Rng r;
        build_grid();
        build_minimap();
        w->verts_total = w->ground.n;
        for (c = 0; c < CHN * CHN; c++) {
            Chunk *ch = &w->chunks[c];
            ch->x0 = ch->z0 = 1e9f; ch->x1 = ch->z1 = -1e9f; ch->y0 = 0; ch->y1 = 1;
            for (i = 0; i < ch->mb.n; i++) {
                Vtx *v = &ch->mb.v[i];
                if (v->x < ch->x0) ch->x0 = v->x; if (v->x > ch->x1) ch->x1 = v->x;
                if (v->z < ch->z0) ch->z0 = v->z; if (v->z > ch->z1) ch->z1 = v->z;
                if (v->y > ch->y1) ch->y1 = v->y;
            }
            w->verts_total += ch->mb.n;
        }
        /* geld op de stoep */
        r.s = 777u;
        w->ncash = 0;
        while (w->ncash < 18) {
            int bi = rng_int(&r, NB), bj = rng_int(&r, NB), side = rng_int(&r, 4);
            float t = rng_range(&r, 4.0f, BLOCKW - 4.0f), x, z;
            float bx0 = block_x0(bi), bz0 = block_x0(bj);
            if (side == 0) { x = bx0 + t; z = bz0 + 1.3f; }
            else if (side == 1) { x = bx0 + BLOCKW - 1.3f; z = bz0 + t; }
            else if (side == 2) { x = bx0 + t; z = bz0 + BLOCKW - 1.3f; }
            else { x = bx0 + 1.3f; z = bz0 + t; }
            w->cash[w->ncash++] = v3(x, CURB, z);
        }
        /* cadeautjes voor de laatste missie */
        {
            static const int gb[5][2] = {{0, 3}, {0, 6}, {5, 6}, {5, 1}, {6, 2}};
            for (i = 0; i < 5; i++) {
                float bx0 = block_x0(gb[i][0]), bz0 = block_x0(gb[i][1]);
                w->gifts[i] = v3(bx0 + BLOCKW * 0.5f, CURB, bz0 + 1.3f);
            }
            w->ngifts = 5;
        }
        return 1;
    }
    return 1;
}

void world_free(World *w) {
    int i;
    for (i = 0; i < CHN * CHN; i++) mb_free(&w->chunks[i].mb);
    mb_free(&w->ground);
    free(w->boxes); free(w->gstart); free(w->gitems); free(w->stamp); free(w->minimap);
    free(w->segs); free(w->sstart); free(w->sitems); free(w->sstamp); free(w->rsegs); free(w->names);
    w->boxes = NULL; w->gstart = w->gitems = w->stamp = NULL; w->minimap = NULL;
    w->segs = NULL; w->sstart = w->sitems = w->sstamp = NULL; w->rsegs = NULL; w->names = NULL;
    w->nsegs = w->nrsegs = w->nboxes = 0;
}

float world_ground_y(const World *w, float x, float z) {
    int bi = (int)floorf((x - ROADW) / PITCH), bj = (int)floorf((z - ROADW) / PITCH);
    if (w->real) return 0.0f;
    if (bi < 0 || bj < 0 || bi >= NB || bj >= NB) return 0.0f;
    if (x - block_x0(bi) > BLOCKW || z - block_x0(bj) > BLOCKW) return 0.0f;
    return CURB;
}

int world_on_road(const World *w, float x, float z) {
    if (w->real) return 1;
    if (x < 0 || z < 0 || x > MAPW || z > MAPW) return 0;
    return world_ground_y(w, x, z) < 0.01f;
}

static void cell_range(const World *w, float x0, float z0, float x1, float z1, int *ax, int *az, int *bx, int *bz) {
    *ax = (int)floorf((x0 - w->gorg) / w->gcell); *bx = (int)floorf((x1 - w->gorg) / w->gcell);
    *az = (int)floorf((z0 - w->gorg) / w->gcell); *bz = (int)floorf((z1 - w->gorg) / w->gcell);
    if (*ax < 0) *ax = 0; if (*az < 0) *az = 0;
    if (*bx >= w->gn) *bx = w->gn - 1; if (*bz >= w->gn) *bz = w->gn - 1;
}

int world_push_circle(World *w, float *px, float *pz, float r, float y, float *nx, float *nz) {
    int ax, az, bx, bz, cx, cz, k, hit = 0;
    float sx = 0, sz = 0;
    if (!w->gstart) return 0;
    cell_range(w, *px - r, *pz - r, *px + r, *pz + r, &ax, &az, &bx, &bz);
    w->curstamp++;
    for (cz = az; cz <= bz; cz++)
        for (cx = ax; cx <= bx; cx++) {
            int c = cz * w->gn + cx;
            for (k = w->gstart[c]; k < w->gstart[c + 1]; k++) {
                int bi = w->gitems[k];
                Box *b;
                float qx, qz, dx, dz, d2;
                if (w->stamp[bi] == w->curstamp) continue;
                w->stamp[bi] = w->curstamp;
                b = &w->boxes[bi];
                if (y > b->y1 - 0.05f) continue;
                qx = clampf(*px, b->x0, b->x1); qz = clampf(*pz, b->z0, b->z1);
                dx = *px - qx; dz = *pz - qz; d2 = dx * dx + dz * dz;
                if (d2 >= r * r) continue;
                if (d2 > 1e-8f) {
                    float d = sqrtf(d2), push = r - d;
                    dx /= d; dz /= d;
                    *px += dx * push; *pz += dz * push;
                    sx += dx; sz += dz;
                } else {
                    /* midden zit in de doos: kortste weg naar buiten */
                    float l = *px - b->x0, rr = b->x1 - *px, t = *pz - b->z0, bt = b->z1 - *pz;
                    float m = fminf(fminf(l, rr), fminf(t, bt));
                    if (m == l) { *px = b->x0 - r; sx -= 1; }
                    else if (m == rr) { *px = b->x1 + r; sx += 1; }
                    else if (m == t) { *pz = b->z0 - r; sz -= 1; }
                    else { *pz = b->z1 + r; sz += 1; }
                }
                hit = 1;
            }
        }
    if (w->nsegs > 0 && w->sstart) {
        w->sstampc++;
        for (cz = az; cz <= bz; cz++)
            for (cx = ax; cx <= bx; cx++) {
                int c = cz * w->gn + cx;
                for (k = w->sstart[c]; k < w->sstart[c + 1]; k++) {
                    int si = w->sitems[k];
                    WSeg *s;
                    float ex, ez, l2, t, qx, qz, dx, dz, d2;
                    if (w->sstamp[si] == w->sstampc) continue;
                    w->sstamp[si] = w->sstampc;
                    s = &w->segs[si];
                    if (y > s->h - 0.05f) continue;
                    ex = s->x1 - s->x0; ez = s->z1 - s->z0; l2 = ex * ex + ez * ez;
                    t = l2 > 1e-6f ? clampf(((*px - s->x0) * ex + (*pz - s->z0) * ez) / l2, 0, 1) : 0;
                    qx = s->x0 + ex * t; qz = s->z0 + ez * t;
                    dx = *px - qx; dz = *pz - qz; d2 = dx * dx + dz * dz;
                    if (d2 >= r * r || d2 < 1e-8f) continue;
                    {
                        float d = sqrtf(d2), push = r - d;
                        dx /= d; dz /= d;
                        *px += dx * push; *pz += dz * push;
                        sx += dx; sz += dz;
                        hit = 1;
                    }
                }
            }
    }
    if (hit) {
        float l = sqrtf(sx * sx + sz * sz);
        if (l < 1e-5f) { sx = 1; sz = 0; l = 1; }
        if (nx) *nx = sx / l;
        if (nz) *nz = sz / l;
    }
    return hit;
}

float world_raycast(World *w, V3 a, V3 b) {
    int ax, az, bx, bz, cx, cz, k;
    float best = 1.0f;
    V3 d = v3sub(b, a);
    if (!w->gstart) return 1.0f;
    cell_range(w, fminf(a.x, b.x), fminf(a.z, b.z), fmaxf(a.x, b.x), fmaxf(a.z, b.z), &ax, &az, &bx, &bz);
    w->curstamp++;
    for (cz = az; cz <= bz; cz++)
        for (cx = ax; cx <= bx; cx++) {
            int c = cz * w->gn + cx;
            for (k = w->gstart[c]; k < w->gstart[c + 1]; k++) {
                int bi = w->gitems[k];
                Box *bb;
                float t0 = 0, t1 = 1;
                int ax2;
                if (w->stamp[bi] == w->curstamp) continue;
                w->stamp[bi] = w->curstamp;
                bb = &w->boxes[bi];
                if (bb->kind != BOX_BUILDING && bb->kind != BOX_WALL) continue;
                for (ax2 = 0; ax2 < 3; ax2++) {
                    float o = ax2 == 0 ? a.x : (ax2 == 1 ? a.y : a.z);
                    float dd = ax2 == 0 ? d.x : (ax2 == 1 ? d.y : d.z);
                    float lo = ax2 == 0 ? bb->x0 : (ax2 == 1 ? -1.0f : bb->z0);
                    float hi = ax2 == 0 ? bb->x1 : (ax2 == 1 ? bb->y1 : bb->z1);
                    if (fabsf(dd) < 1e-6f) {
                        if (o < lo || o > hi) { t0 = 2; break; }
                    } else {
                        float u0 = (lo - o) / dd, u1 = (hi - o) / dd;
                        if (u0 > u1) { float tt = u0; u0 = u1; u1 = tt; }
                        if (u0 > t0) t0 = u0;
                        if (u1 < t1) t1 = u1;
                        if (t0 > t1) { t0 = 2; break; }
                    }
                }
                if (t0 <= 1 && t0 < best) best = t0;
            }
        }
    if (w->nsegs > 0 && w->sstart) {
        w->sstampc++;
        for (cz = az; cz <= bz; cz++)
            for (cx = ax; cx <= bx; cx++) {
                int c = cz * w->gn + cx;
                for (k = w->sstart[c]; k < w->sstart[c + 1]; k++) {
                    int si = w->sitems[k];
                    WSeg *s;
                    float ex, ez, den, t, u;
                    if (w->sstamp[si] == w->sstampc) continue;
                    w->sstamp[si] = w->sstampc;
                    s = &w->segs[si];
                    ex = s->x1 - s->x0; ez = s->z1 - s->z0;
                    den = d.x * ez - d.z * ex;
                    if (fabsf(den) < 1e-6f) continue;
                    t = ((s->x0 - a.x) * ez - (s->z0 - a.z) * ex) / den;
                    u = ((s->x0 - a.x) * d.z - (s->z0 - a.z) * d.x) / den;
                    if (t < 0 || t > 1 || u < 0 || u > 1) continue;
                    if (a.y + d.y * t > s->h) continue;
                    if (t < best) best = t;
                }
            }
    }
    return best;
}
