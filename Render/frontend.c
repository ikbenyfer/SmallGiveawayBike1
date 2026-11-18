#define _POSIX_C_SOURCE 200809L
#include "frontend.h"
#include "render.h"
#include "../Game/nlmap.h"
#include <stdlib.h>

#include "nlstream.h"
#define NL_MAX_AREAS 64
#define NL_DL_R 2

typedef struct {
    char pc[8];
    int active, step, err, bagPdok, keep;
    int req[6], done[6], cached[6];
    char *buf[6]; long len[6];
    NLGeo geo;
    char msg[128];
    double t0;
    long bytes;
} NLJob;

typedef struct { char pc[8]; char name[96]; long kb; double rx, ry; } NLArea;
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

enum { ST_LOGO = 0, ST_LOAD, ST_MENU, ST_SETTINGS, ST_CREDITS, ST_GAME, ST_PAUSE, ST_MISSIONS, ST_NLINPUT, ST_NLLOAD, ST_BIGMAP, ST_SHOP, ST_DEALER, ST_CUSTOMS };
enum { CTL_NONE = 0, CTL_LSTICK, CTL_RSTICK, CTL_JUMP, CTL_ACTION, CTL_GAS, CTL_BRAKE, CTL_PAUSE, CTL_UI, CTL_FIRE, CTL_WSWITCH };

#define NTOUCH 10
#define STICK_R 78.0f

typedef struct { long id; int used, ctl; float x, y, sx, sy; } TSlot;

typedef struct { const char *name; float t0, t1; int frames; double time; float worst; long tris, dc; } Seg;

static struct {
    Renderer R;
    World W;
    Game G;
    int ready;
    int state, prevState;
    float st, t;
    double last;
    /* laden */
    int loadStep, loadTotal, loadDone;
    float loadShown;
    double loadStart, loadSecs;
    int tip;
    /* invoer */
    TSlot ts[NTOUCH];
    Input in;
    int jumpEdge, actionEdge, switchEdge;
    char shopMsg[96];
    float atMaxSpeed;
    float lbx, lby, lkx, lky, rbx, rby, rkx, rky;
    int lOn, rOn;
    /* instellingen */
    int quality, tod, camSpeed, invertY, showFps, resetArm;
    /* menu */
    int pressed;
    float fade, fadeDir;
    int fadeTarget;
    /* fps */
    float fpsAcc, fps, worst, worstShown;
    int fpsFrames;
    float perfT;
    int lastDC, skipStat, missionsFrom;
    /* echte NL-kaart */
    int mode;                 /* 0 = stad, 1 = NL */
    Game Gcity;
    World WNL;
    char pcIn[8]; int pcLen;
    int nlFrom, nlInFrom, nlPhase;
    NLJob job, dlgeo;
    NLArea areas[NL_MAX_AREAS]; int nAreas, areaPage, delArm;
    char queue[8][8]; int nQueue, dlPhase;
    double dlT0, nlT0;
    char dlLast[128], nlLabel[128];
    float nlTx, nlTz, nlLoadSecs, streetT, atNlT, atNlT2, atT3;
    char street[96];
    int atDlPhase;
    /* grote kaart */
    float bmCx, bmCz, bmScale, bmTx, bmTz, bmDownX, bmDownY, bmLastX, bmLastY;
    int bmTarget, bmDrag, bmTried, bmOk, bmW, bmH;
    double bmX0, bmY1, bmMpp;
    Tex bmTex;
    long savedMoney; int savedDone;
    /* stof */
    float dust[40][4];
    /* autotest */
    int at, atStep;
    float atT;
    Seg seg[16]; int nseg;
    char rep[6000]; int replen;
    int atBike, atCar;
    long atMoney0;
} F;

static const char *TIPS[] = {
    "TIP: TAP ENTER NEAR A BIKE OR CAR TO HOP ON",
    "TIP: YELLOW MARKERS ON THE MINIMAP START MISSIONS",
    "TIP: GREEN CASH STACKS ON THE SIDEWALK ARE FREE MONEY",
    "TIP: HOLD GAS AND STEER WITH THE LEFT STICK",
    "TIP: PEOPLE JUMP ASIDE - BUT PLEASE RIDE CAREFULLY",
    "TIP: LOWER THE GRAPHICS SETTING IF THE GAME FEELS SLOW",
    "TIP: THE GOLDEN BIKE STATUE STANDS IN THE CITY PLAZA",
};
#define NTIPS ((int)(sizeof(TIPS) / sizeof(TIPS[0])))

static void rep(const char *fmt, ...) {
    va_list ap;
    char line[256];
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    plat_log("SGB-TEST %s", line);
    if (F.replen + (int)strlen(line) + 2 < (int)sizeof(F.rep)) {
        memcpy(F.rep + F.replen, line, strlen(line));
        F.replen += (int)strlen(line);
        F.rep[F.replen++] = '\n';
        F.rep[F.replen] = 0;
    }
}

static void ui_star(float cx, float cy, float rad, float cr, float cg, float cb, float ca);
static void set_state(int s) { F.prevState = F.state; F.state = s; F.st = 0; F.pressed = 0; }

static void apply_settings(void) {
    static const float sens[3] = {0.6f, 1.0f, 1.5f};
    if (F.loadDone) {
        game_set_quality(&F.G, F.quality);
        F.G.camSens = sens[F.camSpeed % 3];
        F.G.invertY = F.invertY;
    }
    r_set_tod(&F.R, F.tod);
}

static void save_all(void) {
    int done = 0, i;
    plat_save_int("quality", F.quality);
    plat_save_int("tod", F.tod);
    plat_save_int("camspeed", F.camSpeed);
    plat_save_int("inverty", F.invertY);
    plat_save_int("showfps", F.showFps);
    if (F.loadDone && !F.at && F.mode == 0) {
        for (i = 0; i < NMISSIONS; i++) if (F.G.completed[i]) done |= 1 << i;
        plat_save_int("money", (int)F.G.money);
        plat_save_int("done", done);
        plat_save_int("guns", F.G.owned[1] | (F.G.owned[2] << 1) | (F.G.owned[3] << 2));
        plat_save_int("ammo1", F.G.ammo[1]); plat_save_int("ammo2", F.G.ammo[2]); plat_save_int("ammo3", F.G.ammo[3]);
        plat_save_int("armor", (int)F.G.armor);
        {
            int c;
            for (c = 0; c < MAX_OWNED; c++) {
                const struct GarageCar *gc = &F.G.garage[c];
                char key[16];
                snprintf(key, sizeof(key), "car%d", c);
                plat_save_int(key, gc->exists ? (1 | (gc->model << 1) | (gc->paint << 4) | (gc->eng << 9) | (gc->hand << 11) | (gc->neon << 13) | (gc->spoiler << 16)) : 0);
            }
        }
        F.savedMoney = F.G.money; F.savedDone = done;
    }
    plat_save_flush();
}

void fe_init(int w, int h) {
    int i;
    memset(&F, 0, sizeof(F));
    r_init(&F.R, w, h);
    F.quality = plat_load_int("quality", 1);
    F.tod = plat_load_int("tod", 0);
    F.camSpeed = plat_load_int("camspeed", 1);
    F.invertY = plat_load_int("inverty", 0);
    F.showFps = plat_load_int("showfps", 0);
    r_set_tod(&F.R, F.tod);
    world_init(&F.W);
    F.loadTotal = 1 + world_build_steps() + 3;
    F.at = plat_autotest_requested();
    for (i = 0; i < 40; i++) {
        F.dust[i][0] = (float)((i * 97) % 1024);
        F.dust[i][1] = (float)((i * 61) % 768);
        F.dust[i][2] = 6.0f + (i % 7) * 2.5f;
        F.dust[i][3] = 0.4f + (i % 5) * 0.15f;
    }
    F.tip = 0;
    F.ready = 1;
    F.state = ST_LOGO;
    F.lbx = 150; F.lby = h - 150;
    F.rbx = w - 170; F.rby = h - 150;
    if (F.at) plat_log("SGB-TEST autotest aangevraagd");
}

int fe_is_playing(void) { return F.state == ST_GAME; }

void fe_background(void) {
    if (!F.ready) return;
    save_all();
    if (F.state == ST_GAME && !F.at) set_state(ST_PAUSE);
}

/* ------------------------------------------------------------------ laden */

static void load_step(void) {
    int s = F.loadStep;
    int nb = world_build_steps();
    if (s == 0) r_load_font(&F.R);
    else if (s >= 1 && s <= nb) world_build_step(&F.W, s - 1);
    else if (s == nb + 1) r_upload_world(&F.R, &F.W, 0);
    else if (s == nb + 2) r_build_models(&F.R);
    else if (s == nb + 3) {
        int i, done;
        game_init(&F.G, &F.W);
        if (!F.at) {
            F.G.money = plat_load_int("money", 0);
            F.G.moneyShown = F.G.money;
            done = plat_load_int("done", 0);
            for (i = 0; i < NMISSIONS; i++) F.G.completed[i] = (done >> i) & 1;
            {
                int guns = plat_load_int("guns", 0);
                F.G.owned[1] = guns & 1; F.G.owned[2] = (guns >> 1) & 1; F.G.owned[3] = (guns >> 2) & 1;
                F.G.ammo[1] = plat_load_int("ammo1", 0); F.G.ammo[2] = plat_load_int("ammo2", 0); F.G.ammo[3] = plat_load_int("ammo3", 0);
                F.G.armor = (float)plat_load_int("armor", 0);
            }
            {
                int c;
                for (c = 0; c < MAX_OWNED; c++) {
                    char key[16];
                    int v;
                    struct GarageCar *gc = &F.G.garage[c];
                    snprintf(key, sizeof(key), "car%d", c);
                    v = plat_load_int(key, 0);
                    gc->exists = v & 1; gc->model = (v >> 1) & 7; gc->paint = (v >> 4) & 31; gc->eng = (v >> 9) & 3;
                    gc->hand = (v >> 11) & 3; gc->neon = (v >> 13) & 7; gc->spoiler = (v >> 16) & 1;
                    if (gc->model >= CAR_MODELS) gc->model = 0;
                    if (gc->paint >= NPAINTS) gc->paint = 0;
                    if (gc->neon >= NNEONS) gc->neon = 0;
                }
                game_spawn_owned(&F.G);
            }
        }
        F.loadDone = 1;
        apply_settings();
        F.loadSecs = (float)(plat_time() - F.loadStart);
        plat_log("SGB laden klaar: %.2f s, %d vertices in de stad, %d botsvormen, geheugen %ld kB",
                 F.loadSecs, F.W.verts_total, F.W.nboxes, plat_memory_kb());
    }
    F.loadStep++;
}

/* ------------------------------------------------------------------ tekenhulp */

static void draw_backdrop(float alpha, float logoA, float zoomT) {
    Renderer *R = &F.R;
    float s = (float)R->H / 768.0f * (1.0f + 0.025f * (0.5f - 0.5f * cosf(zoomT * 0.12f)));
    float bw = 1408.0f * s, bh = 768.0f * s;
    float bx = (R->W - bw) * 0.5f, by = (R->H - bh) * 0.5f;
    int i;
    if (alpha > 0) ui_quad(R, &R->bg, bx, by, bw, bh, 0, 0, 1, 1, 1, 1, 1, alpha);
    /* lamp-flikkering rechtsboven */
    if (alpha > 0) {
        float fl = 0.75f + 0.15f * sinf(F.t * 7.3f) + 0.1f * sinf(F.t * 23.1f);
        ui_additive(R, 1);
        ui_quad(R, &R->glow, bx + 1068 * s - 110, by + 86 * s - 110, 220, 220, 0, 0, 1, 1, 0.55f * fl * alpha, 0.32f * fl * alpha, 0.12f * fl * alpha, 0);
        /* zachte rode gloed achter het logo */
        {
            float p = 0.5f + 0.5f * sinf(F.t * 1.6f);
            float cx = bx + (R->logoX + R->logoW * 0.5f) * s, cy = by + (R->logoY + R->logoH * 0.5f) * s;
            ui_quad(R, &R->glow, cx - 520, cy - 330, 1040, 660, 0, 0, 1, 1, (0.10f + 0.07f * p) * alpha * logoA, 0.01f, 0.01f, 0);
        }
        ui_additive(R, 0);
    }
    if (logoA > 0) {
        float ls = 0.94f + 0.06f * logoA;
        float lw = R->logoW * s * ls, lh = R->logoH * s * ls;
        float cx = bx + (R->logoX + R->logoW * 0.5f) * s, cy = by + (R->logoY + R->logoH * 0.5f) * s;
        ui_quad(R, &R->logo, cx - lw * 0.5f, cy - lh * 0.5f, lw, lh, 0, 0, 1, 1, 1, 1, 1, logoA);
    }
    /* stofjes */
    if (alpha > 0) {
        for (i = 0; i < 40; i++) {
            float x = F.dust[i][0] + sinf(F.t * 0.4f + i) * 18.0f;
            float y = fmodf(F.dust[i][1] - F.t * F.dust[i][2] + 2000.0f, 800.0f) - 16.0f;
            float a = (0.10f + 0.12f * sinf(F.t * 1.3f + i * 1.7f)) * alpha;
            if (a > 0) ui_circle(R, x, y, 1.5f + F.dust[i][3] * 2.0f, 1.0f, 0.85f, 0.7f, a);
        }
    }
}

static void bottom_band(float a) {
    Renderer *R = &F.R;
    int k;
    for (k = 0; k < 10; k++) ui_rect(R, 0, R->H - 170.0f + k * 17.0f, (float)R->W, 17.0f, 0, 0, 0, a * (k + 1) * 0.07f);
}

static void big_button(const char *label, float cx, float cy, float size, int pressed, float a) {
    Renderer *R = &F.R;
    float w = ui_text_w(R, label, size);
    if (pressed) {
        ui_additive(R, 1);
        ui_quad(R, &R->glow, cx - w * 0.9f, cy - size * 1.2f, w * 1.8f, size * 2.4f, 0, 0, 1, 1, 0.55f * a, 0.05f * a, 0.05f * a, 0);
        ui_additive(R, 0);
    }
    ui_text(R, label, cx, cy - size * 0.62f, pressed ? size * 1.08f : size, 1, 1, 1, a, 1);
}

static int hit_text(float px, float py, const char *label, float cx, float cy, float size) {
    float w = ui_text_w(&F.R, label, size) * 0.5f + 30.0f;
    return px > cx - w && px < cx + w && py > cy - size * 0.9f && py < cy + size * 0.7f;
}

/* ------------------------------------------------------------------ schermen */

static void draw_logo_and_load(void) {
    Renderer *R = &F.R;
    float logoA, bgA;
    if (F.state == ST_LOGO) {
        logoA = smoothstepf(0.3f, 1.6f, F.st);
        bgA = smoothstepf(1.2f, 2.4f, F.st);
    } else { logoA = 1; bgA = 1; }
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 1);
    draw_backdrop(bgA, logoA, F.t);
    if (F.state == ST_LOAD) {
        float a = smoothstepf(0.0f, 0.5f, F.st);
        float bw = 560.0f, bx = (R->W - bw) * 0.5f, by = R->H - 62.0f;
        float p = F.loadShown;
        char pct[16];
        bottom_band(a);
        ui_rect(R, bx - 3, by - 3, bw + 6, 16, 0, 0, 0, 0.8f * a);
        ui_rect(R, bx, by, bw, 10, 0.18f, 0.06f, 0.06f, a);
        ui_rect(R, bx, by, bw * p, 10, 0.86f, 0.12f, 0.12f, a);
        ui_rect(R, bx, by, bw * p, 4, 1.0f, 0.55f, 0.5f, a * 0.8f);
        {
            /* glimmer die over de balk schuift */
            float gx = bx + fmodf(F.t * 260.0f, bw + 200.0f) - 100.0f;
            if (gx < bx + bw * p) {
                ui_additive(R, 1);
                ui_quad(R, &R->glow, gx - 50, by - 20, 100, 50, 0, 0, 1, 1, 0.5f * a, 0.3f * a, 0.3f * a, 0);
                ui_additive(R, 0);
            }
        }
        if (R->font.id) {
            float ta = a;
            snprintf(pct, sizeof(pct), "%d%%", (int)(p * 100.0f + 0.5f));
            ui_text(R, TIPS[F.tip % NTIPS], R->W * 0.5f, by - 46, 22, 0.92f, 0.9f, 0.86f, ta, 1);
            ui_text(R, pct, bx + bw + 16, by - 8, 20, 1, 0.8f, 0.8f, a, 0);
            ui_text(R, "LOADING", bx - 16, by - 8, 20, 1, 0.8f, 0.8f, a, 2);
        }
        /* draaiend wieltje links onder */
        {
            int k;
            for (k = 0; k < 8; k++) {
                float ang = F.t * 4.0f + k * (2 * PI_F / 8);
                float aa = a * (0.15f + 0.85f * (k / 7.0f));
                ui_circle(R, 60 + cosf(ang) * 18, R->H - 57 + sinf(ang) * 18, 4, 1, 1, 1, aa);
            }
            ui_ring(R, 60, R->H - 57, 25, 3, 1, 1, 1, a * 0.4f);
        }
    }
}

static const char *MENU_ITEMS[5] = {"PLAY", "MISSIONS", "REAL NL", "SETTINGS", "CREDITS"};
#define MENU_X(i) (R->W * 0.5f + ((i) - 2.0f) * 198.0f)

static void draw_menu(void) {
    Renderer *R = &F.R;
    float a = smoothstepf(0.0f, 0.6f, F.st);
    int i, done = 0;
    char line[64];
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 1);
    draw_backdrop(1, 1, F.t);
    bottom_band(1);
    for (i = 0; i < 5; i++) {
        float cx = MENU_X(i);
        float pulse = (i == 0) ? 0.04f * sinf(F.t * 3.0f) : 0;
        big_button(MENU_ITEMS[i], cx, R->H - 74.0f, 38.0f * (1.0f + pulse), F.pressed == i + 1, a);
    }
    for (i = 0; i < NMISSIONS; i++) done += F.G.completed[i];
    snprintf(line, sizeof(line), "$%ld   -   %d/%d MISSIONS", F.G.money, done, NMISSIONS);
    ui_text(R, line, 22, 18, 22, 1, 1, 1, a * 0.85f, 0);
    ui_text(R, "iOS LEGACY EDITION  v1.0", R->W - 20.0f, 18, 18, 0.85f, 0.85f, 0.85f, a * 0.7f, 2);
}

