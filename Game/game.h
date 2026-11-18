/* Spellogica zonder OpenGL: speler, voertuigen, voetgangers, verkeer, missies, camera. */
#ifndef SGB_GAME_H
#define SGB_GAME_H

#include "world.h"

#define MAX_VEH 60
#define MAX_OWNED 4
#define MAX_POLICE 3
#define MAX_PED 20
#define MAX_COPS 4      /* laatste 4 voetganger-plekken: agenten */
#define MAX_TRACERS 12
#define MAX_TRAFFIC 10
#define NMISSIONS 19
#define MAX_CP 10

enum { NEED_ANY = 0, NEED_BIKE = 1, NEED_CAR = 2, NEED_FOOT = 3 };
enum { MODE_FOOT = 0, MODE_BIKE = 1, MODE_CAR = 2 };
enum { PED_WALK = 0, PED_DODGE, PED_DOWN, PED_IDLE, PED_FLEE };

typedef struct {
    float mx, my;          /* linker stick: -1..1, my>0 = vooruit */
    float lx, ly;          /* rechter stick (camera) */
    int jump, action, gas, brake;
    int jumpPressed, actionPressed;
    int fire, switchPressed;
} Input;

typedef struct {
    int active, type, variant;
    float x, z, y, h, speed, steer, lean, wheel;
    int traffic, police, wrecked, copsOut;
    float hp;
    int custom, model, paint, eng, hand, neon, spoiler, garageSlot;   /* spelerauto / BLOCK CUSTOMS */
    int ti, tj, dir, phase;   /* verkeer: doel-kruispunt en richting */
    float tx, tz, stuck, ghost, hitcool;
} Vehicle;

typedef struct {
    int active, variant, state;
    float x, z, y, h, speed, anim, timer;
    int bi, bj, corner, dsign;
    float lay;  /* 0..1 omgevallen */
    int cop, shot;
    float hp, shootCool;
} Ped;

typedef struct {
    float x, z, y, vy, h, anim, speed;
    int onGround, veh;
    float cool, hitCool;
} Player;

typedef struct { float x, z, r; int need; const char *text; float hold; int alarm, clean; } Checkpoint;

typedef struct {
    const char *title, *brief;
    V3 giver;
    int ncp, anyOrder, reward;
    float time;
    Checkpoint cp[MAX_CP];
    int gifts;  /* toon cadeautjes op de any-order punten */
    int escape; /* missie: politie afschudden */
    int requireWeapon;
    int startVeh; /* bij starten via missiekeuze: 0 niets, 1 fiets, 2 auto */
    const char *tag;
} MissionDef;

typedef struct {
    World *w;
    Player p;
    Vehicle veh[MAX_VEH];
    int nveh, firstTraffic, firstPolice, firstOwned;
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

    float health, hurtT, hurtFlash, armor;
    int owned[4], ammo[4], weapon, autoAim;
    float fireCool, muzzleT;
    struct { V3 a, b; float t; } tracer[MAX_TRACERS];
    int trNext;
    float holdT; int alarmDone;
    int shopOpen, inShop;
    int statShots, statHits, copShots;
    float lastCopSpawn;
    struct GarageCar { int exists, model, paint, eng, hand, neon, spoiler; } garage[MAX_OWNED];
    int dealerOpen, inDealer, customsOpen, inCustoms;
    int wanted;
    float evadeT, bustT, crimeCool, ramCool;

    float time;
    Rng rng;
    int quality;
    int nl;   /* 1 = echte NL-kaart: geen missies, verkeer of voetgangers */
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
void game_add_wanted(Game *g, int n);
enum { WP_NONE = 0, WP_PISTOL, WP_SMG, WP_SHOTGUN, WP_COUNT };
typedef struct { const char *name; int price, ammoPrice, ammoPack; float damage, delay, range, spread; int pellets; } WeaponDef;
extern const WeaponDef WEAPONS[WP_COUNT];
int game_has_weapon(const Game *g);
int game_buy(Game *g, int item);   /* 0 pistool, 1 SMG, 2 shotgun, 3 kogels, 4 vest */
typedef struct GarageCar GarageCar;
typedef struct { const char *name; int price; float maxF, acc, steer; } CarDef;
extern const CarDef CARS[];
extern const int ENGINE_PRICE[3], HANDLING_PRICE[3];
float car_top_speed(const Vehicle *v);
void game_spawn_owned(Game *g);
int game_buy_car(Game *g, int model);
void game_sell_car(Game *g, int slot);
int game_custom_price(const Game *g, int item);   /* 0 lak, 1 motor, 2 handling, 3 neon, 4 spoiler */
int game_custom(Game *g, int item);
void game_damage(Game *g, float amount);
/* missie starten vanuit het keuzescherm: speler naar het startpunt (evt. met voertuig) */
void game_start_mission_at(Game *g, int m);

#endif
