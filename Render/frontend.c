#include "frontend.h"
#include "render.h"
#include "../Game/nlmap.h"
#include <stdlib.h>
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
    char nlPc[8];
    int nlStep, nlErr, nlBagPdok, nlFrom, nlInFrom;
    char nlErrMsg[128];
    char nlStatus[6][110];
    int nlReq[6], nlDone[6], nlCached[6];
    char *nlBuf[6]; long nlLen[6];
    NLGeo nlGeo;
    NLStats nlStats;
    double nlT0;
    float nlBuildSecs, nlTotalSecs, streetT, atNlT;
    char street[96];
    char nlRecent[5][8]; int nlRecentN;
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

/* ------------------------------------------------------------------ echte NL-kaart: postcode-scherm en laden */

static const char *NL_FILES[5] = {"geo", "bag", "roads", "water", "terrain"};

static void nl_free_bufs(void) {
    int i;
    for (i = 0; i < 6; i++) { free(F.nlBuf[i]); F.nlBuf[i] = NULL; F.nlLen[i] = 0; F.nlReq[i] = 0; F.nlDone[i] = 0; }
}

static void nl_status(int line, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(F.nlStatus[line], sizeof(F.nlStatus[line]), fmt, ap);
    va_end(ap);
}

static void nl_error(const char *msg) {
    snprintf(F.nlErrMsg, sizeof(F.nlErrMsg), "%s", msg);
    F.nlErr = 1;
    plat_log("SGB-NL fout: %s", msg);
}

static void nl_begin(const char *pc, int from) {
    nl_free_bufs();
    memset(F.nlStatus, 0, sizeof(F.nlStatus));
    snprintf(F.nlPc, sizeof(F.nlPc), "%s", pc);
    F.nlStep = 0; F.nlErr = 0; F.nlErrMsg[0] = 0; F.nlBagPdok = 0;
    F.nlFrom = from;
    F.nlT0 = plat_time();
    set_state(ST_NLLOAD);
}

static void nl_exit_to_city(void) {
    if (F.mode != 1) return;
    memcpy(&F.G, &F.Gcity, sizeof(Game));
    F.R.cur = 0;
    F.mode = 0;
    apply_settings();
}

static void nl_save_recent(const char *pc) {
    char *old = NULL, out[64];
    long len = 0;
    int n = 0;
    snprintf(out, sizeof(out), "%s\n", pc);
    if (plat_read_file("nl/recent.txt", &old, &len)) {
        char *line = strtok(old, "\n");
        while (line && n < 4) {
            if (strcmp(line, pc) != 0 && strlen(line) == 6) { strncat(out, line, sizeof(out) - strlen(out) - 2); strcat(out, "\n"); n++; }
            line = strtok(NULL, "\n");
        }
        free(old);
    }
    plat_write_file("nl/recent.txt", out, (long)strlen(out));
}

static void nl_load_recent(void) {
    char *d = NULL;
    long len = 0;
    F.nlRecentN = 0;
    if (plat_read_file("nl/recent.txt", &d, &len)) {
        char *line = strtok(d, "\n");
        while (line && F.nlRecentN < 5) {
            if (strlen(line) == 6) snprintf(F.nlRecent[F.nlRecentN++], 8, "%s", line);
            line = strtok(NULL, "\n");
        }
        free(d);
    }
}

/* index: 0 geo, 1 bag (3D BAG), 2 wegen, 3 water, 4 terrein, 5 bag-reserve (PDOK) */
static void nl_start(int i) {
    char url[700], name[64];
    if (i < 5) {
        snprintf(name, sizeof(name), "nl/%s_%s.json", F.nlPc, NL_FILES[i]);
        if (plat_read_file(name, &F.nlBuf[i], &F.nlLen[i])) { F.nlDone[i] = 1; F.nlCached[i] = 1; return; }
    }
    F.nlCached[i] = 0;
    if (i == 0) nl_url_geocode(url, sizeof(url), F.nlPc);
    else if (i == 1) nl_url_bag3d(url, sizeof(url), F.nlGeo.cx, F.nlGeo.cy);
    else if (i == 2) nl_url_top10(url, sizeof(url), "wegdeel_hartlijn", F.nlGeo.cx, F.nlGeo.cy);
    else if (i == 3) nl_url_top10(url, sizeof(url), "waterdeel_vlak", F.nlGeo.cx, F.nlGeo.cy);
    else if (i == 4) nl_url_top10(url, sizeof(url), "terrein_vlak", F.nlGeo.cx, F.nlGeo.cy);
    else nl_url_bagpdok(url, sizeof(url), F.nlGeo.cx, F.nlGeo.cy);
    F.nlReq[i] = plat_http_start(url);
    if (F.nlReq[i] <= 0) F.nlDone[i] = -1;
}

