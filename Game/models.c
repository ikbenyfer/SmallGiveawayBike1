#include "models.h"

typedef struct { Col shirt, pants, skin, hair, shoe; int cap; } CharLook;

static CharLook look(int v) {
    static const int shirts[CHAR_VARIANTS][3] = {
        {196, 36, 40}, {60, 110, 190}, {230, 200, 70}, {70, 150, 90},
        {230, 230, 225}, {130, 70, 150}, {240, 130, 50}, {60, 60, 66}, {38, 56, 112}};
    static const int pants[CHAR_VARIANTS][3] = {
        {34, 36, 44}, {70, 80, 110}, {60, 50, 40}, {40, 40, 45},
        {90, 100, 130}, {50, 50, 60}, {120, 110, 90}, {150, 140, 120}, {26, 30, 52}};
    static const int skins[4][3] = {{236, 192, 160}, {200, 150, 110}, {150, 100, 70}, {100, 68, 50}};
    static const int hairs[4][3] = {{40, 30, 25}, {120, 80, 40}, {220, 190, 120}, {20, 20, 20}};
    CharLook l;
    v = ((v % CHAR_VARIANTS) + CHAR_VARIANTS) % CHAR_VARIANTS;
    l.shirt = col(shirts[v][0], shirts[v][1], shirts[v][2]);
    l.pants = col(pants[v][0], pants[v][1], pants[v][2]);
    l.skin = col(skins[(v * 3 + 1) % 4][0], skins[(v * 3 + 1) % 4][1], skins[(v * 3 + 1) % 4][2]);
    l.hair = col(hairs[(v * 5 + 2) % 4][0], hairs[(v * 5 + 2) % 4][1], hairs[(v * 5 + 2) % 4][2]);
    l.shoe = col(30, 30, 32);
    l.cap = (v == 0 || v == COP_VARIANT);
    if (v == 0) l.skin = col(220, 170, 130);
    return l;
}

void model_char_body(MeshB *m, int v) {
    CharLook l = look(v);
    /* heup-blok */
    mb_box(m, v3(-0.25f, HIP_Y - 0.08f, -0.14f), v3(0.25f, HIP_Y + 0.12f, 0.14f), l.pants, BOX_NOBOTTOM);
    /* romp, iets taps */
    mb_box(m, v3(-0.26f, HIP_Y + 0.12f, -0.15f), v3(0.26f, 1.56f, 0.15f), l.shirt, BOX_NOBOTTOM);
    /* schouders */
    mb_box(m, v3(-0.30f, 1.44f, -0.14f), v3(0.30f, 1.58f, 0.14f), l.shirt, BOX_NOBOTTOM);
    /* nek + hoofd */
    mb_box(m, v3(-0.07f, 1.58f, -0.07f), v3(0.07f, 1.64f, 0.07f), l.skin, BOX_NOBOTTOM);
    mb_box(m, v3(-0.15f, 1.64f, -0.15f), v3(0.15f, 1.95f, 0.16f), l.skin, BOX_NOBOTTOM);
    /* haar */
    mb_box(m, v3(-0.16f, 1.86f, -0.17f), v3(0.16f, 1.99f, 0.12f), l.hair, 0);
    if (l.cap) {
        Col capc = v == COP_VARIANT ? col(24, 34, 74) : col(30, 30, 34);
        mb_box(m, v3(-0.17f, 1.90f, -0.17f), v3(0.17f, 2.02f, 0.15f), capc, 0);
        mb_box(m, v3(-0.13f, 1.92f, 0.15f), v3(0.13f, 1.95f, 0.30f), capc, 0);
    }
    /* ogen */
    mb_quad(m, v3(-0.10f, 1.80f, 0.162f), v3(-0.04f, 1.80f, 0.162f), v3(-0.04f, 1.85f, 0.162f), v3(-0.10f, 1.85f, 0.162f), v3(0, 0, 1), col(20, 20, 20));
    mb_quad(m, v3(0.04f, 1.80f, 0.162f), v3(0.10f, 1.80f, 0.162f), v3(0.10f, 1.85f, 0.162f), v3(0.04f, 1.85f, 0.162f), v3(0, 0, 1), col(20, 20, 20));
    if (v == COP_VARIANT) {   /* shield badge */
        mb_quad(m, v3(0.06f, 1.34f, 0.152f), v3(0.16f, 1.34f, 0.152f), v3(0.16f, 1.44f, 0.152f), v3(0.06f, 1.44f, 0.152f), v3(0, 0, 1), col_e(240, 200, 70, 120));
    }
    if (v == 0) {
        /* speler: wit logo-vlak op de borst */
        mb_quad(m, v3(-0.12f, 1.22f, 0.152f), v3(0.12f, 1.22f, 0.152f), v3(0.12f, 1.40f, 0.152f), v3(-0.12f, 1.40f, 0.152f), v3(0, 0, 1), col(235, 235, 230));
    }
}