static const char *Q_NAMES[3] = {"LOW", "MEDIUM", "HIGH"};
static const char *T_NAMES[3] = {"DUSK", "NIGHT", "DAY"};
static const char *C_NAMES[3] = {"SLOW", "NORMAL", "FAST"};

static void draw_settings(void) {
    Renderer *R = &F.R;
    int i;
    const char *labels[6] = {"GRAPHICS", "TIME OF DAY", "CAMERA SPEED", "INVERT CAMERA Y", "SHOW FPS", "RESET PROGRESS"};
    const char *vals[6];
    vals[0] = Q_NAMES[F.quality % 3];
    vals[1] = T_NAMES[F.tod % 3];
    vals[2] = C_NAMES[F.camSpeed % 3];
    vals[3] = F.invertY ? "ON" : "OFF";
    vals[4] = F.showFps ? "ON" : "OFF";
    vals[5] = F.resetArm ? "TAP AGAIN!" : "";
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 1);
    draw_backdrop(0.35f, 0.0f, F.t);
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 0.45f);
    ui_text(R, "SETTINGS", R->W * 0.5f, 40, 60, 1, 1, 1, 1, 1);
    for (i = 0; i < 6; i++) {
        float y = 160.0f + i * 74.0f;
        int pr = F.pressed == i + 1;
        ui_rect(R, 180, y - 6, R->W - 360.0f, 60, pr ? 0.5f : 0.08f, 0.04f, 0.04f, 0.55f);
        ui_text(R, labels[i], 210, y, 30, 1, 1, 1, 1, 0);
        ui_text(R, vals[i], R->W - 210.0f, y, 30, 1.0f, 0.8f, 0.3f, 1, 2);
    }
    ui_text(R, "Graphics changes draw distance and the number of people/cars.", R->W * 0.5f, 612, 18, 0.8f, 0.8f, 0.8f, 1, 1);
    big_button("BACK", R->W * 0.5f, R->H - 74.0f, 48, F.pressed == 10, 1);
}

static void draw_credits(void) {
    Renderer *R = &F.R;
    static const char *L[] = {
        "SMALL GIVEAWAY BIKE 1", "iOS LEGACY EDITION", "",
        "A SMALL OPEN-WORLD CITY GAME", "MADE FOR THE iPAD 2 (iOS 9, ARMv7)", "",
        "IDEA, LOGO AND ART DIRECTION", "YF", "",
        "PROGRAMMING", "CUSTOM OPENGL ES 2.0 ENGINE IN C / OBJECTIVE-C", "WRITTEN WITH CLAUDE (ANTHROPIC)", "",
        "ALL BUILDINGS, PEOPLE AND VEHICLES", "ARE ORIGINAL LOW-POLY MODELS", "",
        "THANKS FOR PLAYING!", "", "TAP TO GO BACK"};
    int n = (int)(sizeof(L) / sizeof(L[0])), i;
    float y0 = R->H + 20.0f - F.st * 45.0f;
    if (y0 < 120) y0 = 120;
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 1);
    draw_backdrop(0.3f, 0.0f, F.t);
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 0.5f);
    for (i = 0; i < n; i++) {
        int head = (i == 0 || i == 6 || i == 9 || i == 13);
        ui_text(R, L[i], R->W * 0.5f, y0 + i * 34.0f, head ? 34 : 24, 1, head ? 1 : 0.85f, head ? 1 : 0.8f, 1, 1);
    }
}

/* ------------------------------------------------------------------ HUD */

static void fmt_money(char *b, size_t n, long m) {
    if (m >= 1000) snprintf(b, n, "$%ld,%03ld", m / 1000, m % 1000);
    else snprintf(b, n, "$%ld", m);
}

static void draw_minimap(void) {
    Renderer *R = &F.R;
    Game *g = &F.G;
    float mx = 18, my = 18, ms = 196, half = ms * 0.5f, cx = mx + half, cy = my + half;
    float span = F.G.w->mmSpan, org = F.G.w->mmOrg, orgZ = F.G.w->mmOrgZ, rad = 75.0f;
    V3 f = fwd_of(g->camYaw), rt = right_of(g->camYaw);
    float px = g->p.x, pz = g->p.z;
    float xy[8], uv[8];
    int k, i;
    static const float cs[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    ui_rect(R, mx - 4, my - 4, ms + 8, ms + 8, 0, 0, 0, 0.7f);
    for (k = 0; k < 4; k++) {
        float sx = cs[k][0], sy = cs[k][1];
        float wx = px + rt.x * sx * rad + f.x * (-sy) * rad;
        float wz = pz + rt.z * sx * rad + f.z * (-sy) * rad;
        xy[k * 2] = cx + sx * half; xy[k * 2 + 1] = cy + sy * half;
        uv[k * 2] = (wx - org) / span; uv[k * 2 + 1] = (wz - orgZ) / span;
    }
    ui_quad4(R, &R->rw[R->cur].minimap, xy, uv, 1, 1, 1, 1);
    /* fietsen in de buurt */
    for (i = 0; i < g->nveh; i++) {
        Vehicle *v = &g->veh[i];
        float rx, rz, sx, sy;
        if (!v->active || i == g->p.veh) continue;
        rx = v->x - px; rz = v->z - pz;
        sx = (rx * rt.x + rz * rt.z) / rad; sy = -(rx * f.x + rz * f.z) / rad;
        if (fabsf(sx) > 0.95f || fabsf(sy) > 0.95f) continue;
        if (v->police) {
            int ph = ((int)(F.t * 6.0f)) & 1;
            ui_circle(R, cx + sx * half, cy + sy * half, 6, ph ? 1.0f : 0.2f, 0.15f, ph ? 0.2f : 1.0f, 1);
            continue;
        }
        if (v->type == VEH_BIKE) ui_circle(R, cx + sx * half, cy + sy * half, 3.5f, 0.3f, 0.85f, 1.0f, 1);
        else if (!v->traffic) ui_rect(R, cx + sx * half - 2.5f, cy + sy * half - 2.5f, 5, 5, 0.9f, 0.9f, 0.9f, 0.7f);
    }
    {
        V3 tg[12];
        int n = game_targets(g, tg, 12);
        for (k = 0; k < n; k++) {
            float rx = tg[k].x - px, rz = tg[k].z - pz;
            float sx = (rx * rt.x + rz * rt.z) / rad, sy = -(rx * f.x + rz * f.z) / rad;
            float m = fmaxf(fabsf(sx), fabsf(sy));
            int done = g->mission < 0 && tg[k].y > 0.5f;
            if (m > 0.9f) { sx = sx / m * 0.9f; sy = sy / m * 0.9f; }
            if (g->mission < 0) ui_icon(R, done ? ICON_DONE : ICON_MISSION, cx + sx * half, cy + sy * half, 20, 1);
            else {
                ui_circle(R, cx + sx * half, cy + sy * half, 8, 0, 0, 0, 0.8f);
                ui_circle(R, cx + sx * half, cy + sy * half, 6, 1.0f, 0.82f, 0.15f, 1);
            }
        }
    }
    if (!g->nl) {
        V3 pts[4];
        int q;
        static const float bc[4][3] = {{0.95f, 0.15f, 0.15f}, {0.2f, 0.75f, 0.3f}, {1.0f, 0.55f, 0.1f}, {0.2f, 0.45f, 1.0f}};
        pts[0] = g->w->poi[POI_GUNSHOP];
        pts[1] = g->w->poi[POI_BANK];
        pts[2] = g->w->poi[POI_DEALER];
        pts[3] = g->w->poi[POI_CUSTOMS];
        for (q = 0; q < 4; q++) {
            float rx = pts[q].x - px, rz = pts[q].z - pz;
            float sx = (rx * rt.x + rz * rt.z) / rad, sy = -(rx * f.x + rz * f.z) / rad;
            float m = fmaxf(fabsf(sx), fabsf(sy));
            if (m > 0.9f) { sx = sx / m * 0.9f; sy = sy / m * 0.9f; }
            (void)bc;
            ui_icon(R, q == 0 ? ICON_GUN : (q == 1 ? ICON_BANK : (q == 2 ? ICON_CAR : ICON_CUSTOMS)), cx + sx * half, cy + sy * half, 24, 1);
        }
    }
    /* speler-pijl */
    {
        float a = -(g->p.h - g->camYaw);
        float fx = sinf(a), fy = -cosf(a);
        float rx2 = -fy, ry2 = fx;
        float t[8];
        float uvz[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
        t[0] = cx + fx * 11; t[1] = cy + fy * 11;
        t[2] = cx - fx * 7 + rx2 * 7; t[3] = cy - fy * 7 + ry2 * 7;
        t[4] = cx - fx * 3; t[5] = cy - fy * 3;
        t[6] = cx - fx * 7 - rx2 * 7; t[7] = cy - fy * 7 - ry2 * 7;
        ui_circle(R, cx, cy, 13, 0, 0, 0, 0.5f);
        ui_quad4(R, &R->white, t, uvz, 1, 1, 1, 1);
    }
}

static void draw_button(float x, float y, float r, const char *label, int down, int active, float lsize) {
    Renderer *R = &F.R;
    float a = active ? 1.0f : 0.45f;
    ui_circle(R, x, y, r, down ? 0.75f : 0.05f, down ? 0.1f : 0.05f, down ? 0.1f : 0.06f, 0.45f * a);
    ui_ring(R, x, y, r, 3.0f, 1, 1, 1, 0.75f * a);
    ui_text(R, label, x, y - lsize * 0.62f, lsize, 1, 1, 1, a, 1);
}

static int ctl_down(int ctl) {
    int i;
    for (i = 0; i < NTOUCH; i++) if (F.ts[i].used && F.ts[i].ctl == ctl) return 1;
    return 0;
}

/* knop-posities */
static void btn_pos(int ctl, float *x, float *y, float *r) {
    Renderer *R = &F.R;
    float W = (float)R->W, H = (float)R->H;
    int mode = F.loadDone ? game_mode(&F.G) : MODE_FOOT;
    *x = 0; *y = 0; *r = 0;
    if (ctl == CTL_PAUSE) { *x = W - 42; *y = 40; *r = 28; return; }
    if (mode == MODE_FOOT) {
        int armed = F.loadDone && game_has_weapon(&F.G);
        if (ctl == CTL_JUMP) { *x = W - 74; *y = H - 300; *r = 52; }
        if (ctl == CTL_ACTION) { *x = W - 196; *y = H - 330; *r = 48; }
        if (ctl == CTL_FIRE && armed) { *x = W - 74; *y = H - 430; *r = 54; }
        if (ctl == CTL_WSWITCH && armed) { *x = W - 200; *y = H - 455; *r = 36; }
    } else {
        if (ctl == CTL_GAS) { *x = W - 74; *y = H - 300; *r = 60; }
        if (ctl == CTL_BRAKE) { *x = W - 200; *y = H - 318; *r = 48; }
        if (ctl == CTL_ACTION) { *x = W - 120; *y = H - 440; *r = 40; }
    }
}

static void draw_hud(void) {
    Renderer *R = &F.R;
    Game *g = &F.G;
    float W = (float)R->W, H = (float)R->H;
    char b[96];
    int mode = game_mode(g);
    float x, y, r;

    draw_minimap();
    if (F.showFps) {
        snprintf(b, sizeof(b), "%.0f FPS  min %.0f  dc %d", F.fps, F.worstShown, R->drawCalls);
        ui_text(R, b, 20, 222, 18, 1, 1, 0.6f, 1, 0);
    }
    /* geld */
    fmt_money(b, sizeof(b), g->moneyShown);
    {
        float sz = 44.0f * (1.0f + 0.15f * clampf(g->moneyFlash, 0, 1));
        ui_text(R, b, W - 86, 16, sz, 0.55f, 1.0f, 0.55f, 1, 2);
    }
    /* gezondheid */
    {
        float hw = 210.0f, hx = W - 86 - hw, hy = 70.0f, hp = clampf(g->health / 100.0f, 0, 1);
        float low = hp < 0.3f ? 0.5f + 0.5f * sinf(F.t * 10.0f) : 1.0f;
        ui_rect(R, hx - 3, hy - 3, hw + 6, 18, 0, 0, 0, 0.7f);
        ui_rect(R, hx, hy, hw, 12, 0.25f, 0.05f, 0.05f, 0.9f);
        ui_rect(R, hx, hy, hw * hp, 12, (1.0f - hp) * 0.9f + 0.1f, 0.25f + hp * 0.65f, 0.2f, low);
        ui_text(R, "+", hx - 8, hy - 9, 26, 1, 0.35f, 0.35f, 1, 2);
    }
    if (g->armor > 0.5f) {
        float hw = 210.0f, hx = W - 86 - hw;
        ui_rect(R, hx, 85, hw * clampf(g->armor / 100.0f, 0, 1), 5, 0.35f, 0.65f, 1.0f, 0.95f);
    }
    if (g->weapon != WP_NONE && game_mode(g) == MODE_FOOT) {
        snprintf(b, sizeof(b), "%s  %d", WEAPONS[g->weapon].name, g->ammo[g->weapon]);
        ui_text(R, b, W - 86, 128, 24, g->ammo[g->weapon] > 0 ? 1.0f : 1.0f, g->ammo[g->weapon] > 0 ? 1.0f : 0.4f, g->ammo[g->weapon] > 0 ? 1.0f : 0.4f, 1, 2);
    }
    if (g->mission >= 0 && g->mdef[g->mission].cp[g->mstep < g->mdef[g->mission].ncp ? g->mstep : 0].hold > 0 && g->holdT > 0) {
        const Checkpoint *c = &g->mdef[g->mission].cp[g->mstep];
        ui_rect(R, W * 0.5f - 200, 124, 400, 22, 0, 0, 0, 0.7f);
        ui_rect(R, W * 0.5f - 197, 127, 394.0f * clampf(g->holdT / c->hold, 0, 1), 16, 1.0f, 0.75f, 0.2f, 1);
        ui_text(R, "DRILLING THE VAULT", W * 0.5f, 150, 20, 1, 0.85f, 0.4f, 1, 1);
    }
    /* gezocht-sterren */
    if (g->wanted > 0) {
        int k;
        float blink = g->evadeT > 0.5f ? 0.45f + 0.55f * (0.5f + 0.5f * sinf(F.t * 8.0f)) : 1.0f;
        for (k = 0; k < 3; k++) {
            float sx = W - 100 - (2 - k) * 36.0f, sy = 108.0f;
            ui_star(sx, sy, 17, 0, 0, 0, 0.7f);
            if (k < g->wanted) ui_star(sx, sy, 14, 1.0f, 0.82f, 0.15f, blink);
            else ui_star(sx, sy, 14, 0.3f, 0.3f, 0.3f, 0.6f);
        }
        if (g->bustT > 0.2f) ui_text(R, "GETTING BUSTED!", W * 0.5f, H * 0.62f, 34, 1.0f, 0.3f, 0.3f, 0.6f + 0.4f * sinf(F.t * 12.0f), 1);
    }
    if (g->hurtFlash > 0) {
        float a = clampf(g->hurtFlash, 0, 0.6f) * 0.6f;
        ui_rect(R, 0, 0, W, 24, 0.9f, 0, 0, a);
        ui_rect(R, 0, H - 24, W, 24, 0.9f, 0, 0, a);
        ui_rect(R, 0, 24, 24, H - 48, 0.9f, 0, 0, a);
        ui_rect(R, W - 24, 24, 24, H - 48, 0.9f, 0, 0, a);
    }
    /* pauzeknop */
    btn_pos(CTL_PAUSE, &x, &y, &r);
    ui_circle(R, x, y, r, 0, 0, 0, 0.45f);
    ui_rect(R, x - 9, y - 11, 6, 22, 1, 1, 1, 0.9f);
    ui_rect(R, x + 3, y - 11, 6, 22, 1, 1, 1, 0.9f);

    if (F.mode == 1) {
        F.streetT -= 1.0f / 30.0f;
        if (F.streetT <= 0) {
            const char *sn = nl_street_name(g->w, g->p.x, g->p.z);
            snprintf(F.street, sizeof(F.street), "%s", sn ? sn : "");
            F.streetT = 0.4f;
        }
        if (F.street[0]) {
            float tw = ui_text_w(R, F.street, 30);
            ui_rect(R, W * 0.5f - tw * 0.5f - 20, 12, tw + 40, 48, 0, 0, 0, 0.55f);
            ui_text(R, F.street, W * 0.5f, 18, 30, 1, 1, 1, 1, 1);
        }
        if (!ns_ready_at(g->p.x, g->p.z)) ui_text(R, "LOADING MAP...", W * 0.5f, 70, 24, 1, 0.85f, 0.4f, 0.6f + 0.4f * sinf(F.t * 6), 1);
        else if (ns_offline_hole(g->p.x, g->p.z)) ui_text(R, "NO MAP DATA HERE (offline / not downloaded)", W * 0.5f, 70, 20, 1, 0.5f, 0.5f, 1, 1);
        ui_text(R, "3D BAG + PDOK (CC BY 4.0)", 20, 246, 14, 0.9f, 0.9f, 0.9f, 0.8f, 0);
    }
    ui_text(R, "TAP MAP", 116, 222, 14, 1, 1, 1, 0.55f, 1);
    /* missie-paneel */
    if (g->mission >= 0) {
        const MissionDef *m = &g->mdef[g->mission];
        const char *obj = "";
        float dist = -1;
        V3 tg[12];
        int n = game_targets(g, tg, 12), k;
        if (m->escape) obj = "Lose the police! Stay far away from them.";
        else if (g->mstep < m->anyOrder) obj = m->cp[0].text;
        else if (g->mstep < m->ncp) obj = m->cp[g->mstep].text;
        for (k = 0; k < n; k++) {
            float d = sqrtf((tg[k].x - g->p.x) * (tg[k].x - g->p.x) + (tg[k].z - g->p.z) * (tg[k].z - g->p.z));
            if (dist < 0 || d < dist) dist = d;
        }
        ui_rect(R, W * 0.5f - 250, 10, 500, m->time > 0 ? 104 : 80, 0, 0, 0, 0.5f);
        ui_text(R, m->title, W * 0.5f, 14, 22, 1.0f, 0.82f, 0.2f, 1, 1);
        ui_text(R, obj, W * 0.5f, 40, 26, 1, 1, 1, 1, 1);
        if (m->time > 0) {
            int s = (int)ceilf(g->mtime);
            snprintf(b, sizeof(b), "%d:%02d   %.0f m", s / 60, s % 60, dist);
            ui_text(R, b, W * 0.5f, 72, 26, g->mtime < 15 ? 1.0f : 0.9f, g->mtime < 15 ? 0.3f : 0.9f, g->mtime < 15 ? 0.3f : 0.9f, 1, 1);
        } else {
            if (dist >= 0) snprintf(b, sizeof(b), "%.0f m", dist);
            else b[0] = 0;
            ui_text(R, b, W * 0.5f + 230, 14, 18, 0.9f, 0.9f, 0.9f, 1, 2);
        }
    }
    /* banner */
    if (g->bannerT > 0) {
        float a = clampf(g->bannerT / 0.5f, 0, 1) * clampf((4.5f - g->bannerT) / 0.3f + 0.2f, 0, 1);
        ui_rect(R, 0, H * 0.30f, W, 130, 0, 0, 0, 0.55f * a);
        ui_text(R, g->banner, W * 0.5f, H * 0.30f + 10, 60, g->bannerGood ? 1.0f : 1.0f, g->bannerGood ? 0.85f : 0.3f, g->bannerGood ? 0.25f : 0.3f, a, 1);
        ui_text(R, g->sub, W * 0.5f, H * 0.30f + 84, 26, 1, 1, 1, a, 1);
    }
    if (g->msgT > 0) {
        float a = clampf(g->msgT / 0.3f, 0, 1);
        ui_text(R, g->msg, W * 0.5f, H - 230, 30, 1, 1, 1, a, 1);
    }

    /* sticks */
    ui_circle(R, F.lbx, F.lby, STICK_R, 0, 0, 0, F.lOn ? 0.35f : 0.22f);
    ui_ring(R, F.lbx, F.lby, STICK_R, 2.5f, 1, 1, 1, 0.5f);
    ui_circle(R, F.lOn ? F.lkx : F.lbx, F.lOn ? F.lky : F.lby, 36, 1, 1, 1, F.lOn ? 0.6f : 0.35f);
    ui_text(R, mode == MODE_FOOT ? "MOVE" : "STEER", F.lbx, F.lby + STICK_R + 2, 16, 1, 1, 1, 0.6f, 1);
    ui_circle(R, F.rbx, F.rby, STICK_R, 0, 0, 0, F.rOn ? 0.35f : 0.22f);
    ui_ring(R, F.rbx, F.rby, STICK_R, 2.5f, 1, 1, 1, 0.5f);
    ui_circle(R, F.rOn ? F.rkx : F.rbx, F.rOn ? F.rky : F.rby, 36, 1, 1, 1, F.rOn ? 0.6f : 0.35f);
    ui_text(R, "CAMERA", F.rbx, F.rby + STICK_R + 2, 16, 1, 1, 1, 0.6f, 1);

    if (mode == MODE_FOOT) {
        int near = game_near_vehicle(g);
        const char *lab = "ENTER";
        if (near >= 0) lab = g->veh[near].type == VEH_BIKE ? "RIDE" : "DRIVE";
        btn_pos(CTL_JUMP, &x, &y, &r); draw_button(x, y, r, "JUMP", ctl_down(CTL_JUMP), 1, 24);
        if (game_has_weapon(g)) {
            btn_pos(CTL_FIRE, &x, &y, &r);
            ui_circle(R, x, y, r, ctl_down(CTL_FIRE) ? 0.95f : 0.6f, 0.08f, 0.08f, 0.55f);
            ui_ring(R, x, y, r, 3.0f, 1, 1, 1, 0.8f);
            ui_text(R, "FIRE", x, y - 15, 26, 1, 1, 1, 1, 1);
            btn_pos(CTL_WSWITCH, &x, &y, &r);
            draw_button(x, y, r, "GUN", ctl_down(CTL_WSWITCH), 1, 18);
        }
        btn_pos(CTL_ACTION, &x, &y, &r);
        if (near >= 0) {
            float p = 0.5f + 0.5f * sinf(F.t * 6.0f);
            ui_additive(R, 1);
            ui_quad(R, &R->glow, x - r * 1.8f, y - r * 1.8f, r * 3.6f, r * 3.6f, 0, 0, 1, 1, 0.5f * p, 0.35f * p, 0.05f, 0);
            ui_additive(R, 0);
        }
        draw_button(x, y, r, lab, ctl_down(CTL_ACTION), near >= 0, 22);
    } else {
        btn_pos(CTL_GAS, &x, &y, &r); draw_button(x, y, r, "GAS", ctl_down(CTL_GAS), 1, 28);
        btn_pos(CTL_BRAKE, &x, &y, &r); draw_button(x, y, r, "BRAKE", ctl_down(CTL_BRAKE), 1, 20);
        btn_pos(CTL_ACTION, &x, &y, &r); draw_button(x, y, r, "EXIT", ctl_down(CTL_ACTION), 1, 20);
        {
            Vehicle *v = &g->veh[g->p.veh];
            snprintf(b, sizeof(b), "%d KM/H", (int)(fabsf(v->speed) * 3.6f + 0.5f));
            ui_text(R, b, F.rbx, F.rby - STICK_R - 34, 24, 1, 1, 1, 0.85f, 1);
        }
    }
}

static void draw_missions(void) {
    Renderer *R = &F.R;
    int i;
    char b[32];
    if (F.missionsFrom == ST_PAUSE) {
        r_draw_scene(R, &F.G);
        ui_begin(R);
        ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 0.75f);
    } else {
        ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 1);
        draw_backdrop(0.35f, 0.0f, F.t);
        ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 0.5f);
    }
    ui_text(R, "MISSIONS", R->W * 0.5f, 18, 52, 1, 1, 1, 1, 1);
    for (i = 0; i < NMISSIONS; i++) {
        const MissionDef *m = &F.G.mdef[i];
        float x = (i & 1) ? 522.0f : 62.0f, y = 96.0f + (i / 2) * 98.0f;
        int pr = F.pressed == i + 1, done = F.G.completed[i];
        ui_rect(R, x, y, 440, 90, pr ? 0.55f : 0.06f, pr ? 0.08f : 0.05f, pr ? 0.08f : 0.06f, 0.78f);
        ui_rect(R, x, y, 6, 90, done ? 0.35f : 0.95f, done ? 0.9f : 0.75f, done ? 0.4f : 0.15f, 1);
        snprintf(b, sizeof(b), "%d. %s", i + 1, m->title);
        ui_text(R, b, x + 18, y + 6, 30, 1, 1, 1, 1, 0);
        {
            float bw = ui_text_w(R, m->brief, 15.0f), bs = bw > 404.0f ? 15.0f * 404.0f / bw : 15.0f;
            ui_text(R, m->brief, x + 18, y + 44 + (15.0f - bs) * 0.5f, bs, 0.82f, 0.82f, 0.82f, 1, 0);
        }
        snprintf(b, sizeof(b), "$%d", done ? m->reward / 2 : m->reward);
        ui_text(R, b, x + 426, y + 8, 26, 0.55f, 1.0f, 0.55f, 1, 2);
        ui_text(R, m->tag ? m->tag : "", x + 426, y + 66, 17, 1.0f, 0.8f, 0.3f, 1, 2);
        if (done) ui_text(R, "DONE", x + 18, y + 66, 17, 0.4f, 1.0f, 0.45f, 1, 0);
        else if (m->time > 0) {
            snprintf(b, sizeof(b), "TIME %d:%02d", (int)m->time / 60, (int)m->time % 60);
            ui_text(R, b, x + 18, y + 66, 17, 0.9f, 0.9f, 0.9f, 1, 0);
        }
    }
    big_button("BACK", R->W * 0.5f, R->H - 60.0f, 44, F.pressed == 100, 1);
}

