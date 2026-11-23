#define _POSIX_C_SOURCE 200809L
#ifdef _WIN32
#define strtok_r strtok_s
#endif
#include "frontend.h"
#include "render.h"
#include "../Game/nlmap.h"
#include <stdlib.h>

#include "nlstream.h"
#include "mp.h"
#include "../Game/home.h"
#include "../Game/biz.h"
#include "../Game/stores.h"
#include "../Game/voice.h"
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

enum { ST_LOGO = 0, ST_LOAD, ST_MENU, ST_SETTINGS, ST_CREDITS, ST_GAME, ST_PAUSE, ST_MISSIONS, ST_NLINPUT, ST_NLLOAD, ST_BIGMAP, ST_SHOP, ST_DEALER, ST_CUSTOMS, ST_MP, ST_HOME, ST_BIZ, ST_SLOTS, ST_CONTROLS };
enum { CTL_NONE = 0, CTL_LSTICK, CTL_RSTICK, CTL_JUMP, CTL_ACTION, CTL_GAS, CTL_BRAKE, CTL_PAUSE, CTL_UI, CTL_FIRE, CTL_WSWITCH, CTL_RUN };

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
    int atShot2, bizShop, atBiz, controlsFrom;
    long atMart0;
    int atCam; V3 atCamPos, atCamTgt;
    int slot, slotDelArm;   /* opslagslot 1..3; slotDelArm = slot dat op bevestiging wacht */
    long sfxPlayed;
    float lbx, lby, lkx, lky, rbx, rby, rkx, rky;
    int lOn, rOn;
    /* instellingen */
    int quality, tod, camSpeed, invertY, showFps, resetArm, autoAim, musicOn, sfxOn;
    /* menu */
    int pressed;
    float fade, fadeDir;
    int fadeTarget;
    /* fps */
    float fpsAcc, fps, worst, worstShown;
    int fpsFrames;
    float perfT;
    int lastDC, skipStat, missionsFrom, missionPage;
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
    int SW;                 /* UI-breedte van het scherm (768 hoog); menu's staan in een 1024-paneel in het midden */
} F;

