#define _POSIX_C_SOURCE 200809L
#include "frontend.h"
#include "render.h"
#include "../Game/nlmap.h"
#include <stdlib.h>

#define NL_MAX_AREAS 64

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

typedef struct { char pc[8]; char name[96]; long kb; int heights; } NLArea;
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

enum { ST_LOGO = 0, ST_LOAD, ST_MENU, ST_SETTINGS, ST_CREDITS, ST_GAME, ST_PAUSE, ST_MISSIONS, ST_NLINPUT, ST_NLLOAD };
enum { CTL_NONE = 0, CTL_LSTICK, CTL_RSTICK, CTL_JUMP, CTL_ACTION, CTL_GAS, CTL_BRAKE, CTL_PAUSE, CTL_UI };

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
    int jumpEdge, actionEdge;
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
    int nlFrom, nlInFrom, nlOffline;
    NLJob job, dl;
    NLArea areas[NL_MAX_AREAS]; int nAreas, areaPage, delArm;
    char queue[8][8]; int nQueue;
    char dlLast[128];
    NLStats nlStats;
    float nlBuildSecs, nlTotalSecs, streetT, atNlT;
    char street[96];
    int atDlPhase;
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
    float span = F.G.w->mmSpan, org = F.G.w->mmOrg, rad = 75.0f;
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
        uv[k * 2] = (wx - org) / span; uv[k * 2 + 1] = (wz - org) / span;
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
            ui_circle(R, cx + sx * half, cy + sy * half, 8, 0, 0, 0, 0.8f);
            ui_circle(R, cx + sx * half, cy + sy * half, 6, done ? 0.4f : 1.0f, done ? 1.0f : 0.82f, 0.15f, 1);
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
        if (ctl == CTL_JUMP) { *x = W - 74; *y = H - 300; *r = 52; }
        if (ctl == CTL_ACTION) { *x = W - 196; *y = H - 330; *r = 48; }
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
        ui_text(R, F.nlStats.heightsReal ? "3D BAG + PDOK (CC BY 4.0)" : "PDOK (CC BY 4.0) - heights estimated",
                20, 246, 14, 0.9f, 0.9f, 0.9f, 0.8f, 0);
    }
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
    ui_text(R, "MISSIONS", R->W * 0.5f, 26, 56, 1, 1, 1, 1, 1);
    for (i = 0; i < NMISSIONS; i++) {
        const MissionDef *m = &F.G.mdef[i];
        float x = (i & 1) ? 522.0f : 62.0f, y = 112.0f + (i / 2) * 104.0f;
        int pr = F.pressed == i + 1, done = F.G.completed[i];
        ui_rect(R, x, y, 440, 94, pr ? 0.55f : 0.06f, pr ? 0.08f : 0.05f, pr ? 0.08f : 0.06f, 0.78f);
        ui_rect(R, x, y, 6, 94, done ? 0.35f : 0.95f, done ? 0.9f : 0.75f, done ? 0.4f : 0.15f, 1);
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

/* ------------------------------------------------------------------ echte NL-kaart: downloaden, offline opslaan, spelen
 * Een NLJob haalt de 5 bestanden van een postcode (uit de cache of van internet) en bewaart ze in Documents/nl/.
 * F.job = de job om te spelen (houdt de data vast om de wijk te bouwen),
 * F.dl  = de download-job op de achtergrond (bewaart alleen, voor offline spelen). */

static const char *NL_FILES[6] = {"geo", "bag", "roads", "water", "terrain", "bagp"};

static void job_reset(NLJob *J) {
    int i;
    for (i = 0; i < 6; i++) { free(J->buf[i]); J->buf[i] = NULL; J->len[i] = 0; J->req[i] = 0; J->done[i] = 0; J->cached[i] = 0; }
    J->active = 0; J->step = 0; J->err = 0; J->msg[0] = 0; J->bagPdok = 0; J->bytes = 0;
}

static void job_begin(NLJob *J, const char *pc, int keep) {
    job_reset(J);
    snprintf(J->pc, sizeof(J->pc), "%s", pc);
    J->keep = keep;
    J->active = 1;
    J->t0 = plat_time();
}

static void job_error(NLJob *J, const char *msg) {
    snprintf(J->msg, sizeof(J->msg), "%s", msg);
    J->err = 1;
    J->active = 0;
    plat_log("SGB-NL fout (%s): %s", J->pc, msg);
}

static void job_start(NLJob *J, int i) {
    char url[700], name[64];
    snprintf(name, sizeof(name), "nl/%s_%s.json", J->pc, NL_FILES[i]);
    if (plat_read_file(name, &J->buf[i], &J->len[i])) {
        J->done[i] = 1; J->cached[i] = 1; J->bytes += J->len[i];
        if (!J->keep && i != 0) { free(J->buf[i]); J->buf[i] = NULL; }
        return;
    }
    J->cached[i] = 0;
    if (i == 0) nl_url_geocode(url, sizeof(url), J->pc);
    else if (i == 1) nl_url_bag3d(url, sizeof(url), J->geo.cx, J->geo.cy);
    else if (i == 2) nl_url_top10(url, sizeof(url), "wegdeel_hartlijn", J->geo.cx, J->geo.cy);
    else if (i == 3) nl_url_top10(url, sizeof(url), "waterdeel_vlak", J->geo.cx, J->geo.cy);
    else if (i == 4) nl_url_top10(url, sizeof(url), "terrein_vlak", J->geo.cx, J->geo.cy);
    else nl_url_bagpdok(url, sizeof(url), J->geo.cx, J->geo.cy);
    J->req[i] = plat_http_start(url);
    if (J->req[i] <= 0) J->done[i] = -1;
}

static void job_poll(NLJob *J, int i) {
    int r;
    if (J->done[i] || J->req[i] <= 0) return;
    r = plat_http_poll(J->req[i], &J->buf[i], &J->len[i]);
    if (r == 0) return;
    J->done[i] = r > 0 ? 1 : -1;
    if (r > 0) {
        if (!(i == 0 && strstr(J->buf[0], "\"numFound\":0"))) {
            char name[64];
            snprintf(name, sizeof(name), "nl/%s_%s.json", J->pc, NL_FILES[i]);
            plat_write_file(name, J->buf[i], J->len[i]);
        }
        J->bytes += J->len[i];
        if (!J->keep && i != 0) { free(J->buf[i]); J->buf[i] = NULL; }
    }
}

static const char *job_txt(const NLJob *J, int i, char *b, int n) {
    if (J->done[i] == 1) snprintf(b, n, "%s %.0f kB", J->cached[i] ? "OK (offline)" : "OK", J->len[i] / 1024.0);
    else if (J->done[i] == -1) snprintf(b, n, "FAILED");
    else if (J->req[i]) snprintf(b, n, "downloading...");
    else snprintf(b, n, "-");
    return b;
}

static int job_complete(const NLJob *J) {
    return J->done[0] == 1 && (J->done[1] == 1 || J->done[5] == 1) && J->done[2] == 1 && J->done[3] == 1 && J->done[4] == 1;
}

/* 1 = klaar met downloaden (step 3), 0 = bezig; fout -> J->err */
static int job_tick(NLJob *J) {
    int i;
    if (!J->active) return 0;
    switch (J->step) {
    case 0:
        job_start(J, 0);
        J->step = 1;
        return 0;
    case 1:
        job_poll(J, 0);
        if (J->done[0] == -1) { job_error(J, "No connection with PDOK (postcode lookup). Is Wi-Fi on?"); return 0; }
        if (J->done[0] == 1) {
            char *copy = (char *)malloc((size_t)J->len[0] + 1);
            int ok;
            if (!copy) { job_error(J, "Out of memory"); return 0; }
            memcpy(copy, J->buf[0], (size_t)J->len[0] + 1);
            ok = nl_parse_geocode(copy, J->len[0], &J->geo);
            free(copy);
            if (!ok) {
                char name[64];
                snprintf(name, sizeof(name), "nl/%s_geo.json", J->pc);
                plat_delete_file(name);
                job_error(J, "Postcode not found.");
                return 0;
            }
            plat_log("SGB-NL %s -> RD %.0f %.0f (%s)", J->pc, J->geo.cx, J->geo.cy, J->geo.name);
            for (i = 1; i <= 4; i++) job_start(J, i);
            J->step = 2;
        }
        return 0;
    case 2: {
        int pending = 0;
        for (i = 1; i <= 5; i++) job_poll(J, i);
        if (J->done[1] == -1 && !J->req[5] && !J->done[5]) {
            plat_log("SGB-NL 3D BAG niet bereikbaar, reserve: PDOK BAG zonder hoogtes");
            J->bagPdok = 1;
            job_start(J, 5);
        }
        for (i = 1; i <= 4; i++) if (!J->done[i]) pending = 1;
        if (J->bagPdok && !J->done[5]) pending = 1;
        if (pending) return 0;
        J->step = 3;
        return 1;
    }
    default:
        return 1;
    }
}

/* ---- lijst met gedownloade gebieden (Documents/nl/index.txt) */

static void idx_save(void) {
    char buf[64 * 200];
    int i, n = 0;
    buf[0] = 0;
    for (i = 0; i < F.nAreas; i++)
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s\t%ld\t%d\t%s\n", F.areas[i].pc, F.areas[i].kb, F.areas[i].heights, F.areas[i].name);
    plat_write_file("nl/index.txt", buf, (long)n);
}

static void idx_load(void) {
    char *d = NULL, *line, *save = NULL;
    long len = 0;
    F.nAreas = 0;
    if (!plat_read_file("nl/index.txt", &d, &len)) return;
    for (line = strtok_r(d, "\n", &save); line && F.nAreas < NL_MAX_AREAS; line = strtok_r(NULL, "\n", &save)) {
        NLArea *a = &F.areas[F.nAreas];
        char *p1 = strchr(line, '\t'), *p2, *p3;
        if (!p1) continue;
        *p1 = 0; p2 = strchr(p1 + 1, '\t');
        if (!p2) continue;
        *p2 = 0; p3 = strchr(p2 + 1, '\t');
        if (!p3) continue;
        *p3 = 0;
        if (strlen(line) != 6) continue;
        snprintf(a->pc, sizeof(a->pc), "%s", line);
        a->kb = atol(p1 + 1);
        a->heights = atoi(p2 + 1);
        snprintf(a->name, sizeof(a->name), "%s", p3 + 1);
        F.nAreas++;
    }
    free(d);
}

static int idx_find(const char *pc) {
    int i;
    for (i = 0; i < F.nAreas; i++) if (strcmp(F.areas[i].pc, pc) == 0) return i;
    return -1;
}

static void idx_add(const NLJob *J) {
    int i = idx_find(J->pc);
    NLArea a;
    if (!job_complete(J)) return;
    memset(&a, 0, sizeof(a));
    snprintf(a.pc, sizeof(a.pc), "%s", J->pc);
    snprintf(a.name, sizeof(a.name), "%s", J->geo.name[0] ? J->geo.name : J->pc);
    a.kb = (J->bytes + 1023) / 1024;
    a.heights = J->done[1] == 1;
    if (i >= 0) memmove(&F.areas[1], &F.areas[0], sizeof(NLArea) * (size_t)i);   /* naar boven */
    else {
        if (F.nAreas >= NL_MAX_AREAS) F.nAreas = NL_MAX_AREAS - 1;
        memmove(&F.areas[1], &F.areas[0], sizeof(NLArea) * (size_t)F.nAreas);
        F.nAreas++;
    }
    F.areas[0] = a;
    idx_save();
}

static void idx_delete(int i) {
    int k;
    char name[64];
    if (i < 0 || i >= F.nAreas) return;
    for (k = 0; k < 6; k++) {
        snprintf(name, sizeof(name), "nl/%s_%s.json", F.areas[i].pc, NL_FILES[k]);
        plat_delete_file(name);
    }
    plat_log("SGB-NL %s verwijderd", F.areas[i].pc);
    memmove(&F.areas[i], &F.areas[i + 1], sizeof(NLArea) * (size_t)(F.nAreas - i - 1));
    F.nAreas--;
    idx_save();
}

/* ---- download-wachtrij (loopt op de achtergrond, in elk scherm) */

static void dl_enqueue(const char *pc) {
    int i;
    if (F.dl.active && strcmp(F.dl.pc, pc) == 0) return;
    for (i = 0; i < F.nQueue; i++) if (strcmp(F.queue[i], pc) == 0) return;
    if (F.nQueue >= 8) return;
    snprintf(F.queue[F.nQueue++], 8, "%s", pc);
    snprintf(F.dlLast, sizeof(F.dlLast), "%s added to the download list", pc);
}

static void dl_tick(void) {
    if (!F.dl.active) {
        if (F.nQueue == 0) return;
        job_begin(&F.dl, F.queue[0], 0);
        memmove(F.queue[0], F.queue[1], sizeof(F.queue[0]) * (size_t)(F.nQueue - 1));
        F.nQueue--;
        return;
    }
    if (job_tick(&F.dl)) {
        if (job_complete(&F.dl)) {
            idx_add(&F.dl);
            snprintf(F.dlLast, sizeof(F.dlLast), "Saved %s (%.1f MB) - plays offline now", F.dl.pc, F.dl.bytes / 1048576.0);
            plat_log("SGB-NL download klaar: %s, %ld kB, %.1f s, hoogtes %s", F.dl.pc, F.dl.bytes / 1024, plat_time() - F.dl.t0, F.dl.done[1] == 1 ? "echt" : "geschat");
        } else {
            snprintf(F.dlLast, sizeof(F.dlLast), "%s: download incomplete (no connection?) - try again", F.dl.pc);
        }
        F.dl.active = 0;
        job_reset(&F.dl);
    } else if (F.dl.err) {
        snprintf(F.dlLast, sizeof(F.dlLast), "%s: %s", F.dl.pc, F.dl.msg);
        job_reset(&F.dl);
    }
}

/* ---- spelen: wijk bouwen en NL-modus in */

static void nl_begin(const char *pc, int from) {
    job_begin(&F.job, pc, 1);
    F.nlFrom = from;
    set_state(ST_NLLOAD);
}

static void nl_exit_to_city(void) {
    if (F.mode != 1) return;
    memcpy(&F.G, &F.Gcity, sizeof(Game));
    F.R.cur = 0;
    F.mode = 0;
    apply_settings();
}

static void nl_tick(void) {
    NLJob *J = &F.job;
    if (J->err) return;
    if (J->step < 3) { job_tick(J); return; }
    if (J->step == 3) { J->step = 4; return; }   /* een frame "bouwen..." laten zien */
    if (J->step == 4) {
        int b = J->done[1] == 1 ? 1 : 5;
        double t0 = plat_time(), t1;
        int ok;
        idx_add(J);   /* alles binnen -> ook offline beschikbaar */
        ok = nl_build_world(&F.WNL, J->geo.cx, J->geo.cy,
                            J->done[b] == 1 ? J->buf[b] : NULL, J->len[b], b == 5,
                            J->done[2] == 1 ? J->buf[2] : NULL, J->len[2],
                            J->done[3] == 1 ? J->buf[3] : NULL, J->len[3],
                            J->done[4] == 1 ? J->buf[4] : NULL, J->len[4], &F.nlStats);
        t1 = plat_time();
        F.nlOffline = J->cached[0] && (J->cached[1] || J->cached[5]) && J->cached[2] && J->cached[3] && J->cached[4];
        J->step = 5;
        {
            NLGeo g = J->geo;
            job_reset(J);
            J->geo = g;
        }
        if (!ok) { job_error(J, "No map data for this place (no connection?)."); return; }
        r_upload_world(&F.R, &F.WNL, 1);
        plat_log("SGB-NL gebouwd: %d gebouwen (hoogtes %s, max %.0f m), %d wegen, %d water, %d terrein, %d vertices, %d muren; bouwen %.2f s, totaal %.1f s, %s, geheugen %ld kB",
                 F.nlStats.buildings, F.nlStats.heightsReal ? "echt" : "geschat", F.nlStats.maxH, F.nlStats.roads, F.nlStats.water,
                 F.nlStats.terrain, F.nlStats.verts, F.nlStats.segs, t1 - t0, t1 - F.job.t0, F.nlOffline ? "OFFLINE uit opslag" : "online",
                 plat_memory_kb());
        F.nlBuildSecs = (float)(t1 - t0);
        F.nlTotalSecs = (float)(t1 - F.job.t0);
        if (F.mode == 0) memcpy(&F.Gcity, &F.G, sizeof(Game));
        game_init(&F.G, &F.WNL);
        F.mode = 1;
        F.R.cur = 1;
        apply_settings();
        snprintf(F.G.banner, sizeof(F.G.banner), "REAL NL");
        snprintf(F.G.sub, sizeof(F.G.sub), "%s%s", J->geo.name, F.nlOffline ? "  (offline)" : "");
        F.G.bannerT = 5.0f; F.G.bannerGood = 1;
        set_state(ST_GAME);
        F.fade = 1.0f;
    }
}

static void draw_job_lines(const NLJob *J, float x, float y, float size, float gap) {
    Renderer *R = &F.R;
    char a[48], b[48], c[48], line[160];
    if (J->geo.name[0]) snprintf(line, sizeof(line), "Postcode %s: %s", J->pc, J->geo.name);
    else snprintf(line, sizeof(line), "Postcode %s: looking up...", J->pc);
    ui_text(R, line, x, y, size, 1, 1, 1, 1, 0);
    if (J->step < 2) return;
    snprintf(line, sizeof(line), "Buildings + heights (3D BAG): %s", job_txt(J, 1, a, sizeof(a)));
    ui_text(R, line, x, y + gap, size, 1, 1, 1, 1, 0);
    if (J->bagPdok) {
        snprintf(line, sizeof(line), "Backup buildings (PDOK, no heights): %s", job_txt(J, 5, a, sizeof(a)));
        ui_text(R, line, x, y + gap * 2, size, 1, 0.85f, 0.5f, 1, 0);
    }
    snprintf(line, sizeof(line), "Streets: %s   Water: %s   Land: %s", job_txt(J, 2, a, sizeof(a)), job_txt(J, 3, b, sizeof(b)), job_txt(J, 4, c, sizeof(c)));
    ui_text(R, line, x, y + gap * (J->bagPdok ? 3 : 2), size, 1, 1, 1, 1, 0);
}

static void draw_nl_load(void) {
    Renderer *R = &F.R;
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 1);
    draw_backdrop(0.25f, 0.0f, F.t);
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 0.55f);
    ui_text(R, "REAL NETHERLANDS", R->W * 0.5f, 60, 54, 1, 1, 1, 1, 1);
    draw_job_lines(&F.job, 110, 200, 26, 50);
    if (F.job.step >= 3 && !F.job.err) ui_text(R, "Building the 3D neighbourhood...", 110, 420, 26, 1, 0.85f, 0.4f, 1, 0);
    if (F.job.err) {
        ui_text(R, F.job.msg, R->W * 0.5f, 520, 28, 1.0f, 0.4f, 0.4f, 1, 1);
        ui_text(R, "TAP TO GO BACK", R->W * 0.5f, 600, 34, 1, 1, 1, 0.6f + 0.4f * sinf(F.t * 4), 1);
    } else {
        int k;
        for (k = 0; k < 8; k++) {
            float ang = F.t * 4.0f + k * (2 * PI_F / 8);
            ui_circle(R, R->W * 0.5f + cosf(ang) * 22, 560 + sinf(ang) * 22, 5, 1, 1, 1, 0.15f + 0.85f * (k / 7.0f));
        }
    }
    ui_text(R, "Map data: 3D BAG (TU Delft / 3DGI), BAG + TOP10NL (Kadaster, PDOK) - CC BY 4.0", R->W * 0.5f, R->H - 40.0f, 16, 0.8f, 0.8f, 0.8f, 1, 1);
}