static void ui_star(float cx, float cy, float rad, float cr, float cg, float cb, float ca) {
    Renderer *R = &F.R;
    float uv[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    int k;
    for (k = 0; k < 5; k++) {
        float a0 = -PI_F * 0.5f + k * (2 * PI_F / 5);
        float a1 = a0 + PI_F / 5, am = a0 - PI_F / 5;
        float ri = rad * 0.45f;
        float xy[8];
        xy[0] = cx; xy[1] = cy;
        xy[2] = cx + cosf(am) * ri; xy[3] = cy + sinf(am) * ri;
        xy[4] = cx + cosf(a0) * rad; xy[5] = cy + sinf(a0) * rad;
        xy[6] = cx + cosf(a1) * ri; xy[7] = cy + sinf(a1) * ri;
        ui_quad4(R, &R->white, xy, uv, cr, cg, cb, ca);
    }
}

/* ------------------------------------------------------------------ echte NL-kaart: tegels, downloaden, grote kaart
 * Spelen gebeurt met tegels van 250 m (Render/nlstream.c). Heemskerk + Beverwijk zitten ingebouwd (offline);
 * elders komen tegels uit de downloads (offline) of van internet, en worden onderweg bijgeladen. */

/* ---- postcode opzoeken: ingebouwd pakket -> opgeslagen -> PDOK */
static void geo_begin(NLJob *J, const char *pc) {
    char name[64];
    char *d = NULL;
    long l = 0;
    memset(J, 0, sizeof(*J));
    snprintf(J->pc, sizeof(J->pc), "%s", pc);
    J->active = 1;
    J->t0 = plat_time();
    if (ns_pack_postcode(pc, &J->geo.cx, &J->geo.cy)) {
        snprintf(J->geo.name, sizeof(J->geo.name), "%.4s %s (built-in map)", pc, pc + 4);
        J->step = 3; J->cached[0] = 1;
        return;
    }
    snprintf(name, sizeof(name), "nl/geo_%s.json", pc);
    if (plat_read_file(name, &d, &l)) {
        int ok = nl_parse_geocode(d, l, &J->geo);
        free(d);
        if (ok) { J->step = 3; J->cached[0] = 1; return; }
    }
    {
        char url[400];
        nl_url_geocode(url, sizeof(url), pc);
        J->req[0] = plat_http_start(url);
        J->step = 1;
    }
}

/* 1 = klaar (ok of fout in J->err) */
static int geo_tick(NLJob *J) {
    char *buf = NULL;
    long len = 0;
    int r;
    if (!J->active) return 1;
    if (J->step >= 3) { J->active = 0; return 1; }
    r = plat_http_poll(J->req[0], &buf, &len);
    if (r == 0) return 0;
    J->active = 0;
    if (r < 0) {
        snprintf(J->msg, sizeof(J->msg), "No internet: postcode %s is not built in and not downloaded.", J->pc);
        J->err = 1;
        return 1;
    }
    {
        char *copy = (char *)malloc((size_t)len + 1);
        int ok;
        memcpy(copy, buf, (size_t)len + 1);
        ok = nl_parse_geocode(copy, len, &J->geo);
        free(copy);
        if (ok) {
            char name[64];
            snprintf(name, sizeof(name), "nl/geo_%s.json", J->pc);
            plat_write_file(name, buf, len);
            J->step = 3;
        } else {
            snprintf(J->msg, sizeof(J->msg), "Postcode %s not found.", J->pc);
            J->err = 1;
        }
    }
    free(buf);
    return 1;
}

/* ---- lijst met gedownloade gebieden (Documents/nl/areas.txt): pc  kb  rx  ry  naam */

static void idx_save(void) {
    char buf[NL_MAX_AREAS * 200];
    int i, n = 0;
    buf[0] = 0;
    for (i = 0; i < F.nAreas; i++)
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s\t%ld\t%.0f\t%.0f\t%s\n", F.areas[i].pc, F.areas[i].kb, F.areas[i].rx, F.areas[i].ry, F.areas[i].name);
    plat_write_file("nl/areas.txt", buf, (long)n);
}

static void idx_load(void) {
    char *d = NULL, *line, *save = NULL;
    long len = 0;
    F.nAreas = 0;
    if (!plat_read_file("nl/areas.txt", &d, &len)) return;
    for (line = strtok_r(d, "\n", &save); line && F.nAreas < NL_MAX_AREAS; line = strtok_r(NULL, "\n", &save)) {
        NLArea *a = &F.areas[F.nAreas];
        char *f[5];
        int k = 0;
        char *p = line;
        f[k++] = p;
        while (k < 5 && (p = strchr(p, '\t')) != NULL) { *p++ = 0; f[k++] = p; }
        if (k < 5 || strlen(f[0]) != 6) continue;
        snprintf(a->pc, sizeof(a->pc), "%s", f[0]);
        a->kb = atol(f[1]); a->rx = atof(f[2]); a->ry = atof(f[3]);
        snprintf(a->name, sizeof(a->name), "%s", f[4]);
        F.nAreas++;
    }
    free(d);
}

static int idx_find(const char *pc) {
    int i;
    for (i = 0; i < F.nAreas; i++) if (strcmp(F.areas[i].pc, pc) == 0) return i;
    return -1;
}

static void idx_add(const char *pc, const char *name, double rx, double ry, long kb) {
    int i = idx_find(pc);
    NLArea a;
    memset(&a, 0, sizeof(a));
    snprintf(a.pc, sizeof(a.pc), "%s", pc);
    snprintf(a.name, sizeof(a.name), "%s", name);
    a.rx = rx; a.ry = ry; a.kb = kb;
    if (i >= 0) memmove(&F.areas[1], &F.areas[0], sizeof(NLArea) * (size_t)i);
    else {
        if (F.nAreas >= NL_MAX_AREAS) F.nAreas = NL_MAX_AREAS - 1;
        memmove(&F.areas[1], &F.areas[0], sizeof(NLArea) * (size_t)F.nAreas);
        F.nAreas++;
    }
    F.areas[0] = a;
    idx_save();
}

static void idx_delete(int i) {
    int dx, dy, k;
    NLArea a;
    if (i < 0 || i >= F.nAreas) return;
    a = F.areas[i];
    memmove(&F.areas[i], &F.areas[i + 1], sizeof(NLArea) * (size_t)(F.nAreas - i - 1));
    F.nAreas--;
    for (dy = -NL_DL_R; dy <= NL_DL_R; dy++)
        for (dx = -NL_DL_R; dx <= NL_DL_R; dx++) {
            int tx = (int)floor(a.rx / TILE_M) + dx, ty = (int)floor(a.ry / TILE_M) + dy, shared = 0;
            char name[64];
            for (k = 0; k < F.nAreas; k++) {   /* tegels die een ander gebied ook gebruikt laten staan */
                int ax = (int)floor(F.areas[k].rx / TILE_M), ay = (int)floor(F.areas[k].ry / TILE_M);
                if (abs(ax - tx) <= NL_DL_R && abs(ay - ty) <= NL_DL_R) shared = 1;
            }
            if (shared) continue;
            snprintf(name, sizeof(name), "nl/tiles/%d_%d.t", tx, ty);
            plat_delete_file(name);
        }
    plat_log("SGB-NL gebied %s verwijderd", a.pc);
    idx_save();
}

/* ---- downloads op de achtergrond (in elk scherm) */

static void dl_enqueue(const char *pc) {
    int i;
    double x, y;
    if (ns_pack_postcode(pc, &x, &y)) { snprintf(F.dlLast, sizeof(F.dlLast), "%s is in Heemskerk/Beverwijk: already built in, plays offline.", pc); return; }
    if (F.dlPhase && strcmp(F.dlgeo.pc, pc) == 0) return;
    for (i = 0; i < F.nQueue; i++) if (strcmp(F.queue[i], pc) == 0) return;
    if (F.nQueue >= 8) return;
    snprintf(F.queue[F.nQueue++], 8, "%s", pc);
    snprintf(F.dlLast, sizeof(F.dlLast), "%s added to the download list", pc);
}

static void dl_tick(void) {
    ns_dl_tick();
    if (F.dlPhase == 0) {
        if (F.nQueue == 0) return;
        geo_begin(&F.dlgeo, F.queue[0]);
        memmove(F.queue[0], F.queue[1], sizeof(F.queue[0]) * (size_t)(F.nQueue - 1));
        F.nQueue--;
        F.dlPhase = 1;
        F.dlT0 = plat_time();
        return;
    }
    if (F.dlPhase == 1) {
        if (!geo_tick(&F.dlgeo)) return;
        if (F.dlgeo.err) { snprintf(F.dlLast, sizeof(F.dlLast), "%s", F.dlgeo.msg); F.dlPhase = 0; return; }
        ns_dl_start(F.dlgeo.geo.cx, F.dlgeo.geo.cy, NL_DL_R);
        F.dlPhase = 2;
        return;
    }
    if (F.dlPhase == 2 && !ns_dl_busy()) {
        int done, total, failed;
        long bytes;
        ns_dl_progress(&done, &total, &bytes, &failed);
        if (failed == 0) {
            idx_add(F.dlgeo.pc, F.dlgeo.geo.name, F.dlgeo.geo.cx, F.dlgeo.geo.cy, (bytes + 1023) / 1024);
            snprintf(F.dlLast, sizeof(F.dlLast), "Saved %s (%d tiles, %.1f MB) - plays offline now", F.dlgeo.pc, total, bytes / 1048576.0);
        } else {
            if (failed < total) idx_add(F.dlgeo.pc, F.dlgeo.geo.name, F.dlgeo.geo.cx, F.dlgeo.geo.cy, (bytes + 1023) / 1024);
            snprintf(F.dlLast, sizeof(F.dlLast), "%s: %d of %d tiles failed (no connection?) - tap DOWNLOAD again", F.dlgeo.pc, failed, total);
        }
        plat_log("SGB-NL download %s klaar: %d tegels, %d mislukt, %ld kB, %.1f s", F.dlgeo.pc, total, failed, bytes / 1024, plat_time() - F.dlT0);
        F.dlPhase = 0;
    }
}

/* ---- spelen */

static void nl_play_at(double rx, double ry, const char *label, int from) {
    if (F.mode == 0) memcpy(&F.Gcity, &F.G, sizeof(Game));
    else if (F.mode == 1) ns_end();
    F.mode = 2;   /* laden */
    F.nlFrom = from;
    snprintf(F.nlLabel, sizeof(F.nlLabel), "%s", label);
    ns_begin(&F.R, &F.WNL, floor(rx + 0.5), floor(ry + 0.5));
    F.R.cur = 1;
    F.nlTx = (float)(rx - floor(rx + 0.5)); F.nlTz = (float)(floor(ry + 0.5) - ry);
    F.nlT0 = plat_time();
    F.job.err = 0;
    F.nlPhase = 1;
    set_state(ST_NLLOAD);
}

static void nl_play_pc(const char *pc, int from) {
    geo_begin(&F.job, pc);
    F.nlFrom = from;
    F.nlPhase = 0;
    set_state(ST_NLLOAD);
}

static void nl_exit_to_city(void) {
    if (F.mode == 0) return;
    ns_end();
    memcpy(&F.G, &F.Gcity, sizeof(Game));
    F.R.cur = 0;
    F.mode = 0;
    apply_settings();
}

static void nl_place_vehicles(void) {
    Game *g = &F.G;
    V3 f = fwd_of(g->p.h), r = right_of(g->p.h);
    int i;
    for (i = 0; i < 2 && i < g->nveh; i++) {
        Vehicle *v = &g->veh[i];
        if (i == g->p.veh) continue;
        v->x = g->p.x + (i == 0 ? r.x * 1.3f + f.x * 0.6f : f.x * 7.0f);
        v->z = g->p.z + (i == 0 ? r.z * 1.3f + f.z * 0.6f : f.z * 7.0f);
        v->h = g->p.h; v->speed = 0; v->y = 0;
    }
}

static void nl_tick(void) {
    if (F.nlPhase == 0) {
        if (!geo_tick(&F.job)) return;
        if (F.job.err) return;
        nl_play_at(F.job.geo.cx, F.job.geo.cy, F.job.geo.name, F.nlFrom);
        return;
    }
    ns_update(F.nlTx, F.nlTz);
    if (!ns_ready_at(F.nlTx, F.nlTz)) return;
    if (ns_busy() > 0 && plat_time() - F.nlT0 < 12.0) return;
    {
        World *w = &F.WNL;
        float sx = F.nlTx, sz = F.nlTz, sh = 0;
        int k, tiles, nb, nv, fp, fc, fn;
        ns_find_road(F.nlTx, F.nlTz, &sx, &sz, &sh);
        w->poi[POI_SPAWN] = v3(sx, 0, sz);
        w->spawn_heading = sh;
        for (k = 0; k < POI_COUNT; k++) w->poi[k] = w->poi[POI_SPAWN];
        w->npark = 2;
        w->park[0].type = VEH_BIKE; w->park[0].variant = 0;
        w->park[1].type = VEH_CAR; w->park[1].variant = 1;
        w->park[0].x = w->park[1].x = sx; w->park[0].z = w->park[1].z = sz; w->park[0].h = w->park[1].h = sh;
        w->ncash = 0; w->ngifts = 0; w->nhouses = 0;
        game_init(&F.G, w);
        nl_place_vehicles();
        F.mode = 1;
        apply_settings();
        ns_stats(&tiles, &nb, &nv, &fp, &fc, &fn);
        F.nlLoadSecs = (float)(plat_time() - F.nlT0);
        plat_log("SGB-NL start '%s': %d tegels (pakket %d, opgeslagen %d, internet %d), %d gebouwen, %d vertices, %.1f s, geheugen %ld kB",
                 F.nlLabel, tiles, fp, fc, fn, nb, nv, F.nlLoadSecs, plat_memory_kb());
        snprintf(F.G.banner, sizeof(F.G.banner), "REAL NL");
        snprintf(F.G.sub, sizeof(F.G.sub), "%s", F.nlLabel);
        F.G.bannerT = 5.0f; F.G.bannerGood = 1;
        set_state(ST_GAME);
        F.fade = 1.0f;
    }
}

static void draw_nl_load(void) {
    Renderer *R = &F.R;
    char line[200];
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 1);
    draw_backdrop(0.25f, 0.0f, F.t);
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 0.55f);
    ui_text(R, "REAL NETHERLANDS", R->W * 0.5f, 60, 54, 1, 1, 1, 1, 1);
    if (F.nlPhase == 0) snprintf(line, sizeof(line), "Looking up postcode %s ...", F.job.pc);
    else snprintf(line, sizeof(line), "%s", F.nlLabel);
    ui_text(R, line, R->W * 0.5f, 220, 30, 1, 1, 1, 1, 1);
    if (F.nlPhase == 1) {
        int tiles, nb, nv, fp, fc, fn;
        ns_stats(&tiles, &nb, &nv, &fp, &fc, &fn);
        snprintf(line, sizeof(line), "Map tiles ready: %d / 9   (built-in %d, saved %d, internet %d)", tiles, fp, fc, fn);
        ui_text(R, line, R->W * 0.5f, 290, 22, 1, 0.85f, 0.4f, 1, 1);
        snprintf(line, sizeof(line), "%d buildings with real heights", nb);
        ui_text(R, line, R->W * 0.5f, 330, 22, 0.9f, 0.9f, 0.9f, 1, 1);
    }
    if (F.job.err) {
        ui_text(R, F.job.msg, R->W * 0.5f, 520, 24, 1.0f, 0.4f, 0.4f, 1, 1);
        ui_text(R, "TAP TO GO BACK", R->W * 0.5f, 600, 34, 1, 1, 1, 0.6f + 0.4f * sinf(F.t * 4), 1);
    } else {
        int k;
        for (k = 0; k < 8; k++) {
            float ang = F.t * 4.0f + k * (2 * PI_F / 8);
            ui_circle(R, R->W * 0.5f + cosf(ang) * 22, 470 + sinf(ang) * 22, 5, 1, 1, 1, 0.15f + 0.85f * (k / 7.0f));
        }
    }
    ui_text(R, "Map data: 3D BAG (TU Delft / 3DGI), BAG + TOP10NL (Kadaster, PDOK) - CC BY 4.0", R->W * 0.5f, R->H - 40.0f, 16, 0.8f, 0.8f, 0.8f, 1, 1);
}

