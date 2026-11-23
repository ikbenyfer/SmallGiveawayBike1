#include "models.h"

typedef struct { Col shirt, pants, skin, hair, shoe; int cap; } CharLook;

static CharLook look(int v);
void model_char_colors(int v, Col *skin, Col *hair) { CharLook l = look(v); *skin = l.skin; *hair = l.hair; }

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

void model_char_body_lo(MeshB *m, int v) {
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

void model_bike_lo(MeshB *m, int v) {
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

/* motor: wielbasis 1,4 m, zadel op ~0,9 m (zelfde rijhouding als de fiets) */
void model_moto_lo(MeshB *m, int k) {
    static const int body[MOTO_MODELS][3] = {{30, 90, 200}, {210, 25, 30}};
    Col bc = col(body[k % MOTO_MODELS][0], body[k % MOTO_MODELS][1], body[k % MOTO_MODELS][2]);
    Col dark = col(22, 22, 25), metal = col(175, 175, 182), engine = col(70, 70, 76);
    float r = 0.33f;
    V3 rear = v3(0, r, -0.70f), front = v3(0, r, 0.72f), head = v3(0, 0.98f, 0.52f);
    wheel(m, rear, r, dark, metal);
    wheel(m, front, r, dark, metal);
    /* wielen dikker maken: tweede band iets ernaast */
    wheel(m, v3(0.05f, r, -0.70f), r, dark, metal);
    wheel(m, v3(-0.05f, r, -0.70f), r, dark, metal);
    tube(m, rear, v3(0, 0.40f, -0.18f), 0.045f, metal);             /* achterbrug */
    tube(m, head, front, 0.04f, metal);                               /* voorvork */
    tube(m, v3(0.07f, 0.98f, 0.52f), v3(0.07f, r, 0.72f), 0.03f, metal);
    tube(m, v3(-0.07f, 0.98f, 0.52f), v3(-0.07f, r, 0.72f), 0.03f, metal);
    tube(m, v3(-0.34f, 1.06f, 0.46f), v3(0.34f, 1.06f, 0.46f), 0.03f, dark);   /* stuur */
    tube(m, head, v3(0, 1.06f, 0.46f), 0.035f, dark);
    mb_box(m, v3(-0.17f, 0.22f, -0.28f), v3(0.17f, 0.62f, 0.22f), engine, 0);   /* motorblok */
    mb_box(m, v3(-0.21f, 0.62f, -0.08f), v3(0.21f, 0.92f, 0.42f), bc, 0);       /* tank */
    mb_box(m, v3(-0.17f, 0.80f, -0.62f), v3(0.17f, 0.90f, -0.06f), dark, 0);    /* zadel */
    mb_box(m, v3(-0.14f, 0.74f, -0.92f), v3(0.14f, 0.96f, -0.56f), bc, 0);      /* staart */
    mb_box(m, v3(-0.10f, 0.80f, -0.94f), v3(0.10f, 0.86f, -0.92f), col_e(255, 40, 30, 255), 0); /* achterlicht */
    tube(m, v3(0.20f, 0.30f, 0.05f), v3(0.22f, 0.48f, -0.86f), 0.05f, metal);  /* uitlaat */
    if (k == 1) {   /* superbike: kuip rond de voorkant */
        mb_box(m, v3(-0.25f, 0.42f, 0.30f), v3(0.25f, 1.02f, 0.66f), bc, 0);
        mb_box(m, v3(-0.18f, 1.02f, 0.40f), v3(0.18f, 1.16f, 0.58f), col(40, 55, 75), 0);   /* ruitje */
        mb_box(m, v3(-0.26f, 0.30f, -0.30f), v3(0.26f, 0.62f, 0.30f), bc, BOX_NOTOP);         /* zijkuip */
    }
    mb_box(m, v3(-0.08f, 0.88f, 0.64f), v3(0.08f, 0.98f, 0.70f), col_e(255, 245, 210, 255), 0);   /* koplamp */
}

Col car_color(int v) {
    static const int cc[CAR_VARIANTS][3] = {{200, 30, 34}, {40, 80, 170}, {240, 190, 30}, {225, 225, 225}, {50, 130, 80}, {40, 40, 46}, {238, 238, 242}};
    v = ((v % CAR_VARIANTS) + CAR_VARIANTS) % CAR_VARIANTS;
    return col(cc[v][0], cc[v][1], cc[v][2]);
}

void model_car_lo(MeshB *m, int v) {
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
int model_default_paint(int model) { static const int d[DEALER_MODELS] = {1, 6, 0, 2, 3, 1}; return d[((model % DEALER_MODELS) + DEALER_MODELS) % DEALER_MODELS]; }

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

void model_car_body_lo(MeshB *m, int model) {
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

void model_car_detail_lo(MeshB *m, int model) {
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

void model_spoiler_lo(MeshB *m) {
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

/* ==================================================================================================
 * v3.0 kwaliteitsmodellen: ronde vormen met gladde belichting. De oude blokmodellen (_lo) blijven
 * bestaan als LOD voor dingen ver weg (de iPad 2 hoeft dan minder te tekenen).
 * ================================================================================================== */

static void blob(MeshB *m, V3 c, V3 h, float rv, float rh, int su, int sv, Col col0) {
    SQ s = sq(c, h, rv, rh, col0);
    s.su = su; s.sv = sv;
    mb_sq(m, &s);
}

static int long_sleeves(int v) { return v == COP_VARIANT || v == 7 || v == 5; }

void model_char_body(MeshB *m, int v) {
    CharLook l = look(v);
    Col skinD = col_mul(l.skin, 0.86f), dark = col(20, 18, 18);
    SQ s;
    int k;
    v = ((v % CHAR_VARIANTS) + CHAR_VARIANTS) % CHAR_VARIANTS;
    /* heupen en broek */
    s = sq(v3(0, 0.99f, 0), v3(0.205f, 0.13f, 0.125f), 0.55f, 0.45f, l.pants);
    s.su = 14; s.sv = 8; mb_sq(m, &s);
    blob(m, v3(0, 1.105f, 0), v3(0.212f, 0.028f, 0.13f), 0.3f, 0.45f, 14, 3, col(30, 26, 24));   /* riem */
    blob(m, v3(0, 1.105f, 0.128f), v3(0.03f, 0.022f, 0.008f), 0.4f, 0.4f, 6, 3, col(190, 170, 90)); /* gesp */
    /* romp: smal bij de taille, breed bij de borst */
    s = sq(v3(0, 1.32f, 0), v3(0.25f, 0.235f, 0.14f), 0.55f, 0.5f, l.shirt);
    s.bx = 0.82f; s.bz = 0.9f; s.su = 16; s.sv = 10; mb_sq(m, &s);
    for (k = -1; k <= 1; k += 2) blob(m, v3(0.25f * k, 1.475f, 0), v3(0.075f, 0.06f, 0.095f), 0.8f, 0.8f, 10, 6, l.shirt);
    blob(m, v3(0, 1.54f, 0), v3(0.088f, 0.026f, 0.08f), 0.5f, 0.7f, 10, 4, l.shirt);   /* kraag */
    /* nek en hoofd */
    mb_capsule(m, v3(0, 1.53f, 0), v3(0, 1.67f, 0.006f), 0.058f, 0.052f, 10, l.skin);
    s = sq(v3(0, 1.80f, 0.01f), v3(0.118f, 0.145f, 0.13f), 0.85f, 0.8f, l.skin);
    s.bx = 0.84f; s.bz = 0.92f; s.su = 16; s.sv = 12; mb_sq(m, &s);
    for (k = -1; k <= 1; k += 2) {
        blob(m, v3(0.116f * k, 1.80f, 0.0f), v3(0.022f, 0.04f, 0.03f), 0.8f, 0.8f, 6, 4, skinD);                 /* oor */
        blob(m, v3(0.047f * k, 1.818f, 0.124f), v3(0.027f, 0.019f, 0.012f), 0.8f, 0.8f, 8, 4, col(245, 245, 240)); /* oogwit */
        blob(m, v3(0.047f * k, 1.818f, 0.134f), v3(0.012f, 0.014f, 0.006f), 0.9f, 0.9f, 6, 4, dark);             /* pupil */
        blob(m, v3(0.05f * k, 1.852f, 0.126f), v3(0.034f, 0.008f, 0.012f), 0.5f, 0.6f, 6, 3, col_mul(l.hair, 0.8f)); /* wenkbrauw */
    }
    blob(m, v3(0, 1.783f, 0.137f), v3(0.02f, 0.036f, 0.026f), 0.8f, 0.8f, 6, 5, skinD);           /* neus */
    if (g_sqDetail > 2.0f) {   /* v6.2.0 ULTRA+: iris, oogleden, wimpers, lippen, neusgaten, kin */
        static const int irisC[4][3] = {{92, 62, 38}, {60, 96, 140}, {70, 110, 70}, {50, 36, 26}};
        Col iris = col(irisC[v % 4][0], irisC[v % 4][1], irisC[v % 4][2]);
        for (k = -1; k <= 1; k += 2) {
            blob(m, v3(0.047f * k, 1.818f, 0.1325f), v3(0.0135f, 0.0145f, 0.004f), 0.95f, 0.95f, 12, 5, iris);
            blob(m, v3(0.047f * k, 1.818f, 0.1355f), v3(0.0065f, 0.0065f, 0.003f), 0.95f, 0.95f, 8, 4, dark);
            blob(m, v3(0.044f * k, 1.822f, 0.137f), v3(0.0025f, 0.0025f, 0.0015f), 0.95f, 0.95f, 6, 3, col(250, 250, 250));   /* lichtpuntje */
            blob(m, v3(0.047f * k, 1.832f, 0.127f), v3(0.03f, 0.007f, 0.014f), 0.6f, 0.7f, 10, 3, skinD);   /* bovenooglid */
            blob(m, v3(0.047f * k, 1.836f, 0.133f), v3(0.028f, 0.0025f, 0.006f), 0.4f, 0.5f, 10, 3, col_mul(l.hair, 0.6f));   /* wimpers */
            blob(m, v3(0.011f * k, 1.765f, 0.152f), v3(0.006f, 0.004f, 0.006f), 0.8f, 0.8f, 6, 3, col(60, 40, 36));   /* neusgat */
            blob(m, v3(0.075f * k, 1.765f, 0.11f), v3(0.03f, 0.025f, 0.02f), 0.8f, 0.8f, 8, 5, l.skin);   /* jukbeen */
        }
        blob(m, v3(0, 1.733f, 0.123f), v3(0.034f, 0.007f, 0.011f), 0.6f, 0.6f, 12, 4, col(165, 84, 74));   /* bovenlip */
        blob(m, v3(0, 1.720f, 0.121f), v3(0.031f, 0.008f, 0.012f), 0.6f, 0.6f, 12, 4, col(160, 80, 70));   /* onderlip */
        blob(m, v3(0, 1.69f, 0.105f), v3(0.04f, 0.028f, 0.03f), 0.8f, 0.8f, 10, 5, l.skin);   /* kin */
    }
    blob(m, v3(0, 1.727f, 0.118f), v3(0.038f, 0.009f, 0.012f), 0.6f, 0.6f, 8, 3, col(150, 72, 62)); /* mond */
    /* haar (bovenkant en achterkant), of pet */
    if (l.cap) {
        Col capc = v == COP_VARIANT ? col(24, 34, 74) : col(30, 30, 34);
        s = sq(v3(0, 1.865f, -0.005f), v3(0.132f, 0.10f, 0.146f), 0.7f, 0.8f, capc);
        s.part = 1; s.su = 16; s.sv = 6; mb_sq(m, &s);
        blob(m, v3(0, 1.872f, 0.15f), v3(0.11f, 0.012f, 0.075f), 0.3f, 0.6f, 12, 3, capc);   /* klep */
        if (v == COP_VARIANT) blob(m, v3(0, 1.91f, 0.135f), v3(0.025f, 0.025f, 0.008f), 0.6f, 0.6f, 6, 4, col_e(240, 200, 70, 120));
        blob(m, v3(0, 1.80f, -0.065f), v3(0.122f, 0.075f, 0.085f), 0.7f, 0.8f, 12, 6, l.hair);
    } else {
        s = sq(v3(0, 1.865f, -0.008f), v3(0.13f, 0.095f, 0.145f), 0.65f, 0.8f, l.hair);
        s.part = 1; s.su = 16; s.sv = 6; mb_sq(m, &s);
        blob(m, v3(0, 1.80f, -0.06f), v3(0.126f, 0.1f, 0.09f), 0.7f, 0.8f, 12, 6, l.hair);
        for (k = -1; k <= 1; k += 2) blob(m, v3(0.118f * k, 1.85f, 0.03f), v3(0.02f, 0.05f, 0.06f), 0.7f, 0.8f, 6, 4, l.hair);  /* bakkebaard */
    }
    if (v == COP_VARIANT) {   /* politie: penning, knopen, holster */
        blob(m, v3(0.11f, 1.40f, 0.136f), v3(0.032f, 0.038f, 0.008f), 0.5f, 0.6f, 8, 4, col_e(240, 200, 70, 120));
        for (k = 0; k < 4; k++) blob(m, v3(0, 1.20f + k * 0.08f, 0.142f), v3(0.01f, 0.01f, 0.006f), 1, 1, 6, 3, col(200, 190, 120));
        blob(m, v3(-0.2f, 1.03f, 0.03f), v3(0.035f, 0.07f, 0.05f), 0.4f, 0.5f, 8, 4, dark);
    }
    if (v == 0) {   /* speler: wit logo met rood hartje op de borst */
        blob(m, v3(0, 1.33f, 0.133f), v3(0.11f, 0.08f, 0.012f), 0.35f, 0.5f, 12, 4, col(235, 235, 230));
        blob(m, v3(0, 1.335f, 0.142f), v3(0.035f, 0.03f, 0.006f), 0.7f, 0.7f, 8, 4, col(200, 30, 40));
    }
}

/* been: heup op 0, knie op -KNEE_Y; onderbeen: knie op 0, zool op -(0.92-KNEE_Y) */
void model_char_thigh(MeshB *m, int v) {
    CharLook l = look(v);
    mb_capsule(m, v3(0, 0, 0), v3(0, -KNEE_Y, 0.005f), 0.105f, 0.078f, 12, l.pants);
}

void model_char_shin(MeshB *m, int v) {
    CharLook l = look(v);
    float foot = 0.92f - KNEE_Y;
    mb_capsule(m, v3(0, 0, 0.005f), v3(0, -foot + 0.09f, 0), 0.078f, 0.06f, 12, l.pants);
    blob(m, v3(0, -foot + 0.05f, 0.045f), v3(0.068f, 0.05f, 0.135f), 0.45f, 0.6f, 12, 6, l.shoe);
    blob(m, v3(0, -foot + 0.012f, 0.045f), v3(0.071f, 0.012f, 0.138f), 0.3f, 0.6f, 12, 3, col(200, 200, 195));
}

/* arm: schouder op 0, elleboog op -ELBOW_Y; onderarm: elleboog op 0, hand rond -0.32 */
void model_char_uarm(MeshB *m, int v) {
    CharLook l = look(v);
    if (long_sleeves(v)) mb_capsule(m, v3(0, 0, 0), v3(0, -ELBOW_Y, 0), 0.075f, 0.062f, 10, l.shirt);
    else {
        mb_capsule(m, v3(0, 0, 0), v3(0, -0.15f, 0), 0.078f, 0.072f, 10, l.shirt);
        mb_capsule(m, v3(0, -0.12f, 0), v3(0, -ELBOW_Y, 0), 0.058f, 0.052f, 10, l.skin);
    }
}

void model_char_farm(MeshB *m, int v) {
    CharLook l = look(v);
    mb_capsule(m, v3(0, 0, 0), v3(0, -0.25f, 0), 0.052f, 0.042f, 10, long_sleeves(v) ? l.shirt : l.skin);
    if (long_sleeves(v)) mb_capsule(m, v3(0, -0.2f, 0), v3(0, -0.26f, 0), 0.044f, 0.04f, 8, l.skin);
    blob(m, v3(0, -0.31f, 0.005f), v3(0.04f, 0.06f, 0.03f), 0.7f, 0.7f, 10, 6, l.skin);
    mb_capsule(m, v3(0, -0.28f, 0.03f), v3(0, -0.33f, 0.055f), 0.016f, 0.014f, 6, l.skin);   /* duim */
    if (g_sqDetail > 2.0f) {   /* v6.2.0 ULTRA+: vier losse vingers met kootjes */
        int f;
        for (f = 0; f < 4; f++) {
            float x = -0.027f + f * 0.018f, len = f == 0 || f == 3 ? 0.05f : 0.062f;
            mb_capsule(m, v3(x, -0.35f, 0.006f), v3(x, -0.35f - len * 0.55f, 0.012f), 0.0095f, 0.0088f, 6, l.skin);
            mb_capsule(m, v3(x, -0.35f - len * 0.55f, 0.012f), v3(x, -0.35f - len, 0.022f), 0.0088f, 0.0078f, 6, l.skin);
        }
    }
}

/* ---------------------------------------------------------------- wapens (schoudercoordinaten, loop langs -y, greep naar +z) */

void model_weapon(MeshB *m, int w) {
    Col g = col(34, 34, 38), g2 = col(60, 60, 66), wood = col(120, 74, 40), metal = col(150, 150, 158);
    switch (w) {
    case 2:   /* SMG */
        blob(m, v3(0, -0.74f, -0.035f), v3(0.032f, 0.17f, 0.045f), 0.3f, 0.35f, 10, 8, g);
        mb_rtube(m, v3(0, -0.90f, -0.045f), v3(0, -1.03f, -0.045f), 0.014f, 0.014f, 8, metal);
        blob(m, v3(0, -0.80f, 0.06f), v3(0.018f, 0.025f, 0.075f), 0.3f, 0.4f, 8, 4, g2);       /* magazijn */
        blob(m, v3(0, -0.63f, 0.05f), v3(0.02f, 0.028f, 0.055f), 0.5f, 0.5f, 8, 4, g);         /* greep */
        mb_rtube(m, v3(0, -0.58f, -0.03f), v3(0, -0.44f, -0.02f), 0.012f, 0.012f, 6, metal);  /* kolf */
        blob(m, v3(0, -0.43f, -0.0f), v3(0.018f, 0.015f, 0.05f), 0.4f, 0.4f, 8, 4, g);
        blob(m, v3(0, -0.72f, -0.085f), v3(0.008f, 0.04f, 0.008f), 0.4f, 0.4f, 6, 3, g2);       /* vizier */
        break;
    case 3:   /* shotgun */
        mb_rtube(m, v3(0, -0.60f, -0.06f), v3(0, -1.28f, -0.06f), 0.019f, 0.019f, 10, g);
        mb_rtube(m, v3(0, -0.70f, -0.025f), v3(0, -1.18f, -0.025f), 0.015f, 0.015f, 8, g2);
        blob(m, v3(0, -0.98f, -0.025f), v3(0.028f, 0.11f, 0.028f), 0.6f, 0.6f, 10, 6, wood);   /* pomp */
        blob(m, v3(0, -0.66f, -0.045f), v3(0.03f, 0.09f, 0.045f), 0.3f, 0.35f, 10, 6, g);       /* kast */
        blob(m, v3(0, -0.60f, 0.045f), v3(0.022f, 0.03f, 0.06f), 0.5f, 0.5f, 8, 4, wood);       /* greep */
        {
            SQ s = sq(v3(0, -0.44f, -0.01f), v3(0.026f, 0.13f, 0.045f), 0.4f, 0.45f, wood);
            s.tz = 0.7f; s.shear = -0.02f; s.su = 10; s.sv = 6; mb_sq(m, &s);   /* kolf */
        }
        break;
    default:  /* pistool */
        blob(m, v3(0, -0.72f, -0.045f), v3(0.022f, 0.12f, 0.03f), 0.25f, 0.35f, 10, 6, g);     /* slede */
        blob(m, v3(0, -0.70f, -0.012f), v3(0.019f, 0.09f, 0.018f), 0.3f, 0.4f, 8, 4, g2);       /* frame */
        {
            SQ s = sq(v3(0, -0.615f, 0.045f), v3(0.021f, 0.03f, 0.06f), 0.5f, 0.45f, g);
            s.shear = 0.012f; s.su = 8; s.sv = 4; mb_sq(m, &s);   /* greep */
        }
        mb_rtube(m, v3(0, -0.84f, -0.045f), v3(0, -0.855f, -0.045f), 0.012f, 0.012f, 8, metal);
        mb_rtube(m, v3(0, -0.66f, 0.015f), v3(0, -0.70f, 0.035f), 0.006f, 0.006f, 4, g);       /* trekkerbeugel */
        break;
    }
}

/* ---------------------------------------------------------------- fiets */

/* v5.1.1 (Quest): 0 = hele fiets/motor, 1 = alles behalve de voorkant, 2 = alleen de voorkant
 * (voorwiel, vork en stuur), zodat die in VR mee kan draaien met het stuur */
int g_modelPart;
#define FRONT if (g_modelPart != 1)
#define BODY if (g_modelPart != 2)

void model_bike(MeshB *m, int v) {
    static const int frames[BIKE_VARIANTS][3] = {{210, 30, 36}, {30, 170, 190}, {240, 200, 40}, {235, 235, 235}};
    Col fc, dark = col(25, 25, 28), metal = col(178, 178, 184);
    float r = 0.34f;
    int i;
    V3 rear, front, crank, seat, head;
    v = ((v % BIKE_VARIANTS) + BIKE_VARIANTS) % BIKE_VARIANTS;
    fc = col(frames[v][0], frames[v][1], frames[v][2]);
    rear = v3(0, r, -0.52f); front = v3(0, r, 0.55f);
    crank = v3(0, 0.30f, -0.05f); seat = v3(0, 0.92f, -0.22f); head = v3(0, 0.88f, 0.42f);
    FRONT mb_wheel(m, front, r, 0.05f, dark, metal, 8);
    FRONT mb_rtube(m, v3(0.025f, 0.9f, 0.42f), v3(0.025f, r, 0.55f), 0.014f, 0.012f, 6, metal);
    FRONT mb_rtube(m, v3(-0.025f, 0.9f, 0.42f), v3(-0.025f, r, 0.55f), 0.014f, 0.012f, 6, metal);
    FRONT mb_rtube(m, head, v3(0, 1.04f, 0.38f), 0.018f, 0.018f, 6, metal);
    FRONT mb_capsule(m, v3(-0.27f, 1.05f, 0.36f), v3(0.27f, 1.05f, 0.36f), 0.016f, 0.016f, 8, metal);
    FRONT for (i = -1; i <= 1; i += 2) mb_capsule(m, v3(0.2f * i, 1.05f, 0.36f), v3(0.29f * i, 1.05f, 0.36f), 0.024f, 0.024f, 8, dark);  /* handvatten */
    FRONT blob(m, v3(0, 0.86f, 0.47f), v3(0.04f, 0.04f, 0.03f), 0.7f, 0.7f, 8, 4, col_e(255, 240, 200, 255));   /* lampje */
    if (g_modelPart == 2) return;
    mb_wheel(m, rear, r, 0.05f, dark, metal, 8);
    mb_rtube(m, v3(0.03f, r, -0.52f), v3(0.03f, 0.30f, -0.05f), 0.016f, 0.016f, 6, fc);
    mb_rtube(m, v3(-0.03f, r, -0.52f), v3(-0.03f, 0.30f, -0.05f), 0.016f, 0.016f, 6, fc);
    mb_rtube(m, v3(0.03f, r, -0.52f), seat, 0.015f, 0.015f, 6, fc);
    mb_rtube(m, v3(-0.03f, r, -0.52f), seat, 0.015f, 0.015f, 6, fc);
    mb_capsule(m, crank, seat, 0.026f, 0.024f, 8, fc);
    mb_capsule(m, crank, head, 0.03f, 0.028f, 8, fc);
    mb_capsule(m, v3(0, 0.84f, -0.2f), head, 0.024f, 0.024f, 8, fc);
    {
        SQ s = sq(v3(0, 0.965f, -0.24f), v3(0.075f, 0.03f, 0.13f), 0.5f, 0.6f, dark);
        s.bx = 1.0f; s.tx = 1.0f; s.bz = 1.0f; s.su = 12; s.sv = 4;
        mb_sq(m, &s);   /* zadel */
    }
    mb_rtube(m, seat, v3(0, 0.95f, -0.23f), 0.013f, 0.013f, 6, metal);
    mb_lathe(m, crank, 0, (const float[]){0, 0.09f, 0.09f, 0}, (const float[]){-0.03f, -0.03f, 0.03f, 0.03f},
             (const Col[]){metal, metal, metal, metal}, 4, 12);   /* kettingblad */
    mb_rtube(m, v3(0.06f, 0.30f, -0.05f), v3(0.06f, 0.18f, 0.05f), 0.012f, 0.012f, 4, metal);
    mb_rtube(m, v3(-0.06f, 0.30f, -0.05f), v3(-0.06f, 0.42f, -0.15f), 0.012f, 0.012f, 4, metal);
    blob(m, v3(0.11f, 0.18f, 0.05f), v3(0.05f, 0.012f, 0.03f), 0.4f, 0.4f, 6, 3, dark);
    blob(m, v3(-0.11f, 0.42f, -0.15f), v3(0.05f, 0.012f, 0.03f), 0.4f, 0.4f, 6, 3, dark);
    if (v == 0) {   /* bagagerek met "giveaway"-doosje */
        mb_rtube(m, v3(0, 0.62f, -0.52f), v3(0, 0.62f, -0.2f), 0.012f, 0.012f, 4, metal);
        blob(m, v3(0, 0.73f, -0.46f), v3(0.16f, 0.1f, 0.16f), 0.25f, 0.25f, 12, 4, col(232, 232, 226));
        blob(m, v3(0, 0.735f, -0.46f), v3(0.165f, 0.104f, 0.032f), 0.25f, 0.25f, 12, 4, col(200, 30, 30));
    }
}

/* ---------------------------------------------------------------- motor */

void model_moto(MeshB *m, int k) {
    static const int body[MOTO_MODELS][3] = {{30, 90, 200}, {210, 25, 30}};
    Col bc = col(body[k % MOTO_MODELS][0], body[k % MOTO_MODELS][1], body[k % MOTO_MODELS][2]);
    Col dark = col(22, 22, 25), metal = col(185, 185, 192), engine = col(70, 70, 78);
    float r = 0.33f;
    V3 rear = v3(0, r, -0.70f), front = v3(0, r, 0.72f), head = v3(0, 0.98f, 0.52f);
    SQ s;
    int i;
    FRONT mb_wheel(m, front, r, 0.13f, dark, metal, 5);
    FRONT mb_lathe(m, front, 0, (const float[]){0, 0.14f, 0.14f, 0}, (const float[]){0.07f, 0.07f, 0.08f, 0.08f},
             (const Col[]){metal, metal, metal, metal}, 4, 14);
    FRONT for (i = -1; i <= 1; i += 2) {
        mb_rtube(m, v3(0.075f * i, 1.0f, 0.5f), v3(0.075f * i, r, 0.72f), 0.03f, 0.026f, 8, metal);         /* voorvork */
        mb_capsule(m, v3(0.2f * i, 1.07f, 0.44f), v3(0.36f * i, 1.07f, 0.42f), 0.025f, 0.025f, 8, dark);   /* handvat */
    }
    FRONT mb_capsule(m, v3(-0.22f, 1.06f, 0.46f), v3(0.22f, 1.06f, 0.46f), 0.02f, 0.02f, 8, metal);   /* stuur */
    FRONT mb_capsule(m, head, v3(0, 1.06f, 0.46f), 0.035f, 0.03f, 8, dark);
    FRONT mb_lathe(m, v3(0, 0.93f, 0.6f), 2, (const float[]){0.075f, 0.08f, 0.06f, 0},
             (const float[]){-0.05f, 0.03f, 0.075f, 0.08f},
             (const Col[]){metal, metal, col_e(255, 245, 210, 255), col_e(255, 245, 210, 255)}, 4, 14);   /* koplamp */
    if (k != 1) {
        FRONT blob(m, v3(0, 0.98f, 0.36f), v3(0.08f, 0.05f, 0.06f), 0.6f, 0.6f, 10, 4, dark);   /* klokjes */
        FRONT blob(m, v3(0, 0.72f, 0.72f), v3(0.07f, 0.03f, 0.2f), 0.5f, 0.6f, 10, 4, bc);    /* spatbord */
    }
    if (g_modelPart == 2) return;
    mb_wheel(m, rear, r, 0.17f, dark, metal, 5);
    mb_lathe(m, rear, 0, (const float[]){0, 0.12f, 0.12f, 0}, (const float[]){0.09f, 0.09f, 0.1f, 0.1f},
             (const Col[]){metal, metal, metal, metal}, 4, 14);   /* remschijf */
    for (i = -1; i <= 1; i += 2) {
        mb_rtube(m, v3(0.075f * i, r, -0.70f), v3(0.075f * i, 0.42f, -0.18f), 0.03f, 0.035f, 8, engine);   /* achterbrug */
        mb_rtube(m, v3(0.09f * i, 0.55f, -0.62f), v3(0.13f * i, 0.32f, -0.68f), 0.025f, 0.025f, 6, metal); /* veerpoot */
    }
    /* motorblok met cilinders */
    blob(m, v3(0, 0.42f, -0.04f), v3(0.17f, 0.2f, 0.25f), 0.35f, 0.35f, 14, 6, engine);
    {
        static const float rp[5] = {0, 0.09f, 0.09f, 0.075f, 0}, dp[5] = {0, 0, 0.12f, 0.13f, 0.13f};
        static const float rn[5] = {0, 0.075f, 0.09f, 0.09f, 0}, dn[5] = {-0.13f, -0.13f, -0.12f, 0, 0};
        Col cp[5], cn[5];
        cp[0] = engine; cp[1] = cp[2] = cp[3] = cp[4] = metal;
        cn[0] = cn[1] = cn[2] = cn[3] = metal; cn[4] = engine;
        mb_lathe(m, v3(0.16f, 0.56f, 0.1f), 0, rp, dp, cp, 5, 12);
        mb_lathe(m, v3(-0.16f, 0.56f, 0.1f), 0, rn, dn, cn, 5, 12);
    }
    /* tank */
    s = sq(v3(0, 0.82f, 0.14f), v3(0.21f, 0.14f, 0.27f), 0.55f, 0.55f, bc);
    s.bx = 0.75f; s.tx = 1.0f; s.shear = 0.02f; s.su = 16; s.sv = 8; mb_sq(m, &s);
    blob(m, v3(0, 0.965f, 0.12f), v3(0.05f, 0.01f, 0.05f), 0.6f, 0.9f, 10, 3, metal);   /* tankdop */
    /* zadel en staart */
    s = sq(v3(0, 0.86f, -0.36f), v3(0.17f, 0.06f, 0.28f), 0.45f, 0.5f, dark);
    s.tz = 0.9f; s.su = 14; s.sv = 6; mb_sq(m, &s);
    s = sq(v3(0, 0.86f, -0.75f), v3(0.14f, 0.1f, 0.2f), 0.45f, 0.45f, bc);
    s.shear = -0.05f; s.tx = 0.75f; s.su = 14; s.sv = 6; mb_sq(m, &s);
    blob(m, v3(0, 0.85f, -0.95f), v3(0.08f, 0.03f, 0.02f), 0.5f, 0.6f, 10, 3, col_e(255, 40, 30, 255));   /* achterlicht */
    /* uitlaat */
    mb_capsule(m, v3(0.16f, 0.26f, 0.2f), v3(0.21f, 0.34f, -0.35f), 0.04f, 0.04f, 10, metal);
    mb_lathe(m, v3(0.22f, 0.42f, -0.62f), 2, (const float[]){0, 0.035f, 0.06f, 0.07f, 0.065f, 0},
             (const float[]){-0.2f, -0.2f, -0.18f, 0.1f, 0.28f, 0.3f},
             (const Col[]){dark, dark, metal, metal, metal, metal}, 6, 12);
    if (k == 1) {   /* superbike: kuip, ruitje en zijkuipen */
        s = sq(v3(0, 0.82f, 0.55f), v3(0.24f, 0.3f, 0.2f), 0.45f, 0.55f, bc);
        s.tx = 0.6f; s.shear = -0.12f; s.su = 16; s.sv = 8; mb_sq(m, &s);
        s = sq(v3(0, 1.14f, 0.44f), v3(0.15f, 0.08f, 0.04f), 0.5f, 0.6f, col(40, 55, 75));
        s.shear = -0.06f; s.su = 12; s.sv = 4; mb_sq(m, &s);
        for (i = -1; i <= 1; i += 2) blob(m, v3(0.19f * i, 0.5f, 0.1f), v3(0.05f, 0.18f, 0.32f), 0.4f, 0.5f, 10, 6, bc);
        blob(m, v3(0, 0.6f, 0.64f), v3(0.1f, 0.03f, 0.02f), 0.6f, 0.6f, 8, 3, col_e(255, 245, 210, 255));
    }
}
#undef FRONT
#undef BODY

/* ---------------------------------------------------------------- auto's */

static void car_lights(MeshB *m, float w, float y, float zf, float zr, float lw, float lh) {
    int k;
    for (k = -1; k <= 1; k += 2) {
        blob(m, v3((w - lw) * k, y, zf), v3(lw, lh, 0.05f), 0.5f, 0.5f, 10, 4, col_e(255, 245, 210, 255));
        blob(m, v3((w - lw) * k, y + 0.02f, zr), v3(lw * 1.05f, lh * 0.9f, 0.05f), 0.5f, 0.5f, 10, 4, col_e(230, 30, 30, 200));
    }
}

static void car_wheels(MeshB *m, float wx, float wzf, float wzr, float r, float rr, Col rim) {
    int a;
    for (a = -1; a <= 1; a += 2) {
        mb_wheel(m, v3(wx * a, r, wzf), r, 0.26f, col(22, 22, 25), rim, 5);
        mb_wheel(m, v3(wx * a, rr, wzr), rr, 0.3f, col(22, 22, 25), rim, 5);
    }
}

/* v5.5: deurnaden, handgrepen, sierlijst, ruitenwissers en antenne (op elke auto) */
static void car_extras(MeshB *m, float hw, float y, float zDoor0, float zDoor1, float zWind, float yWind, Col trim) {
    Col seam = col(18, 18, 20), chrome = col(200, 202, 208);
    int k;
    for (k = -1; k <= 1; k += 2) {
        float x = (hw + 0.006f) * k;
        mb_rtube(m, v3(x, y - 0.24f, zDoor0), v3(x, y + 0.27f, zDoor0 + 0.03f), 0.007f, 0.007f, 4, seam);
        mb_rtube(m, v3(x, y - 0.24f, zDoor1), v3(x, y + 0.27f, zDoor1 + 0.03f), 0.007f, 0.007f, 4, seam);
        blob(m, v3(x, y + 0.16f, zDoor0 - 0.24f), v3(0.012f, 0.018f, 0.07f), 0.5f, 0.5f, 6, 3, chrome);   /* handgrepen */
        blob(m, v3(x, y + 0.16f, zDoor1 - 0.24f), v3(0.012f, 0.018f, 0.07f), 0.5f, 0.5f, 6, 3, chrome);
        mb_rtube(m, v3(x, y - 0.06f, zDoor0 + 0.9f), v3(x, y - 0.06f, zDoor1 - 0.9f), 0.008f, 0.008f, 4, trim);   /* sierlijst */
        mb_rtube(m, v3(0.08f * k, yWind, zWind), v3(0.5f * k, yWind + 0.02f, zWind - 0.04f), 0.008f, 0.008f, 4, seam);   /* ruitenwissers */
    }
    mb_rtube(m, v3(-hw * 0.55f, y + 0.82f, zDoor1 - 0.2f), v3(-hw * 0.6f, y + 1.1f, zDoor1 - 0.32f), 0.005f, 0.004f, 4, seam);   /* antenne */
}

void model_car(MeshB *m, int v) {
    Col body = car_color(v), glass = col(40, 58, 82), dark = col(22, 22, 25), chrome = col(192, 194, 200);
    SQ s;
    int k;
    v = ((v % CAR_VARIANTS) + CAR_VARIANTS) % CAR_VARIANTS;
    m->ao = 0.3f;
    g_meshMat = MAT_PAINT;
    s = sq(v3(0, 0.64f, 0), v3(0.92f, 0.33f, 2.03f), 0.22f, 0.16f, body);
    s.tx = 0.95f; s.su = 28; s.sv = 8; mb_sq(m, &s);
    m->ao = 0;
    g_meshMat = MAT_GLASS;
    s = sq(v3(0, 1.17f, -0.18f), v3(0.84f, 0.27f, 0.98f), 0.3f, 0.26f, glass);
    s.tx = 0.84f; s.tz = 0.76f; s.su = 20; s.sv = 6; mb_sq(m, &s);
    g_meshMat = MAT_PAINT;
    s = sq(v3(0, 1.44f, -0.24f), v3(0.72f, 0.045f, 0.75f), 0.4f, 0.25f, body);
    s.su = 20; s.sv = 4; mb_sq(m, &s);
    g_meshMat = MAT_SHINY;
    for (k = -1; k <= 1; k += 2) {   /* stijlen tussen de ramen, spiegels */
        mb_capsule(m, v3(0.77f * k, 0.95f, 0.7f), v3(0.66f * k, 1.43f, 0.45f), 0.035f, 0.03f, 6, body);
        mb_capsule(m, v3(0.8f * k, 0.95f, -0.18f), v3(0.69f * k, 1.43f, -0.2f), 0.03f, 0.03f, 6, body);
        blob(m, v3(0.98f * k, 1.04f, 0.58f), v3(0.08f, 0.05f, 0.06f), 0.5f, 0.6f, 8, 4, body);
    }
    blob(m, v3(0, 0.43f, 2.0f), v3(0.9f, 0.1f, 0.1f), 0.4f, 0.3f, 18, 4, dark);
    blob(m, v3(0, 0.43f, -2.0f), v3(0.9f, 0.1f, 0.1f), 0.4f, 0.3f, 18, 4, dark);
    blob(m, v3(0, 0.64f, 2.03f), v3(0.38f, 0.07f, 0.03f), 0.3f, 0.3f, 10, 3, col(14, 14, 16));   /* grille */
    blob(m, v3(0, 0.62f, -2.04f), v3(0.2f, 0.06f, 0.015f), 0.3f, 0.3f, 8, 3, col(220, 210, 120));  /* kenteken */
    car_lights(m, 0.86f, 0.74f, 1.99f, -2.0f, 0.2f, 0.065f);
    car_wheels(m, 0.84f, 1.3f, -1.3f, 0.34f, 0.34f, chrome);
    car_extras(m, 0.9f, 0.66f, 0.72f, -0.2f, 0.78f, 0.98f, chrome);
    if (v == POLICE_VARIANT) {
        blob(m, v3(0, 0.72f, 0), v3(0.935f, 0.09f, 1.72f), 0.3f, 0.15f, 24, 4, col(30, 50, 140));
        blob(m, v3(0, 1.5f, -0.2f), v3(0.58f, 0.05f, 0.14f), 0.4f, 0.4f, 14, 3, col(30, 30, 34));
    }
    if (v == 2) blob(m, v3(0, 1.57f, -0.15f), v3(0.24f, 0.08f, 0.14f), 0.4f, 0.4f, 10, 4, col_e(255, 230, 120, 160));   /* taxi */
    g_meshMat = MAT_DEFAULT;
}

/* gekochte auto's: carrosserie wit (wordt gekleurd), details apart */
void model_car_body(MeshB *m, int model) {
    Col w = col(236, 236, 236);
    SQ s;
    int k;
    g_meshMat = MAT_PAINT;
    m->ao = 0.28f;
    switch (model) {
    case 1:   /* RHINO V8: lang, hoekig-rond, luchthapper */
        s = sq(v3(0, 0.62f, 0.02f), v3(0.95f, 0.31f, 2.3f), 0.2f, 0.14f, w); s.su = 28; s.sv = 8; mb_sq(m, &s);
        m->ao = 0;
        blob(m, v3(0, 0.95f, 1.1f), v3(0.33f, 0.07f, 0.42f), 0.35f, 0.35f, 14, 4, w);
        s = sq(v3(0, 1.39f, -0.5f), v3(0.78f, 0.04f, 0.62f), 0.4f, 0.25f, w); s.su = 20; s.sv = 4; mb_sq(m, &s);
        for (k = -1; k <= 1; k += 2) mb_capsule(m, v3(0.8f * k, 0.92f, 0.2f), v3(0.7f * k, 1.38f, -0.0f), 0.035f, 0.03f, 6, w);
        break;
    case 2:   /* VORTEX GT: laag, lange neus */
        s = sq(v3(0, 0.5f, 0.05f), v3(0.96f, 0.26f, 2.18f), 0.3f, 0.2f, w); s.su = 28; s.sv = 8; s.tz = 0.95f; mb_sq(m, &s);
        m->ao = 0;
        for (k = -1; k <= 1; k += 2) blob(m, v3(0.9f * k, 0.6f, -1.25f), v3(0.12f, 0.16f, 0.5f), 0.5f, 0.5f, 10, 4, w);   /* achterspatborden */
        s = sq(v3(0, 1.09f, -0.3f), v3(0.66f, 0.03f, 0.5f), 0.4f, 0.3f, w); s.su = 16; s.sv = 3; mb_sq(m, &s);
        break;
    case 3:   /* BOLT X: wig met ingebouwde vleugel */
        s = sq(v3(0, 0.45f, 0.02f), v3(1.0f, 0.24f, 2.28f), 0.25f, 0.18f, w); s.su = 28; s.sv = 8; s.tx = 0.92f; s.shear = 0.0f; mb_sq(m, &s);
        m->ao = 0;
        s = sq(v3(0, 0.6f, -1.2f), v3(0.95f, 0.14f, 0.9f), 0.4f, 0.3f, w); s.su = 20; s.sv = 4; mb_sq(m, &s);
        s = sq(v3(0, 1.0f, 0.1f), v3(0.62f, 0.025f, 0.45f), 0.4f, 0.3f, w); s.su = 16; s.sv = 3; mb_sq(m, &s);
        blob(m, v3(0, 1.06f, -2.0f), v3(0.98f, 0.03f, 0.2f), 0.4f, 0.3f, 18, 3, w);
        for (k = -1; k <= 1; k += 2) mb_rtube(m, v3(0.7f * k, 0.7f, -2.0f), v3(0.7f * k, 1.04f, -2.0f), 0.03f, 0.025f, 6, w);
        break;
    default:  /* CITY HATCH */
        s = sq(v3(0, 0.64f, 0), v3(0.9f, 0.32f, 2.0f), 0.22f, 0.16f, w); s.tx = 0.95f; s.su = 28; s.sv = 8; mb_sq(m, &s);
        m->ao = 0;
        s = sq(v3(0, 1.44f, -0.24f), v3(0.72f, 0.045f, 0.75f), 0.4f, 0.25f, w); s.su = 20; s.sv = 4; mb_sq(m, &s);
        for (k = -1; k <= 1; k += 2) {
            mb_capsule(m, v3(0.77f * k, 0.95f, 0.7f), v3(0.66f * k, 1.43f, 0.45f), 0.035f, 0.03f, 6, w);
            mb_capsule(m, v3(0.8f * k, 0.95f, -0.9f), v3(0.69f * k, 1.43f, -0.95f), 0.04f, 0.035f, 6, w);
        }
        break;
    }
    for (k = -1; k <= 1; k += 2) {   /* spiegels */
        float y = model == 2 ? 0.86f : (model == 3 ? 0.78f : 1.04f), z = model == 3 ? 0.55f : (model == 2 ? 0.45f : 0.58f);
        blob(m, v3(1.0f * k, y, z), v3(0.08f, 0.045f, 0.06f), 0.5f, 0.6f, 8, 4, w);
    }
}

void model_car_detail(MeshB *m, int model) {
    Col glass = col(36, 52, 74), dark = col(22, 22, 25), chrome = col(195, 197, 204), black = col(12, 12, 14);
    SQ s;
    int k;
    g_meshMat = MAT_SHINY;
    switch (model) {
    case 1:
        s = sq(v3(0, 1.13f, -0.48f), v3(0.84f, 0.24f, 0.8f), 0.3f, 0.26f, glass); s.tx = 0.9f; s.tz = 0.75f; s.su = 20; s.sv = 6; mb_sq(m, &s);
        blob(m, v3(0, 0.42f, 2.3f), v3(0.97f, 0.1f, 0.1f), 0.4f, 0.3f, 18, 4, chrome);
        blob(m, v3(0, 0.42f, -2.28f), v3(0.97f, 0.1f, 0.1f), 0.4f, 0.3f, 18, 4, chrome);
        blob(m, v3(0, 0.99f, 1.52f), v3(0.28f, 0.04f, 0.01f), 0.4f, 0.4f, 8, 3, black);
        blob(m, v3(0, 0.66f, 2.32f), v3(0.5f, 0.09f, 0.02f), 0.3f, 0.3f, 10, 3, black);
        car_lights(m, 0.9f, 0.7f, 2.3f, -2.27f, 0.17f, 0.07f);
        for (k = -1; k <= 1; k += 2) mb_rtube(m, v3(0.35f * k, 0.3f, -2.2f), v3(0.35f * k, 0.3f, -2.42f), 0.05f, 0.05f, 10, chrome);   /* uitlaten */
        car_wheels(m, 0.9f, 1.45f, -1.45f, 0.35f, 0.38f, chrome);
        car_extras(m, 0.93f, 0.62f, 0.6f, -0.45f, 0.72f, 0.95f, chrome);
        break;
    case 2:
        s = sq(v3(0, 0.88f, -0.3f), v3(0.8f, 0.22f, 0.86f), 0.3f, 0.3f, glass); s.tx = 0.82f; s.tz = 0.62f; s.shear = -0.12f; s.su = 20; s.sv = 6; mb_sq(m, &s);
        for (k = -1; k <= 1; k += 2) blob(m, v3(0.97f * k, 0.42f, -0.6f), v3(0.03f, 0.08f, 0.3f), 0.4f, 0.4f, 6, 4, black);   /* luchtinlaten */
        blob(m, v3(0, 0.33f, 2.2f), v3(0.85f, 0.04f, 0.08f), 0.4f, 0.3f, 14, 3, dark);
        car_lights(m, 0.88f, 0.55f, 2.17f, -2.1f, 0.18f, 0.05f);
        car_wheels(m, 0.9f, 1.4f, -1.35f, 0.34f, 0.35f, chrome);
        car_extras(m, 0.9f, 0.5f, 0.45f, -0.55f, 0.55f, 0.72f, dark);
        break;
    case 3:
        s = sq(v3(0, 0.8f, 0.15f), v3(0.74f, 0.2f, 0.8f), 0.3f, 0.3f, glass); s.tx = 0.82f; s.tz = 0.6f; s.shear = -0.15f; s.su = 20; s.sv = 6; mb_sq(m, &s);
        for (k = 0; k < 4; k++) blob(m, v3(0, 0.75f, -1.55f + k * 0.22f), v3(0.6f, 0.012f, 0.05f), 0.4f, 0.3f, 10, 3, black);   /* motorkap-lamellen */
        for (k = -1; k <= 1; k += 2) blob(m, v3(1.0f * k, 0.42f, -0.85f), v3(0.025f, 0.11f, 0.38f), 0.4f, 0.4f, 6, 4, black);
        blob(m, v3(0, 0.25f, 2.27f), v3(0.9f, 0.03f, 0.08f), 0.4f, 0.3f, 14, 3, dark);
        car_lights(m, 0.92f, 0.46f, 2.26f, -2.26f, 0.2f, 0.04f);
        car_wheels(m, 0.95f, 1.45f, -1.45f, 0.35f, 0.37f, chrome);
        car_extras(m, 0.95f, 0.45f, 0.75f, -0.35f, 0.85f, 0.66f, dark);
        break;
    default:
        s = sq(v3(0, 1.17f, -0.18f), v3(0.82f, 0.27f, 0.98f), 0.3f, 0.26f, glass); s.tx = 0.86f; s.tz = 0.76f; s.su = 20; s.sv = 6; mb_sq(m, &s);
        blob(m, v3(0, 0.43f, 2.0f), v3(0.9f, 0.1f, 0.1f), 0.4f, 0.3f, 18, 4, dark);
        blob(m, v3(0, 0.43f, -2.0f), v3(0.9f, 0.1f, 0.1f), 0.4f, 0.3f, 18, 4, dark);
        blob(m, v3(0, 0.64f, 2.03f), v3(0.38f, 0.07f, 0.03f), 0.3f, 0.3f, 10, 3, black);
        car_lights(m, 0.84f, 0.74f, 1.99f, -2.0f, 0.2f, 0.065f);
        car_wheels(m, 0.84f, 1.3f, -1.3f, 0.34f, 0.34f, chrome);
        break;
    }
}

void model_spoiler(MeshB *m) {
    Col w = col(236, 236, 236), dark = col(22, 22, 25);
    SQ s = sq(v3(0, 1.15f, -0.02f), v3(0.95f, 0.035f, 0.23f), 0.35f, 0.3f, w);
    int k;
    s.su = 18; s.sv = 4; mb_sq(m, &s);
    for (k = -1; k <= 1; k += 2) {
        mb_rtube(m, v3(0.55f * k, 0.0f, 0), v3(0.55f * k, 1.13f, 0), 0.04f, 0.035f, 8, dark);
        blob(m, v3(0.95f * k, 1.15f, -0.02f), v3(0.015f, 0.09f, 0.24f), 0.4f, 0.4f, 6, 4, dark);
    }
}
