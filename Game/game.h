/* Spellogica zonder OpenGL: speler, voertuigen, voetgangers, verkeer, missies, camera. */
#ifndef SGB_GAME_H
#define SGB_GAME_H

#include "world.h"

#define MAX_VEH 48
#define MAX_PED 16
#define MAX_TRAFFIC 10
#define NMISSIONS 5
#define MAX_CP 10

enum { NEED_ANY = 0, NEED_BIKE = 1, NEED_CAR = 2 };
enum { MODE_FOOT = 0, MODE_BIKE = 1, MODE_CAR = 2 };
enum { PED_WALK = 0, PED_DODGE, PED_DOWN, PED_IDLE, PED_FLEE };

typedef struct {
    float mx, my;          /* linker stick: -1..1, my>0 = vooruit */
    float lx, ly;          /* rechter stick (camera) */
    int jump, action, gas, brake;
    int jumpPressed, actionPressed;
} Input;

typedef struct {
    int active, type, variant;
    float x, z, y, h, speed, steer, lean, wheel;
    int traffic;
    int ti, tj, dir, phase;   /* verkeer: doel-kruispunt en richting */
    float tx, tz, stuck, ghost, hitcool;
} Vehicle;

typedef struct {
    int active, variant, state;
    float x, z, y, h, speed, anim, timer;
    int bi, bj, corner, dsign;
    float lay;  /* 0..1 omgevallen */
} Ped;

typedef struct {
    float x, z, y, vy, h, anim, speed;
    int onGround, veh;
    float cool;
} Player;

typedef struct { float x, z, r; int need; const char *text; } Checkpoint;

typedef struct {
    const char *title, *brief;
    V3 giver;
    int ncp, anyOrder, reward;
    float time;
    Checkpoint cp[MAX_CP];
    int gifts;  /* toon cadeautjes op de any-order punten */
} MissionDef;

typedef struct {
    World *w;
    Player p;
    Vehicle veh[MAX_VEH];
    int nveh, firstTraffic;
    Ped ped[MAX_PED];
    int wantTraffic, wantPeds;

    float camYaw, camPitch, camDist, camIdle;
    V3 camPos, camTarget;
    float camSens; int invertY;

    MissionDef mdef[NMISSIONS];
    int mission, mstep;
    unsigned mmask;
    float mtime, mclock;
    int completed[NMISSIONS];

    char banner[48], sub[96];
    float bannerT; int bannerGood;
    char msg[96]; float msgT;

    long money; float moneyFlash; long moneyShown;
    float cashTimer[24];

    float time;
    Rng rng;
    int quality;
    float drawDist;

    /* statistiek voor test/rapport */
    int statMissionsPassed, statCash, statEnter, statJumps;
} Game;

void game_init(Game *g, World *w);
void game_set_quality(Game *g, int q);  /* 0 laag, 1 midden, 2 hoog */
void game_update(Game *g, const Input *in, float dt);
int game_mode(const Game *g);
/* index van voertuig binnen bereik om in te stappen, of -1 */
int game_near_vehicle(const Game *g);
/* actief checkpoint (voor kaart/pijl); geeft aantal punten in out (max 8) */
int game_targets(const Game *g, V3 *out, int max);
void game_teleport(Game *g, float x, float z, float h);
void game_start_mission(Game *g, int m);
void game_message(Game *g, const char *txt, float t);

#endif