void model_char_leg(MeshB *m, int v) {
    CharLook l = look(v);
    mb_box(m, v3(-0.11f, -0.80f, -0.12f), v3(0.11f, 0.0f, 0.12f), l.pants, 0);
    mb_box(m, v3(-0.12f, -0.92f, -0.14f), v3(0.12f, -0.80f, 0.20f), l.shoe, 0);
}

void model_char_arm(MeshB *m, int v) {
    CharLook l = look(v);
    mb_box(m, v3(-0.08f, -0.50f, -0.09f), v3(0.08f, 0.05f, 0.09f), l.shirt, 0);
    mb_box(m, v3(-0.07f, -0.66f, -0.08f), v3(0.07f, -0.50f, 0.08f), l.skin, 0);
}

/* dunne buis tussen twee punten (vierkante doorsnede) */
static void tube(MeshB *m, V3 a, V3 b, float t, Col c) {
    V3 d = v3sub(b, a);
    float len = v3len(d);
    if (len < 1e-4f) return;
    V3 f = v3scale(d, 1.0f / len);
    V3 up = fabsf(f.y) > 0.9f ? v3(1, 0, 0) : v3(0, 1, 0);
    V3 s = v3norm(v3cross(f, up));
    V3 u = v3cross(s, f);
    V3 pts[4];
    int i;
    for (i = 0; i < 4; i++) {
        float ang = PI_F * 0.5f * i + PI_F * 0.25f;
        pts[i] = v3add(v3scale(s, cosf(ang) * t), v3scale(u, sinf(ang) * t));
    }
    for (i = 0; i < 4; i++) {
        V3 p0 = pts[i], p1 = pts[(i + 1) % 4];
        V3 n = v3norm(v3add(p0, p1));
        mb_quad(m, v3add(a, p0), v3add(a, p1), v3add(b, p1), v3add(b, p0), n, c);
    }
}

static void wheel(MeshB *m, V3 c, float r, Col tire, Col rim) {
    mb_cyl(m, c, r, 0.07f, 10, 0, tire, rim, 1);
    mb_cyl(m, c, r * 0.25f, 0.09f, 6, 0, rim, rim, 1);
}