/* ---- grote kaart (tik op de minimap) */

static void bm_load_overview(void) {
    char *meta = NULL;
    long ml = 0;
    PlatImage img;
    if (F.bmTried) return;
    F.bmTried = 1;
    memset(&img, 0, sizeof(img));
    if (!plat_read_resource("nl_hb_map.txt", &meta, &ml)) return;
    if (sscanf(meta, "%lf %lf %lf %d %d", &F.bmX0, &F.bmY1, &F.bmMpp, &F.bmW, &F.bmH) != 5) { free(meta); return; }
    free(meta);
    if (!plat_load_image("nl_hb_map.png", &img) || !img.rgba) return;
    r_tex_upload(&F.bmTex, img.w, img.h, img.rgba);
    plat_free_image(&img);
    F.bmOk = 1;
}

static void bm_open(void) {
    F.bmCx = F.G.p.x; F.bmCz = F.G.p.z;
    F.bmScale = F.mode == 1 ? 0.5f : 1.6f;
    F.bmTarget = 0;
    if (F.mode == 1) bm_load_overview();
    set_state(ST_BIGMAP);
}

static void bm_w2s(float x, float z, float *sx, float *sy) {
    *sx = F.R.W * 0.5f + (x - F.bmCx) * F.bmScale;
    *sy = F.R.H * 0.5f + (z - F.bmCz) * F.bmScale;
}

static void draw_bigmap(void) {
    Renderer *R = &F.R;
    float x0, y0, x1, y1, sx, sy;
    char line[160];
    int i;
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0.09f, 0.1f, 0.12f, 1);
    if (F.mode == 1) {
        if (F.bmOk) {
            float wx, wz;
            double rx = F.bmX0, ry = F.bmY1;
            ns_from_rd(rx, ry, &wx, &wz);
            bm_w2s(wx, wz, &x0, &y0);
            bm_w2s(wx + (float)(F.bmW * F.bmMpp), wz + (float)(F.bmH * F.bmMpp), &x1, &y1);
            ui_quad(R, &F.bmTex, x0, y0, x1 - x0, y1 - y0, 0, 0, 1, 1, 1, 1, 1, 1);
        }
        for (i = 0; i < ns_map_count(); i++) {   /* bezochte tegels (ook buiten de regio) */
            double rx0, ry0;
            float wx, wz;
            const Tex *t = ns_map_tile(i, &rx0, &ry0);
            if (!t) continue;
            ns_from_rd(rx0, ry0 + TILE_M, &wx, &wz);
            bm_w2s(wx, wz, &x0, &y0);
            if (x0 > R->W || y0 > R->H || x0 + TILE_M * F.bmScale < 0 || y0 + TILE_M * F.bmScale < 0) continue;
            ui_quad(R, t, x0, y0, TILE_M * F.bmScale, TILE_M * F.bmScale, 0, 0, 1, 1, 1, 1, 1, 1);
        }
        for (i = 0; i < ns_pack_nstarts(); i++) {
            double rx, ry;
            float wx, wz;
            const char *nm = ns_pack_start(i, &rx, &ry);
            ns_from_rd(rx, ry, &wx, &wz);
            bm_w2s(wx, wz, &sx, &sy);
            ui_circle(R, sx, sy, 7, 0, 0, 0, 0.8f);
            ui_circle(R, sx, sy, 5, 1.0f, 0.82f, 0.15f, 1);
            if (F.bmScale > 0.15f) ui_text(R, nm, sx + 10, sy - 10, 16, 1, 1, 1, 0.9f, 0);
        }
    } else {
        const World *w = F.G.w;
        static const char *names[4] = {"BLOCK ARMS", "GIVEAWAY BANK", "GIVEAWAY MOTORS", "BLOCK CUSTOMS"};
        static const int pois[4] = {POI_GUNSHOP, POI_BANK, POI_DEALER, POI_CUSTOMS};
        static const int icons[4] = {ICON_GUN, ICON_BANK, ICON_CAR, ICON_CUSTOMS};
        int q;
        bm_w2s(w->mmOrg, w->mmOrgZ, &x0, &y0);
        ui_quad(R, &R->rw[0].minimap, x0, y0, w->mmSpan * F.bmScale, w->mmSpan * F.bmScale, 0, 0, 1, 1, 1, 1, 1, 1);
        for (q = 0; q < NMISSIONS; q++) {
            const MissionDef *md = &F.G.mdef[q];
            bm_w2s(md->giver.x, md->giver.z, &sx, &sy);
            ui_icon(R, F.G.completed[q] ? ICON_DONE : ICON_MISSION, sx, sy, 30, 1);
            if (F.bmScale > 2.2f) ui_text(R, md->title, sx, sy + 15, 15, 1, 1, 0.85f, 0.95f, 1);
        }
        for (q = 0; q < 4; q++) {
            bm_w2s(w->poi[pois[q]].x, w->poi[pois[q]].z, &sx, &sy);
            ui_icon(R, icons[q], sx, sy, 40, 1);
            ui_text(R, names[q], sx, sy + 20, 17, 1, 1, 1, 1, 1);
        }
    }
    /* speler */
    bm_w2s(F.G.p.x, F.G.p.z, &sx, &sy);
    {
        float a = F.G.p.h, fx = sinf(a), fy = -cosf(a), rx2 = -fy, ry2 = fx;
        float t[8], uvz[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
        fy = -fy; ry2 = -ry2;   /* noord boven: +z is omlaag op het scherm */
        fx = sinf(a); fy = cosf(a); rx2 = cosf(a); ry2 = -sinf(a);
        t[0] = sx + fx * 14; t[1] = sy + fy * 14;
        t[2] = sx - fx * 9 + rx2 * 9; t[3] = sy - fy * 9 + ry2 * 9;
        t[4] = sx - fx * 4; t[5] = sy - fy * 4;
        t[6] = sx - fx * 9 - rx2 * 9; t[7] = sy - fy * 9 - ry2 * 9;
        ui_circle(R, sx, sy, 16, 0, 0, 0, 0.6f);
        ui_quad4(R, &R->white, t, uvz, 1.0f, 0.25f, 0.25f, 1);
    }
    if (F.bmTarget) {
        bm_w2s(F.bmTx, F.bmTz, &sx, &sy);
        ui_ring(R, sx, sy, 16, 3, 1, 1, 1, 1);
        ui_rect(R, sx - 1.5f, sy - 26, 3, 20, 1, 1, 1, 1);
        ui_rect(R, sx - 1.5f, sy + 6, 3, 20, 1, 1, 1, 1);
        ui_rect(R, sx - 26, sy - 1.5f, 20, 3, 1, 1, 1, 1);
        ui_rect(R, sx + 6, sy - 1.5f, 20, 3, 1, 1, 1, 1);
    }
    /* knoppen */
    ui_rect(R, 0, 0, (float)R->W, 60, 0, 0, 0, 0.65f);
    if (F.mode == 1) {
        const char *sn = nl_street_name(F.G.w, F.G.p.x, F.G.p.z);
        snprintf(line, sizeof(line), "MAP  -  %s", sn ? sn : F.nlLabel);
    } else snprintf(line, sizeof(line), "MAP  -  SMALL GIVEAWAY CITY");
    ui_text(R, line, 20, 12, 30, 1, 1, 1, 1, 0);
    ui_circle(R, R->W - 40.0f, 30, 24, 0.7f, 0.1f, 0.1f, 0.9f);
    ui_text(R, "X", R->W - 40.0f, 12, 32, 1, 1, 1, 1, 1);
    ui_circle(R, R->W - 50.0f, 150, 34, 0.1f, 0.1f, 0.12f, 0.85f);
    ui_text(R, "+", R->W - 50.0f, 124, 48, 1, 1, 1, 1, 1);
    ui_circle(R, R->W - 50.0f, 230, 34, 0.1f, 0.1f, 0.12f, 0.85f);
    ui_text(R, "-", R->W - 50.0f, 204, 48, 1, 1, 1, 1, 1);
    ui_circle(R, R->W - 50.0f, 310, 34, 0.1f, 0.1f, 0.12f, 0.85f);
    ui_text(R, "ME", R->W - 50.0f, 296, 26, 1, 1, 1, 1, 1);
    if (F.bmTarget && F.mode == 1) {
        ui_rect(R, R->W * 0.5f - 150, R->H - 96.0f, 300, 70, F.pressed == 905 ? 0.9f : 0.75f, 0.12f, 0.12f, 0.95f);
        ui_text(R, "GO HERE", R->W * 0.5f, R->H - 88.0f, 46, 1, 1, 1, 1, 1);
    }
    if (F.mode == 1)
        ui_text(R, "Drag to move, tap to pick a place. Built-in: Heemskerk + Beverwijk. Grey = not loaded.", R->W * 0.5f, R->H - 20.0f, 14, 0.85f, 0.85f, 0.85f, 1, 1);
    else {
        static const char *leg[6] = {"GUNS", "BANK", "CARS", "CUSTOMS", "MISSION", "DONE"};
        int q;
        ui_rect(R, R->W * 0.5f - 330, R->H - 50.0f, 660, 44, 0, 0, 0, 0.6f);
        for (q = 0; q < 6; q++) {
            float lx = R->W * 0.5f - 300 + q * 110.0f;
            ui_icon(R, q, lx, R->H - 28.0f, 30, 1);
            ui_text(R, leg[q], lx + 18, R->H - 38.0f, 16, 1, 1, 1, 1, 0);
        }
    }
}

static int bm_button(float x, float y) {
    if ((x - (F.R.W - 40.0f)) * (x - (F.R.W - 40.0f)) + (y - 30) * (y - 30) < 34 * 34) return 901;
    if ((x - (F.R.W - 50.0f)) * (x - (F.R.W - 50.0f)) + (y - 150) * (y - 150) < 40 * 40) return 902;
    if ((x - (F.R.W - 50.0f)) * (x - (F.R.W - 50.0f)) + (y - 230) * (y - 230) < 40 * 40) return 903;
    if ((x - (F.R.W - 50.0f)) * (x - (F.R.W - 50.0f)) + (y - 310) * (y - 310) < 40 * 40) return 904;
    if (F.bmTarget && F.mode == 1 && x > F.R.W * 0.5f - 150 && x < F.R.W * 0.5f + 150 && y > F.R.H - 96.0f && y < F.R.H - 26.0f) return 905;
    return 0;
}

static void bm_go(float x, float z) {
    Game *g = &F.G;
    game_teleport(g, x, z, g->p.h);
    g->p.speed = 0;
    if (g->p.veh >= 0) g->veh[g->p.veh].speed = 0;
    nl_place_vehicles();
    F.bmTarget = 0;
    plat_log("SGB-NL reis naar %.0f,%.0f", x, z);
}