/* ---- het REAL NL-scherm: links postcode typen, rechts de offline lijst */

#define KEY_DEL 1
#define KEY_PLAY 2
#define KEY_BACK 3
#define KEY_DL 4
#define KEY_PGUP 5
#define KEY_PGDN 6
#define AREA_ROWS 8
static const char *KROWS[4] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};

static void key_rect(int row, int col, float *x, float *y, float *w, float *h) {
    float x0[4] = {34, 34, 61, 88};
    *w = 48; *h = 52;
    *x = x0[row] + col * 54.0f;
    *y = 182.0f + row * 60.0f;
}

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
    for (r = 0; r < AREA_ROWS; r++) {
        int i = F.areaPage * AREA_ROWS + r;
        float ry = 140.0f + r * 64.0f;
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
    /* links: postcode */
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
    ui_text(R, "PLAY = go there now.  DOWNLOAD = save it for offline play.", 304, 510, 16, 0.8f, 0.8f, 0.8f, 1, 1);
    /* downloadstatus */
    ui_rect(R, 34, 538, 540, 128, 0, 0, 0, 0.55f);
    if (F.dl.active) {
        ui_text(R, "DOWNLOADING", 46, 544, 20, 0.5f, 0.8f, 1.0f, 1, 0);
        draw_job_lines(&F.dl, 46, 568, 15, 22);
    } else {
        ui_text(R, F.dlLast[0] ? F.dlLast : "No downloads running.", 46, 552, 17, 0.9f, 0.9f, 0.9f, 1, 0);
    }
    if (F.nQueue > 0) {
        snprintf(line, sizeof(line), "Waiting: %d more", F.nQueue);
        ui_text(R, line, 562, 544, 16, 1, 0.85f, 0.4f, 1, 2);
    }
    big_button("BACK", 120, 712, 40, F.pressed == KEY_BACK, 1);
    /* rechts: gedownloade gebieden */
    for (i = 0; i < F.nAreas; i++) totalMB += F.areas[i].kb / 1024.0f;
    ui_text(R, "DOWNLOADED (OFFLINE)", 610, 82, 26, 1.0f, 0.85f, 0.4f, 1, 0);
    snprintf(line, sizeof(line), "%d areas, %.1f MB on this iPad", F.nAreas, totalMB);
    ui_text(R, line, 610, 112, 16, 0.85f, 0.85f, 0.85f, 1, 0);
    if (F.nAreas == 0) ui_text(R, "Nothing saved yet. Type a postcode and tap DOWNLOAD.", 610, 150, 16, 0.8f, 0.8f, 0.8f, 1, 0);
    for (r = 0; r < AREA_ROWS; r++) {
        int idx = F.areaPage * AREA_ROWS + r;
        float ry = 140.0f + r * 64.0f, nw, ns;
        NLArea *a;
        if (idx >= F.nAreas) break;
        a = &F.areas[idx];
        ui_rect(R, 610, ry, 338, 58, F.pressed == 300 + idx ? 0.55f : 0.08f, F.pressed == 300 + idx ? 0.1f : 0.08f, 0.1f, 0.85f);
        snprintf(line, sizeof(line), "%.4s %s", a->pc, a->pc + 4);
        ui_text(R, line, 622, ry + 2, 28, 1, 1, 1, 1, 0);
        snprintf(line, sizeof(line), "%.1f MB%s", a->kb / 1024.0f, a->heights ? "" : " *");
        ui_text(R, line, 938, ry + 8, 16, 0.6f, 1.0f, 0.6f, 1, 2);
        nw = ui_text_w(R, a->name, 15);
        ns = nw > 316 ? 15.0f * 316.0f / nw : 15.0f;
        ui_text(R, a->name, 622, ry + 36, ns, 0.85f, 0.85f, 0.85f, 1, 0);
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
    else if (F.nAreas > 0) ui_text(R, "Tap an area to play it (works without Wi-Fi). * = heights estimated", 807, 720, 13, 0.75f, 0.75f, 0.75f, 1, 1);
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
        if (nl_normalize_postcode(F.pcIn, norm)) nl_begin(norm, F.nlInFrom);
    } else if (k == KEY_DL) {
        if (nl_normalize_postcode(F.pcIn, norm)) { dl_enqueue(norm); F.pcLen = 0; F.pcIn[0] = 0; }
    } else if (k == KEY_BACK) {
        set_state(F.nlInFrom == ST_PAUSE ? ST_PAUSE : ST_MENU);
    } else if (k == KEY_PGUP) {
        if (F.areaPage > 0) F.areaPage--;
    } else if (k == KEY_PGDN) {
        if ((F.areaPage + 1) * AREA_ROWS < F.nAreas) F.areaPage++;
    } else if (k >= 300 && k < 300 + NL_MAX_AREAS) {
        nl_begin(F.areas[k - 300].pc, F.nlInFrom);
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
    idx_load();
    set_state(ST_NLINPUT);
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
        static const int order[5] = {CTL_PAUSE, CTL_GAS, CTL_BRAKE, CTL_JUMP, CTL_ACTION};
        int k;
        s->ctl = CTL_NONE;
        for (k = 0; k < 5; k++) {
            float x, y, r;
            btn_pos(order[k], &x, &y, &r);
            if (r <= 0) continue;
            if ((s->x - x) * (s->x - x) + (s->y - y) * (s->y - y) < (r * 1.25f) * (r * 1.25f)) {
                s->ctl = order[k];
                if (order[k] == CTL_JUMP) F.jumpEdge = 1;
                if (order[k] == CTL_ACTION) F.actionEdge = 1;
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
    if (F.state == ST_NLLOAD) {
        if (F.job.err && phase == TOUCH_END) open_nl_input(F.nlFrom);
        return;
    }
    if (F.state == ST_MISSIONS) {
        for (i = 0; i < NMISSIONS; i++) {
            float cxl = (i & 1) ? 522.0f : 62.0f, cyl = 112.0f + (i / 2) * 104.0f;
            if (x > cxl && x < cxl + 440 && y > cyl && y < cyl + 94) hit = i + 1;
        }
        if (hit_text(x, y, "BACK", R->W * 0.5f, R->H - 60.0f + 22, 44)) hit = 100;
        if (phase != TOUCH_END) { F.pressed = hit; return; }
        F.pressed = 0;
        if (hit == 100) set_state(F.missionsFrom == ST_PAUSE ? ST_PAUSE : ST_MENU);
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
    F.jumpEdge = F.actionEdge = 0;
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
            rep("NL-modus: postcode 1012JS laden (Dam, Amsterdam)...");
            F.atStep++;
            nl_begin("1012JS", ST_MENU);
            return;
        }
        else if (F.atStep == 43 && F.mode == 1) {
            F.atNlT = t;
            rep("NL geladen in %.1f s (bouwen %.2f s): %d gebouwen (hoogtes %s, hoogste %.0f m), %d wegen, %d water, %d terrein, %d vertices, %d muren, geheugen %ld kB",
                F.nlTotalSecs, F.nlBuildSecs, F.nlStats.buildings, F.nlStats.heightsReal ? "echt uit 3D BAG" : "GESCHAT", F.nlStats.maxH,
                F.nlStats.roads, F.nlStats.water, F.nlStats.terrain, F.nlStats.verts, F.nlStats.segs, plat_memory_kb());
            F.atStep++;
        }
        else if (F.atStep == 44 && t > F.atNlT + 1.5f) { at_shot("17_nl_area"); F.atStep++; }
        else if (F.atStep == 45 && t > F.atNlT + 2.0f) { in->actionPressed = 1; F.atStep++; }
        else if (F.atStep == 46 && t > F.atNlT + 2.3f) { rep("NL: in voertuig, modus=%d", game_mode(g)); F.atStep++; }
        else if (F.atStep == 47 && t > F.atNlT + 7.0f) {
            const char *sn = nl_street_name(g->w, g->p.x, g->p.z);
            at_shot("18_nl_drive");
            rep("NL: straat='%s', positie %.0f,%.0f", sn ? sn : "-", g->p.x, g->p.z);
            F.atStep++;
        }
        else if (F.atStep == 48 && t > F.atNlT + 9.0f) {
            nl_exit_to_city();
            rep("NL-modus verlaten, terug in de stad (modus=%d)", F.mode);
            F.atStep++;
        }
        else if (F.atStep == 49) {
            /* downloadmodus: 9711LM (Groningen) downloaden, dan offline spelen */
            rep("Downloadmodus: 9711LM in de wachtrij");
            idx_load();
            {
                int k = idx_find("9711LM");
                if (k >= 0) idx_delete(k);   /* schone start */
            }
            dl_enqueue("9711LM");
            F.atDlPhase = 1;
            F.atStep++;
            open_nl_input(ST_MENU);
            return;
        }
        else if (F.atStep == 50 && F.mode == 1) {
            rep("Offline gespeeld: %s, %d gebouwen, %d wegen (alles uit opslag: %s)", F.G.sub, F.nlStats.buildings, F.nlStats.roads, F.nlOffline ? "JA" : "NEE");
            F.atNlT = t;
            F.atStep++;
        }
        else if (F.atStep == 51 && t > F.atNlT + 2.0f) { at_shot("20_nl_offline"); F.atStep++; }
        else if (F.atStep == 52 && t > F.atNlT + 3.0f) {
            int k;
            nl_exit_to_city();
            idx_load();
            k = idx_find("9711LM");
            if (k >= 0) idx_delete(k);
            idx_load();
            rep("Verwijderd: 9711LM nog in lijst=%d, gebieden over=%d", idx_find("9711LM") >= 0, F.nAreas);
            F.atStep++;
        }
        else if (F.atStep == 53) {
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
    if (F.mode == 1 && F.atStep < 50 && F.atNlT > 0 && t > F.atNlT + 2.6f && t < F.atNlT + 8.5f) { in->gas = 1; in->mx = 0.15f; }
    if (F.mode == 1 && F.atNlT > 0 && t > F.atNlT + 1.5f && t < F.atNlT + 2.0f) { in->lx = 0.0f; }
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
        if (F.at && F.atDlPhase == 1 && !F.dl.active && F.nQueue == 0 && F.st > 1.0f) {
            int k = idx_find("9711LM");
            rep("Download klaar: '%s' in lijst=%d (%s)", F.dlLast, k >= 0, k >= 0 ? F.areas[k].name : "-");
            at_shot("19_nl_downloads");
            F.atDlPhase = 2;
            if (k >= 0) nl_begin("9711LM", ST_MENU);
            else { set_state(ST_GAME); F.atStep = 53; }
        }
        break;
    case ST_NLLOAD:
        nl_tick();
        if (F.state == ST_NLLOAD) {
            ui_begin(R); draw_nl_load(); ui_flush(R);
            if (F.at && F.job.err) {
                rep("NL-modus MISLUKT: %s", F.job.msg);
                plat_screenshot("17_nl_area", R->W, R->H);
                set_state(ST_GAME);
                F.atStep = F.atStep < 49 ? 53 : 53;
            }
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