void model_bike(MeshB *m, int v) {
    static const int frames[BIKE_VARIANTS][3] = {{210, 30, 36}, {30, 170, 190}, {240, 200, 40}, {235, 235, 235}};
    v = ((v % BIKE_VARIANTS) + BIKE_VARIANTS) % BIKE_VARIANTS;
    Col fc = col(frames[v][0], frames[v][1], frames[v][2]);
    Col dark = col(25, 25, 28), metal = col(170, 170, 175);
    float r = 0.34f;
    V3 rear = v3(0, r, -0.52f), front = v3(0, r, 0.55f);
    wheel(m, rear, r, dark, metal);
    wheel(m, front, r, dark, metal);
    V3 crank = v3(0, 0.30f, -0.05f);
    V3 seat = v3(0, 0.92f, -0.22f);
    V3 head = v3(0, 0.88f, 0.42f);
    tube(m, rear, crank, 0.025f, fc);
    tube(m, rear, seat, 0.025f, fc);
    tube(m, crank, seat, 0.03f, fc);
    tube(m, crank, head, 0.035f, fc);
    tube(m, seat, head, 0.03f, fc);
    tube(m, head, front, 0.025f, metal);
    tube(m, head, v3(0, 1.05f, 0.40f), 0.025f, metal);
    tube(m, v3(-0.28f, 1.05f, 0.38f), v3(0.28f, 1.05f, 0.38f), 0.025f, dark);
    mb_box(m, v3(-0.09f, 0.94f, -0.34f), v3(0.09f, 1.00f, -0.12f), dark, 0);  /* zadel */
    mb_box(m, v3(-0.12f, 0.27f, -0.08f), v3(0.12f, 0.33f, -0.02f), dark, 0);  /* pedalen */
    /* bagagerek met "giveaway"-doosje bij variant 0 */
    if (v == 0) {
        tube(m, v3(0, 0.62f, -0.50f), v3(0, 0.62f, -0.20f), 0.02f, metal);
        mb_box(m, v3(-0.16f, 0.62f, -0.62f), v3(0.16f, 0.82f, -0.30f), col(230, 230, 225), 0);
        mb_box(m, v3(-0.165f, 0.62f, -0.49f), v3(0.165f, 0.83f, -0.43f), col(200, 30, 30), 0);
    }
    mb_box(m, v3(-0.06f, 0.80f, 0.44f), v3(0.06f, 0.90f, 0.52f), col_e(255, 240, 200, 255), 0); /* lampje */
}

Col car_color(int v) {
    static const int cc[CAR_VARIANTS][3] = {{200, 30, 34}, {40, 80, 170}, {240, 190, 30}, {225, 225, 225}, {50, 130, 80}, {40, 40, 46}, {238, 238, 242}};
    v = ((v % CAR_VARIANTS) + CAR_VARIANTS) % CAR_VARIANTS;
    return col(cc[v][0], cc[v][1], cc[v][2]);
}

void model_car(MeshB *m, int v) {
    Col body = car_color(v);
    Col glass = col(40, 55, 75), dark = col(22, 22, 25), metal = col(160, 160, 165);
    m->ao = 0.25f;
    mb_box2(m, v3(-0.9f, 0.30f, -2.0f), v3(0.9f, 0.95f, 2.0f), body, body, BOX_NOBOTTOM);
    m->ao = 0;
    /* cabine: glas met dak */
    mb_box(m, v3(-0.80f, 0.95f, -1.05f), v3(0.80f, 1.45f, 0.75f), glass, BOX_NOBOTTOM | BOX_NOTOP);
    mb_box(m, v3(-0.82f, 1.45f, -1.10f), v3(0.82f, 1.55f, 0.70f), body, BOX_NOBOTTOM);
    /* stijlen */
    mb_box(m, v3(-0.82f, 0.95f, 0.62f), v3(-0.72f, 1.45f, 0.75f), body, 0);
    mb_box(m, v3(0.72f, 0.95f, 0.62f), v3(0.82f, 1.45f, 0.75f), body, 0);
    /* bumpers */
    mb_box(m, v3(-0.92f, 0.28f, 1.95f), v3(0.92f, 0.50f, 2.08f), dark, 0);
    mb_box(m, v3(-0.92f, 0.28f, -2.08f), v3(0.92f, 0.50f, -1.95f), dark, 0);
    /* lichten */
    mb_quad(m, v3(-0.82f, 0.62f, 2.005f), v3(-0.50f, 0.62f, 2.005f), v3(-0.50f, 0.80f, 2.005f), v3(-0.82f, 0.80f, 2.005f), v3(0, 0, 1), col_e(255, 245, 210, 255));
    mb_quad(m, v3(0.50f, 0.62f, 2.005f), v3(0.82f, 0.62f, 2.005f), v3(0.82f, 0.80f, 2.005f), v3(0.50f, 0.80f, 2.005f), v3(0, 0, 1), col_e(255, 245, 210, 255));
    mb_quad(m, v3(-0.84f, 0.62f, -2.005f), v3(-0.52f, 0.62f, -2.005f), v3(-0.52f, 0.78f, -2.005f), v3(-0.84f, 0.78f, -2.005f), v3(0, 0, -1), col_e(230, 30, 30, 200));
    mb_quad(m, v3(0.52f, 0.62f, -2.005f), v3(0.84f, 0.62f, -2.005f), v3(0.84f, 0.78f, -2.005f), v3(0.52f, 0.78f, -2.005f), v3(0, 0, -1), col_e(230, 30, 30, 200));
    /* wielen */
    {
        float wx[2] = {-0.86f, 0.86f}, wz[2] = {-1.3f, 1.3f};
        int a, b;
        for (a = 0; a < 2; a++)
            for (b = 0; b < 2; b++) {
                mb_cyl(m, v3(wx[a], 0.34f, wz[b]), 0.34f, 0.26f, 8, 0, dark, metal, 1);
            }
    }
    if (v == POLICE_VARIANT) { /* politie: blauwe streep, donkere motorkap, voet lichtbalk */
        mb_box(m, v3(-0.915f, 0.55f, -1.7f), v3(0.915f, 0.78f, 1.7f), col(30, 50, 140), 0);
        mb_box(m, v3(-0.88f, 0.95f, 1.05f), v3(0.88f, 0.97f, 1.98f), col(25, 25, 30), 0);
        mb_box(m, v3(-0.6f, 1.55f, -0.35f), v3(0.6f, 1.62f, -0.05f), col(30, 30, 34), 0);
    }
    if (v == 2) { /* taxi-bordje */
        mb_box(m, v3(-0.25f, 1.55f, -0.3f), v3(0.25f, 1.72f, 0.0f), col_e(255, 230, 120, 160), 0);
    }
}