static void bigmap_touch(float x, float y, int phase) {
    if (phase == TOUCH_BEGIN) {
        F.bmDownX = F.bmLastX = x; F.bmDownY = F.bmLastY = y;
        F.bmDrag = 0;
        F.pressed = bm_button(x, y);
        return;
    }
    if (phase == TOUCH_MOVE) {
        if (fabsf(x - F.bmDownX) + fabsf(y - F.bmDownY) > 12 && !F.pressed) F.bmDrag = 1;
        if (F.bmDrag) { F.bmCx -= (x - F.bmLastX) / F.bmScale; F.bmCz -= (y - F.bmLastY) / F.bmScale; }
        F.bmLastX = x; F.bmLastY = y;
        return;
    }
    {
        int b = bm_button(x, y);
        F.pressed = 0;
        if (F.bmDrag) return;
        if (b == 901) set_state(ST_GAME);
        else if (b == 902) F.bmScale = fminf(4.0f, F.bmScale * 1.6f);
        else if (b == 903) F.bmScale = fmaxf(0.06f, F.bmScale / 1.6f);
        else if (b == 904) { F.bmCx = F.G.p.x; F.bmCz = F.G.p.z; }
        else if (b == 905) { bm_go(F.bmTx, F.bmTz); set_state(ST_GAME); }
        else if (F.mode == 1) {
            F.bmTarget = 1;
            F.bmTx = F.bmCx + (x - F.R.W * 0.5f) / F.bmScale;
            F.bmTz = F.bmCz + (y - F.R.H * 0.5f) / F.bmScale;
        }
    }
}

/* ---- het REAL NL-scherm: links postcode, rechts ingebouwd + gedownload */

#define KEY_DEL 1
#define KEY_PLAY 2
#define KEY_BACK 3
#define KEY_DL 4
#define KEY_PGUP 5
#define KEY_PGDN 6
#define AREA_ROWS 5
#define AREA_Y0 330.0f
static const char *KROWS[4] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};

static void key_rect(int row, int col, float *x, float *y, float *w, float *h) {
    float x0[4] = {34, 34, 61, 88};
    *w = 48; *h = 52;
    *x = x0[row] + col * 54.0f;
    *y = 182.0f + row * 60.0f;
}

static void start_rect(int i, float *x, float *y) { *x = 610.0f + (i & 1) * 199.0f; *y = 112.0f + (i / 2) * 50.0f; }

static int nl_key_at(float px, float py) {
    int r, c;
    float x, y, w, h;
    for (r = 0; r < 4; r++)
        for (c = 0; KROWS[r][c]; c++) {
            key_rect(r, c, &x, &y, &w, &h);
            if (px >= x && px < x + w && py >= y && py < y + h) return KROWS[r][c];
        }
    key_rect(3, 7, &x, &y, &w, &h);
    if (px >= x && px < x + 108 && py >= y && py < y + h) return KEY_DEL;
    if (px >= 34 && px < 294 && py >= 430 && py < 500) return KEY_PLAY;
    if (px >= 314 && px < 574 && py >= 430 && py < 500) return KEY_DL;
    if (px >= 20 && px < 220 && py >= 680 && py < 745) return KEY_BACK;
    for (r = 0; r < ns_pack_nstarts() && r < 6; r++) {
        start_rect(r, &x, &y);
        if (px >= x && px < x + 192 && py >= y && py < y + 44) return 500 + r;
    }
    for (r = 0; r < AREA_ROWS; r++) {
        int i = F.areaPage * AREA_ROWS + r;
        float ry = AREA_Y0 + r * 64.0f;
        if (i >= F.nAreas) break;
        if (py >= ry && py < ry + 58) {
            if (px >= 950 && px < 1004) return 400 + i;
            if (px >= 610 && px < 948) return 300 + i;
        }
    }
    if (py >= 662 && py < 712) {
        if (px >= 690 && px < 790) return KEY_PGUP;
        if (px >= 820 && px < 920) return KEY_PGDN;
    }
    return 0;
}

static void draw_nl_input(void) {
    Renderer *R = &F.R;
    int r, c, valid, i;
    char shown[16], norm[8], line[160];
    float x, y, w, h, totalMB = 0;
    if (F.nlInFrom == ST_PAUSE) {
        ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0.02f, 0.02f, 0.03f, 1);
    } else {
        ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 1);
        draw_backdrop(0.3f, 0.0f, F.t);
        ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 0.6f);
    }
    ui_text(R, "REAL NETHERLANDS", R->W * 0.5f, 10, 44, 1, 1, 1, 1, 1);
    ui_rect(R, 64, 84, 480, 84, 0, 0, 0, 0.7f);
    if (F.pcLen > 4) snprintf(shown, sizeof(shown), "%.4s %s", F.pcIn, F.pcIn + 4);
    else snprintf(shown, sizeof(shown), "%s", F.pcIn);
    if (F.pcLen == 0) ui_text(R, "1234 AB", 304, 92, 56, 0.5f, 0.5f, 0.5f, 0.6f, 1);
    else ui_text(R, shown, 304, 92, 56, 1, 1, 1, 1, 1);
    for (r = 0; r < 4; r++)
        for (c = 0; KROWS[r][c]; c++) {
            int ch = KROWS[r][c], ok = (r == 0) ? (F.pcLen < 4 && !(F.pcLen == 0 && ch == '0')) : (F.pcLen >= 4 && F.pcLen < 6);
            char lab[2] = {(char)ch, 0};
            int pr = F.pressed == ch;
            key_rect(r, c, &x, &y, &w, &h);
            ui_rect(R, x, y, w, h, pr ? 0.7f : 0.12f, pr ? 0.1f : 0.12f, pr ? 0.1f : 0.14f, ok ? 0.9f : 0.4f);
            ui_text(R, lab, x + w * 0.5f, y + 8, 34, 1, 1, 1, ok ? 1.0f : 0.35f, 1);
        }
    key_rect(3, 7, &x, &y, &w, &h);
    ui_rect(R, x, y, 108, h, F.pressed == KEY_DEL ? 0.7f : 0.3f, 0.08f, 0.08f, 0.9f);
    ui_text(R, "DELETE", x + 54, y + 12, 26, 1, 1, 1, 1, 1);
    valid = nl_normalize_postcode(F.pcIn, norm);
    ui_rect(R, 34, 430, 260, 70, valid ? (F.pressed == KEY_PLAY ? 0.9f : 0.75f) : 0.2f, valid ? 0.12f : 0.2f, valid ? 0.12f : 0.2f, 0.95f);
    ui_text(R, "PLAY", 164, 440, 46, 1, 1, 1, valid ? 1.0f : 0.4f, 1);
    ui_rect(R, 314, 430, 260, 70, valid ? 0.1f : 0.2f, valid ? (F.pressed == KEY_DL ? 0.6f : 0.45f) : 0.2f, valid ? 0.75f : 0.2f, 0.95f);
    ui_text(R, "DOWNLOAD", 444, 444, 38, 1, 1, 1, valid ? 1.0f : 0.4f, 1);
    ui_text(R, "PLAY = go there now (loads more land while you drive).", 304, 506, 15, 0.8f, 0.8f, 0.8f, 1, 1);
    ui_text(R, "DOWNLOAD = save 1.25 x 1.25 km around it for offline play.", 304, 524, 15, 0.8f, 0.8f, 0.8f, 1, 1);
    ui_rect(R, 34, 548, 540, 118, 0, 0, 0, 0.55f);
    if (F.dlPhase) {
        int done, total, failed;
        long bytes;
        ns_dl_progress(&done, &total, &bytes, &failed);
        ui_text(R, "DOWNLOADING", 46, 554, 20, 0.5f, 0.8f, 1.0f, 1, 0);
        if (F.dlPhase == 1) snprintf(line, sizeof(line), "Postcode %s: looking up...", F.dlgeo.pc);
        else snprintf(line, sizeof(line), "%s: tile %d of %d (%.1f MB)", F.dlgeo.geo.name, done, total, bytes / 1048576.0);
        ui_text(R, line, 46, 582, 17, 1, 1, 1, 1, 0);
        if (F.dlPhase == 2 && total > 0) {
            ui_rect(R, 46, 614, 516, 12, 0.15f, 0.15f, 0.2f, 1);
            ui_rect(R, 46, 614, 516.0f * done / total, 12, 0.3f, 0.7f, 1.0f, 1);
        }
    } else {
        ui_text(R, F.dlLast[0] ? F.dlLast : "No downloads running.", 46, 562, 16, 0.9f, 0.9f, 0.9f, 1, 0);
    }
    if (F.nQueue > 0) {
        snprintf(line, sizeof(line), "Waiting: %d more", F.nQueue);
        ui_text(R, line, 562, 554, 16, 1, 0.85f, 0.4f, 1, 2);
    }
    big_button("BACK", 120, 712, 40, F.pressed == KEY_BACK, 1);
    /* rechts boven: ingebouwd */
    ui_text(R, "BUILT IN: HEEMSKERK + BEVERWIJK", 610, 76, 22, 1.0f, 0.85f, 0.4f, 1, 0);
    if (ns_pack_nstarts() == 0) ui_text(R, "(map pack missing)", 610, 112, 16, 1, 0.5f, 0.5f, 1, 0);
    for (i = 0; i < ns_pack_nstarts() && i < 6; i++) {
        double rx, ry;
        const char *nm = ns_pack_start(i, &rx, &ry);
        float ns2, nw;
        start_rect(i, &x, &y);
        ui_rect(R, x, y, 192, 44, F.pressed == 500 + i ? 0.6f : 0.1f, F.pressed == 500 + i ? 0.1f : 0.3f, 0.12f, 0.9f);
        nw = ui_text_w(R, nm, 17);
        ns2 = nw > 176 ? 17.0f * 176.0f / nw : 17.0f;
        ui_text(R, nm, x + 96, y + 12 + (17 - ns2) * 0.5f, ns2, 1, 1, 1, 1, 1);
    }
    /* rechts onder: gedownload */
    for (i = 0; i < F.nAreas; i++) totalMB += F.areas[i].kb / 1024.0f;
    snprintf(line, sizeof(line), "DOWNLOADED (OFFLINE): %d, %.1f MB", F.nAreas, totalMB);
    ui_text(R, line, 610, 290, 20, 1.0f, 0.85f, 0.4f, 1, 0);
    if (F.nAreas == 0) ui_text(R, "Type a postcode and tap DOWNLOAD.", 610, AREA_Y0 + 6, 16, 0.8f, 0.8f, 0.8f, 1, 0);
    for (r = 0; r < AREA_ROWS; r++) {
        int idx = F.areaPage * AREA_ROWS + r;
        float ry = AREA_Y0 + r * 64.0f, nw, ns3;
        NLArea *a;
        if (idx >= F.nAreas) break;
        a = &F.areas[idx];
        ui_rect(R, 610, ry, 338, 58, F.pressed == 300 + idx ? 0.55f : 0.08f, F.pressed == 300 + idx ? 0.1f : 0.08f, 0.1f, 0.85f);
        snprintf(line, sizeof(line), "%.4s %s", a->pc, a->pc + 4);
        ui_text(R, line, 622, ry + 2, 28, 1, 1, 1, 1, 0);
        snprintf(line, sizeof(line), "%.1f MB", a->kb / 1024.0f);
        ui_text(R, line, 938, ry + 8, 16, 0.6f, 1.0f, 0.6f, 1, 2);
        nw = ui_text_w(R, a->name, 15);
        ns3 = nw > 316 ? 15.0f * 316.0f / nw : 15.0f;
        ui_text(R, a->name, 622, ry + 36, ns3, 0.85f, 0.85f, 0.85f, 1, 0);
        ui_rect(R, 950, ry, 54, 58, F.delArm == idx + 1 ? 0.9f : (F.pressed == 400 + idx ? 0.7f : 0.35f), 0.08f, 0.08f, 0.9f);
        ui_text(R, F.delArm == idx + 1 ? "?" : "X", 977, ry + 10, 32, 1, 1, 1, 1, 1);
    }
    if (F.nAreas > AREA_ROWS) {
        int pages = (F.nAreas + AREA_ROWS - 1) / AREA_ROWS;
        snprintf(line, sizeof(line), "%d/%d", F.areaPage + 1, pages);
        ui_rect(R, 690, 662, 100, 50, 0.15f, 0.15f, 0.18f, 0.9f);
        ui_text(R, "<", 740, 666, 36, 1, 1, 1, F.areaPage > 0 ? 1.0f : 0.3f, 1);
        ui_text(R, line, 805, 676, 18, 1, 1, 1, 1, 1);
        ui_rect(R, 820, 662, 100, 50, 0.15f, 0.15f, 0.18f, 0.9f);
        ui_text(R, ">", 870, 666, 36, 1, 1, 1, F.areaPage + 1 < pages ? 1.0f : 0.3f, 1);
    }
    if (F.delArm) ui_text(R, "Tap ? again to delete that area", 807, 720, 16, 1, 0.5f, 0.5f, 1, 1);
    ui_text(R, "Data: 3D BAG (TU Delft/3DGI), PDOK/Kadaster - CC BY 4.0", R->W * 0.5f, R->H - 20.0f, 13, 0.7f, 0.7f, 0.7f, 1, 1);
}

static void nl_input_tap(float x, float y, int phase) {
    int k = nl_key_at(x, y);
    char norm[8];
    if (phase != TOUCH_END) { F.pressed = k; return; }
    F.pressed = 0;
    if (k < 400 || k >= 400 + NL_MAX_AREAS) { if (k != 0) F.delArm = 0; }
    if (k >= '0' && k <= '9') {
        if (F.pcLen < 4 && !(F.pcLen == 0 && k == '0')) { F.pcIn[F.pcLen++] = (char)k; F.pcIn[F.pcLen] = 0; }
    } else if (k >= 'A' && k <= 'Z') {
        if (F.pcLen >= 4 && F.pcLen < 6) { F.pcIn[F.pcLen++] = (char)k; F.pcIn[F.pcLen] = 0; }
    } else if (k == KEY_DEL) {
        if (F.pcLen > 0) F.pcIn[--F.pcLen] = 0;
    } else if (k == KEY_PLAY) {
        if (nl_normalize_postcode(F.pcIn, norm)) nl_play_pc(norm, F.nlInFrom);
    } else if (k == KEY_DL) {
        if (nl_normalize_postcode(F.pcIn, norm)) { dl_enqueue(norm); F.pcLen = 0; F.pcIn[0] = 0; }
    } else if (k == KEY_BACK) {
        set_state(F.nlInFrom == ST_PAUSE ? ST_PAUSE : ST_MENU);
    } else if (k == KEY_PGUP) {
        if (F.areaPage > 0) F.areaPage--;
    } else if (k == KEY_PGDN) {
        if ((F.areaPage + 1) * AREA_ROWS < F.nAreas) F.areaPage++;
    } else if (k >= 500 && k < 506) {
        double rx, ry;
        const char *nm = ns_pack_start(k - 500, &rx, &ry);
        if (nm) nl_play_at(rx, ry, nm, F.nlInFrom);
    } else if (k >= 300 && k < 300 + NL_MAX_AREAS) {
        NLArea *a = &F.areas[k - 300];
        nl_play_at(a->rx, a->ry, a->name, F.nlInFrom);
    } else if (k >= 400 && k < 400 + NL_MAX_AREAS) {
        if (F.delArm == k - 400 + 1) {
            idx_delete(k - 400);
            F.delArm = 0;
            if (F.areaPage * AREA_ROWS >= F.nAreas && F.areaPage > 0) F.areaPage--;
        } else F.delArm = k - 400 + 1;
    }
}

static void open_nl_input(int from) {
    F.nlInFrom = from;
    F.delArm = 0;
    ns_pack_load();
    idx_load();
    set_state(ST_NLINPUT);
}

static const char *SHOP_DESC[5] = {"Reliable sidearm", "Fast, light, lots of bullets", "Huge damage up close", "Refill all your guns", "Absorbs most damage"};

static void shop_item(int i, char *name, size_t n, int *price, char *info, size_t ni) {
    Game *g = &F.G;
    if (i <= 2) {
        int w = i + 1;
        snprintf(name, n, "%s", WEAPONS[w].name);
        *price = g->owned[w] ? WEAPONS[w].ammoPrice : WEAPONS[w].price;
        if (g->owned[w]) snprintf(info, ni, "OWNED - %d bullets (+%d)", g->ammo[w], WEAPONS[w].ammoPack);
        else snprintf(info, ni, "%s", SHOP_DESC[i]);
    } else if (i == 3) {
        int w;
        *price = 0;
        for (w = 1; w < WP_COUNT; w++) if (g->owned[w]) *price += WEAPONS[w].ammoPrice;
        snprintf(name, n, "AMMO (ALL GUNS)");
        snprintf(info, ni, "%s", *price ? SHOP_DESC[3] : "Buy a gun first");
    } else {
        snprintf(name, n, "BODY ARMOR");
        *price = 300;
        snprintf(info, ni, "%s (now %d%%)", SHOP_DESC[4], (int)g->armor);
    }
}

static void draw_shop(void) {
    Renderer *R = &F.R;
    char name[48], info[80], b[48];
    int i, price;
    r_draw_scene(R, &F.G);
    ui_begin(R);
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 0.72f);
    ui_rect(R, 152, 40, 720, 640, 0.07f, 0.06f, 0.07f, 0.95f);
    ui_rect(R, 152, 40, 720, 70, 0.6f, 0.06f, 0.06f, 1);
    ui_text(R, "BLOCK ARMS", 512, 50, 52, 1, 1, 1, 1, 1);
    fmt_money(b, sizeof(b), F.G.money);
    ui_text(R, b, 852, 128, 34, 0.55f, 1.0f, 0.55f, 1, 2);
    ui_text(R, "Licensed dealer - no refunds", 176, 136, 18, 0.8f, 0.8f, 0.8f, 1, 0);
    for (i = 0; i < 5; i++) {
        float y = 180.0f + i * 88.0f;
        int pr = F.pressed == 600 + i;
        shop_item(i, name, sizeof(name), &price, info, sizeof(info));
        ui_rect(R, 172, y, 680, 78, 0.12f, 0.12f, 0.14f, 0.95f);
        ui_text(R, name, 192, y + 6, 32, 1, 1, 1, 1, 0);
        ui_text(R, info, 192, y + 46, 17, 0.8f, 0.8f, 0.8f, 1, 0);
        ui_rect(R, 712, y + 12, 124, 54, pr ? 0.9f : (F.G.money >= price && price > 0 ? 0.2f : 0.25f), pr ? 0.2f : (F.G.money >= price && price > 0 ? 0.55f : 0.25f), pr ? 0.2f : 0.25f, 1);
        snprintf(b, sizeof(b), "$%d", price);
        ui_text(R, b, 774, y + 20, 30, 1, 1, 1, 1, 1);
    }
    if (F.shopMsg[0]) ui_text(R, F.shopMsg, 512, 626, 20, 1, 0.85f, 0.4f, 1, 1);
    big_button("LEAVE", 512, 724, 40, F.pressed == 690, 1);
    ui_flush(R);
}

