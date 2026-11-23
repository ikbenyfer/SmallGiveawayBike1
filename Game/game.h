/* Spellogica zonder OpenGL: speler, voertuigen, voetgangers, verkeer, missies, camera. */
#ifndef SGB_GAME_H
#define SGB_GAME_H

#include "world.h"
#include "phys.h"

#define MAX_VEH 64
#define MAX_OWNED 4
#define MAX_POLICE 3
#define MAX_PED 20
#define MAX_COPS 4      /* laatste 4 voetganger-plekken: agenten */
#define MAX_TRACERS 12
#define MAX_TRAFFIC 10
#define NMISSIONS 22
#define MAX_CP 10

enum { NEED_ANY = 0, NEED_BIKE = 1, NEED_CAR = 2, NEED_FOOT = 3 };
enum { MODE_FOOT = 0, MODE_BIKE = 1, MODE_CAR = 2 };
enum { PED_WALK = 0, PED_DODGE, PED_DOWN, PED_IDLE, PED_FLEE };
enum { SFX_CLICK = 0, SFX_PISTOL, SFX_SMG, SFX_SHOTGUN, SFX_COPSHOT, SFX_EMPTY, SFX_SCREAM, SFX_HURT, SFX_CRASH,
       SFX_DOOR, SFX_JUMP, SFX_CASH, SFX_CHECK, SFX_PASS, SFX_FAIL, SFX_ALARM, SFX_BUY, SFX_COUNT };
enum { LOOP_ENGINE = 0, LOOP_BIKE, LOOP_SIREN, LOOP_COUNT };
#define MAX_SFXQ 24
#define MAX_REMOTE 3

/* een andere speler (multiplayer); positie komt van het netwerk */
typedef struct {
    int active, id, fresh;
    char name[16];
    float x, y, z, h, tx, ty, tz, th, speed, anim, animLocal, health, flash;
    int mode, vmodel, vpaint, vcustom, vvariant, weapon, shots, shotsSeen;
    double seen;
} RemotePlayer;

typedef struct {
    float mx, my;          /* linker stick: -1..1, my>0 = vooruit */
    float lx, ly;          /* rechter stick (camera) */
    int jump, action, gas, brake;
    int jumpPressed, actionPressed;
    int fire, switchPressed;
    int run;               /* RUN-knop / Shift ingedrukt */
    int up, down;          /* v6.0.3: opstijgen/dalen (voertuigen met vleugels) */
} Input;

typedef struct {
    int active, type, variant;
    float x, z, y, h, speed, steer, lean, wheel;
    int traffic, police, wrecked, copsOut;
    float hp;
    int custom, model, paint, eng, hand, neon, spoiler, garageSlot;   /* spelerauto / BLOCK CUSTOMS */
    int ti, tj, dir, phase;   /* verkeer: doel-kruispunt en richting */
    float tx, tz, stuck, ghost, hitcool;
    float slip;        /* v6.0.1: zijwaartse snelheid (driften) */
    int phys, fly;     /* v6.0.4: rijdt met physics (pm = modelmatrix); vliegend (vleugels + jet) */
    M4 pm;
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
    /* v3.0 animatie in-/uitstappen (alleen beeld): 1 auto in, 2 fiets/motor op, 3 auto uit, 4 fiets/motor af */
    float anT, anDur, anX, anZ, anH;
    int anKind, anVeh;
} Player;

typedef struct { float x, z, r; int need; const char *text; float hold; int alarm, clean; } Checkpoint;