void model_cash(MeshB *m) {
    mb_box(m, v3(-0.35f, 0.0f, -0.18f), v3(0.35f, 0.25f, 0.18f), col_e(60, 190, 80, 120), 0);
    mb_box(m, v3(-0.08f, -0.01f, -0.19f), v3(0.08f, 0.26f, 0.19f), col_e(230, 240, 200, 120), 0);
}

void model_gift(MeshB *m) {
    mb_box(m, v3(-0.4f, 0.0f, -0.4f), v3(0.4f, 0.7f, 0.4f), col_e(210, 30, 40, 80), 0);
    mb_box(m, v3(-0.42f, -0.01f, -0.09f), v3(0.42f, 0.72f, 0.09f), col_e(250, 210, 60, 120), 0);
    mb_box(m, v3(-0.09f, -0.01f, -0.42f), v3(0.09f, 0.72f, 0.42f), col_e(250, 210, 60, 120), 0);
    mb_box(m, v3(-0.2f, 0.72f, -0.06f), v3(0.2f, 0.88f, 0.06f), col_e(250, 210, 60, 120), 0);
}

void model_checkpoint(MeshB *m) {
    mb_cyl(m, v3(0, 0, 0), 1.0f, 1.0f, 16, 1, col_e(255, 255, 255, 255), col(0, 0, 0), 0);
}

void model_arrow(MeshB *m) {
    /* naar beneden wijzende pijl */
    Col c = col_e(255, 255, 255, 255);
    mb_box(m, v3(-0.25f, 0.9f, -0.25f), v3(0.25f, 1.9f, 0.25f), c, 0);
    {
        V3 tip = v3(0, 0, 0);
        V3 p[4] = {v3(-0.7f, 0.9f, -0.7f), v3(0.7f, 0.9f, -0.7f), v3(0.7f, 0.9f, 0.7f), v3(-0.7f, 0.9f, 0.7f)};
        int i;
        for (i = 0; i < 4; i++) {
            V3 a = p[i], b = p[(i + 1) % 4];
            V3 n = v3norm(v3cross(v3sub(b, a), v3sub(tip, a)));
            if (n.y > 0) n = v3scale(n, -1);
            mb_tri(m, a, b, tip, v3norm(v3add(v3scale(v3add(a, b), 0.5f), v3(0, -0.5f, 0))), c);
        }
        mb_quad(m, p[0], p[1], p[2], p[3], v3(0, 1, 0), c);
    }
}