static void shop_tap(float x, float y, int phase) {
    int i, hit = 0;
    for (i = 0; i < 5; i++) {
        float yy = 180.0f + i * 88.0f;
        if (x > 712 && x < 836 && y > yy + 12 && y < yy + 66) hit = 600 + i;
    }
    if (hit_text(x, y, "LEAVE", 512, 724 + 22, 40)) hit = 690;
    if (phase != TOUCH_END) { F.pressed = hit; return; }
    F.pressed = 0;
    if (hit == 690) { set_state(ST_GAME); save_all(); return; }
    if (hit >= 600 && hit < 605) {
        char name[48], info[80];
        int price;
        shop_item(hit - 600, name, sizeof(name), &price, info, sizeof(info));
        if (game_buy(&F.G, hit - 600)) snprintf(F.shopMsg, sizeof(F.shopMsg), "Bought: %s", name);
        else snprintf(F.shopMsg, sizeof(F.shopMsg), price > 0 ? "Not enough money (or you already have it)." : "Buy a gun first.");
        F.G.moneyShown = F.G.money;
        save_all();
    }
}

static const char *CUSTOM_ITEMS[5] = {"PAINT", "ENGINE", "HANDLING", "NEON", "SPOILER"};
static const char *PAINT_NAMES[NPAINTS] = {"RED", "BLUE", "YELLOW", "WHITE", "GREEN", "BLACK", "ORANGE", "PURPLE", "CYAN", "SILVER"};
static const char *NEON_NAMES[NNEONS] = {"OFF", "RED", "BLUE", "GREEN", "PINK", "WHITE"};

static void draw_dealer(void) {
    Renderer *R = &F.R;
    char b[96];
    int i, owned = 0;
    for (i = 0; i < MAX_OWNED; i++) owned += F.G.garage[i].exists;
    r_draw_scene(R, &F.G);
    ui_begin(R);
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 0.7f);
    ui_rect(R, 112, 40, 800, 640, 0.07f, 0.07f, 0.08f, 0.95f);
    ui_rect(R, 112, 40, 800, 70, 0.95f, 0.5f, 0.08f, 1);
    ui_text(R, "GIVEAWAY MOTORS", 512, 50, 50, 1, 1, 1, 1, 1);
    fmt_money(b, sizeof(b), F.G.money);
    ui_text(R, b, 892, 124, 34, 0.55f, 1.0f, 0.55f, 1, 2);
    snprintf(b, sizeof(b), "Your garage: %d / %d cars (parked here at the dealer)", owned, MAX_OWNED);
    ui_text(R, b, 136, 132, 18, 0.85f, 0.85f, 0.85f, 1, 0);
    for (i = 0; i < CAR_MODELS; i++) {
        float y = 178.0f + i * 104.0f;
        int pr = F.pressed == 700 + i, afford = F.G.money >= CARS[i].price;
        Col pc = paint_color(model_default_paint(i));
        ui_rect(R, 132, y, 760, 94, 0.12f, 0.12f, 0.14f, 0.95f);
        ui_rect(R, 132, y, 10, 94, pc.r / 255.0f, pc.g / 255.0f, pc.b / 255.0f, 1);
        ui_text(R, CARS[i].name, 160, y + 6, 36, 1, 1, 1, 1, 0);
        snprintf(b, sizeof(b), "Top speed %d km/h   -   0-100 in %.1f s", (int)(CARS[i].maxF * 3.6f), 27.8f / (CARS[i].acc * 0.75f));
        ui_text(R, b, 160, y + 50, 18, 0.85f, 0.85f, 0.85f, 1, 0);
        {
            float bar = CARS[i].maxF / 34.0f;
            ui_rect(R, 160, y + 76, 300, 6, 0.25f, 0.25f, 0.28f, 1);
            ui_rect(R, 160, y + 76, 300 * bar, 6, 1.0f, 0.55f, 0.1f, 1);
        }
        ui_rect(R, 712, y + 18, 160, 58, pr ? 0.9f : (afford ? 0.2f : 0.25f), pr ? 0.2f : (afford ? 0.55f : 0.25f), pr ? 0.2f : 0.25f, 1);
        fmt_money(b, sizeof(b), CARS[i].price);
        ui_text(R, b, 792, y + 28, 30, 1, 1, 1, 1, 1);
    }
    if (F.shopMsg[0]) ui_text(R, F.shopMsg, 512, 600, 20, 1, 0.85f, 0.4f, 1, 1);
    ui_text(R, "Bought cars are yours: upgrade them at BLOCK CUSTOMS (blue marker).", 512, 634, 16, 0.75f, 0.75f, 0.75f, 1, 1);
    big_button("LEAVE", 512, 724, 40, F.pressed == 790, 1);
    ui_flush(R);
}

static void dealer_tap(float x, float y, int phase) {
    int i, hit = 0;
    for (i = 0; i < CAR_MODELS; i++) {
        float yy = 178.0f + i * 104.0f;
        if (x > 712 && x < 872 && y > yy + 18 && y < yy + 76) hit = 700 + i;
    }
    if (hit_text(x, y, "LEAVE", 512, 746, 40)) hit = 790;
    if (phase != TOUCH_END) { F.pressed = hit; return; }
    F.pressed = 0;
    if (hit == 790) { set_state(ST_GAME); return; }
    if (hit >= 700 && hit < 700 + CAR_MODELS) {
        int r = game_buy_car(&F.G, hit - 700);
        if (r > 0) {
            snprintf(F.G.banner, sizeof(F.G.banner), "NEW CAR!");
            snprintf(F.G.sub, sizeof(F.G.sub), "Your %s is ready. Upgrade it at BLOCK CUSTOMS.", CARS[hit - 700].name);
            F.G.bannerT = 4.0f; F.G.bannerGood = 1;
            F.G.moneyShown = F.G.money;
            save_all();
            set_state(ST_GAME);
        } else snprintf(F.shopMsg, sizeof(F.shopMsg), r < 0 ? "Your garage is full (4 cars)." : "Not enough money.");
    }
}

static void custom_value(int item, char *b, size_t n) {
    const Vehicle *v = &F.G.veh[F.G.p.veh];
    switch (item) {
    case 0: snprintf(b, n, "%s", v->custom ? PAINT_NAMES[v->paint % NPAINTS] : "FACTORY"); break;
    case 1: snprintf(b, n, "LEVEL %d / 3", v->eng); break;
    case 2: snprintf(b, n, "LEVEL %d / 3", v->hand); break;
    case 3: snprintf(b, n, "%s", NEON_NAMES[v->neon % NNEONS]); break;
    default: snprintf(b, n, "%s", v->spoiler ? "ON" : "OFF"); break;
    }
}

static void draw_customs(void) {
    Renderer *R = &F.R;
    char b[96], val[48];
    int i;
    const Vehicle *v;
    if (F.G.p.veh < 0) { set_state(ST_GAME); return; }
    v = &F.G.veh[F.G.p.veh];
    r_draw_scene(R, &F.G);
    ui_begin(R);
    ui_rect(R, 0, 0, 520, (float)R->H, 0, 0, 0, 0.8f);
    ui_rect(R, 0, 0, 520, 70, 0.1f, 0.35f, 0.95f, 1);
    ui_text(R, "BLOCK CUSTOMS", 260, 10, 48, 1, 1, 1, 1, 1);
    snprintf(b, sizeof(b), "%s  -  top speed %d km/h", v->custom ? CARS[v->model % CAR_MODELS].name : "YOUR RIDE", (int)((v->custom ? car_top_speed(v) : 19.0f) * 3.6f));
    ui_text(R, b, 24, 84, 20, 1, 1, 1, 1, 0);
    fmt_money(b, sizeof(b), F.G.money);
    ui_text(R, b, 500, 112, 32, 0.55f, 1.0f, 0.55f, 1, 2);
    for (i = 0; i < 5; i++) {
        float y = 160.0f + i * 92.0f;
        int price = game_custom_price(&F.G, i), pr = F.pressed == 800 + i;
        ui_rect(R, 20, y, 480, 82, 0.12f, 0.12f, 0.15f, 0.95f);
        ui_text(R, CUSTOM_ITEMS[i], 36, y + 6, 32, 1, 1, 1, 1, 0);
        custom_value(i, val, sizeof(val));
        ui_text(R, val, 36, y + 46, 20, 1.0f, 0.8f, 0.3f, 1, 0);
        if (i == 0 && v->custom) { Col pc = paint_color(v->paint); ui_rect(R, 250, y + 46, 40, 22, pc.r / 255.0f, pc.g / 255.0f, pc.b / 255.0f, 1); }
        if (i == 3 && v->neon) { Col nc = neon_color(v->neon); ui_rect(R, 250, y + 46, 40, 22, nc.r / 255.0f, nc.g / 255.0f, nc.b / 255.0f, 1); }
        ui_rect(R, 330, y + 14, 156, 54, pr ? 0.9f : (price >= 0 && F.G.money >= price ? 0.15f : 0.25f), pr ? 0.2f : (price >= 0 && F.G.money >= price ? 0.45f : 0.25f), pr ? 0.2f : (price >= 0 && F.G.money >= price ? 0.9f : 0.25f), 1);
        if (price >= 0) snprintf(b, sizeof(b), "%s $%d", i == 0 || i == 3 ? "NEXT" : (i == 4 ? "SWAP" : "UP"), price);
        else snprintf(b, sizeof(b), "MAX");
        ui_text(R, b, 408, y + 26, 22, 1, 1, 1, 1, 1);
    }
    if (F.shopMsg[0]) ui_text(R, F.shopMsg, 260, 630, 18, 1, 0.85f, 0.4f, 1, 1);
    ui_text(R, v->garageSlot >= 0 ? "Saved on your own car." : "Not your car: changes are not saved.", 260, 660, 15, 0.75f, 0.75f, 0.75f, 1, 1);
    big_button("DONE", 260, 724, 40, F.pressed == 890, 1);
    ui_flush(R);
}

static void customs_tap(float x, float y, int phase) {
    int i, hit = 0;
    for (i = 0; i < 5; i++) {
        float yy = 160.0f + i * 92.0f;
        if (x > 330 && x < 486 && y > yy + 14 && y < yy + 68) hit = 800 + i;
    }
    if (hit_text(x, y, "DONE", 260, 746, 40)) hit = 890;
    if (phase != TOUCH_END) { F.pressed = hit; return; }
    F.pressed = 0;
    if (hit == 890) { set_state(ST_GAME); save_all(); return; }
    if (hit >= 800 && hit < 805) {
        if (game_custom(&F.G, hit - 800)) { F.shopMsg[0] = 0; F.G.moneyShown = F.G.money; save_all(); }
        else snprintf(F.shopMsg, sizeof(F.shopMsg), game_custom_price(&F.G, hit - 800) < 0 ? "Already maxed out." : "Not enough money.");
    }
}

static void draw_pause(void) {
    Renderer *R = &F.R;
    int i;
    const char *items[4] = {"RESUME", "MISSIONS", "SETTINGS", "MAIN MENU"};
    if (F.mode == 1) items[1] = "NEW POSTCODE";
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 0.6f);
    ui_text(R, "PAUSED", R->W * 0.5f, 130, 70, 1, 1, 1, 1, 1);
    for (i = 0; i < 4; i++) big_button(items[i], R->W * 0.5f, 290.0f + i * 100.0f, 48, F.pressed == i + 1, 1);
}

/* ------------------------------------------------------------------ invoer */

static void stick_value(float bx, float by, float kx, float ky, float *ox, float *oy) {
    float dx = (kx - bx) / STICK_R, dy = (ky - by) / STICK_R;
    float l = sqrtf(dx * dx + dy * dy);
    if (l > 1) { dx /= l; dy /= l; l = 1; }
    if (l < 0.1f) { dx = 0; dy = 0; }
    *ox = dx; *oy = -dy;
}

static void game_touch(TSlot *s, int phase) {
    Renderer *R = &F.R;
    if (phase == TOUCH_BEGIN) {
        static const int order[7] = {CTL_PAUSE, CTL_GAS, CTL_BRAKE, CTL_JUMP, CTL_ACTION, CTL_FIRE, CTL_WSWITCH};
        int k;
        s->ctl = CTL_NONE;
        if (s->x < 224 && s->y < 236) { s->ctl = CTL_UI; bm_open(); return; }
        for (k = 0; k < 7; k++) {
            float x, y, r;
            btn_pos(order[k], &x, &y, &r);
            if (r <= 0) continue;
            if ((s->x - x) * (s->x - x) + (s->y - y) * (s->y - y) < (r * 1.25f) * (r * 1.25f)) {
                s->ctl = order[k];
                if (order[k] == CTL_JUMP) F.jumpEdge = 1;
                if (order[k] == CTL_ACTION) F.actionEdge = 1;
                if (order[k] == CTL_WSWITCH) F.switchEdge = 1;
                if (order[k] == CTL_PAUSE) { set_state(ST_PAUSE); save_all(); }
                return;
            }
        }
        if (s->x < R->W * 0.5f && s->y > R->H * 0.3f && !F.lOn) {
            s->ctl = CTL_LSTICK; F.lOn = 1;
            F.lbx = s->x; F.lby = s->y; F.lkx = s->x; F.lky = s->y;
        } else if (s->x >= R->W * 0.5f && s->y > R->H * 0.3f && !F.rOn) {
            s->ctl = CTL_RSTICK; F.rOn = 1;
            F.rbx = s->x; F.rby = s->y; F.rkx = s->x; F.rky = s->y;
        }
        return;
    }
    if (s->ctl == CTL_LSTICK) {
        if (phase == TOUCH_END) { F.lOn = 0; F.lbx = 150; F.lby = R->H - 150.0f; }
        else { F.lkx = s->x; F.lky = s->y; }
    } else if (s->ctl == CTL_RSTICK) {
        if (phase == TOUCH_END) { F.rOn = 0; F.rbx = R->W - 170.0f; F.rby = R->H - 150.0f; }
        else { F.rkx = s->x; F.rky = s->y; }
    }
}

static void start_game(void) {
    set_state(ST_GAME);
    F.fade = 1.0f;
}

static void menu_tap(float x, float y, int phase) {
    Renderer *R = &F.R;
    int i, hit = 0;
    if (F.state == ST_LOGO) {
        if (phase == TOUCH_END && F.st > 0.5f) { set_state(ST_LOAD); F.loadStart = plat_time(); }
        return;
    }
    if (F.state == ST_MENU) {
        for (i = 0; i < 5; i++)
            if (hit_text(x, y, MENU_ITEMS[i], MENU_X(i), R->H - 74.0f + 20, 38)) hit = i + 1;
        if (phase != TOUCH_END) { F.pressed = hit; return; }
        F.pressed = 0;
        if (hit == 1) start_game();
        else if (hit == 2) { set_state(ST_MISSIONS); F.missionsFrom = ST_MENU; }
        else if (hit == 3) open_nl_input(ST_MENU);
        else if (hit == 4) set_state(ST_SETTINGS);
        else if (hit == 5) set_state(ST_CREDITS);
        return;
    }
    if (F.state == ST_NLINPUT) { nl_input_tap(x, y, phase); return; }
    if (F.state == ST_BIGMAP) { bigmap_touch(x, y, phase); return; }
    if (F.state == ST_SHOP) { shop_tap(x, y, phase); return; }
    if (F.state == ST_DEALER) { dealer_tap(x, y, phase); return; }
    if (F.state == ST_CUSTOMS) { customs_tap(x, y, phase); return; }
    if (F.state == ST_NLLOAD) {
        if (F.job.err && phase == TOUCH_END) {
            if (F.mode != 0) nl_exit_to_city();
            open_nl_input(F.nlFrom == ST_PAUSE ? ST_MENU : F.nlFrom);
        }
        return;
    }
    if (F.state == ST_MISSIONS) {
        for (i = 0; i < NMISSIONS; i++) {
            float cxl = (i & 1) ? 522.0f : 62.0f, cyl = 96.0f + (i / 2) * 98.0f;
            if (x > cxl && x < cxl + 440 && y > cyl && y < cyl + 90) hit = i + 1;
        }
        if (hit_text(x, y, "BACK", R->W * 0.5f, R->H - 60.0f + 22, 44)) hit = 100;
        if (phase != TOUCH_END) { F.pressed = hit; return; }
        F.pressed = 0;
        if (hit == 100) set_state(F.missionsFrom == ST_PAUSE ? ST_PAUSE : ST_MENU);
        else if (hit >= 1 && hit <= NMISSIONS && F.G.mdef[hit - 1].requireWeapon && !game_has_weapon(&F.G)) {
            /* geen wapen: start in de stad bij BLOCK ARMS */
            if (F.missionsFrom == ST_PAUSE) set_state(ST_GAME); else start_game();
            game_teleport(&F.G, F.G.w->poi[POI_GUNSHOP].x + 3.0f, F.G.w->poi[POI_GUNSHOP].z - 4.0f, 0);
            game_message(&F.G, "You need a gun for the heist. Walk into BLOCK ARMS (red marker).", 4.0f);
        }
        else if (hit >= 1 && hit <= NMISSIONS) {
            if (F.missionsFrom == ST_PAUSE) set_state(ST_GAME);
            else start_game();
            game_start_mission_at(&F.G, hit - 1);
        }
        return;
    }
    if (F.state == ST_SETTINGS) {
        for (i = 0; i < 6; i++) {
            float ry = 160.0f + i * 74.0f;
            if (x > 180 && x < R->W - 180 && y > ry - 6 && y < ry + 54) hit = i + 1;
        }
        if (hit_text(x, y, "BACK", R->W * 0.5f, R->H - 74.0f + 24, 48)) hit = 10;
        if (phase != TOUCH_END) { F.pressed = hit; return; }
        F.pressed = 0;
        if (hit != 6) F.resetArm = 0;
        switch (hit) {
        case 1: F.quality = (F.quality + 1) % 3; break;
        case 2: F.tod = (F.tod + 1) % 3; break;
        case 3: F.camSpeed = (F.camSpeed + 1) % 3; break;
        case 4: F.invertY = !F.invertY; break;
        case 5: F.showFps = !F.showFps; break;
        case 6:
            if (F.resetArm) {
                for (i = 0; i < NMISSIONS; i++) F.G.completed[i] = 0;
                F.G.money = 0; F.G.moneyShown = 0;
                F.resetArm = 0;
            } else F.resetArm = 1;
            break;
        case 10: {
            int back = F.prevState == ST_PAUSE ? ST_PAUSE : ST_MENU;
            set_state(back);
            break;
        }
        default: break;
        }
        apply_settings();
        save_all();
        return;
    }
    if (F.state == ST_CREDITS) {
        if (phase == TOUCH_END) set_state(ST_MENU);
        return;
    }
    if (F.state == ST_PAUSE) {
        const char *pi[4] = {"RESUME", "MISSIONS", "SETTINGS", "MAIN MENU"};
        if (F.mode == 1) pi[1] = "NEW POSTCODE";
        for (i = 0; i < 4; i++)
            if (hit_text(x, y, pi[i], R->W * 0.5f, 290.0f + i * 100.0f + 24, 48)) hit = i + 1;
        if (phase != TOUCH_END) { F.pressed = hit; return; }
        F.pressed = 0;
        if (hit == 1) set_state(ST_GAME);
        else if (hit == 2 && F.mode == 1) open_nl_input(ST_PAUSE);
        else if (hit == 2) { set_state(ST_MISSIONS); F.missionsFrom = ST_PAUSE; }
        else if (hit == 3) { set_state(ST_SETTINGS); F.prevState = ST_PAUSE; }
        else if (hit == 4) { save_all(); nl_exit_to_city(); set_state(ST_MENU); }
    }
}