typedef struct {
    const char *title, *brief;
    V3 giver;
    int ncp, anyOrder, reward;
    int repeatReward; /* beloning bij opnieuw spelen (0 = de helft) */
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
    int sfxq[MAX_SFXQ]; float sfxx[MAX_SFXQ], sfxz[MAX_SFXQ]; int nsfx;
    float screamCool, hurtSndCool;
    RemotePlayer remote[MAX_REMOTE];
    struct { int target; float dmg; } hitq[8];
    int nhit;
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

    /* rennen */
    float stamina, staminaCool; int tired, running;
    /* eigen huis (alleen in de stad) */
    int homeLvl, garLvl, homeMarker, homeMenuOpen;
    float homeCool;
    /* v3.2 winkels: training, kleding, luxe in huis, energie */
    int cardio, tough, outfits, outfit, decor;
    float boostT;
    int bizOpen, inBiz;   /* bizOpen = winkel + 1 (frontend opent het menu) */
    /* v4.0: binnen in winkels lopen; GIVEAWAY MART overvallen */
    int storeMarker, statRobs;
    float storeCool, clerkScare;
    float robCool;                /* v5.2: seconden tot de GIVEAWAY MART weer overvallen kan worden */
    /* v4.5: stemmen en cutscenes (zie voice.h) */
    int voReq;                    /* regel die de frontend moet afspelen: -1 niets, -2 stoppen */
    int voLine, voPrio; float voT;   /* huidige ondertitel en hoe lang nog */
    int voQ[4], voQP[4], voQn;
    float barkCool;
    int cut, cutMission, cutIdx, cutSkip, cutSpk, noCutscenes;
    float cutT, cutLineT, cutShotT, cutX, cutZ, cutH;
    /* v5.0 VR: richten met de rechtercontroller (first person) */
    int vrAim; float vrAimYaw; V3 vrMuzzle;
    float camFov;      /* v6.0: beeldhoek (graden), wijder bij hoge snelheid */
    /* v6.0.1: driften (zijwaarts glijden) en de PHYSICS-modus */
    float driftT;
    int physOn, physWantBuild, physBroken;
    int vphys, physVeh, pcarVeh;   /* v6.0.4: physics voor je eigen voertuig, extra plek voor de PHYSICS-modus */
    float flipT;
    int nl_noPhys;   /* (reserve) */
    Craft craft;
    PDesign design;
    float physSaveX, physSaveZ, physSaveH;
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
void game_start_mission(Game *g, int m);   /* met cutscene (als die er is) */
void game_mission_begin(Game *g, int m);   /* direct, zonder cutscene */
void game_message(Game *g, const char *txt, float t);
void game_sfx(Game *g, int id, float x, float z);
void game_add_wanted(Game *g, int n);
/* v6.0.4 PHYSICS-modus: bestaand voertuig kiezen (kind: 0 auto, 1 dealer-auto, 2 motor, 3 fiets), eventueel vliegend */
void game_phys_start(Game *g, int here, int kind, int model, int fly);
void game_phys_stop(Game *g);
int game_store_inside(const Game *g);   /* winkel + 1 als de speler binnen in een winkel staat */
void game_store_enter(Game *g, int store);
enum { WP_NONE = 0, WP_PISTOL, WP_SMG, WP_SHOTGUN, WP_COUNT };
typedef struct { const char *name; int price, ammoPrice, ammoPack; float damage, delay, range, spread; int pellets; } WeaponDef;
extern const WeaponDef WEAPONS[WP_COUNT];
int game_has_weapon(const Game *g);
int game_buy(Game *g, int item);   /* 0 pistool, 1 SMG, 2 shotgun, 3 kogels, 4 vest */
typedef struct GarageCar GarageCar;
typedef struct { const char *name; int price; float maxF, acc, steer; } CarDef;
extern const CarDef CARS[];   /* DEALER_MODELS: eerst auto's, dan motoren (IS_MOTO) */
extern const int ENGINE_PRICE[3], HANDLING_PRICE[3];
float car_top_speed(const Vehicle *v);
void game_spawn_owned(Game *g);
int game_home_upgrade(Game *g, int house);   /* 1 gelukt, 0 te weinig geld, -1 al het grootst */
int game_home_inside(const Game *g);         /* 0 buiten, 1 in huis, 2 in de garage */
int game_buy_car(Game *g, int model);
void game_sell_car(Game *g, int slot);
int game_custom_price(const Game *g, int item);   /* 0 lak, 1 motor, 2 handling, 3 neon, 4 spoiler */
int game_custom(Game *g, int item);
void game_damage(Game *g, float amount);
/* missie starten vanuit het keuzescherm: speler naar het startpunt (evt. met voertuig) */
void game_start_mission_at(Game *g, int m);

#endif