static const char *TIPS[] = {
    "TIP: TAP ENTER NEAR A BIKE OR CAR TO HOP ON",
    "TIP: START MISSIONS FROM THE MISSIONS MENU",
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
static void draw_mp_hud(void);
static void start_game(void);
static void pad_load_binds(void);
/* Alle besturing loslaten (sticks terug, knoppen los). Nodig als er een menu opengaat terwijl je
 * de joystick vasthoudt: het loslaten komt dan bij het menu binnen en de stick zou blijven hangen. */
static void reset_controls(void) {
    int i;
    for (i = 0; i < NTOUCH; i++) if (F.ts[i].used) F.ts[i].ctl = CTL_UI;
    F.lOn = F.rOn = 0;
    F.lbx = 150; F.lby = F.R.H - 150.0f;
    F.rbx = (float)fe_ui_width() - 170.0f; F.rby = F.R.H - 150.0f;
    F.jumpEdge = F.actionEdge = F.switchEdge = 0;
    memset(&F.in, 0, sizeof(F.in));
}

/* desktop-besturing (buiten F, want fe_init wist F) */
static int g_desktop, g_vpW, g_vpH;
static Input g_dk;

void fe_set_desktop(int on) { g_desktop = on; }
void fe_key_input(const void *keys) { g_dk = *(const Input *)keys; }
static int g_vpX, g_vpY;
void fe_resize_viewport(int w, int h) {
    g_vpW = w; g_vpH = h;
    if (w > 0 && h > 0) {   /* breedbeeld: meer ruimte opzij, hoogte blijft 768 */
        int sw = (int)(768.0f * (float)w / (float)h + 0.5f);
        F.SW = sw < 1024 ? 1024 : sw > 2560 ? 2560 : sw;
    }
}
int fe_ui_width(void) { return F.SW > 0 ? F.SW : F.R.W; }

/* menu's zijn ontworpen voor 1024x768: op een breed scherm tekenen ze in een 1024-paneel in het midden */
static void panel_on(void) {
    int sw = fe_ui_width();
    if (sw > 1024 && F.R.H == 768) { F.R.W = 1024; F.R.uiOff = (float)(sw - 1024) * 0.5f; }
}
static void panel_off(void) {
    F.R.W = fe_ui_width(); F.R.uiOff = 0;
}
static int panel_state(int st) { return st != ST_GAME && st != ST_BIGMAP; }
void fe_set_viewport_origin(int x, int y) { g_vpX = x; g_vpY = y; }
int fe_wants_mouse(void) { return g_desktop && F.ready && F.state == ST_GAME && !F.at; }

static void set_state(int s) {
    if (s != F.state && (F.state == ST_GAME || s != ST_GAME)) reset_controls();
    F.prevState = F.state; F.state = s; F.st = 0; F.pressed = 0;
}

static void apply_settings(void) {
    static const float sens[3] = {0.6f, 1.0f, 1.5f};
    ns_set_detail(F.quality >= 2);
    if (F.loadDone) {
        game_set_quality(&F.G, F.quality);
        F.G.camSens = sens[F.camSpeed % 3];
        F.G.invertY = F.invertY;
        F.G.autoAim = F.autoAim;
    }
    r_set_tod(&F.R, F.tod);
}

/* ------------------------------------------------------------------ opslagslots (v3.3)
 * Voortgang staat per slot onder "s<slot>_<naam>"; instellingen blijven gedeeld.
 * Oude saves (zonder slots) worden bij de eerste start naar slot 1 verplaatst. */

#define NSLOTS 3
static const char *SLOT_KEYS[] = {"used", "money", "done", "guns", "ammo1", "ammo2", "ammo3", "armor", "home", "garage",
                                  "cardio", "tough", "outfits", "outfit", "decor", "car0", "car1", "car2", "car3"};
#define NSLOT_KEYS ((int)(sizeof(SLOT_KEYS) / sizeof(SLOT_KEYS[0])))

static void slot_key(char *b, size_t n, int slot, const char *k) { snprintf(b, n, "s%d_%s", slot, k); }
static void ssave(const char *k, int v) { char b[40]; slot_key(b, sizeof(b), F.slot, k); plat_save_int(b, v); }
static int sload(const char *k, int def) { char b[40]; slot_key(b, sizeof(b), F.slot, k); return plat_load_int(b, def); }
static int sload_in(int slot, const char *k, int def) { char b[40]; slot_key(b, sizeof(b), slot, k); return plat_load_int(b, def); }

static void slots_migrate(void) {
    int k;
    if (plat_load_int("slotsv", 0) >= 1) return;
    for (k = 1; k < NSLOT_KEYS; k++) {
        char b[40];
        slot_key(b, sizeof(b), 1, SLOT_KEYS[k]);
        plat_save_int(b, plat_load_int(SLOT_KEYS[k], 0));
    }
    {
        char b[40];
        slot_key(b, sizeof(b), 1, "used");
        plat_save_int(b, plat_load_int("money", 0) != 0 || plat_load_int("done", 0) != 0 || plat_load_int("car0", 0) != 0);
    }
    plat_save_int("slotsv", 1);
    plat_save_int("slot", 1);
    plat_save_flush();
    plat_log("SGB opslag: oude save naar slot 1 verplaatst");
}

static void slot_erase(int slot) {
    int k;
    for (k = 0; k < NSLOT_KEYS; k++) {
        char b[40];
        slot_key(b, sizeof(b), slot, SLOT_KEYS[k]);
        plat_save_int(b, 0);
    }
    plat_save_flush();
}

static void save_all(void) {
    int done = 0, i;
    plat_save_int("quality", F.quality);
    plat_save_int("tod", F.tod);
    plat_save_int("camspeed", F.camSpeed);
    plat_save_int("inverty", F.invertY);
    plat_save_int("showfps", F.showFps);
    plat_save_int("autoaim", F.autoAim);
    plat_save_int("music", F.musicOn);
    plat_save_int("sfx", F.sfxOn);
    if (F.loadDone && !F.at && F.mode == 0) {
        for (i = 0; i < NMISSIONS; i++) if (F.G.completed[i]) done |= 1 << i;
        ssave("used", 1);
        ssave("money", (int)F.G.money);
        ssave("done", done);
        ssave("guns", F.G.owned[1] | (F.G.owned[2] << 1) | (F.G.owned[3] << 2));
        ssave("ammo1", F.G.ammo[1]); ssave("ammo2", F.G.ammo[2]); ssave("ammo3", F.G.ammo[3]);
        ssave("armor", (int)F.G.armor);
        ssave("home", F.G.homeLvl);
        ssave("garage", F.G.garLvl);
        ssave("cardio", F.G.cardio);
        ssave("tough", F.G.tough);
        ssave("outfits", F.G.outfits);
        ssave("outfit", F.G.outfit);
        ssave("decor", F.G.decor);
        ssave("robcool", (int)ceilf(F.G.robCool));
        {
            int c;
            for (c = 0; c < MAX_OWNED; c++) {
                const struct GarageCar *gc = &F.G.garage[c];
                char key[16];
                snprintf(key, sizeof(key), "car%d", c);
                ssave(key, gc->exists ? (1 | (gc->model << 1) | (gc->paint << 4) | (gc->eng << 9) | (gc->hand << 11) | (gc->neon << 13) | (gc->spoiler << 16)) : 0);
            }
        }
        F.savedMoney = F.G.money; F.savedDone = done;
    }
    plat_save_flush();
}

void fe_init(int w, int h) {
    int i;
    memset(&F, 0, sizeof(F));
    {   /* v5.5: modellen en stad met meer detail op hoge kwaliteit (geldt vanaf het opstarten) */
        int q0 = plat_load_int("quality", 1);
        g_sqDetail = q0 >= 2 ? 1.4f : 1.0f;
        world_set_detail(q0);
    }
    r_init(&F.R, w, h);
    g_sqDetail = 1.0f;
    F.SW = w;
    F.quality = plat_load_int("quality", 1);
    F.tod = plat_load_int("tod", 0);
    F.camSpeed = plat_load_int("camspeed", 1);
    F.invertY = plat_load_int("inverty", 0);
    F.showFps = plat_load_int("showfps", 0);
    F.autoAim = plat_load_int("autoaim", 1);
    F.musicOn = plat_load_int("music", 1);
    pad_load_binds();
    {
        int id = plat_load_int("mpid", 0);
        char nm[16];
        if (id <= 0) { id = 1000 + (int)(fmod(plat_time() * 7919.0, 9000.0)); plat_save_int("mpid", id); plat_save_flush(); }
        snprintf(nm, sizeof(nm), "Player %d", id);
        mp_set_name(nm);
    }
    F.sfxOn = plat_load_int("sfx", 1);
    {
        static const char *const sfx[SFX_COUNT] = {"click", "pistol", "smg", "shotgun", "copshot", "empty", "scream", "hurt", "crash",
                                                   "door", "jump", "cash", "check", "pass", "fail", "alarm", "buy"};
        static const char *const loops[LOOP_COUNT] = {"engine", "bike", "siren"};
        plat_audio_init(sfx, SFX_COUNT, loops, LOOP_COUNT);
    }
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

static void load_slot(void) {
    int i, done;
    game_init(&F.G, &F.W);
    F.G.noCutscenes = F.at;   /* autotest: cutscenes alleen in de eigen teststap */
    if (!F.at) {
        F.G.money = sload("money", 0);
        F.G.moneyShown = F.G.money;
        done = sload("done", 0);
        for (i = 0; i < NMISSIONS; i++) F.G.completed[i] = (done >> i) & 1;
        {
            int guns = sload("guns", 0);
            F.G.owned[1] = guns & 1; F.G.owned[2] = (guns >> 1) & 1; F.G.owned[3] = (guns >> 2) & 1;
            F.G.ammo[1] = sload("ammo1", 0); F.G.ammo[2] = sload("ammo2", 0); F.G.ammo[3] = sload("ammo3", 0);
            F.G.armor = (float)sload("armor", 0);
            F.G.homeLvl = sload("home", 0);
            F.G.garLvl = sload("garage", 0);
            if (F.G.homeLvl < 0 || F.G.homeLvl >= HOME_LEVELS) F.G.homeLvl = 0;
            if (F.G.garLvl < 0 || F.G.garLvl >= GARAGE_LEVELS) F.G.garLvl = 0;
            F.G.cardio = sload("cardio", 0); if (F.G.cardio < 0 || F.G.cardio > 3) F.G.cardio = 0;
            F.G.tough = sload("tough", 0); if (F.G.tough < 0 || F.G.tough > 3) F.G.tough = 0;
            F.G.outfits = sload("outfits", 0);
            F.G.outfit = sload("outfit", 0); if (F.G.outfit < 0 || F.G.outfit >= BIZ_OUTFITS) F.G.outfit = 0;
            F.G.decor = sload("decor", 0) & ((1 << BIZ_DECOR) - 1);
            F.G.robCool = (float)sload("robcool", 0);
            F.G.stamina = game_stamina_max(&F.G);
        }
        {
            int c;
            for (c = 0; c < MAX_OWNED; c++) {
                char key[16];
                int v;
                struct GarageCar *gc = &F.G.garage[c];
                snprintf(key, sizeof(key), "car%d", c);
                v = sload(key, 0);
                gc->exists = v & 1; gc->model = (v >> 1) & 7; gc->paint = (v >> 4) & 31; gc->eng = (v >> 9) & 3;
                gc->hand = (v >> 11) & 3; gc->neon = (v >> 13) & 7; gc->spoiler = (v >> 16) & 1;
                if (gc->model >= DEALER_MODELS) gc->model = 0;
                if (gc->paint >= NPAINTS) gc->paint = 0;
                if (gc->neon >= NNEONS) gc->neon = 0;
            }
            game_spawn_owned(&F.G);
        }
    }
}

static void load_step(void) {
    int s = F.loadStep;
    int nb = world_build_steps();
    if (s == 0) r_load_font(&F.R);
    else if (s >= 1 && s <= nb) world_build_step(&F.W, s - 1);
    else if (s == nb + 1) r_upload_world(&F.R, &F.W, 0);
    else if (s == nb + 2) r_build_models(&F.R);
    else if (s == nb + 3) {
        slots_migrate();
        F.slot = plat_load_int("slot", 1);
        if (F.slot < 1 || F.slot > NSLOTS) F.slot = 1;
        load_slot();
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
    float s0 = (float)R->H / 768.0f, s;
    if ((float)fe_ui_width() / 1408.0f > s0) s0 = (float)fe_ui_width() / 1408.0f;   /* heel breed scherm */
    s = s0 * (1.0f + 0.025f * (0.5f - 0.5f * cosf(zoomT * 0.12f)));
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

static const char *MENU_ITEMS[6] = {"PLAY", "MISSIONS", "REAL NL", "ONLINE", "SETTINGS", "CREDITS"};
#define MENU_X(i) (R->W * 0.5f + ((i) - 2.5f) * 166.0f)

static void draw_menu(void) {
    Renderer *R = &F.R;
    float a = smoothstepf(0.0f, 0.6f, F.st);
    int i, done = 0;
    char line[64];
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 1);
    draw_backdrop(1, 1, F.t);
    bottom_band(1);
    for (i = 0; i < 6; i++) {
        float cx = MENU_X(i);
        float pulse = (i == 0) ? 0.04f * sinf(F.t * 3.0f) : 0;
        big_button(MENU_ITEMS[i], cx, R->H - 74.0f, 32.0f * (1.0f + pulse), F.pressed == i + 1, a);
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
    const char *labels[9] = {"GRAPHICS", "TIME OF DAY", "CAMERA SPEED", "INVERT CAMERA Y", "SHOW FPS", "AUTO AIM", "MUSIC", "SOUND EFFECTS", "RESET PROGRESS"};
    const char *vals[9];
    vals[0] = Q_NAMES[F.quality % 3];
    vals[1] = T_NAMES[F.tod % 3];
    vals[2] = C_NAMES[F.camSpeed % 3];
    vals[3] = F.invertY ? "ON" : "OFF";
    vals[4] = F.showFps ? "ON" : "OFF";
    vals[5] = F.autoAim ? "ON (POLICE FIRST)" : "OFF";
    vals[6] = F.musicOn ? "ON" : "OFF";
    vals[7] = F.sfxOn ? "ON" : "OFF";
    vals[8] = F.resetArm ? "TAP AGAIN!" : "";
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 1);
    draw_backdrop(0.35f, 0.0f, F.t);
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 0.45f);
    ui_text(R, "SETTINGS", R->W * 0.5f, 40, 60, 1, 1, 1, 1, 1);
    ui_rect(R, R->W - 230.0f, 36, 200, 56, F.pressed == 20 ? 0.6f : 0.15f, F.pressed == 20 ? 0.2f : 0.35f, F.pressed == 20 ? 0.2f : 0.6f, 0.95f);
    ui_text(R, "CONTROLS", R->W - 130.0f, 50, 26, 1, 1, 1, 1, 1);
    for (i = 0; i < 9; i++) {
        float y = 118.0f + i * 58.0f;
        int pr = F.pressed == i + 1;
        ui_rect(R, 180, y - 5, R->W - 360.0f, 50, pr ? 0.5f : 0.08f, 0.04f, 0.04f, 0.55f);
        ui_text(R, labels[i], 210, y, 28, 1, 1, 1, 1, 0);
        ui_text(R, vals[i], R->W - 210.0f, y, 28, 1.0f, 0.8f, 0.3f, 1, 2);
    }
    ui_text(R, "Graphics changes draw distance and the number of people/cars.", R->W * 0.5f, 648, 16, 0.8f, 0.8f, 0.8f, 1, 1);
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
        V3 pts[6 + BIZ_COUNT];
        int q;
        static const float bc[4][3] = {{0.95f, 0.15f, 0.15f}, {0.2f, 0.75f, 0.3f}, {1.0f, 0.55f, 0.1f}, {0.2f, 0.45f, 1.0f}};
        pts[0] = g->w->poi[POI_GUNSHOP];
        pts[1] = g->w->poi[POI_BANK];
        pts[2] = g->w->poi[POI_DEALER];
        pts[3] = g->w->poi[POI_CUSTOMS];
        home_door(g->homeLvl, &pts[4].x, &pts[4].z);
        for (q = 0; q < BIZ_COUNT; q++) pts[5 + q] = g->w->poi[biz_poi(q)];
        pts[5 + BIZ_COUNT] = g->w->poi[POI_MART];
        for (q = 0; q < 6 + BIZ_COUNT; q++) {
            float rx = pts[q].x - px, rz = pts[q].z - pz;
            float sx = (rx * rt.x + rz * rt.z) / rad, sy = -(rx * f.x + rz * f.z) / rad;
            float m = fmaxf(fabsf(sx), fabsf(sy));
            if (m > 0.9f) { sx = sx / m * 0.9f; sy = sy / m * 0.9f; }
            (void)bc;
            if (q == 5 + BIZ_COUNT) {   /* v4.0 GIVEAWAY MART: groene stip */
                ui_circle(R, cx + sx * half, cy + sy * half, 10, 0, 0, 0, 0.85f);
                ui_circle(R, cx + sx * half, cy + sy * half, 8, 0.25f, 0.85f, 0.35f, 1);
                ui_circle(R, cx + sx * half, cy + sy * half, 3, 1, 1, 1, 1);
                continue;
            }
            ui_icon(R, q >= 5 ? ICON_BURGER + (q - 5) : (q == 0 ? ICON_GUN : (q == 1 ? ICON_BANK : (q == 2 ? ICON_CAR : (q == 3 ? ICON_CUSTOMS : ICON_HOME)))),
                    cx + sx * half, cy + sy * half, q >= 5 ? 20 : 24, 1);
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
        if (ctl == CTL_RUN) { *x = W - 316; *y = H - 300; *r = 44; }
        if (ctl == CTL_FIRE && armed) { *x = W - 74; *y = H - 430; *r = 54; }
        if (ctl == CTL_WSWITCH && armed) { *x = W - 200; *y = H - 455; *r = 36; }
    } else {
        if (ctl == CTL_GAS) { *x = W - 74; *y = H - 300; *r = 60; }
        if (ctl == CTL_BRAKE) { *x = W - 200; *y = H - 318; *r = 48; }
        if (ctl == CTL_ACTION) { *x = W - 120; *y = H - 440; *r = 40; }
    }
}

/* ------------------------------------------------------------------ controllers + toetsen instellen (v3.4)
 * Elk platform levert een "logische" controller (Xbox-indeling) via fe_pad_input. Acties zijn vrij te
 * koppelen (CONTROLS-scherm); op de pc ook de toetsen. In menu's stuurt de controller een cursor. */

static const char *PB_NAMES[PB_COUNT] = {"A", "B", "X", "Y", "LB", "RB", "LT", "RT", "BACK", "START", "L3", "R3", "D-UP", "D-DOWN", "D-LEFT", "D-RIGHT"};
static const char *PA_NAMES[PA_COUNT] = {"JUMP", "ENTER / EXIT", "FIRE", "SWITCH GUN", "RUN", "GAS", "BRAKE / REVERSE", "MAP", "PAUSE"};
static const int PA_DEFAULT[PA_COUNT] = {PB_A, PB_Y, PB_RT, PB_X, PB_B, PB_RT, PB_LT, PB_BACK, PB_START};
static const char *KA_NAMES[KA_COUNT] = {"FORWARD / GAS", "BACK / BRAKE", "LEFT", "RIGHT", "JUMP", "ENTER / EXIT", "FIRE (also mouse)", "SWITCH GUN", "RUN", "MAP"};

static struct {
    int connected;
    float ax[6];
    unsigned btn, prev;
    char name[48];
    int bind[PA_COUNT];
    int kbind[KA_COUNT], kdef[KA_COUNT], haveKeys;
    int active;          /* laatste invoer kwam van de controller (touch-knoppen verbergen) */
    float cx, cy;        /* menucursor */
    int cursorDown;
    int wait;            /* 1..PA_COUNT: wacht op knop; 100+k: wacht op toets */
    int edgeJump, edgeAction, edgeSwitch;
} P;

static void pad_load_binds(void) {
    int k;
    for (k = 0; k < PA_COUNT; k++) {
        char key[16];
        snprintf(key, sizeof(key), "pb%d", k);
        P.bind[k] = plat_load_int(key, PA_DEFAULT[k]);
        if (P.bind[k] < 0 || P.bind[k] >= PB_COUNT) P.bind[k] = PA_DEFAULT[k];
    }
    for (k = 0; k < KA_COUNT; k++) {
        char key[16];
        snprintf(key, sizeof(key), "kb%d", k);
        P.kbind[k] = plat_load_int(key, 0);
    }
    P.cx = 512; P.cy = 384;
}

static void pad_save_binds(void) {
    int k;
    for (k = 0; k < PA_COUNT; k++) { char key[16]; snprintf(key, sizeof(key), "pb%d", k); plat_save_int(key, P.bind[k]); }
    for (k = 0; k < KA_COUNT; k++) { char key[16]; snprintf(key, sizeof(key), "kb%d", k); plat_save_int(key, P.kbind[k]); }
    plat_save_flush();
}

void fe_set_key_defaults(const int *codes, int n) {
    int k;
    for (k = 0; k < KA_COUNT && k < n; k++) P.kdef[k] = codes[k];
    P.haveKeys = 1;
}

int fe_key_bind(int action) {
    if (action < 0 || action >= KA_COUNT) return 0;
    return P.kbind[action] ? P.kbind[action] : P.kdef[action];
}

int fe_bind_waiting(void) { return P.wait != 0; }

void fe_raw_key(int code) {
    if (P.wait >= 100 && P.wait < 100 + KA_COUNT) {
        int a = P.wait - 100, k;
        for (k = 0; k < KA_COUNT; k++) if (k != a && fe_key_bind(k) == code) P.kbind[k] = fe_key_bind(a);   /* wisselen */
        P.kbind[a] = code;
        P.wait = 0;
        pad_save_binds();
    }
}

void fe_pad_input(int connected, const float *axes, unsigned buttons, const char *name) {
    int k;
    if (!connected && P.connected) { P.active = 0; P.wait = P.wait >= 100 ? P.wait : 0; }
    if (connected && !P.connected && F.ready && F.loadDone && F.state == ST_GAME) game_message(&F.G, "Controller connected", 2.0f);
    P.connected = connected;
    for (k = 0; k < 6; k++) P.ax[k] = connected && axes ? axes[k] : 0;
    if (P.ax[4] > 0.35f) buttons |= 1u << PB_LT;
    if (P.ax[5] > 0.35f) buttons |= 1u << PB_RT;
    P.btn = connected ? buttons : 0;
    snprintf(P.name, sizeof(P.name), "%s", connected && name && name[0] ? name : "Controller");
    if (connected && (P.btn || fabsf(P.ax[0]) > 0.3f || fabsf(P.ax[1]) > 0.3f || fabsf(P.ax[2]) > 0.3f || fabsf(P.ax[3]) > 0.3f)) P.active = 1;
}

static int pb_held(int b) { return (P.btn >> b) & 1; }
static int pb_edge(int b) { return ((P.btn >> b) & 1) && !((P.prev >> b) & 1); }
static int pa_held(int a) { return pb_held(P.bind[a]); }
static int pa_edge(int a) { return pb_edge(P.bind[a]); }
static float dz(float v) { return fabsf(v) < 0.18f ? 0.0f : (v - (v > 0 ? 0.18f : -0.18f)) / 0.82f; }

static void bm_open(void);

/* elke frame, voor de schermen: cursor in menu's, pauze, kaart, knop koppelen */
static void pad_tick(float dt) {
    Renderer *R = &F.R;
    int k;
    if (!P.connected) { P.prev = 0; return; }
    if (P.wait >= 1 && P.wait <= PA_COUNT) {   /* CONTROLS: volgende knop koppelen */
        for (k = 0; k < PB_COUNT; k++) if (pb_edge(k)) {
            P.bind[P.wait - 1] = k;
            P.wait = 0;
            pad_save_binds();
            if (F.sfxOn) plat_sfx(SFX_CLICK, 0.6f);
            break;
        }
        P.prev = P.btn;
        return;
    }
    P.edgeJump = P.edgeAction = P.edgeSwitch = 0;
    if (F.state == ST_GAME) {
        if (pa_edge(PA_PAUSE)) { set_state(ST_PAUSE); save_all(); }
        else if (pa_edge(PA_MAP)) bm_open();
        P.edgeJump = pa_edge(PA_JUMP); P.edgeAction = pa_edge(PA_ACTION); P.edgeSwitch = pa_edge(PA_SWITCH);
    } else if (F.state != ST_LOGO && F.state != ST_LOAD && F.state != ST_NLLOAD) {
        float mx = dz(P.ax[0]) + (pb_held(PB_RIGHT) ? 1.0f : 0) - (pb_held(PB_LEFT) ? 1.0f : 0);
        float my = dz(P.ax[1]) + (pb_held(PB_DOWN) ? 1.0f : 0) - (pb_held(PB_UP) ? 1.0f : 0);
        float sp = 620.0f * (pb_held(PB_RB) ? 1.8f : 1.0f);
        if (mx || my) {
            P.cx = clampf(P.cx + mx * sp * dt, 0, (float)R->W);
            P.cy = clampf(P.cy + my * sp * dt, 0, (float)R->H);
            if (P.cursorDown) fe_touch(-77, TOUCH_MOVE, P.cx, P.cy);
        }
        if (pb_edge(PB_A)) { P.cursorDown = 1; fe_touch(-77, TOUCH_BEGIN, P.cx, P.cy); }
        else if (P.cursorDown && !pb_held(PB_A)) { P.cursorDown = 0; fe_touch(-77, TOUCH_END, P.cx, P.cy); }
        if (pb_edge(PB_B) && !P.cursorDown) fe_key_command(FE_CMD_BACK);
        else if (pb_edge(PB_START) && F.state == ST_PAUSE) set_state(ST_GAME);
    }
    P.prev = P.btn;
}

static void pad_build_input(Input *in) {
    if (!P.connected) return;
    {
        float mx = dz(P.ax[0]), my = -dz(P.ax[1]), lx = dz(P.ax[2]), ly = -dz(P.ax[3]);
        if (pb_held(PB_LEFT)) mx -= 1; if (pb_held(PB_RIGHT)) mx += 1;
        if (pb_held(PB_UP)) my += 1; if (pb_held(PB_DOWN)) my -= 1;
        in->mx = clampf(in->mx + mx, -1, 1);
        in->my = clampf(in->my + my, -1, 1);
        in->lx = clampf(in->lx + lx * 1.1f, -1, 1);
        in->ly = clampf(in->ly + ly, -1, 1);
    }
    in->jumpPressed |= P.edgeJump;
    in->actionPressed |= P.edgeAction;
    in->switchPressed |= P.edgeSwitch;
    in->run |= pa_held(PA_RUN);
    if (F.G.p.veh >= 0) {
        in->gas |= pa_held(PA_GAS) || P.ax[5] > 0.15f * (P.bind[PA_GAS] == PB_RT);
        in->brake |= pa_held(PA_BRAKE) || P.ax[4] > 0.15f * (P.bind[PA_BRAKE] == PB_LT);
    } else in->fire |= pa_held(PA_FIRE);
    P.edgeJump = P.edgeAction = P.edgeSwitch = 0;
}

/* v5.0 VR: toegang voor de Quest-laag, en de laserpointer als cursor */
void fe_set_pad_name(int b, const char *name) { if (b >= 0 && b < PB_COUNT && name) PB_NAMES[b] = name; }
void *fe_renderer(void) { return &F.R; }
void *fe_game(void) { return &F.G; }
int fe_in_game(void) { return F.loadDone && F.state == ST_GAME; }
void fe_vr_pointer(int on, float x, float y) {
    P.active = 1;   /* VR: nooit touch-knoppen tonen */
    if (on) { P.cx = clampf(x, 0, (float)F.R.W); P.cy = clampf(y, 0, (float)F.R.H); }
}

static void draw_pad_cursor(void) {
    Renderer *R = &F.R;
    if (!P.connected || !P.active || F.state == ST_GAME || F.state == ST_LOGO || F.state == ST_LOAD) return;
    ui_begin(R);
    ui_circle(R, P.cx, P.cy, 15, 0, 0, 0, 0.5f);
    ui_circle(R, P.cx, P.cy, 10, P.cursorDown ? 1.0f : 1.0f, P.cursorDown ? 0.6f : 0.85f, 0.2f, 0.95f);
    ui_ring(R, P.cx, P.cy, 15, 2, 1, 1, 1, 0.9f);
    ui_flush(R);
}

/* ---- CONTROLS-scherm ---- */

static const char *key_label(int a) {
    const char *n = plat_key_name(fe_key_bind(a));
    return n && n[0] ? n : "-";
}

static void draw_controls(void) {
    Renderer *R = &F.R;
    char b[96];
    int k;
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 1);
    draw_backdrop(0.3f, 0.0f, F.t);
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 0.62f);
    ui_text(R, "CONTROLS", R->W * 0.5f, 14, 50, 1, 1, 1, 1, 1);
    if (P.connected) snprintf(b, sizeof(b), "Controller: %s - tap a button to change it", P.name);
    else snprintf(b, sizeof(b), "No controller found - pair one via Bluetooth or plug it in (USB)");
    ui_text(R, b, R->W * 0.5f, 70, 18, P.connected ? 0.5f : 1.0f, P.connected ? 1.0f : 0.75f, P.connected ? 0.55f : 0.4f, 1, 1);
    ui_text(R, "CONTROLLER", 280, 104, 22, 1.0f, 0.85f, 0.4f, 1, 1);
    for (k = 0; k < PA_COUNT; k++) {
        float y = 136.0f + k * 54.0f;
        int pr = F.pressed == 1800 + k, w = P.wait == k + 1;
        ui_rect(R, 60, y, 440, 46, 0.08f, 0.08f, 0.1f, 0.85f);
        ui_text(R, PA_NAMES[k], 76, y + 10, 22, 1, 1, 1, 1, 0);
        ui_rect(R, 360, y + 5, 128, 36, w ? 0.9f : (pr ? 0.6f : 0.2f), w ? 0.6f : (pr ? 0.2f : 0.35f), w ? 0.1f : 0.6f, 1);
        ui_text(R, w ? "PRESS..." : PB_NAMES[P.bind[k] % PB_COUNT], 424, y + 10, w ? 18 : 22, 1, 1, 1, 1, 1);
    }
    ui_text(R, "Left stick: move / steer   Right stick: camera   (fixed)", 280, 136.0f + PA_COUNT * 54.0f + 4, 14, 0.8f, 0.8f, 0.8f, 1, 1);
    if (g_desktop) {
        ui_text(R, "KEYBOARD", 744, 104, 22, 1.0f, 0.85f, 0.4f, 1, 1);
        for (k = 0; k < KA_COUNT; k++) {
            float y = 136.0f + k * 48.0f;
            int pr = F.pressed == 1850 + k, w = P.wait == 100 + k;
            ui_rect(R, 524, y, 440, 42, 0.08f, 0.08f, 0.1f, 0.85f);
            ui_text(R, KA_NAMES[k], 540, y + 9, 20, 1, 1, 1, 1, 0);
            ui_rect(R, 808, y + 4, 144, 34, w ? 0.9f : (pr ? 0.6f : 0.2f), w ? 0.6f : (pr ? 0.2f : 0.35f), w ? 0.1f : 0.6f, 1);
            ui_text(R, w ? "PRESS A KEY" : key_label(k), 880, y + 9, w ? 15 : 18, 1, 1, 1, 1, 1);
        }
    } else {
        ui_text(R, "How to connect:", 560, 140, 22, 1.0f, 0.85f, 0.4f, 1, 0);
        ui_text(R, "Settings > Bluetooth on the iPad, then pair", 560, 176, 17, 0.9f, 0.9f, 0.9f, 1, 0);
        ui_text(R, "an MFi game controller (or plug it in).", 560, 200, 17, 0.9f, 0.9f, 0.9f, 1, 0);
        ui_text(R, "In menus the left stick moves a cursor:", 560, 250, 17, 0.9f, 0.9f, 0.9f, 1, 0);
        ui_text(R, "A = tap, B = back, RB = faster cursor.", 560, 274, 17, 0.9f, 0.9f, 0.9f, 1, 0);
    }
    if (P.wait) ui_text(R, P.wait >= 100 ? "Press the new key (ESC = cancel)" : "Press the new controller button (tap the screen = cancel)", R->W * 0.5f, 650, 20, 1, 0.85f, 0.3f, 1, 1);
    ui_rect(R, 120, 690, 260, 56, F.pressed == 1898 ? 0.6f : 0.25f, 0.12f, 0.12f, 0.95f);
    ui_text(R, "RESET DEFAULTS", 250, 706, 22, 1, 1, 1, 1, 1);
    big_button("BACK", R->W * 0.5f + 160, 696, 44, F.pressed == 1899, 1);
}