static void nl_poll(int i) {
    int r;
    if (F.nlDone[i] || F.nlReq[i] <= 0) return;
    r = plat_http_poll(F.nlReq[i], &F.nlBuf[i], &F.nlLen[i]);
    if (r == 0) return;
    F.nlDone[i] = r > 0 ? 1 : -1;
    if (r > 0 && i < 5 && !(i == 0 && strstr(F.nlBuf[0], "\"numFound\":0"))) {
        char name[64];
        snprintf(name, sizeof(name), "nl/%s_%s.json", F.nlPc, NL_FILES[i]);
        plat_write_file(name, F.nlBuf[i], F.nlLen[i]);
    }
}

static const char *nl_state_txt(int i) {
    static char b[6][48];
    if (F.nlDone[i] == 1) snprintf(b[i], sizeof(b[i]), "%s %.0f kB", F.nlCached[i] ? "OK (saved)" : "OK", F.nlLen[i] / 1024.0);
    else if (F.nlDone[i] == -1) snprintf(b[i], sizeof(b[i]), "FAILED");
    else snprintf(b[i], sizeof(b[i]), "downloading...");
    return b[i];
}

static void nl_tick(void) {
    int i;
    if (F.nlErr) return;
    switch (F.nlStep) {
    case 0:
        nl_status(0, "Looking up postcode %s ...", F.nlPc);
        nl_start(0);
        F.nlStep = 1;
        break;
    case 1:
        nl_poll(0);
        if (F.nlDone[0] == -1) { nl_error("No connection with PDOK (postcode lookup). Is Wi-Fi on?"); break; }
        if (F.nlDone[0] == 1) {
            if (!nl_parse_geocode(F.nlBuf[0], F.nlLen[0], &F.nlGeo)) { nl_error("Postcode not found."); break; }
            nl_status(0, "Postcode: %s", F.nlGeo.name);
            plat_log("SGB-NL %s -> RD %.0f %.0f (%s)", F.nlPc, F.nlGeo.cx, F.nlGeo.cy, F.nlGeo.name);
            for (i = 1; i <= 4; i++) nl_start(i);
            F.nlStep = 2;
        }
        break;
    case 2: {
        int pending = 0;
        for (i = 1; i <= 5; i++) nl_poll(i);
        if (F.nlDone[1] == -1 && !F.nlReq[5] && !F.nlDone[5]) {
            plat_log("SGB-NL 3D BAG niet bereikbaar, reserve: PDOK BAG zonder hoogtes");
            F.nlBagPdok = 1;
            nl_start(5);
        }
        nl_status(1, "Buildings + heights (3D BAG): %s", nl_state_txt(1));
        if (F.nlBagPdok) nl_status(2, "Backup buildings (PDOK BAG, no heights): %s", nl_state_txt(5));
        else F.nlStatus[2][0] = 0;
        nl_status(3, "Streets (PDOK TOP10NL): %s", nl_state_txt(2));
        nl_status(4, "Water + land use (TOP10NL): %s / %s", nl_state_txt(3), nl_state_txt(4));
        for (i = 1; i <= 4; i++) if (!F.nlDone[i]) pending = 1;
        if (F.nlBagPdok && !F.nlDone[5]) pending = 1;
        if (!pending) { F.nlStep = 3; nl_status(5, "Building the 3D neighbourhood..."); }
        break;
    }
    case 3:
        F.nlStep = 4;   /* een frame met de melding laten zien */
        break;
    case 4: {
        int b = F.nlBagPdok ? 5 : 1;
        double t0 = plat_time();
        int ok = nl_build_world(&F.WNL, F.nlGeo.cx, F.nlGeo.cy,
                                F.nlDone[b] == 1 ? F.nlBuf[b] : NULL, F.nlLen[b], F.nlBagPdok,
                                F.nlDone[2] == 1 ? F.nlBuf[2] : NULL, F.nlLen[2],
                                F.nlDone[3] == 1 ? F.nlBuf[3] : NULL, F.nlLen[3],
                                F.nlDone[4] == 1 ? F.nlBuf[4] : NULL, F.nlLen[4], &F.nlStats);
        double t1 = plat_time();
        nl_free_bufs();
        if (!ok) { nl_error("No map data received for this place."); break; }
        r_upload_world(&F.R, &F.WNL, 1);
        plat_log("SGB-NL gebouwd: %d gebouwen (hoogtes %s, max %.0f m), %d wegen, %d water, %d terrein, %d vertices, %d muren; bouwen %.2f s, totaal %.1f s, geheugen %ld kB",
                 F.nlStats.buildings, F.nlStats.heightsReal ? "echt" : "geschat", F.nlStats.maxH, F.nlStats.roads, F.nlStats.water,
                 F.nlStats.terrain, F.nlStats.verts, F.nlStats.segs, t1 - t0, t1 - F.nlT0, plat_memory_kb());
        F.nlBuildSecs = (float)(t1 - t0);
        F.nlTotalSecs = (float)(t1 - F.nlT0);
        if (F.mode == 0) memcpy(&F.Gcity, &F.G, sizeof(Game));
        game_init(&F.G, &F.WNL);
        F.mode = 1;
        F.R.cur = 1;
        apply_settings();
        snprintf(F.G.banner, sizeof(F.G.banner), "REAL NL");
        snprintf(F.G.sub, sizeof(F.G.sub), "%s", F.nlGeo.name);
        F.G.bannerT = 5.0f; F.G.bannerGood = 1;
        nl_save_recent(F.nlPc);
        set_state(ST_GAME);
        F.fade = 1.0f;
        break;
    }
    default: break;
    }
}