void fe_touch(long id, int phase, float x, float y) {
    int i, slot = -1;
    if (!F.ready || F.at) return;
    for (i = 0; i < NTOUCH; i++) if (F.ts[i].used && F.ts[i].id == id) { slot = i; break; }
    if (phase == TOUCH_BEGIN && slot < 0) {
        for (i = 0; i < NTOUCH; i++) if (!F.ts[i].used) { slot = i; break; }
        if (slot < 0) return;
        memset(&F.ts[slot], 0, sizeof(TSlot));
        F.ts[slot].used = 1; F.ts[slot].id = id;
        F.ts[slot].sx = x; F.ts[slot].sy = y;
    }
    if (slot < 0) return;
    F.ts[slot].x = x; F.ts[slot].y = y;
    if (F.state == ST_GAME) {
        if (phase == TOUCH_BEGIN) game_touch(&F.ts[slot], TOUCH_BEGIN);
        else game_touch(&F.ts[slot], phase);
    } else {
        F.ts[slot].ctl = CTL_UI;
        menu_tap(x, y, phase);
    }
    if (phase == TOUCH_END) F.ts[slot].used = 0;
}

static void build_input(void) {
    Input *in = &F.in;
    memset(in, 0, sizeof(*in));
    if (F.lOn) stick_value(F.lbx, F.lby, F.lkx, F.lky, &in->mx, &in->my);
    if (F.rOn) stick_value(F.rbx, F.rby, F.rkx, F.rky, &in->lx, &in->ly);
    in->jump = ctl_down(CTL_JUMP);
    in->action = ctl_down(CTL_ACTION);
    in->gas = ctl_down(CTL_GAS);
    in->brake = ctl_down(CTL_BRAKE);
    in->jumpPressed = F.jumpEdge;
    in->actionPressed = F.actionEdge;
    in->fire = ctl_down(CTL_FIRE);
    in->switchPressed = F.switchEdge;
    F.jumpEdge = F.actionEdge = F.switchEdge = 0;
}

/* ------------------------------------------------------------------ automatische test */

static Seg *seg_cur(void) {
    int i;
    for (i = 0; i < F.nseg; i++) if (F.atT >= F.seg[i].t0 && F.atT < F.seg[i].t1) return &F.seg[i];
    return NULL;
}

static void seg_add(const char *name, float t0, float t1) {
    Seg *s = &F.seg[F.nseg++];
    memset(s, 0, sizeof(*s));
    s->name = name; s->t0 = t0; s->t1 = t1;
}

static int find_veh(int type, int variant) {
    int i;
    for (i = 0; i < F.G.nveh; i++) {
        Vehicle *v = &F.G.veh[i];
        if (v->active && !v->traffic && v->type == type && (variant < 0 || v->variant == variant)) return i;
    }
    return -1;
}

static void near_vehicle_tp(int idx) {
    Vehicle *v = &F.G.veh[idx];
    V3 r = right_of(v->h);
    game_teleport(&F.G, v->x - r.x * 1.3f, v->z - r.z * 1.3f, v->h);
}

static char pendingShot[48];
static void at_shot(const char *name) {
    snprintf(pendingShot, sizeof(pendingShot), "%s", name);
    rep("screenshot %s (t=%.1f, modus=%d, dc=%d, tris=%d)", name, F.atT, game_mode(&F.G), F.lastDC, F.R.drawnTris);
}

/* script: per stap een tijd; geeft invoer voor dit frame */
static void autotest_tick(float dt) {
    Game *g = &F.G;
    Input *in = &F.in;
    float t;
    Seg *s;
    F.atT += dt;
    t = F.atT;
    memset(in, 0, sizeof(*in));
    if (F.atStep == 0) {
        F.nseg = 0;
        seg_add("lopen", 0.5f, 4.5f);
        seg_add("fiets", 6.5f, 12.5f);
        seg_add("auto", 16.5f, 22.5f);
        seg_add("missie", 25.0f, 33.0f);
        seg_add("kwaliteit-laag", 36.0f, 41.0f);
        seg_add("kwaliteit-midden", 42.0f, 47.0f);
        seg_add("kwaliteit-hoog", 48.0f, 53.0f);
        seg_add("nacht-hoog", 54.0f, 58.0f);
        seg_add("politie", 59.0f, 66.0f);
        F.atMoney0 = g->money;
        g->p.cool = 24.5f;   /* geen missie starten tijdens het rondrijden */
        rep("start spel, geld=%ld, voertuigen=%d", g->money, g->nveh);
        F.atStep = 1;
    }
    /* stappen op volgorde */
    switch (F.atStep) {
    case 1: if (t > 0.6f) { at_shot("02_spawn"); F.atStep++; } break;
    case 2: if (t > 3.0f) { at_shot("03_walk"); F.atStep++; } break;
    case 3: if (t > 4.6f) { in->jumpPressed = 1; F.atStep++; } break;
    case 4: if (t > 4.85f) { at_shot("04_jump"); rep("sprong: y=%.2f (grond %.2f)", g->p.y, world_ground_y(g->w, g->p.x, g->p.z)); F.atStep++; } break;
    case 5:
        if (t > 5.5f) {
            F.atBike = find_veh(VEH_BIKE, 0);
            if (F.atBike >= 0) near_vehicle_tp(F.atBike);
            F.atStep++;
        }
        break;
    case 6:
        if (t > 5.9f) {
            in->actionPressed = 1;
            F.atStep++;
        }
        break;
    case 7: if (t > 6.2f) { rep("op de fiets: modus=%d (1=fiets)", game_mode(g)); F.atStep++; } break;
    case 8: if (t > 10.0f) { at_shot("05_bike"); F.atStep++; } break;
    case 9: if (t > 14.2f) { in->actionPressed = 1; F.atStep++; } break;
    case 10: if (t > 14.5f) { rep("van de fiets: modus=%d (0=lopen)", game_mode(g)); F.atStep++; } break;
    case 11:
        if (t > 15.0f) {
            F.atCar = find_veh(VEH_CAR, -1);
            if (F.atCar >= 0) near_vehicle_tp(F.atCar);
            F.atStep++;
        }
        break;
    case 12: if (t > 15.4f) { in->actionPressed = 1; F.atStep++; } break;
    case 13: if (t > 15.8f) { rep("in de auto: modus=%d (2=auto)", game_mode(g)); F.atStep++; } break;
    case 14: if (t > 20.0f) { at_shot("06_car"); F.atStep++; } break;
    case 15: if (t > 24.3f) { in->actionPressed = 1; F.atStep++; } break;
    case 16:
        if (t > 25.0f) {
            rep("uit de auto: modus=%d", game_mode(g));
            game_teleport(g, g->mdef[0].giver.x, g->mdef[0].giver.z + 0.5f, PI_F);
            F.atStep++;
        }
        break;
    case 17: if (t > 25.8f) { rep("missie actief=%d (0 verwacht)", g->mission); at_shot("07_mission_start"); F.atStep++; } break;
    case 18:
        if (t > 26.5f) {
            int b = find_veh(VEH_BIKE, 0);
            if (b >= 0) near_vehicle_tp(b);
            F.atStep++;
        }
        break;
    case 19: if (t > 26.9f) { in->actionPressed = 1; F.atStep++; } break;
    default:
        if (F.atStep >= 20 && F.atStep < 26) {
            /* door de checkpoints van missie 0 (teleport, op de fiets) */
            int k = F.atStep - 20;
            if (t > 27.3f + k * 0.9f) {
                if (g->mission == 0 && g->mstep < g->mdef[0].ncp) {
                    Checkpoint *c = &g->mdef[0].cp[g->mstep];
                    game_teleport(g, c->x, c->z, g->p.h);
                }
                if (k == 2) at_shot("08_mission_cp");
                F.atStep++;
            }
        } else if (F.atStep == 26 && t > 32.8f) {
            rep("missie 0 gehaald=%d, geld %ld -> %ld, banner='%s'", g->completed[0], F.atMoney0, g->money, g->banner);
            at_shot("09_mission_passed");
            F.atStep++;
        } else if (F.atStep == 27 && t > 33.5f) {
            in->actionPressed = 1;  /* van de fiets */
            F.atStep++;
        } else if (F.atStep == 28 && t > 35.5f) {
            game_teleport(g, road_c(4) - 3, road_c(3) - 8, PI_F * 0.25f);
            F.quality = 0; apply_settings();
            F.atStep++;
        } else if (F.atStep == 29 && t > 39.0f) { at_shot("10_quality_low"); F.atStep++; }
        else if (F.atStep == 30 && t > 41.5f) { F.quality = 1; apply_settings(); F.atStep++; }
        else if (F.atStep == 31 && t > 45.0f) { at_shot("11_quality_medium"); F.atStep++; }
        else if (F.atStep == 32 && t > 47.5f) { F.quality = 2; apply_settings(); F.atStep++; }
        else if (F.atStep == 33 && t > 51.0f) { at_shot("12_quality_high"); F.atStep++; }
        else if (F.atStep == 34 && t > 53.5f) { F.tod = 1; apply_settings(); F.atStep++; }
        else if (F.atStep == 35 && t > 56.0f) { at_shot("13_night"); F.atStep++; }
        else if (F.atStep == 36 && t > 58.5f) {
            F.tod = 0; apply_settings();
            F.quality = 1; apply_settings();
            game_add_wanted(g, 2);
            rep("misdaad gesimuleerd: sterren=%d", g->wanted);
            F.atStep++;
        }
        else if (F.atStep == 37 && t > 62.5f) {
            int k, np = 0;
            for (k = 0; k < MAX_POLICE; k++) np += g->veh[g->firstPolice + k].active;
            at_shot("14_police");
            rep("politie: %d auto's actief, sterren=%d, bust-timer=%.1f, gezondheid=%.0f", np, g->wanted, g->bustT, g->health);
            F.atStep++;
        }
        else if (F.atStep == 38 && t > 66.0f) {
            float h0 = g->health;
            game_damage(g, 30.0f);
            rep("gezondheid na schade: %.0f -> %.0f, sterren=%d (banner '%s')", h0, g->health, g->wanted, g->banner);
            F.atStep++;
        }
        else if (F.atStep == 39 && t > 67.0f) {
            F.missionsFrom = ST_MENU;
            set_state(ST_MISSIONS);
            F.atStep++;
        }
        else if (F.atStep == 40 && t > 70.0f) {
            game_start_mission_at(g, 9);
            rep("missiekeuze -> GRAND TOUR: missie=%d, modus=%d (2=auto)", g->mission, game_mode(g));
            F.atStep++;
        }
        else if (F.atStep == 41 && t > 72.0f) { at_shot("16_mission_select_start"); F.atStep++; }
        else if (F.atStep == 42 && t > 74.5f) {
            double rx, ry;
            const char *nm;
            ns_pack_load();
            nm = ns_pack_start(0, &rx, &ry);
            F.atStep++;
            if (!nm) { rep("NL: GEEN ingebouwd kaartpakket gevonden"); F.atStep = 70; return; }
            rep("NL ingebouwd: start bij '%s' (offline pakket)", nm);
            nl_play_at(rx, ry, nm, ST_MENU);
            return;
        }
        else if (F.atStep == 43 && F.mode == 1) {
            int tl, nb, nv, fp, fc, fn;
            ns_stats(&tl, &nb, &nv, &fp, &fc, &fn);
            rep("NL ingebouwd geladen in %.1f s: %d tegels (pakket %d, opgeslagen %d, internet %d), %d gebouwen, %d vertices, geheugen %ld kB",
                F.nlLoadSecs, tl, fp, fc, fn, nb, nv, plat_memory_kb());
            F.atNlT = t;
            F.atStep++;
        }
        else if (F.atStep == 44 && t > F.atNlT + 1.5f) { at_shot("17_nl_hb"); F.atStep++; }
        else if (F.atStep == 45 && t > F.atNlT + 2.0f) { in->actionPressed = 1; F.atStep++; }
        else if (F.atStep == 46 && t > F.atNlT + 7.5f) {
            const char *sn = nl_street_name(g->w, g->p.x, g->p.z);
            at_shot("18_nl_drive");
            rep("NL: modus=%d, straat='%s'", game_mode(g), sn ? sn : "-");
            F.atStep++;
        }
        else if (F.atStep == 47 && t > F.atNlT + 8.0f) {
            /* streaming: 600 m naar het oosten reizen, nieuwe tegels moeten bijladen */
            game_teleport(g, g->p.x + 600.0f, g->p.z, g->p.h);
            rep("NL: 600 m verplaatst, tegels laden...");
            F.atNlT2 = t;
            F.atStep++;
        }
        else if (F.atStep == 48 && (ns_ready_at(g->p.x, g->p.z) && ns_busy() == 0 ? 1 : t > F.atNlT2 + 20.0f)) {
            int tl, nb, nv, fp, fc, fn;
            ns_stats(&tl, &nb, &nv, &fp, &fc, &fn);
            rep("NL streaming na %.1f s: klaar=%d, %d tegels in geheugen, pakket-tegels totaal %d, %d gebouwen", t - F.atNlT2, ns_ready_at(g->p.x, g->p.z), tl, fp, nb);
            at_shot("19_nl_stream");
            F.atStep++;
        }
        else if (F.atStep == 49) {
            bm_open();
            F.atStep++;
            return;
        }
        else if (F.atStep == 50 && F.state == ST_GAME) {
            nl_exit_to_city();
            rep("NL online: postcode 1012JS (Amsterdam, buiten de regio -> internet)");
            F.atStep++;
            nl_play_pc("1012JS", ST_MENU);
            return;
        }
        else if (F.atStep == 51 && F.mode == 1) {
            int tl, nb, nv, fp, fc, fn;
            ns_stats(&tl, &nb, &nv, &fp, &fc, &fn);
            rep("NL online geladen in %.1f s: %d tegels (internet %d, opgeslagen %d), %d gebouwen", F.nlLoadSecs, tl, fn, fc, nb);
            game_teleport(g, g->p.x + 300.0f, g->p.z - 300.0f, g->p.h);
            F.atNlT2 = t;
            F.atStep++;
        }
        else if (F.atStep == 52 && (ns_ready_at(g->p.x, g->p.z) && ns_busy() == 0 ? 1 : t > F.atNlT2 + 25.0f)) {
            int tl, nb, nv, fp, fc, fn;
            const char *sn = nl_street_name(g->w, g->p.x, g->p.z);
            ns_stats(&tl, &nb, &nv, &fp, &fc, &fn);
            rep("NL online streaming na %.1f s: klaar=%d, internet-tegels totaal %d, straat='%s'", t - F.atNlT2, ns_ready_at(g->p.x, g->p.z), fn, sn ? sn : "-");
            at_shot("20_nl_online");
            F.atStep++;
        }
        else if (F.atStep == 53) {
            int k;
            nl_exit_to_city();
            idx_load();
            k = idx_find("9711LM");
            if (k >= 0) idx_delete(k);
            rep("Downloadmodus: 9711LM (Groningen) downloaden");
            dl_enqueue("9711LM");
            F.atDlPhase = 1;
            F.atStep++;
            open_nl_input(ST_MENU);
            return;
        }
        else if (F.atStep == 54 && F.mode == 1) {
            int tl, nb, nv, fp, fc, fn;
            ns_stats(&tl, &nb, &nv, &fp, &fc, &fn);
            rep("Offline gespeeld (9711LM): %d tegels uit opslag, %d van internet, %d gebouwen", fc, fn, nb);
            F.atNlT = t;
            F.atStep++;
        }
        else if (F.atStep == 55 && t > F.atNlT + 2.0f) { at_shot("22_nl_offline"); F.atStep++; }
        else if (F.atStep == 56 && t > F.atNlT + 3.0f) {
            int k;
            nl_exit_to_city();
            idx_load();
            k = idx_find("9711LM");
            if (k >= 0) idx_delete(k);
            idx_load();
            rep("Verwijderd: 9711LM nog in lijst=%d", idx_find("9711LM") >= 0);
            F.atStep = 70;
        }
        else if (F.atStep == 70) {
            if (g->p.veh >= 0) { g->veh[g->p.veh].speed = 0; g->p.veh = -1; }
            g->mission = -1;
            g->money += 3000;
            g->wanted = 0;
            rep("Wapenwinkel: naar BLOCK ARMS");
            game_teleport(g, g->w->poi[POI_GUNSHOP].x, g->w->poi[POI_GUNSHOP].z + 3.0f, PI_F);
            F.atStep++;
        }
        else if (F.atStep == 71) {
            game_teleport(g, g->w->poi[POI_GUNSHOP].x, g->w->poi[POI_GUNSHOP].z, PI_F);
            F.atStep++;
        }
        else if (F.atStep == 72 && g->owned[1]) {
            /* schieten: naar de dichtstbijzijnde voetganger */
            int i, best = -1;
            float bd = 1e9f;
            for (i = 0; i < MAX_PED - MAX_COPS; i++) {
                float dx = g->ped[i].x - g->p.x, dz = g->ped[i].z - g->p.z;
                if (g->ped[i].active && g->ped[i].state != PED_DOWN && dx * dx + dz * dz < bd) { bd = dx * dx + dz * dz; best = i; }
            }
            if (best >= 0) {
                Ped *p = &g->ped[best];
                game_teleport(g, p->x - 6.0f, p->z, PI_F * 0.5f);
                g->camYaw = PI_F * 0.5f;
            }
            F.atT3 = t;
            F.atStep++;
        }
        else if (F.atStep == 73 && t > F.atT3 + 0.8f) { at_shot("24_shooting"); F.atStep++; }
        else if (F.atStep == 74 && t > F.atT3 + 2.0f) {
            rep("Schieten: %d schoten, %d raak, sterren=%d, kogels over %d", g->statShots, g->statHits, g->wanted, g->ammo[1]);
            g->wanted = 0;
            {
                int k;
                for (k = 0; k < MAX_POLICE; k++) g->veh[g->firstPolice + k].active = 0;
                for (k = MAX_PED - MAX_COPS; k < MAX_PED; k++) g->ped[k].active = 0;
            }
            game_start_mission_at(g, 10);
            rep("Overval gestart: missie=%d", g->mission);
            game_teleport(g, g->w->poi[POI_VAULT].x, g->w->poi[POI_VAULT].z, PI_F);
            F.atT3 = t;
            F.atStep++;
        }
        else if (F.atStep == 75 && t > F.atT3 + 6.0f) {
            at_shot("25_heist_alarm");
            rep("Overval: alarm, sterren=%d, kluis %.0f%%", g->wanted, 100.0f * g->holdT / 15.0f);
            F.atStep++;
        }
        else if (F.atStep == 76 && (g->mstep >= 1 || t > F.atT3 + 30.0f)) {
            int k, cops = 0;
            for (k = MAX_PED - MAX_COPS; k < MAX_PED; k++) cops += g->ped[k].active;
            rep("Kluis open=%d na %.1f s, agenten te voet=%d, agentschoten=%d, gezondheid=%.0f, vest=%.0f", g->mstep >= 1, t - F.atT3, cops, g->copShots, g->health, g->armor);
            at_shot("26_heist_cops");
            g->wanted = 0;
            for (k = 0; k < MAX_POLICE; k++) g->veh[g->firstPolice + k].active = 0;
            for (k = MAX_PED - MAX_COPS; k < MAX_PED; k++) g->ped[k].active = 0;
            g->health = 100;
            game_teleport(g, g->w->poi[POI_GARAGE].x, g->w->poi[POI_GARAGE].z, 0);
            F.atT3 = t;
            F.atStep++;
        }
        else if (F.atStep == 77 && t > F.atT3 + 1.5f) {
            rep("Overval klaar=%d, banner='%s', geld $%ld", g->completed[10], g->banner, g->money);
            at_shot("27_heist_passed");
            F.atStep = 78;
        }
        else if (F.atStep == 78) {
            if (g->p.veh >= 0) { g->veh[g->p.veh].speed = 0; g->p.veh = -1; }
            g->money += 30000;
            g->wanted = 0;
            game_teleport(g, g->w->poi[POI_DEALER].x, g->w->poi[POI_DEALER].z + 4.0f, 0);
            rep("Autodealer: GIVEAWAY MOTORS");
            F.atStep++;
        }
        else if (F.atStep == 79) { game_teleport(g, g->w->poi[POI_DEALER].x, g->w->poi[POI_DEALER].z, 0); F.atStep++; }
        else if (F.atStep == 80 && g->p.veh >= 0) {
            rep("In de nieuwe auto: model %s, eigen=%d", CARS[g->veh[g->p.veh].model].name, g->veh[g->p.veh].garageSlot >= 0);
            /* naar de ringweg voor een snelheidstest */
            game_teleport(g, road_c(0) + LANE_OFF, road_c(1), 0);
            F.atMaxSpeed = 0;
            F.atT3 = t;
            F.atStep++;
        }
        else if (F.atStep == 81 && t > F.atT3 + 7.0f) {
            at_shot("29_supercar");
            rep("BOLT X topsnelheid in test: %.0f km/h", F.atMaxSpeed * 3.6f);
            g->veh[g->p.veh].speed = 0;
            game_teleport(g, g->w->poi[POI_CUSTOMS].x, g->w->poi[POI_CUSTOMS].z + 8.0f, PI_F);
            F.atStep++;
        }
        else if (F.atStep == 82) {
            g->veh[g->p.veh].speed = 0;
            game_teleport(g, g->w->poi[POI_CUSTOMS].x, g->w->poi[POI_CUSTOMS].z, PI_F);
            F.atStep = 84;
        }
        else if (F.atStep == 85) {
            game_teleport(g, road_c(0) + LANE_OFF, road_c(1), 0);
            g->veh[g->p.veh].speed = 0;
            g->camYaw = PI_F * 0.75f;
            F.atMaxSpeed = 0;
            F.atT3 = t;
            F.atStep++;
        }
        else if (F.atStep == 86 && t > F.atT3 + 1.0f) { at_shot("31_custom_car"); F.atStep++; }
        else if (F.atStep == 87 && t > F.atT3 + 8.0f) {
            rep("Na tuning topsnelheid in test: %.0f km/h", F.atMaxSpeed * 3.6f);
            F.atStep = 88;
        }
        else if (F.atStep == 88) {
            bm_open();
            F.bmScale = 1.4f;
            F.bmCx = F.G.w->poi[POI_BANK].x; F.bmCz = F.G.w->poi[POI_BANK].z;
            F.atStep = 89;
            return;
        }
        else if (F.atStep == 89 && F.state == ST_GAME) {
            F.atStep = 90;
        }
        else if (F.atStep == 90) {
            int i;
            rep("---- FPS per onderdeel (echte tijd; frames met screenshot niet meegeteld) ----");
            for (i = 0; i < F.nseg; i++) {
                Seg *q = &F.seg[i];
                if (q->frames > 0)
                    rep("%-18s gem %.1f fps, traagste frame %.1f ms (= %.1f fps), %ld draw calls, %ld driehoeken gem.",
                        q->name, q->frames / q->time, q->worst * 1000.0f, q->worst > 0 ? 1.0f / q->worst : 0.0f,
                        q->dc / q->frames, q->tris / q->frames);
            }
            rep("laadtijd %.2f s, geheugen %ld kB, stad %d vertices, %d botsvormen", F.loadSecs, plat_memory_kb(), F.W.verts_total, F.W.nboxes);
            rep("geld verdiend in test: %ld, missies gehaald: %d, in/uitstappen: %d, sprongen: %d", g->money - F.atMoney0, g->statMissionsPassed, g->statEnter, g->statJumps);
            plat_write_text("autotest_report.txt", F.rep);
            F.quality = plat_load_int("quality", 1);
            F.tod = plat_load_int("tod", 0);
            apply_settings();
            F.atStep++;
            F.at = 0;
            plat_autotest_done();
            /* echte voortgang terugzetten */
            {
                int i2, done = plat_load_int("done", 0);
                game_init(&F.G, &F.W);
                F.G.money = plat_load_int("money", 0);
                F.G.moneyShown = F.G.money;
                for (i2 = 0; i2 < NMISSIONS; i2++) F.G.completed[i2] = (done >> i2) & 1;
                apply_settings();
            }
            set_state(ST_MENU);
            return;
        }
        break;
    }
    /* doorlopende invoer per fase */
    if (t > 0.5f && t < 4.5f) { in->my = 1.0f; in->mx = 0.25f; in->lx = 0.25f; }
    if (t > 6.3f && t < 12.5f) { in->gas = 1; in->mx = 0.35f * sinf(t * 0.9f); }
    if (t > 12.5f && t < 14.1f) in->brake = 1;
    if (t > 16.0f && t < 22.5f) { in->gas = 1; in->mx = 0.4f * sinf(t * 0.7f); }
    if (t > 22.5f && t < 24.2f) in->brake = 1;
    if (t > 36.0f && t < 58.0f) { in->lx = 0.35f; in->my = 0.0f; }
    if (F.atStep >= 73 && F.atStep <= 74) in->fire = 1;
    if ((F.atStep == 81 && t > F.atT3 + 0.3f) || (F.atStep >= 86 && F.atStep <= 87 && t > F.atT3 + 1.2f)) {
        in->gas = 1;
        if (g->p.veh >= 0 && fabsf(g->veh[g->p.veh].speed) > F.atMaxSpeed) F.atMaxSpeed = fabsf(g->veh[g->p.veh].speed);
    }
    if (F.mode == 1 && F.atStep >= 45 && F.atStep <= 47 && t > F.atNlT + 2.6f && t < F.atNlT + 7.4f) { in->gas = 1; in->mx = 0.1f; }
    s = seg_cur();
    if (F.skipStat > 0) { F.skipStat--; s = NULL; }  /* frames rond een screenshot niet meetellen */
    if (s) {
        s->frames++;
        s->time += dt;
        if (dt > s->worst && s->frames > 3) s->worst = dt;
        s->dc += F.lastDC;
        s->tris += F.R.drawnTris;
    }
}