static void controls_tap(float x, float y, int phase) {
    Renderer *R = &F.R;
    int k, hit = 0;
    if (P.wait) { if (phase == TOUCH_END) P.wait = 0; return; }   /* tikken = annuleren */
    for (k = 0; k < PA_COUNT; k++) {
        float yy = 136.0f + k * 54.0f;
        if (x > 360 && x < 488 && y > yy + 5 && y < yy + 41) hit = 1800 + k;
    }
    if (g_desktop)
        for (k = 0; k < KA_COUNT; k++) {
            float yy = 136.0f + k * 48.0f;
            if (x > 808 && x < 952 && y > yy + 4 && y < yy + 38) hit = 1850 + k;
        }
    if (x > 120 && x < 380 && y > 690 && y < 746) hit = 1898;
    if (hit_text(x, y, "BACK", R->W * 0.5f + 160, 696 + 22, 44)) hit = 1899;
    if (phase != TOUCH_END) { F.pressed = hit; return; }
    F.pressed = 0;
    if (hit >= 1800 && hit < 1800 + PA_COUNT) P.wait = hit - 1800 + 1;
    else if (hit >= 1850 && hit < 1850 + KA_COUNT) P.wait = 100 + hit - 1850;
    else if (hit == 1898) {
        for (k = 0; k < PA_COUNT; k++) P.bind[k] = PA_DEFAULT[k];
        for (k = 0; k < KA_COUNT; k++) P.kbind[k] = 0;
        pad_save_binds();
    } else if (hit == 1899) set_state(ST_SETTINGS);
}

/* v4.5 ondertitel: naam in kleur, tekst eronder, netjes afgebroken */
static void draw_subtitle(int line, float y, float alpha, int cutStyle) {
    Renderer *R = &F.R;
    float W = (float)R->W, size = cutStyle ? 26.0f : 21.0f, maxw = cutStyle ? (W < 1100 ? W - 120 : 980) : (W - 440 < 760 ? W - 440 : 760);
    char rows[4][160], word[64];
    const char *s, *t;
    int nr = 0, k, spk;
    float bw = 0;
    if (line < 0 || line >= VO_LINES) return;
    spk = VO[line].spk;
    rows[0][0] = 0;
    for (s = VO[line].text; *s && nr < 4;) {
        char tryb[200];
        int n = 0;
        while (*s == ' ') s++;
        for (t = s; *t && *t != ' ' && n < 63; t++) word[n++] = *t;
        word[n] = 0;
        if (!n) break;
        snprintf(tryb, sizeof(tryb), "%s%s%s", rows[nr], rows[nr][0] ? " " : "", word);
        if (rows[nr][0] && ui_text_w(R, tryb, size) > maxw) { nr++; if (nr >= 4) break; snprintf(rows[nr], sizeof(rows[nr]), "%s", word); }
        else snprintf(rows[nr], sizeof(rows[nr]), "%s", tryb);
        s = t;
    }
    if (nr < 4 && rows[nr][0]) nr++;
    for (k = 0; k < nr; k++) { float w2 = ui_text_w(R, rows[k], size); if (w2 > bw) bw = w2; }
    if (!cutStyle) ui_rect(R, W * 0.5f - bw * 0.5f - 18, y - size - 14, bw + 36, size * 1.25f * nr + size + 26, 0, 0, 0, 0.55f * alpha);
    {
        float cr = spk == VS_YOU ? 0.45f : (spk == VS_COP ? 0.4f : 1.0f), cg = spk == VS_YOU ? 0.85f : (spk == VS_COP ? 0.6f : 0.82f), cb = spk == VS_YOU ? 1.0f : (spk == VS_COP ? 1.0f : 0.25f);
        ui_text(R, VO_SPK_NAME[spk], W * 0.5f, y - size - 6, size * 0.75f, cr, cg, cb, alpha, 1);
    }
    for (k = 0; k < nr; k++) ui_text(R, rows[k], W * 0.5f, y + k * size * 1.25f, size, 1, 1, 1, alpha, 1);
}

static void draw_cutscene(void) {
    Renderer *R = &F.R;
    Game *g = &F.G;
    float W = (float)R->W, H = (float)R->H, k = clampf(g->cutT / 0.5f, 0, 1), bar = H * 0.13f * k;
    if (R->vr.active) bar = H * 0.06f * k;   /* VR: geen filmbalken voor je ogen, alleen ruimte voor de tekst */
    else {
        ui_rect(R, 0, 0, W, bar, 0, 0, 0, 1);
        ui_rect(R, 0, H - bar, W, bar, 0, 0, 0, 1);
    }
    if (g->cutIdx < 0 && g->cutMission >= 0 && g->cutMission < NMISSIONS)
        ui_text(R, g->mdef[g->cutMission].title, W * 0.5f, H * 0.5f - 20, 44, 1, 0.85f, 0.25f, clampf(g->cutT * 2.0f, 0, 1), 1);
    if (g->cutIdx >= 0 && g->voT > 0) draw_subtitle(g->voLine, H - bar - 70, 1, 1);
    ui_text(R, "TAP TO SKIP", W - 30, H - bar * 0.5f - 10, 18, 1, 1, 1, 0.55f + 0.25f * sinf(g->cutT * 3.0f), 2);
}