static void draw_nl_load(void) {
    Renderer *R = &F.R;
    int i;
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 1);
    draw_backdrop(0.25f, 0.0f, F.t);
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 0.55f);
    ui_text(R, "REAL NETHERLANDS", R->W * 0.5f, 60, 54, 1, 1, 1, 1, 1);
    for (i = 0; i < 6; i++)
        if (F.nlStatus[i][0]) ui_text(R, F.nlStatus[i], 130, 190.0f + i * 50.0f, 26, 1, 1, 1, 1, 0);
    if (F.nlErr) {
        ui_text(R, F.nlErrMsg, R->W * 0.5f, 520, 28, 1.0f, 0.4f, 0.4f, 1, 1);
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

/* toetsenbord */
#define KEY_DEL 1
#define KEY_GO 2
#define KEY_BACK 3
static const char *KROWS[4] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};

static void key_rect(int row, int col, float *x, float *y, float *w, float *h) {
    float x0[4] = {92, 92, 134, 176};
    *w = 76; *h = 64;
    *x = x0[row] + col * 84.0f;
    *y = 250.0f + row * 80.0f;
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
    if (px >= x && px < x + 160 && py >= y && py < y + h) return KEY_DEL;
    if (px >= 392 && px < 632 && py >= 572 && py < 646) return KEY_GO;
    if (px >= 20 && px < 220 && py >= 670 && py < 740) return KEY_BACK;
    for (r = 0; r < F.nlRecentN; r++) {
        float rx = 380.0f + r * 125.0f;
        if (px >= rx && px < rx + 115 && py >= 672 && py < 722) return 200 + r;
    }
    return 0;
}