void model_siren(MeshB *m) {
    mb_box(m, v3(-0.26f, 0.0f, -0.13f), v3(0.0f, 0.16f, 0.13f), col_e(255, 255, 255, 255), 0);
}

void model_gun(MeshB *m) {
    Col g = col(34, 34, 38);
    mb_box(m, v3(-0.035f, -0.95f, -0.05f), v3(0.035f, -0.60f, 0.05f), g, 0);     /* loop */
    mb_box(m, v3(-0.03f, -0.70f, 0.04f), v3(0.03f, -0.62f, 0.16f), g, 0);       /* greep */
}

void model_tracer(MeshB *m) {
    mb_box(m, v3(-0.025f, -0.025f, 0.0f), v3(0.025f, 0.025f, 1.0f), col_e(255, 230, 140, 255), 0);
}

/* ---- spelerauto's: carrosserie (wit, wordt gekleurd) en details apart */

static const int PAINTS[NPAINTS][3] = {
    {200, 30, 34}, {40, 80, 190}, {245, 200, 30}, {235, 235, 235}, {40, 150, 80},
    {30, 30, 34}, {245, 120, 20}, {130, 50, 170}, {40, 190, 210}, {160, 165, 172}};
static const int NEONS[NNEONS][3] = {{0, 0, 0}, {255, 40, 40}, {40, 120, 255}, {40, 255, 90}, {255, 60, 220}, {255, 255, 255}};

Col paint_color(int i) { i = ((i % NPAINTS) + NPAINTS) % NPAINTS; return col(PAINTS[i][0], PAINTS[i][1], PAINTS[i][2]); }
Col neon_color(int i) { i = ((i % NNEONS) + NNEONS) % NNEONS; return col(NEONS[i][0], NEONS[i][1], NEONS[i][2]); }
int model_default_paint(int model) { static const int d[CAR_MODELS] = {1, 6, 0, 2}; return d[model % CAR_MODELS]; }

static void wheels4(MeshB *m, float wx, float wzf, float wzr, float r, float rr) {
    Col dark = col(22, 22, 25), rim = col(185, 185, 192);
    int a;
    for (a = -1; a <= 1; a += 2) {
        mb_cyl(m, v3(wx * a, r, wzf), r, 0.26f, 10, 0, dark, rim, 1);
        mb_cyl(m, v3(wx * a, rr, wzr), rr, 0.30f, 10, 0, dark, rim, 1);
    }
}

static void lights(MeshB *m, float w, float y0, float y1, float zf, float zr) {
    mb_quad(m, v3(-w, y0, zf), v3(-w + 0.32f, y0, zf), v3(-w + 0.32f, y1, zf), v3(-w, y1, zf), v3(0, 0, 1), col_e(255, 245, 210, 255));
    mb_quad(m, v3(w - 0.32f, y0, zf), v3(w, y0, zf), v3(w, y1, zf), v3(w - 0.32f, y1, zf), v3(0, 0, 1), col_e(255, 245, 210, 255));
    mb_quad(m, v3(-w, y0, zr), v3(-w + 0.34f, y0, zr), v3(-w + 0.34f, y1, zr), v3(-w, y1, zr), v3(0, 0, -1), col_e(230, 30, 30, 200));
    mb_quad(m, v3(w - 0.34f, y0, zr), v3(w, y0, zr), v3(w, y1, zr), v3(w - 0.34f, y1, zr), v3(0, 0, -1), col_e(230, 30, 30, 200));
}