static void draw_hud(void) {
    Renderer *R = &F.R;
    Game *g = &F.G;
    float W = (float)R->W, H = (float)R->H;
    char b[96];
    int mode = game_mode(g);
    float x, y, r;

    if (g->cut) { draw_cutscene(); return; }
    if (g->robCool > 0 && !g->nl) {   /* v5.2: wachttijd van de GIVEAWAY MART-kassa */
        char rb[48];
        int t = (int)ceilf(g->robCool);
        snprintf(rb, sizeof(rb), "MART REFILL %d:%02d", t / 60, t % 60);
        ui_rect(R, W - 262, 160, 242, 30, 0, 0, 0, 0.45f);
        ui_circle(R, W - 246, 175, 7, 0.25f, 0.85f, 0.35f, 1);
        ui_text(R, rb, W - 30, 163, 20, 1, 1, 1, 0.95f, 2);
    }
    if (g->voT > 0 && g->voLine >= 0) draw_subtitle(g->voLine, H - 150, clampf(g->voT / 0.3f, 0, 1), 0);

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
    if (g->stamina < game_stamina_max(g) - 0.5f || g->running || g->boostT > 0) {   /* uithoudingsvermogen (rennen) */
        float hw = 210.0f, hx = W - 86 - hw, st = clampf(g->stamina / game_stamina_max(g), 0, 1);
        if (g->boostT > 0) { snprintf(b, sizeof(b), "ENERGY %.0f", g->boostT); ui_text(R, b, hx - 10, 92, 15, 0.4f, 1.0f, 0.9f, 1, 2); }
        float bl = g->tired ? 0.5f + 0.5f * sinf(F.t * 9.0f) : 1.0f;
        ui_rect(R, hx - 3, 93, hw + 6, 11, 0, 0, 0, 0.6f);
        ui_rect(R, hx, 95, hw * st, 7, g->tired ? 1.0f : 0.95f, g->tired ? 0.35f : 0.8f, 0.15f, bl);
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
    ui_text(R, g_desktop ? "M = MAP" : "TAP MAP", 116, 222, 14, 1, 1, 1, 0.55f, 1);
    draw_mp_hud();
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

    if (g_desktop) {
        ui_rect(R, 0, H - 30, W, 30, 0, 0, 0, 0.45f);
        {
            char hb[200];
            if (mode == MODE_FOOT)
                snprintf(hb, sizeof(hb), "%s/%s/%s/%s walk   MOUSE look   CLICK fire   %s jump   %s enter   %s run   %s gun   %s map   ESC pause",
                         key_label(KA_FWD), key_label(KA_LEFT), key_label(KA_BACK), key_label(KA_RIGHT), key_label(KA_JUMP), key_label(KA_ACTION),
                         key_label(KA_RUN), key_label(KA_SWITCH), key_label(KA_MAP));
            else
                snprintf(hb, sizeof(hb), "%s gas   %s brake/reverse   %s/%s steer   MOUSE look   %s exit   %s map   ESC pause",
                         key_label(KA_FWD), key_label(KA_BACK), key_label(KA_LEFT), key_label(KA_RIGHT), key_label(KA_ACTION), key_label(KA_MAP));
            ui_text(R, hb, W * 0.5f, H - 26, 15, 1, 1, 1, 0.85f, 1);
        }
        if (mode != MODE_FOOT) {
            Vehicle *v = &g->veh[g->p.veh];
            snprintf(b, sizeof(b), "%d KM/H", (int)(fabsf(v->speed) * 3.6f + 0.5f));
            ui_text(R, b, W - 24, H - 80, 34, 1, 1, 1, 0.9f, 2);
        }
        if (mode == MODE_FOOT && game_near_vehicle(g) >= 0) ui_text(R, "PRESS E", W * 0.5f, H - 90, 26, 1, 0.85f, 0.3f, 0.6f + 0.4f * sinf(F.t * 6), 1);
        return;
    }
    if (P.connected && P.active) {   /* controller: geen touch-knoppen */
        char hb[160];
        snprintf(hb, sizeof(hb), mode == MODE_FOOT ? "%s jump   %s enter   %s run   %s fire   %s gun   %s map   %s pause"
                                                   : "%s gas   %s brake   %s exit   %s map   %s pause",
                 mode == MODE_FOOT ? PB_NAMES[P.bind[PA_JUMP]] : PB_NAMES[P.bind[PA_GAS]],
                 mode == MODE_FOOT ? PB_NAMES[P.bind[PA_ACTION]] : PB_NAMES[P.bind[PA_BRAKE]],
                 mode == MODE_FOOT ? PB_NAMES[P.bind[PA_RUN]] : PB_NAMES[P.bind[PA_ACTION]],
                 mode == MODE_FOOT ? PB_NAMES[P.bind[PA_FIRE]] : PB_NAMES[P.bind[PA_MAP]],
                 mode == MODE_FOOT ? PB_NAMES[P.bind[PA_SWITCH]] : PB_NAMES[P.bind[PA_PAUSE]],
                 PB_NAMES[P.bind[PA_MAP]], PB_NAMES[P.bind[PA_PAUSE]]);
        ui_rect(R, 0, H - 30, W, 30, 0, 0, 0, 0.45f);
        ui_text(R, hb, W * 0.5f, H - 26, 16, 1, 1, 1, 0.85f, 1);
        if (mode != MODE_FOOT) {
            Vehicle *v = &g->veh[g->p.veh];
            snprintf(hb, sizeof(hb), "%d KM/H", (int)(fabsf(v->speed) * 3.6f + 0.5f));
            ui_text(R, hb, W - 24, H - 80, 34, 1, 1, 1, 0.9f, 2);
        }
        return;
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
        btn_pos(CTL_RUN, &x, &y, &r); draw_button(x, y, r, "RUN", ctl_down(CTL_RUN), !g->tired, 22);
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


/* missie i is vrij als het de eerste is, als hij al eens gehaald is, of als de vorige gehaald is */
static int mission_unlocked(int i) { return i <= 0 || F.G.completed[i] || F.G.completed[i - 1]; }

static void draw_missions(void) {
    Renderer *R = &F.R;
    int i;
    char b[32];
    if (F.missionsFrom == ST_PAUSE) {
        r_draw_scene(R, &F.G);
        panel_on();
        ui_begin(R);
        ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 0.75f);
    } else {
        ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 1);
        draw_backdrop(0.35f, 0.0f, F.t);
        ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 0.5f);
    }
    ui_text(R, "MISSIONS", R->W * 0.5f, 18, 52, 1, 1, 1, 1, 1);
    if (F.missionPage * 12 >= NMISSIONS) F.missionPage = 0;
    for (i = F.missionPage * 12; i < NMISSIONS && i < F.missionPage * 12 + 12; i++) {
        const MissionDef *m = &F.G.mdef[i];
        int j = i - F.missionPage * 12;
        float x = (j & 1) ? 522.0f : 62.0f, y = 96.0f + (j / 2) * 98.0f;
        int pr = F.pressed == i + 1, done = F.G.completed[i], open = mission_unlocked(i);
        float la = open ? 1.0f : 0.45f;
        ui_rect(R, x, y, 440, 90, pr && open ? 0.55f : 0.06f, pr && open ? 0.08f : 0.05f, pr && open ? 0.08f : 0.06f, 0.78f);
        ui_rect(R, x, y, 6, 90, !open ? 0.4f : (done ? 0.35f : 0.95f), !open ? 0.4f : (done ? 0.9f : 0.75f), !open ? 0.45f : (done ? 0.4f : 0.15f), 1);
        snprintf(b, sizeof(b), "%d. %s", i + 1, m->title);
        ui_text(R, b, x + 18, y + 6, 30, 1, 1, 1, la, 0);
        if (!open) {
            char lb[64];
            snprintf(lb, sizeof(lb), "LOCKED - finish mission %d first", i);
            ui_text(R, lb, x + 18, y + 44, 15, 1.0f, 0.55f, 0.45f, 1, 0);
        } else {
            float bw = ui_text_w(R, m->brief, 15.0f), bs = bw > 404.0f ? 15.0f * 404.0f / bw : 15.0f;
            ui_text(R, m->brief, x + 18, y + 44 + (15.0f - bs) * 0.5f, bs, 0.82f, 0.82f, 0.82f, 1, 0);
        }
        snprintf(b, sizeof(b), "$%d", done ? (m->repeatReward ? m->repeatReward : m->reward / 2) : m->reward);
        ui_text(R, b, x + 426, y + 8, 26, 0.55f, 1.0f, 0.55f, 1, 2);
        ui_text(R, m->tag ? m->tag : "", x + 426, y + 66, 17, 1.0f, 0.8f, 0.3f, 1, 2);
        if (done) ui_text(R, "DONE", x + 18, y + 66, 17, 0.4f, 1.0f, 0.45f, 1, 0);
        else if (m->time > 0) {
            snprintf(b, sizeof(b), "TIME %d:%02d", (int)m->time / 60, (int)m->time % 60);
            ui_text(R, b, x + 18, y + 66, 17, 0.9f, 0.9f, 0.9f, 1, 0);
        }
    }
    if (F.shopMsg[0] && F.st < 4.0f) ui_text(R, F.shopMsg, R->W * 0.5f, R->H - 112.0f, 20, 1, 0.75f, 0.4f, 1, 1);
    big_button("BACK", R->W * 0.5f, R->H - 60.0f, 44, F.pressed == 100, 1);
    {
        int pages = (NMISSIONS + 11) / 12;
        char pg[16];
        snprintf(pg, sizeof(pg), "PAGE %d/%d", F.missionPage + 1, pages);
        ui_text(R, pg, R->W * 0.5f, 74, 18, 0.85f, 0.85f, 0.85f, 1, 1);
        ui_rect(R, 62, R->H - 92.0f, 150, 62, F.pressed == 101 ? 0.6f : 0.12f, 0.12f, 0.15f, 0.9f);
        ui_text(R, "< PREV", 137, R->H - 78.0f, 28, 1, 1, 1, F.missionPage > 0 ? 1.0f : 0.3f, 1);
        ui_rect(R, R->W - 212.0f, R->H - 92.0f, 150, 62, F.pressed == 102 ? 0.6f : 0.12f, 0.12f, 0.15f, 0.9f);
        ui_text(R, "NEXT >", R->W - 137.0f, R->H - 78.0f, 28, 1, 1, 1, F.missionPage + 1 < pages ? 1.0f : 0.3f, 1);
    }
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
        static const char *names[5] = {"BLOCK ARMS", "GIVEAWAY BANK", "GIVEAWAY MOTORS", "BLOCK CUSTOMS", "YOUR HOME"};
        static const int pois[4] = {POI_GUNSHOP, POI_BANK, POI_DEALER, POI_CUSTOMS};
        static const int icons[4] = {ICON_GUN, ICON_BANK, ICON_CAR, ICON_CUSTOMS};
        int q;
        bm_w2s(w->mmOrg, w->mmOrgZ, &x0, &y0);
        ui_quad(R, &R->rw[0].minimap, x0, y0, w->mmSpan * F.bmScale, w->mmSpan * F.bmScale, 0, 0, 1, 1, 1, 1, 1, 1);
        if (F.G.mission >= 0) {   /* alleen het doel van de lopende missie */
            V3 tg[12];
            int n2 = game_targets(&F.G, tg, 12);
            for (q = 0; q < n2; q++) {
                bm_w2s(tg[q].x, tg[q].z, &sx, &sy);
                ui_icon(R, ICON_MISSION, sx, sy, 30, 1);
            }
        }
        for (q = 0; q < 5 + BIZ_COUNT; q++) {
            float px2, pz2;
            if (q < 4) { px2 = w->poi[pois[q]].x; pz2 = w->poi[pois[q]].z; }
            else if (q == 4) home_door(F.G.homeLvl, &px2, &pz2);
            else { px2 = w->poi[biz_poi(q - 5)].x; pz2 = w->poi[biz_poi(q - 5)].z; }
            bm_w2s(px2, pz2, &sx, &sy);
            ui_icon(R, q < 4 ? icons[q] : (q == 4 ? ICON_HOME : ICON_BURGER + (q - 5)), sx, sy, q >= 5 ? 34 : 40, 1);
            ui_text(R, q < 5 ? names[q] : biz_name(q - 5), sx, sy + 18, 16, 1, 1, 1, 1, 1);
        }
        {   /* v4.0 GIVEAWAY MART */
            bm_w2s(w->poi[POI_MART].x, w->poi[POI_MART].z, &sx, &sy);
            ui_circle(R, sx, sy, 15, 0, 0, 0, 0.85f);
            ui_circle(R, sx, sy, 12, 0.25f, 0.85f, 0.35f, 1);
            ui_text(R, "$", sx, sy - 9, 18, 1, 1, 1, 1, 1);
            ui_text(R, store_name(STORE_MART), sx, sy + 18, 16, 1, 1, 1, 1, 1);
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
        static const char *leg[5] = {"GUNS", "BANK", "CARS", "CUSTOMS", "MISSION GOAL"};
        int q;
        ui_rect(R, R->W * 0.5f - 300, R->H - 50.0f, 600, 44, 0, 0, 0, 0.6f);
        for (q = 0; q < 5; q++) {
            float lx = R->W * 0.5f - 270 + q * 112.0f;
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
    panel_on();
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
        if (game_buy(&F.G, hit - 600)) { snprintf(F.shopMsg, sizeof(F.shopMsg), "Bought: %s", name); if (F.sfxOn) plat_sfx(SFX_BUY, 0.8f); }
        else snprintf(F.shopMsg, sizeof(F.shopMsg), price > 0 ? "Not enough money (or you already have it)." : "Buy a gun first.");
        F.G.moneyShown = F.G.money;
        save_all();
    }
}

/* ------------------------------------------------------------------ ONLINE multiplayer: scherm, HUD, test */

static char mpCode[12];
static int mpCodeLen, g_mptest;
static double mptestT0;
static int fe_quit;

static void mp_host_here(void) {
    if (F.mode == 1) { double ox, oy; ns_origin(&ox, &oy); mp_host(1, ox, oy, F.nlLabel); }
    else mp_host(0, 0, 0, "Small Giveaway City");
}

static void mp_try_enter_world(void) {
    int world;
    double ox, oy;
    char label[64];
    if (!mp_take_world(&world, &ox, &oy, label, sizeof(label))) return;
    mp_lan_listen(0);
    if (world == 1) nl_play_at(ox, oy, label[0] ? label : "Online", ST_MENU);
    else {
        if (F.mode != 0) nl_exit_to_city();
        start_game();
    }
    snprintf(F.G.banner, sizeof(F.G.banner), "ONLINE");
    snprintf(F.G.sub, sizeof(F.G.sub), "Connected - you are playing together");
    F.G.bannerT = 4.0f; F.G.bannerGood = 1;
}

static void mp_key_rect(int row, int col, float *x, float *y) {
    float x0[4] = {548, 548, 570, 592};
    *x = x0[row] + col * 44.0f;
    *y = 330.0f + row * 52.0f;
}

static int mp_hit(float px, float py) {
    int r, c;
    float x, y;
    if (px > 40 && px < 500 && py > 112 && py < 182) return 1001;
    for (r = 0; r < 4; r++)
        for (c = 0; KROWS[r][c]; c++) {
            mp_key_rect(r, c, &x, &y);
            if (px >= x && px < x + 40 && py >= y && py < y + 46) return KROWS[r][c];
        }
    mp_key_rect(3, 7, &x, &y);
    if (px >= x && px < x + 108 && py >= y && py < y + 46) return 1003;
    if (px > 548 && px < 988 && py > 548 && py < 610) return 1002;
    if (px > 20 && px < 220 && py > 680 && py < 745) return 1004;
    for (r = 0; r < mp_lan_count() && r < 4; r++) {
        float ry = 360.0f + r * 64.0f;
        if (px > 40 && px < 500 && py > ry && py < ry + 56) return 1100 + r;
    }
    return 0;
}

static void draw_mp_screen(void) {
    Renderer *R = &F.R;
    char b[200];
    int r, c, i;
    float x, y;
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 1);
    draw_backdrop(0.3f, 0.0f, F.t);
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 0.6f);
    ui_text(R, "ONLINE MULTIPLAYER", R->W * 0.5f, 12, 46, 1, 1, 1, 1, 1);
    snprintf(b, sizeof(b), "You are: %s   -   up to 4 players, peer to peer", mp_name());
    ui_text(R, b, R->W * 0.5f, 66, 18, 0.85f, 0.85f, 0.85f, 1, 1);
    /* links: hosten + LAN */
    ui_rect(R, 40, 112, 460, 70, F.pressed == 1001 ? 0.9f : 0.75f, 0.12f, 0.12f, 0.95f);
    ui_text(R, "HOST A GAME", 270, 124, 40, 1, 1, 1, 1, 1);
    ui_text(R, "Starts the city. People on your Wi-Fi see it below;", 270, 192, 15, 0.85f, 0.85f, 0.85f, 1, 1);
    ui_text(R, "over the internet they type your invite code.", 270, 210, 15, 0.85f, 0.85f, 0.85f, 1, 1);
    ui_text(R, "Tip: host from the pause menu to play together in REAL NL.", 270, 228, 15, 0.85f, 0.85f, 0.85f, 1, 1);
    ui_text(R, "GAMES ON YOUR WI-FI", 40, 320, 24, 1.0f, 0.85f, 0.4f, 1, 0);
    if (mp_lan_count() == 0) ui_text(R, "Searching... (nobody is hosting on this network yet)", 40, 362, 16, 0.8f, 0.8f, 0.8f, 1, 0);
    for (i = 0; i < mp_lan_count() && i < 4; i++) {
        const MPLan *l = mp_lan(i);
        float ry = 360.0f + i * 64.0f;
        ui_rect(R, 40, ry, 460, 56, F.pressed == 1100 + i ? 0.6f : 0.1f, F.pressed == 1100 + i ? 0.1f : 0.3f, 0.12f, 0.9f);
        snprintf(b, sizeof(b), "%s", l->name);
        ui_text(R, b, 56, ry + 6, 26, 1, 1, 1, 1, 0);
        snprintf(b, sizeof(b), "%d/4 players - %s - tap to JOIN", l->players, l->world ? "REAL NL" : "city");
        ui_text(R, b, 56, ry + 34, 15, 0.85f, 0.85f, 0.85f, 1, 0);
    }
    /* rechts: code invoeren */
    ui_text(R, "JOIN WITH A CODE", 548, 112, 24, 1.0f, 0.85f, 0.4f, 1, 0);
    ui_text(R, "Ask the host for the invite code (10 letters/digits).", 548, 142, 15, 0.85f, 0.85f, 0.85f, 1, 0);
    ui_rect(R, 548, 168, 440, 70, 0, 0, 0, 0.7f);
    {
        char shown[16];
        if (mpCodeLen > 5) snprintf(shown, sizeof(shown), "%.5s-%s", mpCode, mpCode + 5);
        else snprintf(shown, sizeof(shown), "%s", mpCode);
        if (!mpCodeLen) ui_text(R, "XXXXX-XXXXX", 768, 178, 44, 0.5f, 0.5f, 0.5f, 0.6f, 1);
        else ui_text(R, shown, 768, 178, 44, 1, 1, 1, 1, 1);
    }
    for (r = 0; r < 4; r++)
        for (c = 0; KROWS[r][c]; c++) {
            char lab[2] = {KROWS[r][c], 0};
            mp_key_rect(r, c, &x, &y);
            ui_rect(R, x, y, 40, 46, F.pressed == KROWS[r][c] ? 0.7f : 0.12f, 0.12f, 0.14f, 0.9f);
            ui_text(R, lab, x + 20, y + 8, 28, 1, 1, 1, mpCodeLen < 10 ? 1.0f : 0.35f, 1);
        }
    mp_key_rect(3, 7, &x, &y);
    ui_rect(R, x, y, 108, 46, F.pressed == 1003 ? 0.7f : 0.3f, 0.08f, 0.08f, 0.9f);
    ui_text(R, "DELETE", x + 54, y + 12, 22, 1, 1, 1, 1, 1);
    ui_rect(R, 548, 548, 440, 62, mpCodeLen == 10 ? (F.pressed == 1002 ? 0.6f : 0.15f) : 0.2f, mpCodeLen == 10 ? 0.45f : 0.2f, mpCodeLen == 10 ? 0.9f : 0.2f, 0.95f);
    ui_text(R, "JOIN", 768, 556, 40, 1, 1, 1, mpCodeLen == 10 ? 1.0f : 0.4f, 1);
    if (mp_status()[0]) ui_text(R, mp_status(), R->W * 0.5f, 632, 18, 1, 0.85f, 0.4f, 1, 1);
    big_button("BACK", 120, 712, 40, F.pressed == 1004, 1);
    ui_text(R, "Traffic, people and police are separate for each player.", R->W * 0.5f + 100, 724, 14, 0.7f, 0.7f, 0.7f, 1, 1);
}

static void mp_screen_tap(float x, float y, int phase) {
    int k = mp_hit(x, y);
    if (phase != TOUCH_END) { F.pressed = k; return; }
    F.pressed = 0;
    if (k == 1001) {
        if (F.mode != 0) nl_exit_to_city();
        mp_lan_listen(0);
        start_game();
        mp_host_here();
        snprintf(F.G.banner, sizeof(F.G.banner), "HOSTING ONLINE");
        snprintf(F.G.sub, sizeof(F.G.sub), "Friends can join now - the invite code is in the pause menu");
        F.G.bannerT = 5.0f; F.G.bannerGood = 1;
    } else if (k == 1002 && mpCodeLen == 10) {
        uint32_t ip;
        int port;
        if (mp_code_decode(mpCode, &ip, &port)) mp_join(ip, port);
    } else if (k == 1003) {
        if (mpCodeLen > 0) mpCode[--mpCodeLen] = 0;
    } else if (k == 1004) {
        if (mp_mode() == MP_JOINING) mp_leave();
        mp_lan_listen(0);
        set_state(ST_MENU);
    } else if (k >= 1100 && k < 1100 + MP_MAXLAN) {
        const MPLan *l = mp_lan(k - 1100);
        if (l) mp_join(l->ip, l->port);
    } else if (((k >= 'A' && k <= 'Z') || (k >= '0' && k <= '9')) && mpCodeLen < 10) {
        mpCode[mpCodeLen++] = (char)k; mpCode[mpCodeLen] = 0;
    }
}

static void open_mp_screen(void) {
    mp_lan_listen(1);
    set_state(ST_MP);
}

/* naamlabels en lijst in de HUD */
static void draw_mp_hud(void) {
    Renderer *R = &F.R;
    Game *g = &F.G;
    char b[96];
    int i, n = 0;
    if (!mp_mode()) return;
    for (i = 0; i < MAX_REMOTE; i++) {
        RemotePlayer *rp = &g->remote[i];
        const float *m = R->vp.m;
        float px = rp->x, py = rp->y + 2.4f + (rp->mode == 2 ? 0.4f : 0.0f), pz = rp->z, cx, cy, cw;
        if (!rp->active) continue;
        n++;
        cx = m[0] * px + m[4] * py + m[8] * pz + m[12];
        cy = m[1] * px + m[5] * py + m[9] * pz + m[13];
        cw = m[3] * px + m[7] * py + m[11] * pz + m[15];
        if (cw > 0.5f && cw < 120.0f) {
            float sx = (cx / cw * 0.5f + 0.5f) * R->W, sy = (0.5f - cy / cw * 0.5f) * R->H;
            float tw = ui_text_w(R, rp->name, 18);
            ui_rect(R, sx - tw * 0.5f - 8, sy - 4, tw + 16, 26, 0, 0, 0, 0.55f);
            ui_text(R, rp->name, sx, sy - 1, 18, 0.6f, 0.9f, 1.0f, 1, 1);
            ui_rect(R, sx - 24, sy + 23, 48, 4, 0.3f, 0, 0, 0.8f);
            ui_rect(R, sx - 24, sy + 23, 48 * clampf(rp->health / 100.0f, 0, 1), 4, 0.3f, 1.0f, 0.3f, 0.9f);
        }
    }
    snprintf(b, sizeof(b), "ONLINE %s - %d/4", mp_mode() == MP_HOST ? "HOST" : "PLAYER", n + 1);
    ui_rect(R, 14, 268, 206, 24 + n * 20, 0, 0, 0, 0.5f);
    ui_text(R, b, 22, 270, 17, 0.6f, 0.9f, 1.0f, 1, 0);
    for (i = 0, n = 0; i < MAX_REMOTE; i++) if (g->remote[i].active) { ui_text(R, g->remote[i].name, 30, 292.0f + n * 20, 15, 1, 1, 1, 0.9f, 0); n++; }
}