/* ------------------------------------------------------------------ hoofdlus */

void fe_frame(double now) {
    float dt;
    Renderer *R = &F.R;
    if (!F.ready) return;
    dt = F.last > 0 ? (float)(now - F.last) : 1.0f / 30.0f;
    if (dt < 0) dt = 0;
    if (dt > 0.25f) dt = 0.25f;
    F.last = now;
    F.t += dt;
    F.st += dt;

    /* fps-meter */
    F.fpsAcc += dt; F.fpsFrames++;
    if (dt > F.worst) F.worst = dt;
    if (F.fpsAcc >= 1.0f) {
        F.fps = F.fpsFrames / F.fpsAcc;
        F.worstShown = F.worst > 0 ? 1.0f / F.worst : 0;
        F.fpsAcc = 0; F.fpsFrames = 0; F.worst = 0;
    }

    R->drawCalls = 0;
    if (F.loadDone) dl_tick();
    if (F.mode == 1 && (F.state == ST_GAME || F.state == ST_PAUSE || F.state == ST_BIGMAP)) ns_update(F.G.p.x, F.G.p.z);
    glViewport(0, 0, R->W, R->H);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    switch (F.state) {
    case ST_LOGO:
        if (F.st > 2.5f) { set_state(ST_LOAD); F.loadStart = plat_time(); }
        ui_begin(R);
        draw_logo_and_load();
        ui_flush(R);
        break;
    case ST_LOAD: {
        double t0 = plat_time();
        int n = 0;
        while (F.loadStep < F.loadTotal && (n == 0 || plat_time() - t0 < 0.025)) { load_step(); n++; }
        F.loadShown = approachf(F.loadShown, (float)F.loadStep / (float)F.loadTotal, dt * 0.9f);
        if (F.st > (float)(F.tip + 1) * 3.2f) F.tip++;
        ui_begin(R);
        draw_logo_and_load();
        ui_flush(R);
        if (F.loadDone && F.loadShown >= 0.999f && F.st > 2.0f) set_state(ST_MENU);
        break;
    }
    case ST_MENU:
        if (F.at && F.st > 1.5f && F.atStep == 0) {
            ui_begin(R); draw_menu(); ui_flush(R);
            at_shot("01_menu");
            start_game();
            break;
        }
        ui_begin(R); draw_menu(); ui_flush(R);
        break;
    case ST_SETTINGS:
        ui_begin(R); draw_settings(); ui_flush(R);
        break;
    case ST_CREDITS:
        ui_begin(R); draw_credits(); ui_flush(R);
        break;
    case ST_NLINPUT:
        ui_begin(R); draw_nl_input(); ui_flush(R);
        if (F.at && F.atDlPhase == 1 && F.dlPhase == 0 && F.nQueue == 0 && F.st > 1.0f) {
            int k = idx_find("9711LM");
            rep("Download klaar: '%s' in lijst=%d", F.dlLast, k >= 0);
            at_shot("21_nl_downloads");
            F.atDlPhase = 2;
            if (k >= 0) nl_play_at(F.areas[k].rx, F.areas[k].ry, F.areas[k].name, ST_MENU);
            else { set_state(ST_GAME); F.atStep = 70; }
        }
        break;
    case ST_NLLOAD:
        nl_tick();
        if (F.state == ST_NLLOAD) {
            ui_begin(R); draw_nl_load(); ui_flush(R);
            if (F.at && F.job.err) {
                rep("NL MISLUKT: %s", F.job.msg);
                if (F.mode != 0) nl_exit_to_city();
                set_state(ST_GAME);
                F.atStep = F.atStep <= 52 ? 53 : 70;
            }
        }
        break;
    case ST_SHOP:
        draw_shop();
        if (F.at && F.st > 1.0f) {
            at_shot("23_gunshop");
            game_buy(&F.G, 0);
            game_buy(&F.G, 4);
            rep("Winkel: pistool=%d (%d kogels), vest=%.0f, geld over $%ld", F.G.owned[1], F.G.ammo[1], F.G.armor, F.G.money);
            set_state(ST_GAME);
        }
        break;
    case ST_DEALER:
        draw_dealer();
        if (F.at && F.st > 1.0f) {
            at_shot("28_dealer");
            rep("Dealer: BOLT X kopen: %d (geld voor $%ld)", game_buy_car(&F.G, 3), F.G.money);
            set_state(ST_GAME);
        }
        break;
    case ST_CUSTOMS:
        draw_customs();
        if (F.at && F.st > 1.0f && F.atStep == 84) {
            int ok = game_custom(&F.G, 0) + game_custom(&F.G, 1) + game_custom(&F.G, 1) + game_custom(&F.G, 1) + game_custom(&F.G, 2) + game_custom(&F.G, 3) + game_custom(&F.G, 3) + game_custom(&F.G, 4);
            at_shot("30_customs");
            rep("Customs: %d aanpassingen, motor %d, handling %d, neon %d, spoiler %d, top %d km/h, garage-lak %d",
                ok, F.G.veh[F.G.p.veh].eng, F.G.veh[F.G.p.veh].hand, F.G.veh[F.G.p.veh].neon, F.G.veh[F.G.p.veh].spoiler,
                (int)(car_top_speed(&F.G.veh[F.G.p.veh]) * 3.6f), F.G.garage[0].paint);
            F.atStep++;
            set_state(ST_GAME);
        }
        break;
    case ST_BIGMAP:
        ui_begin(R); draw_bigmap(); ui_flush(R);
        if (F.at && F.st > 1.0f) {
            at_shot(F.mode == 1 ? "19b_bigmap" : "32_bigmap_city");
            rep("Grote kaart: overzicht geladen=%d, %d tegeltjes, zoom %.2f", F.bmOk, ns_map_count(), F.bmScale);
            set_state(ST_GAME);
        }
        break;
    case ST_MISSIONS:
        if (F.missionsFrom != ST_PAUSE) ui_begin(R);
        draw_missions();
        ui_flush(R);
        if (F.at && F.st > 1.0f) { at_shot("15_missions"); set_state(ST_GAME); }
        break;
    case ST_GAME:
    case ST_PAUSE:
        if (F.state == ST_GAME) {
            if (F.at) autotest_tick(dt);
            else build_input();
            if (F.state == ST_GAME || F.at) game_update(&F.G, &F.in, dt);
            if (F.G.shopOpen) { F.G.shopOpen = 0; F.shopMsg[0] = 0; set_state(ST_SHOP); }
            if (F.G.dealerOpen) { F.G.dealerOpen = 0; F.shopMsg[0] = 0; set_state(ST_DEALER); }
            if (F.G.customsOpen) { F.G.customsOpen = 0; F.shopMsg[0] = 0; set_state(ST_CUSTOMS); }
            if (!F.at && F.mode == 0) {
                int i, done = 0;
                for (i = 0; i < NMISSIONS; i++) if (F.G.completed[i]) done |= 1 << i;
                if (done != F.savedDone || (F.G.money != F.savedMoney && F.G.bannerT <= 0 && F.G.msgT <= 0)) save_all();
            }
        }
        r_draw_scene(R, &F.G);
        ui_begin(R);
        if (F.state == ST_GAME) draw_hud();
        else { draw_hud(); draw_pause(); }
        if (F.fade > 0) {
            ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, clampf(F.fade, 0, 1));
            F.fade -= dt * 2.0f;
        }
        ui_flush(R);
        F.perfT += dt;
        if (F.perfT > 10.0f) {
            F.perfT = 0;
            plat_log("SGB-PERF fps=%.1f min=%.1f dc=%d tris=%d modus=%d kwaliteit=%d peds=%d verkeer=%d",
                     F.fps, F.worstShown, R->drawCalls, R->drawnTris, game_mode(&F.G), F.quality, F.G.wantPeds, F.G.wantTraffic);
        }
        break;
    default: break;
    }
    F.lastDC = R->drawCalls;
    if (pendingShot[0]) {
        plat_screenshot(pendingShot, R->W, R->H);
        F.skipStat = 2;
        pendingShot[0] = 0;
    }
}