void model_car_body(MeshB *m, int model) {
    Col w = col(236, 236, 236);
    m->ao = 0.25f;
    switch (model) {
    case 1:   /* RHINO V8: lange motorkap, luchthapper */
        mb_box2(m, v3(-0.95f, 0.30f, -2.25f), v3(0.95f, 0.92f, 2.30f), w, w, BOX_NOBOTTOM);
        m->ao = 0;
        mb_box(m, v3(-0.35f, 0.92f, 0.7f), v3(0.35f, 1.08f, 1.5f), w, BOX_NOBOTTOM);
        mb_box(m, v3(-0.84f, 1.36f, -1.25f), v3(0.84f, 1.44f, 0.25f), w, BOX_NOBOTTOM);
        mb_box(m, v3(-0.84f, 0.92f, 0.12f), v3(-0.74f, 1.36f, 0.25f), w, 0);
        mb_box(m, v3(0.74f, 0.92f, 0.12f), v3(0.84f, 1.36f, 0.25f), w, 0);
        break;
    case 2:   /* VORTEX GT: laag en spits */
        mb_box2(m, v3(-0.95f, 0.25f, -2.1f), v3(0.95f, 0.72f, 1.5f), w, w, BOX_NOBOTTOM);
        m->ao = 0;
        mb_box(m, v3(-0.93f, 0.25f, 1.5f), v3(0.93f, 0.58f, 2.25f), w, BOX_NOBOTTOM);
        mb_box(m, v3(-0.76f, 1.1f, -0.85f), v3(0.76f, 1.16f, 0.35f), w, BOX_NOBOTTOM);
        mb_box(m, v3(-0.98f, 0.45f, -1.6f), v3(0.98f, 0.62f, -0.9f), w, 0);
        break;
    case 3:   /* BOLT X: wig, kleine cabine, ingebouwde vleugel */
        mb_box2(m, v3(-1.0f, 0.22f, -2.25f), v3(1.0f, 0.62f, 1.3f), w, w, BOX_NOBOTTOM);
        m->ao = 0;
        mb_box(m, v3(-0.98f, 0.22f, 1.3f), v3(0.98f, 0.48f, 2.3f), w, BOX_NOBOTTOM);
        mb_box(m, v3(-0.7f, 0.96f, -0.35f), v3(0.7f, 1.01f, 0.55f), w, BOX_NOBOTTOM);
        mb_box(m, v3(-0.98f, 1.02f, -2.2f), v3(0.98f, 1.09f, -1.8f), w, 0);
        break;
    default:  /* CITY HATCH: zelfde vorm als de gewone auto's */
        mb_box2(m, v3(-0.9f, 0.30f, -2.0f), v3(0.9f, 0.95f, 2.0f), w, w, BOX_NOBOTTOM);
        m->ao = 0;
        mb_box(m, v3(-0.82f, 1.45f, -1.10f), v3(0.82f, 1.55f, 0.70f), w, BOX_NOBOTTOM);
        mb_box(m, v3(-0.82f, 0.95f, 0.62f), v3(-0.72f, 1.45f, 0.75f), w, 0);
        mb_box(m, v3(0.72f, 0.95f, 0.62f), v3(0.82f, 1.45f, 0.75f), w, 0);
        break;
    }
}