void fe_set_mptest(int mode) { g_mptest = mode; }
int fe_should_quit(void) { return fe_quit; }

static void mptest_tick(void) {
    char rep2[600];
    int i, n;
    if (!g_mptest || !F.loadDone) return;
    if (mptestT0 == 0 && F.state == ST_MENU) {
        mptestT0 = plat_time();
        if (g_mptest == 1) {
            mp_set_name("TestHost");
            start_game();
            mp_host_here();
        } else {
            mp_set_name("TestJoiner");
            open_mp_screen();
        }
    }
    if (mptestT0 == 0) return;
    if (g_mptest == 2 && mp_mode() == MP_OFF && mp_lan_count() > 0) {
        const MPLan *l = mp_lan(0);
        plat_log("SGB-MP test: LAN-spel gevonden '%s', verbinden", l->name);
        mp_join(l->ip, l->port);
    }
    /* rondjes lopen zodat de ander beweging ziet */
    g_dk.my = 1.0f; g_dk.mx = g_mptest == 1 ? 0.6f : -0.6f;
    if (plat_time() - mptestT0 > 30.0) {
        n = snprintf(rep2, sizeof(rep2), "mptest %s: modus=%d, spelers=%d, status='%s', code-internet='%s', code-lan='%s'\n",
                     g_mptest == 1 ? "host" : "join", mp_mode(), mp_players(), mp_status(), mp_code_internet(), mp_code_lan());
        for (i = 0; i < MAX_REMOTE; i++) {
            RemotePlayer *rp = &F.G.remote[i];
            if (!rp->active) continue;
            n += snprintf(rep2 + n, sizeof(rep2) - (size_t)n, "  ziet '%s' (id %d) op %.1f, %.1f, modus %d, gezondheid %.0f\n", rp->name, rp->id, rp->x, rp->z, rp->mode, rp->health);
        }
        plat_log("%s", rep2);
        plat_write_text(g_mptest == 1 ? "mp_report_host.txt" : "mp_report_join.txt", rep2);
        mp_leave();
        fe_quit = 1;
        g_mptest = 0;
    }
}

static const char *CUSTOM_ITEMS[5] = {"PAINT", "ENGINE", "HANDLING", "NEON", "SPOILER"};
static const char *PAINT_NAMES[NPAINTS] = {"RED", "BLUE", "YELLOW", "WHITE", "GREEN", "BLACK", "ORANGE", "PURPLE", "CYAN", "SILVER"};
static const char *NEON_NAMES[NNEONS] = {"OFF", "RED", "BLUE", "GREEN", "PINK", "WHITE"};



/* ------------------------------------------------------------------ v3.2: winkels in de stad (generiek menu) */

static void draw_biz(void) {
    Renderer *R = &F.R;
    Game *g = &F.G;
    BizItem it[BIZ_MAX_ITEMS];
    char b[64];
    float cr, cg, cb, rowH;
    int i, n = biz_items(g, F.bizShop, it);
    biz_color(F.bizShop, &cr, &cg, &cb);
    rowH = n > 5 ? 60.0f : 84.0f;
    r_draw_scene(R, g);
    panel_on();
    ui_begin(R);
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 0.7f);
    ui_rect(R, 132, 30, 760, 650, 0.07f, 0.07f, 0.09f, 0.96f);
    ui_rect(R, 132, 30, 760, 66, cr * 0.85f, cg * 0.85f, cb * 0.85f, 1);
    ui_icon(R, ICON_BURGER + F.bizShop, 180, 63, 46, 1);
    ui_text(R, biz_name(F.bizShop), 512, 38, 48, 1, 1, 1, 1, 1);
    fmt_money(b, sizeof(b), g->money);
    ui_text(R, b, 872, 104, 30, 0.55f, 1.0f, 0.55f, 1, 2);
    ui_text(R, biz_tagline(F.bizShop), 156, 110, 17, 0.8f, 0.8f, 0.8f, 1, 0);
    for (i = 0; i < n; i++) {
        float y = 146.0f + i * rowH, bh = rowH - 22.0f;
        int pr = F.pressed == 1500 + i, ok = it[i].state == 0 && g->money >= it[i].price;
        ui_rect(R, 152, y, 720, rowH - 8, 0.12f, 0.12f, 0.15f, 0.95f);
        if (it[i].state == 1) ui_rect(R, 152, y, 8, rowH - 8, cr, cg, cb, 1);
        ui_text(R, it[i].name, 172, y + 4, rowH > 70 ? 30 : 24, 1, 1, 1, it[i].state == 2 ? 0.5f : 1.0f, 0);
        ui_text(R, it[i].info, 172, y + (rowH > 70 ? 44 : 32), 15, 0.8f, 0.8f, 0.8f, 1, 0);
        if (F.bizShop == BIZ_BLING && it[i].state == 1) { ui_text(R, "OWNED", 780, y + bh * 0.5f - 6, 22, cr, cg, cb, 1, 1); continue; }
        if (F.bizShop == BIZ_DRIP && it[i].state == 1) { ui_text(R, "WEARING", 780, y + bh * 0.5f - 6, 22, cr, cg, cb, 1, 1); continue; }
        if (it[i].state == 2) continue;
        ui_rect(R, 712, y + 7, 140, bh, pr ? 0.9f : (ok ? 0.2f : 0.25f), pr ? 0.2f : (ok ? 0.55f : 0.25f), pr ? 0.2f : 0.25f, 1);
        if (it[i].price > 0) { snprintf(b, sizeof(b), "$%d", it[i].price); ui_text(R, b, 782, y + bh * 0.5f - 6, 26, 1, 1, 1, 1, 1); }
        else ui_text(R, F.bizShop == BIZ_DRIP ? "WEAR" : "FREE", 782, y + bh * 0.5f - 6, 24, 1, 1, 1, 1, 1);
    }
    if (F.shopMsg[0]) ui_text(R, F.shopMsg, 512, 650, 20, 1, 0.85f, 0.4f, 1, 1);
    big_button("LEAVE", 512, 718, 40, F.pressed == 1590, 1);
    ui_flush(R);
}

static void biz_tap(float x, float y, int phase) {
    BizItem it[BIZ_MAX_ITEMS];
    int i, hit = 0, n = biz_items(&F.G, F.bizShop, it);
    float rowH = n > 5 ? 60.0f : 84.0f;
    for (i = 0; i < n; i++) {
        float yy = 146.0f + i * rowH;
        if (x > 712 && x < 852 && y > yy + 4 && y < yy + rowH - 10) hit = 1500 + i;
    }
    if (hit_text(x, y, "LEAVE", 512, 740, 40)) hit = 1590;
    if (phase != TOUCH_END) { F.pressed = hit; return; }
    F.pressed = 0;
    if (hit == 1590) { set_state(ST_GAME); save_all(); return; }
    if (hit >= 1500 && hit < 1500 + n) {
        if (biz_buy(&F.G, F.bizShop, hit - 1500, F.shopMsg, sizeof(F.shopMsg)) && F.sfxOn) plat_sfx(SFX_BUY, 0.8f);
        F.G.moneyShown = F.G.money;
        save_all();
    }
}

/* ------------------------------------------------------------------ eigen huis: laptop-scherm */

static const char *HOME_NAMES[HOME_LEVELS] = {"SMALL HOUSE", "FAMILY HOUSE", "VILLA"};

static void draw_home(void) {
    Renderer *R = &F.R;
    Game *g = &F.G;
    char b[128];
    int k, hp = home_upgrade_price(1, g->homeLvl), gp = home_upgrade_price(0, g->garLvl), owned = 0;
    for (k = 0; k < MAX_OWNED; k++) owned += g->garage[k].exists;
    r_draw_scene(R, g);
    panel_on();
    ui_begin(R);
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 0.65f);
    ui_rect(R, 132, 60, 760, 600, 0.07f, 0.07f, 0.09f, 0.96f);
    ui_rect(R, 132, 60, 760, 70, 0.55f, 0.25f, 0.8f, 1);
    ui_text(R, "YOUR HOME", 512, 70, 50, 1, 1, 1, 1, 1);
    fmt_money(b, sizeof(b), g->money);
    ui_text(R, b, 872, 142, 30, 0.55f, 1.0f, 0.55f, 1, 2);
    /* huis */
    ui_rect(R, 160, 190, 704, 170, 0.12f, 0.12f, 0.15f, 0.95f);
    snprintf(b, sizeof(b), "HOUSE: %s", HOME_NAMES[g->homeLvl < 0 ? 0 : g->homeLvl % HOME_LEVELS]);
    ui_text(R, b, 184, 200, 32, 1, 1, 1, 1, 0);
    for (k = 0; k < HOME_LEVELS; k++) ui_rect(R, 184 + k * 64.0f, 248, 56, 12, k <= g->homeLvl ? 0.6f : 0.25f, k <= g->homeLvl ? 0.35f : 0.25f, k <= g->homeLvl ? 0.9f : 0.3f, 1);
    ui_text(R, g->homeLvl == 0 ? "Next: FAMILY HOUSE - two floors, kitchen and a porch." : (g->homeLvl == 1 ? "Next: VILLA - big glass front, balcony, dining room, library." : "This is the biggest house. Enjoy the villa life!"),
            184, 276, 17, 0.85f, 0.85f, 0.85f, 1, 0);
    if (hp >= 0) {
        int af = g->money >= hp;
        ui_rect(R, 624, 296, 220, 52, F.pressed == 801 ? 0.9f : (af ? 0.2f : 0.25f), F.pressed == 801 ? 0.2f : (af ? 0.55f : 0.25f), 0.25f, 1);
        fmt_money(b, sizeof(b), hp);
        ui_text(R, "UPGRADE", 734, 300, 20, 1, 1, 1, 1, 1);
        ui_text(R, b, 734, 322, 22, 1, 1, 1, 1, 1);
    }
    /* garage */
    ui_rect(R, 160, 380, 704, 170, 0.12f, 0.12f, 0.15f, 0.95f);
    snprintf(b, sizeof(b), "UNDERGROUND GARAGE: %d SPOT%s", g->garLvl + 1, g->garLvl ? "S" : "");
    ui_text(R, b, 184, 390, 32, 1, 1, 1, 1, 0);
    for (k = 0; k < GARAGE_LEVELS; k++) ui_rect(R, 184 + k * 64.0f, 438, 56, 12, k <= g->garLvl ? 0.6f : 0.25f, k <= g->garLvl ? 0.35f : 0.25f, k <= g->garLvl ? 0.9f : 0.3f, 1);
    snprintf(b, sizeof(b), "You own %d vehicle%s. Bought cars park here; the rest wait at GIVEAWAY MOTORS.", owned, owned == 1 ? "" : "s");
    ui_text(R, b, 184, 466, 17, 0.85f, 0.85f, 0.85f, 1, 0);
    if (gp >= 0) {
        int af = g->money >= gp;
        ui_rect(R, 624, 486, 220, 52, F.pressed == 802 ? 0.9f : (af ? 0.2f : 0.25f), F.pressed == 802 ? 0.2f : (af ? 0.55f : 0.25f), 0.25f, 1);
        fmt_money(b, sizeof(b), gp);
        ui_text(R, "+1 SPOT", 734, 490, 20, 1, 1, 1, 1, 1);
        ui_text(R, b, 734, 512, 22, 1, 1, 1, 1, 1);
    }
    if (F.shopMsg[0]) ui_text(R, F.shopMsg, 512, 566, 20, 1, 0.85f, 0.4f, 1, 1);
    ui_text(R, "Tip: sleep in the bed to restore your health. The garage door is next to the TV.", 512, 600, 15, 0.75f, 0.75f, 0.75f, 1, 1);
    big_button("LEAVE", 512, 724, 40, F.pressed == 890, 1);
    ui_flush(R);
}

static void home_tap(float x, float y, int phase) {
    int hit = 0;
    if (x > 624 && x < 844 && y > 296 && y < 348) hit = 801;
    if (x > 624 && x < 844 && y > 486 && y < 538) hit = 802;
    if (hit_text(x, y, "LEAVE", 512, 746, 40)) hit = 890;
    if (phase != TOUCH_END) { F.pressed = hit; return; }
    F.pressed = 0;
    if (hit == 890) { set_state(ST_GAME); return; }
    if (hit == 801 || hit == 802) {
        int r = game_home_upgrade(&F.G, hit == 801);
        if (r > 0) {
            if (F.sfxOn) plat_sfx(SFX_BUY, 0.8f);
            snprintf(F.shopMsg, sizeof(F.shopMsg), hit == 801 ? "Your house is bigger now!" : "Your garage has an extra spot!");
            F.G.moneyShown = F.G.money;
            save_all();
        } else snprintf(F.shopMsg, sizeof(F.shopMsg), r < 0 ? "Already the biggest." : "Not enough money.");
    }
}

static void draw_dealer(void) {
    Renderer *R = &F.R;
    char b[96];
    int i, owned = 0;
    for (i = 0; i < MAX_OWNED; i++) owned += F.G.garage[i].exists;
    r_draw_scene(R, &F.G);
    panel_on();
    ui_begin(R);
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 0.7f);
    ui_rect(R, 112, 40, 800, 640, 0.07f, 0.07f, 0.08f, 0.95f);
    ui_rect(R, 112, 40, 800, 70, 0.95f, 0.5f, 0.08f, 1);
    ui_text(R, "GIVEAWAY MOTORS", 512, 50, 50, 1, 1, 1, 1, 1);
    fmt_money(b, sizeof(b), F.G.money);
    ui_text(R, b, 892, 124, 34, 0.55f, 1.0f, 0.55f, 1, 2);
    snprintf(b, sizeof(b), "Your garage: %d / %d vehicles (parked here at the dealer)", owned, MAX_OWNED);
    ui_text(R, b, 136, 132, 18, 0.85f, 0.85f, 0.85f, 1, 0);
    for (i = 0; i < DEALER_MODELS; i++) {
        float y = 166.0f + i * 72.0f;
        int pr = F.pressed == 700 + i, afford = F.G.money >= CARS[i].price, moto = IS_MOTO(i);
        Col pc = paint_color(model_default_paint(i));
        ui_rect(R, 132, y, 760, 66, 0.12f, 0.12f, 0.14f, 0.95f);
        ui_rect(R, 132, y, 10, 66, pc.r / 255.0f, pc.g / 255.0f, pc.b / 255.0f, 1);
        ui_text(R, CARS[i].name, 160, y + 4, 28, 1, 1, 1, 1, 0);
        if (moto) {
            float tw = ui_text_w(R, CARS[i].name, 28);
            ui_rect(R, 170 + tw, y + 10, 72, 22, 0.95f, 0.5f, 0.08f, 1);
            ui_text(R, "MOTOR", 206 + tw, y + 12, 16, 1, 1, 1, 1, 1);
        }
        snprintf(b, sizeof(b), "Top speed %d km/h   -   0-100 in %.1f s", (int)(CARS[i].maxF * 3.6f), 27.8f / (CARS[i].acc * 0.75f));
        ui_text(R, b, 160, y + 38, 15, 0.85f, 0.85f, 0.85f, 1, 0);
        {
            float bar = CARS[i].maxF / 36.0f;
            ui_rect(R, 480, y + 45, 200, 5, 0.25f, 0.25f, 0.28f, 1);
            ui_rect(R, 480, y + 45, 200 * bar, 5, 1.0f, 0.55f, 0.1f, 1);
        }
        ui_rect(R, 712, y + 10, 160, 46, pr ? 0.9f : (afford ? 0.2f : 0.25f), pr ? 0.2f : (afford ? 0.55f : 0.25f), pr ? 0.2f : 0.25f, 1);
        fmt_money(b, sizeof(b), CARS[i].price);
        ui_text(R, b, 792, y + 16, 26, 1, 1, 1, 1, 1);
    }
    if (F.shopMsg[0]) ui_text(R, F.shopMsg, 512, 604, 20, 1, 0.85f, 0.4f, 1, 1);
    ui_text(R, "Cars can be upgraded at BLOCK CUSTOMS (blue marker). Motorbikes are ready to ride.", 512, 636, 16, 0.75f, 0.75f, 0.75f, 1, 1);
    big_button("LEAVE", 512, 724, 40, F.pressed == 790, 1);
    ui_flush(R);
}

