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

/* v6.0 materialen (in Vtx.pad): de HQ-shader geeft elk materiaal eigen detail, glans en reflectie */
enum { MAT_DEFAULT = 0, MAT_ASPHALT, MAT_BRICK, MAT_GLASS, MAT_PAINT, MAT_SHINY, MAT_GRASS, MAT_CONCRETE, MAT_ROOF, MAT_WATER,
       MAT_SKIN, MAT_CLOTH, MAT_HAIR, MAT_RUBBER };   /* v6.2.0: huid, stof, haar, rubber */
extern int g_meshMat;
/* v6.2.0: automatisch materiaal kiezen op kleur: 1 = personage (huid/haar/stof/ogen), 2 = voertuig (banden = rubber) */
extern int g_autoMat;
extern Col g_amSkin, g_amHair;   /* materiaal voor alles wat nu gebouwd wordt */

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
/* v7.0.0: driehoek met eigen normaal en kleur per hoekpunt en een vast materiaal (gladde, organische modellen) */
void mb_tri_nm(MeshB *m, V3 a, V3 b, V3 c, V3 na, V3 nb, V3 nc, Col ca, Col cb, Col cc, int mat);

/* v7.0.0 PRO-engine (nieuwe nabewerking en hoge-detail mensen): alleen op de pc (Windows/Linux) */
#if !defined(__APPLE__) && !defined(__ANDROID__) && !defined(__SWITCH__)
#define SGB_PRO_OK 1
#else
#define SGB_PRO_OK 0
#endif

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

/* ---- gladde vormen (v3.0): eigen normaal per hoekpunt, dus zacht belicht in plaats van blokkig ---- */

/* Driehoek met normaal per hoek (de draairichting volgt de gemiddelde normaal). */
void mb_tri_n(MeshB *m, V3 a, V3 b, V3 c, V3 na, V3 nb, V3 nc, Col ca, Col cb, Col cc);

/* Superellipsoide: van bol (round 1) tot afgerond blok (round ~0.15).
 * c = midden, h = halve maten. Taper: x/z-schaal onderaan (bx,bz) en bovenaan (tx,tz).
 * shear: z-verschuiving bovenaan (+) en onderaan (-). part: 0 heel, 1 alleen bovenhelft, -1 onderhelft. */
typedef struct {
    V3 c, h;
    float roundV, roundH;
    int su, sv;
    float bx, bz, tx, tz, shear;
    int part;
    Col col;
} SQ;
SQ sq(V3 c, V3 h, float roundV, float roundH, Col col);   /* standaard 16x10 segmenten, geen taper */
void mb_sq(MeshB *m, const SQ *s);
extern float g_sqDetail;   /* v5.5: 1.0 normaal; >1 meer segmenten (hoge kwaliteit, bij het bouwen van de modellen) */

/* Ronde buis van a naar b (straal ra -> rb): capsule met bolle uiteinden, of open buis. */
void mb_capsule(MeshB *m, V3 a, V3 b, float ra, float rb, int seg, Col c);
void mb_rtube(MeshB *m, V3 a, V3 b, float ra, float rb, int seg, Col c);

/* Draaivorm: profiel (straal r[i], positie d[i] langs de as) rond as (0=x, 1=y, 2=z) door c. */
void mb_lathe(MeshB *m, V3 c, int axis, const float *r, const float *d, const Col *cols, int n, int seg);

/* Wiel langs de x-as: afgeronde band, velg, optioneel spaken. */
void mb_wheel(MeshB *m, V3 c, float r, float width, Col tire, Col rim, int spokes);

#endif