void model_car_detail(MeshB *m, int model) {
    Col glass = col(36, 50, 70), dark = col(22, 22, 25), chrome = col(190, 192, 198);
    switch (model) {
    case 1:
        mb_box(m, v3(-0.80f, 0.92f, -1.2f), v3(0.80f, 1.36f, 0.2f), glass, BOX_NOBOTTOM | BOX_NOTOP);
        mb_box(m, v3(-0.97f, 0.30f, 2.25f), v3(0.97f, 0.52f, 2.40f), chrome, 0);
        mb_box(m, v3(-0.97f, 0.30f, -2.38f), v3(0.97f, 0.52f, -2.22f), chrome, 0);
        mb_box(m, v3(-0.3f, 1.0f, 1.5f), v3(0.3f, 1.08f, 1.52f), dark, 0);
        lights(m, 0.9f, 0.62f, 0.8f, 2.305f, -2.255f);
        wheels4(m, 0.9f, 1.45f, -1.45f, 0.35f, 0.38f);
        break;
    case 2:
        mb_box(m, v3(-0.80f, 0.72f, -0.9f), v3(0.80f, 1.1f, 0.6f), glass, BOX_NOBOTTOM | BOX_NOTOP);
        mb_box(m, v3(-0.99f, 0.3f, -1.5f), v3(-0.95f, 0.45f, -0.9f), dark, 0);
        mb_box(m, v3(0.95f, 0.3f, -1.5f), v3(0.99f, 0.45f, -0.9f), dark, 0);
        mb_box(m, v3(-0.9f, 0.25f, 2.25f), v3(0.9f, 0.35f, 2.3f), dark, 0);
        lights(m, 0.88f, 0.42f, 0.54f, 2.255f, -2.105f);
        wheels4(m, 0.9f, 1.4f, -1.35f, 0.34f, 0.35f);
        break;
    case 3:
        mb_box(m, v3(-0.74f, 0.62f, -0.4f), v3(0.74f, 0.96f, 0.95f), glass, BOX_NOBOTTOM | BOX_NOTOP);
        {
            int k;
            for (k = 0; k < 4; k++) mb_box(m, v3(-0.7f, 0.62f, -1.75f + k * 0.28f), v3(0.7f, 0.66f, -1.65f + k * 0.28f), dark, 0);
        }
        mb_box(m, v3(-0.75f, 0.62f, -2.05f), v3(-0.65f, 1.02f, -1.95f), dark, 0);
        mb_box(m, v3(0.65f, 0.62f, -2.05f), v3(0.75f, 1.02f, -1.95f), dark, 0);
        mb_box(m, v3(-1.01f, 0.28f, -1.2f), v3(-0.99f, 0.5f, -0.5f), dark, 0);
        mb_box(m, v3(0.99f, 0.28f, -1.2f), v3(1.01f, 0.5f, -0.5f), dark, 0);
        lights(m, 0.92f, 0.34f, 0.44f, 2.305f, -2.255f);
        wheels4(m, 0.95f, 1.45f, -1.45f, 0.35f, 0.37f);
        break;
    default:
        mb_box(m, v3(-0.80f, 0.95f, -1.05f), v3(0.80f, 1.45f, 0.75f), glass, BOX_NOBOTTOM | BOX_NOTOP);
        mb_box(m, v3(-0.92f, 0.28f, 1.95f), v3(0.92f, 0.50f, 2.08f), dark, 0);
        mb_box(m, v3(-0.92f, 0.28f, -2.08f), v3(0.92f, 0.50f, -1.95f), dark, 0);
        lights(m, 0.82f, 0.62f, 0.80f, 2.005f, -2.005f);
        wheels4(m, 0.86f, 1.3f, -1.3f, 0.34f, 0.34f);
        break;
    }
}

void model_spoiler(MeshB *m) {
    Col w = col(236, 236, 236), dark = col(22, 22, 25);
    mb_box(m, v3(-0.95f, 1.12f, -0.25f), v3(0.95f, 1.18f, 0.2f), w, 0);
    mb_box(m, v3(-0.6f, 0.0f, -0.05f), v3(-0.5f, 1.12f, 0.05f), dark, 0);
    mb_box(m, v3(0.5f, 0.0f, -0.05f), v3(0.6f, 1.12f, 0.05f), dark, 0);
}

void model_shadow(MeshB *m) { mb_disc(m, v3(0, 0, 0), 1.0f, 12, col(0, 0, 0)); }

void model_marker(MeshB *m) {
    int i, seg = 20;
    Col c = col_e(255, 255, 255, 255);
    for (i = 0; i < seg; i++) {
        float a0 = 2 * PI_F * i / seg, a1 = 2 * PI_F * (i + 1) / seg;
        V3 o0 = v3(cosf(a0) * 1.0f, 0, sinf(a0) * 1.0f), o1 = v3(cosf(a1) * 1.0f, 0, sinf(a1) * 1.0f);
        V3 i0 = v3scale(o0, 0.8f), i1 = v3scale(o1, 0.8f);
        mb_quad(m, i0, o0, o1, i1, v3(0, 1, 0), c);
    }
}