static void dealer_tap(float x, float y, int phase) {
    int i, hit = 0;
    for (i = 0; i < DEALER_MODELS; i++) {
        float yy = 166.0f + i * 72.0f;
        if (x > 712 && x < 872 && y > yy + 6 && y < yy + 60) hit = 700 + i;
    }
    if (hit_text(x, y, "LEAVE", 512, 746, 40)) hit = 790;
    if (phase != TOUCH_END) { F.pressed = hit; return; }
    F.pressed = 0;
    if (hit == 790) { set_state(ST_GAME); return; }
    if (hit >= 700 && hit < 700 + DEALER_MODELS) {
        int r = game_buy_car(&F.G, hit - 700);
        if (r > 0) {
            if (F.sfxOn) plat_sfx(SFX_BUY, 0.8f);
            if (IS_MOTO(hit - 700)) {
                snprintf(F.G.banner, sizeof(F.G.banner), "NEW MOTORBIKE!");
                snprintf(F.G.sub, sizeof(F.G.sub), "Your %s is ready. Ride safe!", CARS[hit - 700].name);
            } else {
                snprintf(F.G.banner, sizeof(F.G.banner), "NEW CAR!");
                snprintf(F.G.sub, sizeof(F.G.sub), "Your %s is ready. Upgrade it at BLOCK CUSTOMS.", CARS[hit - 700].name);
            }
            F.G.bannerT = 4.0f; F.G.bannerGood = 1;
            F.G.moneyShown = F.G.money;
            save_all();
            set_state(ST_GAME);
        } else snprintf(F.shopMsg, sizeof(F.shopMsg), r < 0 ? "Your garage is full (4 vehicles)." : "Not enough money.");
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
    panel_on();
    ui_begin(R);
    ui_rect(R, 0, 0, 520, (float)R->H, 0, 0, 0, 0.8f);
    ui_rect(R, 0, 0, 520, 70, 0.1f, 0.35f, 0.95f, 1);
    ui_text(R, "BLOCK CUSTOMS", 260, 10, 48, 1, 1, 1, 1, 1);
    snprintf(b, sizeof(b), "%s  -  top speed %d km/h", v->custom ? CARS[v->model % DEALER_MODELS].name : "YOUR RIDE", (int)((v->custom ? car_top_speed(v) : 19.0f) * 3.6f));
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
        if (game_custom(&F.G, hit - 800)) { F.shopMsg[0] = 0; F.G.moneyShown = F.G.money; save_all(); if (F.sfxOn) plat_sfx(SFX_BUY, 0.7f); }
        else snprintf(F.shopMsg, sizeof(F.shopMsg), game_custom_price(&F.G, hit - 800) < 0 ? "Already maxed out." : "Not enough money.");
    }
}