static void draw_nl_input(void) {
    Renderer *R = &F.R;
    int r, c, valid;
    char shown[16], norm[8];
    float x, y, w, h;
    if (F.nlInFrom == ST_PAUSE) {
        ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0.02f, 0.02f, 0.03f, 1);
    } else {
        ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 1);
        draw_backdrop(0.3f, 0.0f, F.t);
        ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 0.55f);
    }
    ui_text(R, "REAL NETHERLANDS", R->W * 0.5f, 18, 50, 1, 1, 1, 1, 1);
    ui_text(R, "Type a Dutch postcode and explore the real streets and buildings in 3D", R->W * 0.5f, 84, 20, 0.85f, 0.85f, 0.85f, 1, 1);
    ui_rect(R, 312, 128, 400, 96, 0, 0, 0, 0.7f);
    if (F.pcLen > 4) snprintf(shown, sizeof(shown), "%.4s %s", F.pcIn, F.pcIn + 4);
    else snprintf(shown, sizeof(shown), "%s", F.pcIn);
    if (F.pcLen == 0) ui_text(R, "1234 AB", 512, 138, 64, 0.5f, 0.5f, 0.5f, 0.6f, 1);
    else ui_text(R, shown, 512, 138, 64, 1, 1, 1, 1, 1);
    for (r = 0; r < 4; r++)
        for (c = 0; KROWS[r][c]; c++) {
            int ch = KROWS[r][c], ok = (r == 0) ? (F.pcLen < 4 && !(F.pcLen == 0 && ch == '0')) : (F.pcLen >= 4 && F.pcLen < 6);
            char lab[2] = {(char)ch, 0};
            int pr = F.pressed == ch;
            key_rect(r, c, &x, &y, &w, &h);
            ui_rect(R, x, y, w, h, pr ? 0.7f : 0.12f, pr ? 0.1f : 0.12f, pr ? 0.1f : 0.14f, ok ? 0.9f : 0.4f);
            ui_text(R, lab, x + w * 0.5f, y + 8, 40, 1, 1, 1, ok ? 1.0f : 0.35f, 1);
        }
    key_rect(3, 7, &x, &y, &w, &h);
    ui_rect(R, x, y, 160, h, F.pressed == KEY_DEL ? 0.7f : 0.3f, 0.08f, 0.08f, 0.9f);
    ui_text(R, "DELETE", x + 80, y + 14, 30, 1, 1, 1, 1, 1);
    valid = nl_normalize_postcode(F.pcIn, norm);
    ui_rect(R, 392, 572, 240, 74, valid ? (F.pressed == KEY_GO ? 0.9f : 0.75f) : 0.2f, valid ? 0.12f : 0.2f, valid ? 0.12f : 0.2f, 0.95f);
    ui_text(R, "GO!", 512, 580, 50, 1, 1, 1, valid ? 1.0f : 0.4f, 1);
    big_button("BACK", 120, 704, 40, F.pressed == KEY_BACK, 1);
    if (F.nlRecentN > 0) {
        ui_text(R, "RECENT:", 370, 682, 24, 0.85f, 0.85f, 0.85f, 1, 2);
        for (r = 0; r < F.nlRecentN; r++) {
            float rx = 380.0f + r * 125.0f;
            ui_rect(R, rx, 672, 115, 50, F.pressed == 200 + r ? 0.6f : 0.1f, 0.12f, 0.16f, 0.9f);
            ui_text(R, F.nlRecent[r], rx + 57, 680, 28, 1.0f, 0.85f, 0.4f, 1, 1);
        }
    }
    ui_text(R, "Needs Wi-Fi the first time; visited places are saved on the iPad.  Data: 3D BAG, PDOK/Kadaster (CC BY 4.0)",
            R->W * 0.5f, R->H - 26.0f, 14, 0.75f, 0.75f, 0.75f, 1, 1);
}

static void nl_input_tap(float x, float y, int phase) {
    int k = nl_key_at(x, y);
    char norm[8];
    if (phase != TOUCH_END) { F.pressed = k; return; }
    F.pressed = 0;
    if (k >= '0' && k <= '9') {
        if (F.pcLen < 4 && !(F.pcLen == 0 && k == '0')) { F.pcIn[F.pcLen++] = (char)k; F.pcIn[F.pcLen] = 0; }
    } else if (k >= 'A' && k <= 'Z') {
        if (F.pcLen >= 4 && F.pcLen < 6) { F.pcIn[F.pcLen++] = (char)k; F.pcIn[F.pcLen] = 0; }
    } else if (k == KEY_DEL) {
        if (F.pcLen > 0) F.pcIn[--F.pcLen] = 0;
    } else if (k == KEY_GO) {
        if (nl_normalize_postcode(F.pcIn, norm)) nl_begin(norm, F.nlInFrom);
    } else if (k == KEY_BACK) {
        set_state(F.nlInFrom == ST_PAUSE ? ST_PAUSE : ST_MENU);
    } else if (k >= 200 && k < 200 + F.nlRecentN) {
        snprintf(F.pcIn, sizeof(F.pcIn), "%s", F.nlRecent[k - 200]);
        F.pcLen = 6;
        nl_begin(F.pcIn, F.nlInFrom);
    }
}

static void open_nl_input(int from) {
    F.nlInFrom = from;
    nl_load_recent();
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
        if (F.nlErr && phase == TOUCH_END) open_nl_input(F.nlFrom);
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
    if (F.mode == 1 && F.atNlT > 0 && t > F.atNlT + 2.6f && t < F.atNlT + 8.5f) { in->gas = 1; in->mx = 0.15f; }
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
        break;
    case ST_NLLOAD:
        nl_tick();
        if (F.state == ST_NLLOAD) {
            ui_begin(R); draw_nl_load(); ui_flush(R);
            if (F.at && F.nlErr) {
                rep("NL-modus MISLUKT: %s", F.nlErrMsg);
                plat_screenshot("17_nl_area", R->W, R->H);
                set_state(ST_GAME);
                F.atStep = 49;
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
