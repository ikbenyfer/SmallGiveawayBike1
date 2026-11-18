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

enum { TOD_DUSK = 0, TOD_NIGHT = 1, TOD_DAY = 2 };

typedef struct {
    int W, H;
    GLuint progW, progU;
    GLint wVP, wModel, wSun, wSunCol, wAmb, wCam, wFog, wFogCol, wTint, wEmis, wAlpha, wUnlit;
    GLint uProj, uTex;

    GMesh body[CHAR_VARIANTS], leg[CHAR_VARIANTS], arm[CHAR_VARIANTS];
    GMesh bike[BIKE_VARIANTS], car[CAR_VARIANTS];
    GMesh cash, gift, cp, arrow, shadow, marker, siren;
    struct RWorld {
        GMesh ground;
        GMesh chunk[CHN * CHN];
        float chb[CHN * CHN][6];
        Tex minimap;
    } rw[2];
    int cur;   /* 0 = verzonnen stad, 1 = echte NL-kaart */

    Tex white, circle, glow, font, logo, bg;
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
/* align: 0 links, 1 midden, 2 rechts */
void ui_text(Renderer *r, const char *s, float x, float y, float size, float cr, float cg, float cb, float ca, int align);

#endif