static void draw_pause(void) {
    Renderer *R = &F.R;
    int i;
    const char *items[5] = {"RESUME", "MISSIONS", "HOST ONLINE", "SETTINGS", "MAIN MENU"};
    if (F.mode == 1) items[1] = "NEW POSTCODE";
    if (mp_mode()) items[2] = "LEAVE ONLINE";
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 0.6f);
    ui_text(R, "PAUSED", R->W * 0.5f, 130, 70, 1, 1, 1, 1, 1);
    for (i = 0; i < 5; i++) big_button(items[i], R->W * 0.5f, 250.0f + i * 88.0f, 44, F.pressed == i + 1, 1);
    if (mp_mode() == MP_HOST) {
        char b[160];
        ui_rect(R, 112, 676, 800, 84, 0, 0, 0, 0.7f);
        snprintf(b, sizeof(b), "INVITE CODE (internet): %s      Wi-Fi code: %s", mp_code_internet()[0] ? mp_code_internet() : "looking up...", mp_code_lan());
        ui_text(R, b, R->W * 0.5f, 684, 22, 1, 0.85f, 0.4f, 1, 1);
        ui_text(R, mp_internet_note(), R->W * 0.5f, 718, 14, 0.85f, 0.85f, 0.85f, 1, 1);
    }
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
    if (F.G.cut) {   /* v4.5: tikken = cutscene overslaan */
        s->ctl = CTL_UI;
        if (phase == TOUCH_BEGIN && F.G.cutT > 0.4f) F.G.cutSkip = 1;
        return;
    }
    if (phase == TOUCH_BEGIN) {
        static const int order[8] = {CTL_PAUSE, CTL_GAS, CTL_BRAKE, CTL_JUMP, CTL_ACTION, CTL_FIRE, CTL_WSWITCH, CTL_RUN};
        int k;
        s->ctl = CTL_NONE;
        if (s->x < 224 && s->y < 236) { s->ctl = CTL_UI; bm_open(); return; }
        for (k = 0; k < 8; k++) {
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

/* ------------------------------------------------------------------ slotscherm (PLAY) */

static const char *HOUSE_SHORT[3] = {"small house", "family house", "villa"};

static void draw_slots(void) {
    Renderer *R = &F.R;
    char b[128];
    int sl;
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 1);
    draw_backdrop(0.35f, 0.0f, F.t);
    ui_rect(R, 0, 0, (float)R->W, (float)R->H, 0, 0, 0, 0.55f);
    ui_text(R, "CHOOSE A SAVE SLOT", R->W * 0.5f, 26, 50, 1, 1, 1, 1, 1);
    for (sl = 1; sl <= NSLOTS; sl++) {
        float y = 110.0f + (sl - 1) * 170.0f;
        int used = sload_in(sl, "used", 0), cur = sl == F.slot, k, done = 0, dm = sload_in(sl, "done", 0);
        long money = sload_in(sl, "money", 0);
        int pr = F.pressed == 1700 + sl, pd = F.pressed == 1710 + sl;
        for (k = 0; k < NMISSIONS; k++) done += (dm >> k) & 1;
        ui_rect(R, 112, y, 800, 150, pr ? 0.35f : 0.08f, pr ? 0.1f : 0.08f, pr ? 0.1f : 0.1f, 0.88f);
        ui_rect(R, 112, y, 8, 150, cur ? 1.0f : 0.4f, cur ? 0.8f : 0.4f, cur ? 0.2f : 0.45f, 1);
        snprintf(b, sizeof(b), "SLOT %d%s", sl, cur ? "   (last played)" : "");
        ui_text(R, b, 140, y + 10, 34, 1, 1, 1, 1, 0);
        if (used) {
            int hl = sload_in(sl, "home", 0);
            fmt_money(b, sizeof(b), money);
            ui_text(R, b, 140, y + 58, 30, 0.55f, 1.0f, 0.55f, 1, 0);
            snprintf(b, sizeof(b), "%d / %d missions done  -  %s", done, NMISSIONS, HOUSE_SHORT[hl < 0 || hl > 2 ? 0 : hl]);
            ui_text(R, b, 140, y + 100, 20, 0.85f, 0.85f, 0.85f, 1, 0);
        } else ui_text(R, "EMPTY - start a brand new game", 140, y + 70, 22, 0.7f, 0.7f, 0.7f, 1, 0);
        ui_rect(R, 690, y + 22, 200, 60, pr ? 0.9f : 0.2f, pr ? 0.2f : 0.55f, 0.2f, 1);
        ui_text(R, used ? "PLAY" : "NEW GAME", 790, y + 36, used ? 32 : 26, 1, 1, 1, 1, 1);
        if (used) {
            int arm = F.slotDelArm == sl;
            ui_rect(R, 690, y + 94, 200, 40, pd || arm ? 0.85f : 0.3f, 0.12f, 0.12f, 1);
            ui_text(R, arm ? "TAP AGAIN TO DELETE" : "DELETE", 790, y + 102, arm ? 15 : 20, 1, 1, 1, 1, 1);
        }
    }
    big_button("BACK", R->W * 0.5f, R->H - 64.0f, 42, F.pressed == 1790, 1);
}

static void slots_tap(float x, float y, int phase) {
    Renderer *R = &F.R;
    int sl, hit = 0;
    for (sl = 1; sl <= NSLOTS; sl++) {
        float yy = 110.0f + (sl - 1) * 170.0f;
        if (x > 690 && x < 890 && y > yy + 22 && y < yy + 82) hit = 1700 + sl;
        if (x > 690 && x < 890 && y > yy + 94 && y < yy + 134 && sload_in(sl, "used", 0)) hit = 1710 + sl;
    }
    if (hit_text(x, y, "BACK", R->W * 0.5f, R->H - 64.0f + 21, 42)) hit = 1790;
    if (phase != TOUCH_END) { F.pressed = hit; return; }
    F.pressed = 0;
    if (hit == 1790) { F.slotDelArm = 0; set_state(ST_MENU); return; }
    if (hit > 1710 && hit <= 1710 + NSLOTS) {
        sl = hit - 1710;
        if (F.slotDelArm != sl) { F.slotDelArm = sl; return; }
        F.slotDelArm = 0;
        slot_erase(sl);
        if (sl == F.slot) load_slot();
        return;
    }
    if (hit > 1700 && hit <= 1700 + NSLOTS) {
        sl = hit - 1700;
        F.slotDelArm = 0;
        if (F.mode != 0) nl_exit_to_city();
        if (sl != F.slot) {
            save_all();                 /* huidig slot eerst bewaren */
            F.slot = sl;
            plat_save_int("slot", sl);
            load_slot();
            apply_settings();
        }
        if (!sload("used", 0)) { ssave("used", 1); plat_save_flush(); }
        start_game();
    }
}


static void menu_tap(float x, float y, int phase) {
    Renderer *R = &F.R;
    int i, hit = 0;
    if (F.state == ST_LOGO) {
        if (phase == TOUCH_END && F.st > 0.5f) { set_state(ST_LOAD); F.loadStart = plat_time(); }
        return;
    }
    if (F.state == ST_MENU) {
        for (i = 0; i < 6; i++)
            if (hit_text(x, y, MENU_ITEMS[i], MENU_X(i), R->H - 74.0f + 18, 32)) hit = i + 1;
        if (phase != TOUCH_END) { F.pressed = hit; return; }
        F.pressed = 0;
        if (hit == 1) { F.slotDelArm = 0; set_state(ST_SLOTS); }
        else if (hit == 2) { F.shopMsg[0] = 0; set_state(ST_MISSIONS); F.missionsFrom = ST_MENU; }
        else if (hit == 3) open_nl_input(ST_MENU);
        else if (hit == 4) open_mp_screen();
        else if (hit == 5) set_state(ST_SETTINGS);
        else if (hit == 6) set_state(ST_CREDITS);
        return;
    }
    if (F.state == ST_SLOTS) { slots_tap(x, y, phase); return; }
    if (F.state == ST_CONTROLS) { controls_tap(x, y, phase); return; }
    if (F.state == ST_NLINPUT) { nl_input_tap(x, y, phase); return; }
    if (F.state == ST_BIGMAP) { bigmap_touch(x, y, phase); return; }
    if (F.state == ST_SHOP) { shop_tap(x, y, phase); return; }
    if (F.state == ST_MP) { mp_screen_tap(x, y, phase); return; }
    if (F.state == ST_DEALER) { dealer_tap(x, y, phase); return; }
    if (F.state == ST_HOME) { home_tap(x, y, phase); return; }
    if (F.state == ST_BIZ) { biz_tap(x, y, phase); return; }
    if (F.state == ST_CUSTOMS) { customs_tap(x, y, phase); return; }
    if (F.state == ST_NLLOAD) {
        if (F.job.err && phase == TOUCH_END) {
            if (F.mode != 0) nl_exit_to_city();
            open_nl_input(F.nlFrom == ST_PAUSE ? ST_MENU : F.nlFrom);
        }
        return;
    }
    if (F.state == ST_MISSIONS) {
        for (i = F.missionPage * 12; i < NMISSIONS && i < F.missionPage * 12 + 12; i++) {
            int j = i - F.missionPage * 12;
            float cxl = (j & 1) ? 522.0f : 62.0f, cyl = 96.0f + (j / 2) * 98.0f;
            if (x > cxl && x < cxl + 440 && y > cyl && y < cyl + 90) hit = i + 1;
        }
        if (hit_text(x, y, "BACK", R->W * 0.5f, R->H - 60.0f + 22, 44)) hit = 100;
        if (x > 62 && x < 212 && y > R->H - 92.0f && y < R->H - 30.0f) hit = 101;
        if (x > R->W - 212.0f && x < R->W - 62.0f && y > R->H - 92.0f && y < R->H - 30.0f) hit = 102;
        if (phase != TOUCH_END) { F.pressed = hit; return; }
        F.pressed = 0;
        if (hit == 101) { if (F.missionPage > 0) F.missionPage--; return; }
        if (hit == 102) { if ((F.missionPage + 1) * 12 < NMISSIONS) F.missionPage++; return; }
        if (hit == 100) set_state(F.missionsFrom == ST_PAUSE ? ST_PAUSE : ST_MENU);
        else if (hit >= 1 && hit <= NMISSIONS && !mission_unlocked(hit - 1)) {
            snprintf(F.shopMsg, sizeof(F.shopMsg), "Locked: finish mission %d (%s) first.", hit - 1, F.G.mdef[hit - 2].title);
            F.st = 0;
        }
        else if (hit >= 1 && hit <= NMISSIONS && F.G.mdef[hit - 1].requireWeapon && !game_has_weapon(&F.G)) {
            /* geen wapen: start in de stad bij BLOCK ARMS */
            if (F.missionsFrom == ST_PAUSE) set_state(ST_GAME); else start_game();
            game_teleport(&F.G, F.G.w->poi[POI_GUNSHOP].x + 3.0f, F.G.w->poi[POI_GUNSHOP].z - 4.0f, 0);
            game_message(&F.G, "You need a gun for this mission. Walk into BLOCK ARMS (red marker).", 4.0f);
        }
        else if (hit >= 1 && hit <= NMISSIONS) {
            if (F.missionsFrom == ST_PAUSE) set_state(ST_GAME);
            else start_game();
            game_start_mission_at(&F.G, hit - 1);
        }
        return;
    }
    if (F.state == ST_SETTINGS) {
        for (i = 0; i < 9; i++) {
            float ry = 118.0f + i * 58.0f;
            if (x > 180 && x < R->W - 180 && y > ry - 5 && y < ry + 45) hit = i + 1;
        }
        if (hit_text(x, y, "BACK", R->W * 0.5f, R->H - 74.0f + 24, 48)) hit = 10;
        if (x > R->W - 230.0f && x < R->W - 30.0f && y > 36 && y < 92) hit = 20;
        if (phase != TOUCH_END) { F.pressed = hit; return; }
        F.pressed = 0;
        if (hit != 9) F.resetArm = 0;
        if (hit == 20) { F.controlsFrom = F.prevState; set_state(ST_CONTROLS); return; }
        switch (hit) {
        case 1: F.quality = (F.quality + 1) % 3; break;
        case 2: F.tod = (F.tod + 1) % 3; break;
        case 3: F.camSpeed = (F.camSpeed + 1) % 3; break;
        case 4: F.invertY = !F.invertY; break;
        case 5: F.showFps = !F.showFps; break;
        case 6: F.autoAim = !F.autoAim; break;
        case 7: F.musicOn = !F.musicOn; break;
        case 8: F.sfxOn = !F.sfxOn; break;
        case 9:
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
        const char *pi[5] = {"RESUME", "MISSIONS", "HOST ONLINE", "SETTINGS", "MAIN MENU"};
        if (F.mode == 1) pi[1] = "NEW POSTCODE";
        if (mp_mode()) pi[2] = "LEAVE ONLINE";
        for (i = 0; i < 5; i++)
            if (hit_text(x, y, pi[i], R->W * 0.5f, 250.0f + i * 88.0f + 22, 44)) hit = i + 1;
        if (phase != TOUCH_END) { F.pressed = hit; return; }
        F.pressed = 0;
        if (hit == 1) set_state(ST_GAME);
        else if (hit == 2 && F.mode == 1) open_nl_input(ST_PAUSE);
        else if (hit == 2) { F.shopMsg[0] = 0; set_state(ST_MISSIONS); F.missionsFrom = ST_PAUSE; }
        else if (hit == 3) {
            if (mp_mode()) { mp_leave(); game_message(&F.G, "You left the online game", 2.0f); set_state(ST_GAME); }
            else mp_host_here();
        }
        else if (hit == 4) { set_state(ST_SETTINGS); F.prevState = ST_PAUSE; }
        else if (hit == 5) { save_all(); mp_leave(); nl_exit_to_city(); set_state(ST_MENU); }
    }
}

void fe_touch(long id, int phase, float x, float y) {
    int i, slot = -1;
    if (!F.ready || F.at) return;
    if (id != -77 && phase == TOUCH_BEGIN) P.active = 0;
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
        int pan = panel_state(F.state) && fe_ui_width() > 1024;
        F.ts[slot].ctl = CTL_UI;
        if (phase == TOUCH_END && F.sfxOn && F.pressed) plat_sfx(SFX_CLICK, 0.5f);
        if (pan) { panel_on(); x -= F.R.uiOff; }
        menu_tap(x, y, phase);
        if (pan) panel_off();
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
    in->run = ctl_down(CTL_RUN);
    F.jumpEdge = F.actionEdge = F.switchEdge = 0;
    pad_build_input(in);
    if (g_desktop) {
        in->mx = clampf(in->mx + g_dk.mx, -1, 1);
        in->my = clampf(in->my + g_dk.my, -1, 1);
        in->gas |= g_dk.gas && F.G.p.veh >= 0;
        in->brake |= g_dk.brake && F.G.p.veh >= 0;
        in->fire |= g_dk.fire;
        in->run |= g_dk.run;
        in->jumpPressed |= g_dk.jumpPressed;
        in->actionPressed |= g_dk.actionPressed;
        in->switchPressed |= g_dk.switchPressed;
        if (F.G.p.veh >= 0) { if (g_dk.gas) in->my = 0; }   /* W = gas, niet dubbel */
        g_dk.jumpPressed = g_dk.actionPressed = g_dk.switchPressed = 0;
    }
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
            game_start_mission_at(g, 0);
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
            rep("NL online: postcode 1012JS (Amsterdam, buiten de regio -> internet), echte dakvlakken (3D BAG LoD 2.2)");
            ns_set_detail(1);
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
            rep("NL gebouwen: %d, vertices in geheugen %d", nb, nv);
            g->camPitch = 1.0f;
            F.atT3 = t;
            F.atStep = 152;
        }
        else if (F.atStep == 152 && t > F.atT3 + 0.8f) {   /* v5.5: daken van bovenaf (luchtcamera) */
            F.atCam = 1;
            F.atCamPos = v3(g->p.x - 70.0f, g->p.y + 75.0f, g->p.z + 70.0f);
            F.atCamTgt = v3(g->p.x, g->p.y, g->p.z);
            F.atT3 = t;
            F.atStep = 153;
        }
        else if (F.atStep == 153 && t > F.atT3 + 0.5f) {
            at_shot("20b_nl_roofs");
            F.atCamPos = v3(g->p.x + 25.0f, g->p.y + 22.0f, g->p.z - 30.0f);
            F.atT3 = t;
            F.atStep = 154;
        }
        else if (F.atStep == 154 && t > F.atT3 + 0.5f) {
            at_shot("20c_nl_roofs_close");
            F.atCam = 0;
            g->camPitch = 0.25f;
            ns_set_detail(F.quality >= 2);
            F.atStep = 53;
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
        else if (F.atStep == 71) {   /* v4.0: naar binnen en naar de toonbank lopen */
            V3 sp[4]; int sk[4], q, ns; float sr[4];
            game_store_enter(g, STORE_GUN);
            ns = store_markers(STORE_GUN, sp, sk, sr, 4);
            for (q = 0; q < ns; q++) if (sk[q] == SM_COUNTER) game_teleport(g, sp[q].x, sp[q].z, 0);
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
            F.atStep = 100;
        }
        else if (F.atStep == 100) {   /* eigen huis: voor de deur */
            float dx, dz;
            if (g->p.veh >= 0) { g->veh[g->p.veh].speed = 0; g->p.veh = -1; }
            g->wanted = 0;
            g->money += 200000;
            home_door(g->homeLvl, &dx, &dz);
            game_teleport(g, dx - 9.0f, dz + 4.0f, PI_F * 0.5f);
            g->camYaw = PI_F * 0.42f;
            F.atT3 = t;
            F.atStep++;
        }
        else if (F.atStep == 101 && t > F.atT3 + 1.2f) { g->camYaw = PI_F * 0.42f; at_shot("33_home_outside"); F.atT3 = t; F.atStep++; }
        else if (F.atStep == 102 && t > F.atT3 + 0.3f) {
            float dx, dz;
            home_door(g->homeLvl, &dx, &dz);
            game_teleport(g, dx, dz, PI_F * 0.5f);
            F.atT3 = t;
            F.atStep++;
        }
        else if (F.atStep == 103 && t > F.atT3 + 1.2f) {
            rep("Huis: binnen=%d (1 verwacht), huis %d, garage %d", game_home_inside(g), g->homeLvl, g->garLvl);
            at_shot("34_home_inside");
            F.atT3 = t;
            F.atStep++;
        }
        else if (F.atStep == 104 && t > F.atT3 + 0.3f) {   /* naar de laptop: menu gaat open */
            V3 hp[12]; int hk[12], q, n2; float hr[12];
            n2 = home_markers(g->homeLvl, g->garLvl, hp, hk, hr, 12);
            for (q = 0; q < n2; q++) if (hk[q] == HM_LAPTOP) game_teleport(g, hp[q].x, hp[q].z, PI_F * 0.5f);
            F.atT3 = t;
            F.atStep++;
        }
        else if (F.atStep == 105 && F.state == ST_GAME && t > F.atT3 + 0.5f) {
            int a = game_home_upgrade(g, 1), b = game_home_upgrade(g, 1), c = game_home_upgrade(g, 0) + game_home_upgrade(g, 0) + game_home_upgrade(g, 0);
            float x0, z0, x1, z1;
            rep("Upgrades: huis %d+%d -> niveau %d, garage +%d -> %d plekken, geld $%ld", a, b, g->homeLvl, c, g->garLvl + 1, g->money);
            home_room(g->homeLvl, &x0, &z0, &x1, &z1);
            game_teleport(g, x0 + 2.5f, z1 - 2.0f, PI_F * 0.75f);
            g->camYaw = PI_F * 0.75f;
            F.atT3 = t;
            F.atStep++;
        }
        else if (F.atStep == 106 && t > F.atT3 + 1.2f) { g->camYaw = PI_F * 0.75f; at_shot("36_villa_inside"); F.atT3 = t; F.atStep++; }
        else if (F.atStep == 107 && t > F.atT3 + 0.3f) {
            V3 hp[12]; int hk[12], q, n2; float hr[12];
            n2 = home_markers(g->homeLvl, g->garLvl, hp, hk, hr, 12);
            for (q = 0; q < n2; q++) if (hk[q] == HM_TOGARAGE) game_teleport(g, hp[q].x, hp[q].z, PI_F);
            F.atT3 = t;
            F.atStep = 110;
        }
        else if (F.atStep == 110 && t > F.atT3 + 1.5f) {
            int k, inGar = 0;
            for (k = 0; k < MAX_OWNED; k++) if (g->garage[k].exists && g->veh[g->firstOwned + k].x < HOME_ZONE_X) inGar++;
            rep("Garage: binnen=%d (2 verwacht), %d eigen auto('s) in de garage", game_home_inside(g), inGar);
            g->camYaw = -PI_F * 0.35f;
            F.atT3 = t;
            F.atStep++;
        }
        else if (F.atStep == 111 && t > F.atT3 + 0.6f) { g->camYaw = -PI_F * 0.35f; at_shot("37_garage"); F.atT3 = t; F.atStep++; }
        else if (F.atStep == 112 && t > F.atT3 + 0.3f) {
            V3 hp[12]; int hk[12], q, n2; float hr[12];
            n2 = home_markers(g->homeLvl, g->garLvl, hp, hk, hr, 12);
            for (q = 0; q < n2; q++) if (hk[q] == HM_GAR_EXIT) game_teleport(g, hp[q].x, hp[q].z, -PI_F * 0.5f);
            F.atT3 = t;
            F.atStep++;
        }
        else if (F.atStep == 113 && t > F.atT3 + 1.0f) {
            float dx, dz;
            rep("Uit de garage: buiten=%d, op (%.0f, %.0f)", !game_home_inside(g), g->p.x, g->p.z);
            home_door(g->homeLvl, &dx, &dz);
            game_teleport(g, dx - 10.0f, dz + 5.0f, PI_F * 0.5f);
            g->camYaw = PI_F * 0.42f;
            F.atT3 = t;
            F.atStep++;
        }
        else if (F.atStep == 114 && t > F.atT3 + 1.2f) { g->camYaw = PI_F * 0.42f; at_shot("38_villa_outside"); F.atT3 = t; F.atStep++; }
        else if (F.atStep == 120) {   /* naar de volgende winkel (v4.0: binnen naar de toonbank) */
            V3 pp = g->w->poi[biz_poi(F.atBiz)];
            g->money += 50000;
            if (F.atBiz == BIZ_SPRAY) game_teleport(g, pp.x, pp.z, 0);
            else {
                V3 sp[4]; int sk[4], q, ns; float sr[4];
                int st = F.atBiz - BIZ_BURGER + STORE_BURGER;
                game_store_enter(g, st);
                ns = store_markers(st, sp, sk, sr, 4);
                for (q = 0; q < ns; q++) if (sk[q] == SM_COUNTER) game_teleport(g, sp[q].x, sp[q].z, 0);
            }
            F.atT3 = t;
            F.atStep++;
        }
        else if (F.atStep == 121 && F.state == ST_GAME && t > F.atT3 + 1.5f) {
            V3 pp = g->w->poi[biz_poi(F.atBiz)];
            float side = fmodf(pp.z - ROADW, PITCH) < BLOCKW * 0.5f ? -1.0f : 1.0f;
            game_teleport(g, pp.x, pp.z + side * 4.0f, 0);   /* van de ring af, weer buiten */
            F.atBiz++;
            if (F.atBiz >= BIZ_COUNT) { F.atBiz = 0; F.atStep = 130; } else F.atStep = 120;
        }
        else if (F.atStep == 130) {   /* v4.0: elke winkel van binnen bekijken */
            game_store_enter(g, F.atBiz);
            g->camYaw = 0.0f; g->camPitch = 0.25f;
            F.atT3 = t;
            F.atStep++;
        }
        else if (F.atStep == 131 && t > F.atT3 + 1.2f) {
            char nm[48];
            snprintf(nm, sizeof(nm), "46_store_%d_%s", F.atBiz, F.atBiz == STORE_MART ? "mart" : (F.atBiz == STORE_GUN ? "guns" : "biz"));
            g->camYaw = 0.0f;
            rep("Winkel %s: binnen=%d (verwacht %d)", store_name(F.atBiz), game_store_inside(g), F.atBiz + 1);
            at_shot(nm);
            F.atT3 = t;
            if (F.atBiz == STORE_MART) F.atStep = 132;
            else { F.atBiz++; F.atStep = 130; }
        }
        else if (F.atStep == 132) {   /* kassa overvallen (v5.2: met een wapen in de hand) */
            V3 sp[4]; int sk[4], q, ns; float sr[4];
            g->owned[WP_PISTOL] = 1; g->weapon = WP_PISTOL;
            F.atMart0 = g->money;
            ns = store_markers(STORE_MART, sp, sk, sr, 4);
            for (q = 0; q < ns; q++) if (sk[q] == SM_ROB) game_teleport(g, sp[q].x, sp[q].z, 0);
            g->camYaw = 0.7f;
            F.atT3 = t;
            F.atStep++;
        }
        else if (F.atStep == 133 && t > F.atT3 + 1.5f) {
            rep("Supermarkt-overval: geld +%ld (10000 verwacht), sterren=%d, overvallen=%d", g->money - F.atMart0, g->wanted, g->statRobs);
            g->camYaw = 0.7f;
            at_shot("47_mart_robbed");
            F.atT3 = t;
            F.atStep++;
        }
        else if (F.atStep == 134 && t > F.atT3 + 0.3f) {
            V3 sp[4]; int sk[4], q, ns; float sr[4];
            ns = store_markers(STORE_MART, sp, sk, sr, 4);
            for (q = 0; q < ns; q++) if (sk[q] == SM_EXIT) game_teleport(g, sp[q].x, sp[q].z, PI_F);
            F.atT3 = t;
            F.atStep++;
        }
        else if (F.atStep == 135 && t > F.atT3 + 1.5f) {
            int k;
            rep("Uit de supermarkt: buiten=%d, op (%.0f, %.0f), sterren=%d, wachttijd %.0f s", !game_store_inside(g), g->p.x, g->p.z, g->wanted, g->robCool);
            g->camYaw = 0.0f;
            at_shot("48_mart_outside");
            g->wanted = 0;
            for (k = 0; k < MAX_POLICE; k++) g->veh[g->firstPolice + k].active = 0;
            for (k = MAX_PED - MAX_COPS; k < MAX_PED; k++) g->ped[k].active = 0;
            F.atT3 = t;
            F.atStep = 136;
        }
        else if (F.atStep == 136) {   /* v5.2: meteen weer naar binnen en nog eens overvallen: mag niet */
            V3 sp[4]; int sk[4], q, ns; float sr[4];
            F.atMart0 = g->money;
            game_store_enter(g, STORE_MART);
            ns = store_markers(STORE_MART, sp, sk, sr, 4);
            for (q = 0; q < ns; q++) if (sk[q] == SM_ROB) game_teleport(g, sp[q].x, sp[q].z, 0);
            F.atT3 = t;
            F.atStep++;
        }
        else if (F.atStep == 137 && t > F.atT3 + 1.5f) {
            rep("Tweede overval binnen 3 min: geld +%ld (0 verwacht), wachttijd %.0f s", g->money - F.atMart0, g->robCool);
            at_shot("49_mart_cooldown");
            g->weapon = WP_NONE;
            g->robCool = 0;
            F.atMart0 = g->money;
            g->storeMarker = SM_NONE;
            F.atT3 = t;
            F.atStep++;
        }
        else if (F.atStep == 138 && t > F.atT3 + 1.0f) {
            V3 sp[4]; int sk[4], q, ns; float sr[4];
            rep("Overval zonder wapen: geld +%ld (0 verwacht)", g->money - F.atMart0);
            ns = store_markers(STORE_MART, sp, sk, sr, 4);
            for (q = 0; q < ns; q++) if (sk[q] == SM_EXIT) game_teleport(g, sp[q].x, sp[q].z, PI_F);
            g->wanted = 0;
            F.atT3 = t;
            F.atStep = 140;
        }
        else if (F.atStep == 140) {   /* v4.5: cutscene voor missie 1 bekijken */
            g->noCutscenes = 0;
            game_start_mission_at(g, 0);
            rep("Cutscene gestart: cut=%d, missie=%d (nog -1 verwacht), contact %s", g->cut, g->mission, VO_SPK_NAME[g->cutSpk]);
            F.atT3 = t;
            F.atStep++;
        }
        else if (F.atStep == 141 && t > F.atT3 + 0.7f) { at_shot("50_cutscene_wide"); F.atStep++; }
        else if (F.atStep == 142 && g->cutIdx == 0 && g->cutShotT > 1.2f) { at_shot("51_cutscene_contact"); F.atStep++; }
        else if (F.atStep == 143 && ((g->cutIdx == 2 && g->cutShotT > 0.8f) || t > F.atT3 + 30.0f)) { at_shot("52_cutscene_player"); F.atStep++; }
        else if (F.atStep == 144 && ((g->cutIdx == 3 && g->cutShotT > 1.0f) || t > F.atT3 + 40.0f)) {
            rep("Cutscene: regel %d van %d, ondertitel '%s'", g->cutIdx + 1, VO_MIS[0].nintro, g->voLine >= 0 ? VO[g->voLine].text : "-");
            g->cutSkip = 1;
            F.atT3 = t;
            F.atStep++;
        }
        else if (F.atStep == 145 && t > F.atT3 + 0.6f) {
            rep("Cutscene overgeslagen: cut=%d, missie=%d (0 verwacht)", g->cut, g->mission);
            at_shot("53_after_cutscene");
            g->mission = -1;
            g->noCutscenes = 1;
            vo_stop(g);
            F.atT3 = t;
            F.atStep = 122;
        }
        else if (F.atStep == 122) {   /* thuis: luxe spullen bekijken */
            float x0, z0, x1, z1;
            home_room(g->homeLvl, &x0, &z0, &x1, &z1);
            game_teleport(g, x0 + 4.0f, (z0 + z1) * 0.5f + 1.0f, -PI_F * 0.75f);
            g->camYaw = -PI_F * 0.75f;
            F.atT3 = t;
            F.atStep++;
        }
        else if (F.atStep == 123 && t > F.atT3 + 1.2f) { g->camYaw = -PI_F * 0.75f; at_shot("45_home_bling"); F.atT3 = t; F.atStep++; }
        else if (F.atStep == 124 && t > F.atT3 + 0.3f) {
            game_teleport(g, road_c(2), road_c(1) + 6.0f, 0);
            F.atStep = 90;
        }
        else if (F.atStep == 115 && t > F.atT3 + 0.3f) {
            game_teleport(g, road_c(2), road_c(1) + 6.0f, 0);
            g->stamina = 100; g->tired = 0;
            F.atMaxSpeed = 0;
            F.atT3 = t;
            F.atStep = 108;
        }
        else if (F.atStep == 108) {   /* rennen tot je buiten adem bent */
            in->my = 1.0f; in->run = 1;
            if (t > F.atT3 + 2.0f && F.atMaxSpeed < g->p.speed) F.atMaxSpeed = g->p.speed;
            if (t > F.atT3 + 2.5f && F.atStep == 108 && !F.atShot2) { at_shot("39_running"); F.atShot2 = 1; }
            if (t > F.atT3 + 7.0f) {
                rep("Rennen: topsnelheid %.1f m/s, uithoudingsvermogen %.0f, uitgeput=%d", F.atMaxSpeed, g->stamina, g->tired);
                F.atBiz = 0;
                F.atStep = 120;
            }
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
            rep("geluid: %ld geluidsgebeurtenissen in de test (muziek %s, effecten %s)", F.sfxPlayed, F.musicOn ? "aan" : "uit", F.sfxOn ? "aan" : "uit");
            rep("geld verdiend in test: %ld, missies gehaald: %d, in/uitstappen: %d, sprongen: %d", g->money - F.atMoney0, g->statMissionsPassed, g->statEnter, g->statJumps);
            plat_write_text("autotest_report.txt", F.rep);
            F.quality = plat_load_int("quality", 1);
            F.tod = plat_load_int("tod", 0);
            apply_settings();
            F.atStep++;
            F.at = 0;
            plat_autotest_done();
            /* echte voortgang terugzetten */
            load_slot();
            apply_settings();
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
    if (F.loadDone) {
        const char *ev;
        mptest_tick();
        mp_tick(&F.G, dt);
        if (mp_mode() == MP_CLIENT) mp_try_enter_world();
        if ((ev = mp_event()) != NULL && F.state == ST_GAME) game_message(&F.G, ev, 3.0f);
    }
    if (F.mode == 1 && (F.state == ST_GAME || F.state == ST_PAUSE || F.state == ST_BIGMAP)) ns_update(F.G.p.x, F.G.p.z);
    if (!F.at) pad_tick(dt);
    glViewport(g_vpX, g_vpY, g_vpW > 0 ? g_vpW : R->W, g_vpH > 0 ? g_vpH : R->H);
    panel_off();
    if (panel_state(F.state) && F.state != ST_PAUSE && F.state != ST_SHOP && F.state != ST_BIZ && F.state != ST_HOME &&
        F.state != ST_DEALER && F.state != ST_CUSTOMS && !(F.state == ST_MISSIONS && F.missionsFrom == ST_PAUSE)) panel_on();
    glClearColor(0, 0, 0, R->vr.active ? 0.0f : 1.0f);   /* VR: UI-laag is doorzichtig waar niets getekend wordt */
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
    case ST_SLOTS:
        ui_begin(R); draw_slots(); ui_flush(R);
        break;
    case ST_CONTROLS:
        ui_begin(R); draw_controls(); ui_flush(R);
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
    case ST_MP:
        ui_begin(R); draw_mp_screen(); ui_flush(R);
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
    case ST_BIZ:
        draw_biz();
        if (F.at && F.st > 1.0f) {
            static const char *shots[BIZ_COUNT] = {"40_burger", "41_gym", "42_drip", "43_bling", "44_spray"};
            char msg[96];
            int k, nb = 0;
            at_shot(shots[F.bizShop % BIZ_COUNT]);
            if (F.bizShop == BIZ_BURGER) nb += biz_buy(&F.G, BIZ_BURGER, 1, msg, sizeof(msg));
            if (F.bizShop == BIZ_GYM) nb += biz_buy(&F.G, BIZ_GYM, 0, msg, sizeof(msg)) + biz_buy(&F.G, BIZ_GYM, 1, msg, sizeof(msg));
            if (F.bizShop == BIZ_DRIP) nb += biz_buy(&F.G, BIZ_DRIP, 3, msg, sizeof(msg));
            if (F.bizShop == BIZ_BLING) for (k = 0; k < BIZ_DECOR; k++) nb += biz_buy(&F.G, BIZ_BLING, k, msg, sizeof(msg));
            rep("Winkel %s: %d aankopen, geld $%ld (cardio %d, taai %d, kleding %d, decor %d)", biz_name(F.bizShop), nb, F.G.money,
                F.G.cardio, F.G.tough, F.G.outfit, F.G.decor);
            set_state(ST_GAME);
        }
        break;
    case ST_HOME:
        draw_home();
        if (F.at && F.st > 1.0f) {
            at_shot("35_home_laptop");
            rep("Huis-menu: huis %d, garage %d, geld $%ld", F.G.homeLvl, F.G.garLvl, F.G.money);
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
        if (F.at && F.st > 1.0f && F.missionPage == 0) { at_shot("15_missions"); F.missionPage = 1; F.st = 0; }
        else if (F.at && F.st > 1.0f && F.missionPage == 1) { at_shot("15b_missions_page2"); F.missionPage = 0; set_state(ST_GAME); }
        break;
    case ST_GAME:
    case ST_PAUSE:
        if (F.state == ST_GAME) {
            if (F.at) autotest_tick(dt);
            else build_input();
            if (F.state == ST_GAME || F.at) game_update(&F.G, &F.in, dt);
            if (F.atCam) { F.G.camPos = F.atCamPos; F.G.camTarget = F.atCamTgt; }   /* testmodus: vaste luchtcamera */
            if (F.G.shopOpen) { F.G.shopOpen = 0; F.shopMsg[0] = 0; set_state(ST_SHOP); }
            if (F.G.dealerOpen) { F.G.dealerOpen = 0; F.shopMsg[0] = 0; set_state(ST_DEALER); }
            if (F.G.customsOpen) { F.G.customsOpen = 0; F.shopMsg[0] = 0; set_state(ST_CUSTOMS); }
            if (F.G.homeMenuOpen) { F.G.homeMenuOpen = 0; F.shopMsg[0] = 0; set_state(ST_HOME); }
            if (F.G.bizOpen) { F.bizShop = F.G.bizOpen - 1; F.G.bizOpen = 0; F.shopMsg[0] = 0; set_state(ST_BIZ); }
            if (!F.at && F.mode == 0) {
                int i, done = 0;
                for (i = 0; i < NMISSIONS; i++) if (F.G.completed[i]) done |= 1 << i;
                if (done != F.savedDone || (F.G.money != F.savedMoney && F.G.bannerT <= 0 && F.G.msgT <= 0)) save_all();
            }
        }
        r_draw_scene(R, &F.G);
        ui_begin(R);
        draw_hud();
        if (F.state == ST_PAUSE) { ui_flush(R); panel_on(); ui_begin(R); draw_pause(); }   /* (niet als er net een winkel/huis-menu opengaat) */
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
    /* ---- geluid */
    {
        int playing = F.loadDone && (F.state == ST_GAME || F.state == ST_PAUSE || F.state == ST_BIGMAP || F.state == ST_SHOP ||
                                     F.state == ST_DEALER || F.state == ST_CUSTOMS);
        Game *g = &F.G;
        float eng = 0, engRate = 1, bike = 0, bikeRate = 1, siren = 0;
        int i;
        plat_music(F.musicOn && playing);
        if (g->voReq != -1 && F.loadDone) {   /* v4.5 stemmen */
            plat_voice(g->voReq >= 0 && F.sfxOn ? VO[g->voReq].key : NULL, 1.0f);
            g->voReq = -1;
        }
        if (F.sfxOn && F.state == ST_GAME && F.loadDone) {
            for (i = 0; i < g->nsfx; i++) {
                float dx = g->sfxx[i] - g->p.x, dz = g->sfxz[i] - g->p.z;
                float vol = clampf(1.0f - sqrtf(dx * dx + dz * dz) / 70.0f, 0, 1);
                if (g->sfxq[i] == SFX_PISTOL || g->sfxq[i] == SFX_SMG || g->sfxq[i] == SFX_SHOTGUN) vol *= 0.8f;
                plat_sfx(g->sfxq[i], vol);
            }
            if (g->p.veh >= 0) {
                const Vehicle *v = &g->veh[g->p.veh];
                float top = v->custom ? car_top_speed(v) : (v->type == VEH_BIKE ? 11.5f : 19.0f);
                float sp = clampf(fabsf(v->speed) / top, 0, 1);
                if (v->type == VEH_CAR || v->custom) {   /* auto of motor: motorgeluid */ eng = 0.22f + 0.45f * sp; engRate = 0.75f + 1.2f * sp; }
                else if (fabsf(v->speed) > 0.8f) { bike = 0.25f + 0.35f * sp; bikeRate = 0.7f + 1.0f * sp; }
            }
            for (i = 0; i < MAX_POLICE; i++) {
                const Vehicle *v = &g->veh[g->firstPolice + i];
                float dx, dz, d, s;
                if (!v->active || v->wrecked || g->wanted <= 0) continue;
                dx = v->x - g->p.x; dz = v->z - g->p.z; d = sqrtf(dx * dx + dz * dz);
                s = clampf(1.0f - d / 130.0f, 0, 1) * 0.55f;
                if (s > siren) siren = s;
            }
        }
        if (F.loadDone) { F.sfxPlayed += g->nsfx; g->nsfx = 0; }
        plat_loop(LOOP_ENGINE, eng, engRate);
        plat_loop(LOOP_BIKE, bike, bikeRate);
        plat_loop(LOOP_SIREN, siren, 1.0f);
    }
    panel_off();
    draw_pad_cursor();
    F.lastDC = R->drawCalls;
    if (pendingShot[0]) {
        plat_screenshot(pendingShot, g_vpW > 0 ? g_vpW : R->W, g_vpH > 0 ? g_vpH : R->H);
        F.skipStat = 2;
        pendingShot[0] = 0;
    }
}

/* ------------------------------------------------------------------ desktop-invoer */

void fe_mouse_look(float dx, float dy) {
    static const float sens[3] = {0.6f, 1.0f, 1.5f};
    float s;
    if (!F.ready || F.state != ST_GAME || !F.loadDone) return;
    s = sens[F.camSpeed % 3];
    F.G.camYaw = wrap_angle(F.G.camYaw - dx * 0.0035f * s);
    F.G.camPitch = clampf(F.G.camPitch + dy * 0.0028f * s * (F.invertY ? -1.0f : 1.0f), -0.15f, 1.15f);
    F.G.camIdle = 0;
}

void fe_mouse_wheel(float dy) {
    if (F.state == ST_BIGMAP) F.bmScale = clampf(F.bmScale * (dy > 0 ? 1.25f : 0.8f), 0.06f, 4.0f);
}

int fe_text_wanted(void) { return F.ready && (F.state == ST_NLINPUT || F.state == ST_MP); }

void fe_text_char(int c) {
    char norm[8];
    if (F.ready && F.state == ST_MP) {
        if (c >= 'a' && c <= 'z') c -= 32;
        if (((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) && mpCodeLen < 10) { mpCode[mpCodeLen++] = (char)c; mpCode[mpCodeLen] = 0; }
        else if (c == 8 && mpCodeLen > 0) mpCode[--mpCodeLen] = 0;
        else if (c == 13 && mpCodeLen == 10) { uint32_t ip; int port; if (mp_code_decode(mpCode, &ip, &port)) mp_join(ip, port); }
        return;
    }
    if (!F.ready || F.state != ST_NLINPUT) return;
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c >= '0' && c <= '9') {
        if (F.pcLen < 4 && !(F.pcLen == 0 && c == '0')) { F.pcIn[F.pcLen++] = (char)c; F.pcIn[F.pcLen] = 0; }
    } else if (c >= 'A' && c <= 'Z') {
        if (F.pcLen >= 4 && F.pcLen < 6) { F.pcIn[F.pcLen++] = (char)c; F.pcIn[F.pcLen] = 0; }
    } else if (c == 8) {
        if (F.pcLen > 0) F.pcIn[--F.pcLen] = 0;
    } else if (c == 13) {
        if (nl_normalize_postcode(F.pcIn, norm)) nl_play_pc(norm, F.nlInFrom);
    }
}

void fe_key_command(int cmd) {
    if (!F.ready || F.at) return;
    if (cmd == FE_CMD_MAP) {
        if (F.state == ST_GAME) bm_open();
        else if (F.state == ST_BIGMAP) set_state(ST_GAME);
        return;
    }
    switch (F.state) {
    case ST_GAME: set_state(ST_PAUSE); save_all(); break;
    case ST_PAUSE: case ST_BIGMAP: case ST_SHOP: case ST_DEALER: case ST_HOME: case ST_BIZ: set_state(ST_GAME); save_all(); break;
    case ST_CUSTOMS: set_state(ST_GAME); save_all(); break;
    case ST_SETTINGS: set_state(F.prevState == ST_PAUSE ? ST_PAUSE : ST_MENU); break;
    case ST_MISSIONS: set_state(F.missionsFrom == ST_PAUSE ? ST_PAUSE : ST_MENU); break;
    case ST_SLOTS: F.slotDelArm = 0; set_state(ST_MENU); break;
    case ST_CONTROLS: if (P.wait) P.wait = 0; else set_state(ST_SETTINGS); break;
    case ST_CREDITS: set_state(ST_MENU); break;
    case ST_NLINPUT: set_state(F.nlInFrom == ST_PAUSE ? ST_PAUSE : ST_MENU); break;
    case ST_MP: if (mp_mode() == MP_JOINING) mp_leave(); mp_lan_listen(0); set_state(ST_MENU); break;
    default: break;
    }
}
