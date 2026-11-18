/* OpenGL ES 2.0 renderer: 3D-wereld, modellen en een 2D-sprite-batch voor menu's en HUD. */
#ifndef SGB_RENDER_H
#define SGB_RENDER_H

#include "gl_inc.h"
#include "platform.h"
#include "../Game/game.h"
#include "../Game/models.h"

typedef struct { GLuint vbo; int count; } GMesh;
typedef struct { GLuint id; int w, h; } Tex;

typedef struct {
    float x, y, u, v;
    uint8_t r, g, b, a;
} UIVtx;

#define UI_MAX 6000
#define RW_SLOTS 128

enum { TOD_DUSK = 0, TOD_NIGHT = 1, TOD_DAY = 2 };

typedef struct {
    int W, H;
    GLuint progW, progU;
    GLint wVP, wModel, wSun, wSunCol, wAmb, wCam, wFog, wFogCol, wTint, wEmis, wAlpha, wUnlit;
    GLint uProj, uTex;

    /* v3.0: gladde modellen dichtbij, de oude blokmodellen (Lo) ver weg */
    GMesh body[CHAR_VARIANTS], thigh[CHAR_VARIANTS], shin[CHAR_VARIANTS], uarm[CHAR_VARIANTS], farm[CHAR_VARIANTS];
    GMesh bodyLo[CHAR_VARIANTS], leg[CHAR_VARIANTS], arm[CHAR_VARIANTS];
    GMesh bike[BIKE_VARIANTS], car[CAR_VARIANTS], bikeLo[BIKE_VARIANTS], carLo[CAR_VARIANTS];
    GMesh cash, gift, cp, arrow, shadow, marker, siren, gun, tracer, spoiler, weapon[4];
    GMesh carBody[CAR_MODELS], carDetail[CAR_MODELS], moto[MOTO_MODELS];
    GMesh carBodyLo[CAR_MODELS], carDetailLo[CAR_MODELS], motoLo[MOTO_MODELS];
    GMesh homeExt[3], homeInt[3], homeGar[4];   /* eigen huis: buiten, binnen, ondergrondse garage */
    GMesh decor[3][5];                            /* luxe spullen uit BLING, per huisgrootte */
    struct RWorld {
        GMesh ground;
        GMesh chunk[RW_SLOTS];
        float chb[RW_SLOTS][6];
        Tex minimap;
    } rw[2];
    int cur;   /* 0 = verzonnen stad, 1 = echte NL-kaart */

    Tex white, circle, glow, font, logo, bg, icons;
    FontData fd;
    int logoX, logoY, logoW, logoH;   /* plaats van het uitgeknipte logo in de achtergrond */

    UIVtx ub[UI_MAX];
    int un;
    GLuint curTex;
    int additive;

    M4 vp, view, proj;
    V3 cam;
    float planes[6][4];
    int tod;
    float skyTop[3], skyHor[3], fogCol[3], sun[3], sunCol[3], amb[3], emis;

    int drawCalls, drawnChunks, drawnTris;
} Renderer;

int r_init(Renderer *r, int w, int h);          /* shaders, basistexturen, logo + achtergrond */
int r_load_font(Renderer *r);
void r_upload_world(Renderer *r, World *w, int slot); /* chunks naar VBO's, CPU-geheugen vrij */
void r_build_models(Renderer *r);
void r_slot_upload(Renderer *r, int world, int slot, MeshB *m);   /* tegel-deelmesh in slot */
void r_slot_clear(Renderer *r, int world, int slot);
void r_tex_upload(Tex *t, int w, int h, const uint8_t *rgba);
void r_set_tod(Renderer *r, int tod);
void r_draw_scene(Renderer *r, Game *g);
void r_resize(Renderer *r, int w, int h);

/* --- 2D --- */
void ui_begin(Renderer *r);
void ui_flush(Renderer *r);
void ui_additive(Renderer *r, int on);
void ui_quad(Renderer *r, const Tex *t, float x, float y, float w, float h, float u0, float v0, float u1, float v1, float cr, float cg, float cb, float ca);
void ui_quad4(Renderer *r, const Tex *t, const float *xy, const float *uv, float cr, float cg, float cb, float ca);
void ui_grad(Renderer *r, float x, float y, float w, float h, const float *top, const float *bot, float a);
void ui_rect(Renderer *r, float x, float y, float w, float h, float cr, float cg, float cb, float ca);
void ui_circle(Renderer *r, float x, float y, float rad, float cr, float cg, float cb, float ca);
void ui_ring(Renderer *r, float x, float y, float rad, float thick, float cr, float cg, float cb, float ca);
float ui_text_w(Renderer *r, const char *s, float size);
/* kaart-icoontje: 0 wapenwinkel, 1 bank, 2 dealer, 3 customs, 4 missie, 5 missie gehaald */
enum { ICON_GUN = 0, ICON_BANK, ICON_CAR, ICON_CUSTOMS, ICON_MISSION, ICON_DONE, ICON_HOME,
       ICON_BURGER, ICON_GYM, ICON_DRIP, ICON_BLING, ICON_SPRAY, ICON_COUNT };
void ui_icon(Renderer *r, int idx, float cx, float cy, float size, float alpha);
/* align: 0 links, 1 midden, 2 rechts */
void ui_text(Renderer *r, const char *s, float x, float y, float size, float cr, float cg, float cb, float ca, int align);

#endif
