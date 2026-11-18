/* MeshBuilder: bouwt low-poly geometrie op de CPU (zonder OpenGL).
 * Vertex = positie (float3) + normaal (byte3) + kleur (rgb) + emissie (a). 20 bytes. */
#ifndef SGB_MESH_H
#define SGB_MESH_H

#include "mathx.h"

typedef struct {
    float x, y, z;
    int8_t nx, ny, nz, pad;
    uint8_t r, g, b, a; /* a = zelf-licht (ramen, lampen) */
} Vtx;

typedef struct { uint8_t r, g, b, a; } Col;

static inline Col col(int r, int g, int b) { Col c; c.r = (uint8_t)r; c.g = (uint8_t)g; c.b = (uint8_t)b; c.a = 0; return c; }
static inline Col col_e(int r, int g, int b, int e) { Col c = col(r, g, b); c.a = (uint8_t)e; return c; }
static inline Col col_mul(Col c, float f) {
    Col o = c;
    o.r = (uint8_t)clampf(c.r * f, 0, 255); o.g = (uint8_t)clampf(c.g * f, 0, 255); o.b = (uint8_t)clampf(c.b * f, 0, 255);
    return o;
}

typedef struct {
    Vtx *v;
    int n, cap;
    M4 xf;
    int has_xf;
    float ao; /* donkerder onderaan wanden (0 = uit) */
} MeshB;

void mb_init(MeshB *m);
void mb_free(MeshB *m);
void mb_reset(MeshB *m);
void mb_set_xf(MeshB *m, M4 xf);
void mb_clear_xf(MeshB *m);

/* Vierhoek; de draairichting wordt automatisch goed gezet t.o.v. normaal n. */
void mb_quad4(MeshB *m, V3 a, V3 b, V3 c, V3 d, V3 n, Col ca, Col cb, Col cc, Col cd);
void mb_quad(MeshB *m, V3 a, V3 b, V3 c, V3 d, V3 n, Col c0);
void mb_tri(MeshB *m, V3 a, V3 b, V3 c, V3 n, Col c0);

#define BOX_NOBOTTOM 1
#define BOX_NOTOP 2
/* As-uitgelijnde doos. top = kleur bovenkant, side = zijkanten. */
void mb_box2(MeshB *m, V3 mn, V3 mx, Col top, Col side, int flags);
void mb_box(MeshB *m, V3 mn, V3 mx, Col c, int flags);
/* Cilinder langs as (0=x, 1=y, 2=z) vanaf center c, straal r, lengte len (gecentreerd voor x/z, omhoog voor y). */
void mb_cyl(MeshB *m, V3 c, float r, float len, int seg, int axis, Col side, Col cap, int caps);
/* Kegel/piramide langs y. */
void mb_cone(MeshB *m, V3 base, float r, float h, int seg, Col c);
/* Zadeldak over rechthoek x0..x1, z0..z1 op hoogte y, nok langs x (ridge_x=1) of z. */
void mb_roof(MeshB *m, float x0, float z0, float x1, float z1, float y, float h, int ridge_x, Col roof, Col gable);
/* Plat vlak (bovenaanzicht) */
void mb_flat(MeshB *m, float x0, float z0, float x1, float z1, float y, Col c);
/* Schijf (plat, naar boven) */
void mb_disc(MeshB *m, V3 c, float r, int seg, Col col0);

#endif
