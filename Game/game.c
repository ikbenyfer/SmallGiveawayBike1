#include <math.h>
#include "game.h"
#include "stores.h"
#include "voice.h"
#include "home.h"
#include "biz.h"
#include "models.h"
#include <string.h>
#include <stdio.h>

static const int DIRX[4] = {1, 0, -1, 0};
static const int DIRZ[4] = {0, 1, 0, -1};

static float dir_heading(int d) { return atan2f((float)DIRX[d], (float)DIRZ[d]); }
static void player_pos(const Game *g, float *x, float *z);
static void update_cop(Game *g, Ped *p, float dt);

void game_sfx(Game *g, int id, float x, float z) {
    if (g->nsfx >= MAX_SFXQ) return;
    g->sfxq[g->nsfx] = id; g->sfxx[g->nsfx] = x; g->sfxz[g->nsfx] = z;
    g->nsfx++;
}

void game_message(Game *g, const char *txt, float t) {
    snprintf(g->msg, sizeof(g->msg), "%s", txt);
    g->msgT = t;
}

static void banner(Game *g, const char *a, const char *b, int good, float t) {
    snprintf(g->banner, sizeof(g->banner), "%s", a);
    snprintf(g->sub, sizeof(g->sub), "%s", b ? b : "");
    g->bannerT = t;
    g->bannerGood = good;
}

/* ------------------------------------------------------------------ missies */

static void cp_set(Checkpoint *c, float x, float z, float r, int need, const char *text) {
    c->x = x; c->z = z; c->r = r; c->need = need; c->text = text;
}

static void setup_missions(Game *g) {
    World *w = g->w;
    MissionDef *m;
    int i;
    memset(g->mdef, 0, sizeof(g->mdef));

    m = &g->mdef[0];
    m->title = "WELCOME RIDE";
    m->brief = "Hop on the Giveaway Bike and ride to the markers.";
    m->giver = w->poi[POI_PLAZA];
    m->reward = 100;
    cp_set(&m->cp[0], w->poi[POI_GIVEAWAY_BIKE].x, w->poi[POI_GIVEAWAY_BIKE].z, 2.2f, NEED_BIKE, "Get on the Giveaway Bike (red)");
    cp_set(&m->cp[1], road_c(4), block_x0(3) + 22, 3.5f, NEED_BIKE, "Ride to the marker");
    cp_set(&m->cp[2], road_c(4), road_c(5), 3.5f, NEED_BIKE, "Ride to the next marker");
    cp_set(&m->cp[3], road_c(2), road_c(5), 3.5f, NEED_BIKE, "Almost there");
    cp_set(&m->cp[4], w->poi[POI_PLAZA].x, w->poi[POI_PLAZA].z + 3, 3.0f, NEED_BIKE, "Back to the plaza");
    m->ncp = 5;

    m = &g->mdef[1];
    m->title = "PIZZA PANIC";
    m->brief = "Deliver 3 hot pizzas to the suburb before they get cold!";
    m->giver = w->poi[POI_PIZZA];
    m->reward = 250;
    m->time = 170;
    for (i = 0; i < 3; i++) {
        V3 h = w->houses[(i * 5 + 1) % (w->nhouses > 0 ? w->nhouses : 1)];
        cp_set(&m->cp[i], h.x, h.z, 2.6f, NEED_ANY, i == 0 ? "Deliver pizza 1 of 3" : (i == 1 ? "Deliver pizza 2 of 3" : "Deliver the last pizza"));
    }
    m->ncp = 3;

    m = &g->mdef[2];
    m->title = "STREET SPRINT";
    m->brief = "Bike race! Hit all checkpoints before the clock runs out.";
    m->giver = w->poi[POI_BIKESHOP];
    m->reward = 300;
    m->time = 110;
    {
        static const int pts[8][2] = {{1, 5}, {1, 6}, {3, 6}, {3, 5}, {5, 5}, {5, 7}, {7, 7}, {7, 4}};
        for (i = 0; i < 8; i++)
            cp_set(&m->cp[i], road_c(pts[i][0]), road_c(pts[i][1]), 4.0f, NEED_BIKE, "Race: next checkpoint");
        m->ncp = 8;
    }

    m = &g->mdef[3];
    m->title = "JOYRIDE RETURN";
    m->brief = "Someone 'borrowed' the red car. Find it and bring it to the garage.";
    m->giver = w->poi[POI_GARAGE];
    m->giver.x -= 6;
    m->reward = 400;
    m->time = 140;
    cp_set(&m->cp[0], w->poi[POI_REDCAR].x, w->poi[POI_REDCAR].z, 3.0f, NEED_CAR, "Get in the red car (parking lot)");
    cp_set(&m->cp[1], w->poi[POI_GARAGE].x, w->poi[POI_GARAGE].z, 4.0f, NEED_CAR, "Drive it into the garage");
    m->ncp = 2;

    m = &g->mdef[4];
    m->title = "THE BIG GIVEAWAY";
    m->brief = "Collect 5 gift boxes around town and bring them to the park stage.";
    m->giver = w->poi[POI_STAGE];
    m->reward = 750;
    m->time = 300;
    for (i = 0; i < w->ngifts && i < 5; i++) cp_set(&m->cp[i], w->gifts[i].x, w->gifts[i].z, 2.0f, NEED_ANY, "Collect the gift boxes");
    cp_set(&m->cp[5], w->poi[POI_STAGE].x, w->poi[POI_STAGE].z, 3.5f, NEED_ANY, "Bring the gifts to the park stage");
    m->ncp = 6;
    m->anyOrder = 5;
    m->gifts = 1;

    m = &g->mdef[5];
    m->title = "PARK RUN";
    m->brief = "No wheels allowed! Run the park course on foot.";
    m->giver = v3(block_x0(3) + 22, 0, block_x0(0) + 27);
    m->reward = 200;
    m->time = 80;
    {
        static const float pts[7][2] = {{190, 20}, {214, 20}, {214, 48}, {202, 62}, {214, 80}, {190, 104}, {190, 76}};
        for (i = 0; i < 7; i++) cp_set(&m->cp[i], pts[i][0], pts[i][1], 2.6f, NEED_FOOT, "Run to the next checkpoint");
        m->ncp = 7;
    }

    m = &g->mdef[6];
    m->title = "TAXI RUSH";
    m->brief = "Be a taxi driver for a day. Pick up and drop off passengers.";
    m->giver = v3(block_x0(1) + 22, 0, block_x0(2) + 22);
    m->reward = 350;
    m->time = 160;
    m->startVeh = 2;
    cp_set(&m->cp[0], road_c(2), road_c(5), 4.0f, NEED_CAR, "Pick up the passenger");
    cp_set(&m->cp[1], road_c(6), road_c(6), 4.0f, NEED_CAR, "Drop off on the east side");
    cp_set(&m->cp[2], road_c(5), road_c(1), 4.0f, NEED_CAR, "Next fare: to the suburb");
    cp_set(&m->cp[3], road_c(1), road_c(1), 4.0f, NEED_CAR, "Last fare: north-west corner");
    m->ncp = 4;

    m = &g->mdef[7];
    m->title = "ALLEY SHORTCUT";
    m->brief = "Deliver a secret package through the back alleys on your bike.";
    m->giver = v3(14.5f, 0, block_x0(3) + 22);
    m->reward = 350;
    m->time = 150;
    m->startVeh = 1;
    {
        static const float pts[10][2] = {{22, 202}, {50, 202}, {130, 258}, {162, 258}, {298, 202},
                                         {330, 202}, {354, 258}, {386, 258}, {274, 314}, {242, 314}};
        for (i = 0; i < 10; i++) cp_set(&m->cp[i], pts[i][0], pts[i][1], 3.0f, NEED_BIKE, "Ride through the alley");
        m->ncp = 10;
    }

    m = &g->mdef[8];
    m->title = "COP DODGE";
    m->brief = "The police think you stole this car. Lose them!";
    m->giver = v3(block_x0(3) + 22, 0, block_x0(6) + 22);
    m->reward = 400;
    m->escape = 1;
    m->startVeh = 2;
    m->ncp = 0;

    m = &g->mdef[9];
    m->title = "GRAND TOUR";
    m->brief = "Drive the full ring road around the city before time runs out.";
    m->giver = v3(block_x0(0) + 22, 0, block_x0(5) + 22);
    m->reward = 500;
    m->time = 130;
    m->startVeh = 2;
    {
        static const int pts[8][2] = {{0, 7}, {3, 7}, {7, 7}, {7, 4}, {7, 0}, {3, 0}, {0, 0}, {0, 4}};
        for (i = 0; i < 8; i++) cp_set(&m->cp[i], road_c(pts[i][0]), road_c(pts[i][1]), 5.0f, NEED_CAR, "Ring road: next checkpoint");
        m->ncp = 8;
    }

    m = &g->mdef[10];
    m->title = "THE BIG HEIST";
    m->brief = "Crack the Giveaway Bank vault, lose the cops and bring the cash to the hideout.";
    m->giver = w->poi[POI_BANK];
    m->reward = 5000;
    m->requireWeapon = 1;
    cp_set(&m->cp[0], w->poi[POI_VAULT].x, w->poi[POI_VAULT].z, 2.8f, NEED_FOOT, "Crack the vault - stay close!");
    m->cp[0].hold = 15.0f;
    m->cp[0].alarm = 3;
    cp_set(&m->cp[1], w->poi[POI_GARAGE].x, w->poi[POI_GARAGE].z, 4.5f, NEED_ANY, "Lose the police, then bring the cash to the garage");
    m->cp[1].clean = 1;
    m->ncp = 2;

    /* ---- extra missies (11-18) ---- */
#define SW(i, j) (block_x0(i) + BLOCKW * 0.5f), (block_x0(j) + 1.3f)      /* midden noordstoep van blok i,j */
#define RC(i, j) road_c(i), road_c(j)                                       /* kruispunt */
    m = &g->mdef[11];
    m->title = "NIGHT DELIVERY";
    m->brief = "Bring late-night parcels to five shops across town by bike.";
    m->giver = v3(road_c(1), 0, road_c(2));
    m->reward = 450; m->time = 210; m->startVeh = 1;
    {
        static const int s[5][2] = {{0, 2}, {6, 2}, {3, 5}, {6, 5}, {2, 6}};
        for (i = 0; i < 5; i++) cp_set(&m->cp[i], block_x0(s[i][0]) + BLOCKW * 0.5f, block_x0(s[i][1]) + 1.3f, 2.8f, NEED_BIKE, "Drop the parcel at the shop");
        m->ncp = 5;
    }

    m = &g->mdef[12];
    m->title = "CASH COLLECTOR";
    m->brief = "Six cash bags were dropped around town. Grab them all and bring them to the plaza.";
    m->giver = v3(road_c(3), 0, road_c(3));
    m->reward = 600; m->time = 180; m->gifts = 2; m->anyOrder = 6;
    {
        static const int s[6][2] = {{1, 1}, {5, 0}, {6, 4}, {4, 6}, {0, 5}, {2, 3}};
        for (i = 0; i < 6; i++) cp_set(&m->cp[i], block_x0(s[i][0]) + BLOCKW * 0.5f, block_x0(s[i][1]) + 1.3f, 2.2f, NEED_ANY, "Collect the cash bags");
        cp_set(&m->cp[6], w->poi[POI_PLAZA].x, w->poi[POI_PLAZA].z, 3.0f, NEED_ANY, "Bring the cash to the plaza");
        m->ncp = 7;
    }

    m = &g->mdef[13];
    m->title = "DOWNTOWN DASH";
    m->brief = "On foot through downtown: reach every checkpoint before the clock hits zero.";
    m->giver = v3(road_c(3), 0, road_c(2));
    m->reward = 350; m->time = 120;
    {
        static const int s[8][2] = {{3, 2}, {4, 2}, {5, 2}, {5, 3}, {5, 4}, {4, 5}, {3, 5}, {2, 4}};
        for (i = 0; i < 8; i++) cp_set(&m->cp[i], road_c(s[i][0]), road_c(s[i][1]), 3.0f, NEED_FOOT, "Run to the next checkpoint");
        m->ncp = 8;
    }

    m = &g->mdef[14];
    m->title = "SPEED TRAP";
    m->brief = "A fast car race on the long roads. Floor it!";
    m->giver = v3(road_c(1), 0, road_c(1));
    m->reward = 700; m->time = 75; m->startVeh = 2;
    {
        static const int s[6][2] = {{6, 1}, {6, 6}, {1, 6}, {1, 3}, {4, 3}, {4, 1}};
        for (i = 0; i < 6; i++) cp_set(&m->cp[i], road_c(s[i][0]), road_c(s[i][1]), 5.0f, NEED_CAR, "Race: next checkpoint");
        m->ncp = 6;
    }

    m = &g->mdef[15];
    m->title = "CORNER STORE STICK-UP";
    m->brief = "Rob the bike shop register, then lose the police and hide at the garage.";
    m->giver = v3(w->poi[POI_BIKESHOP].x, 0, w->poi[POI_BIKESHOP].z + 3.0f);
    m->reward = 1500; m->requireWeapon = 1;
    cp_set(&m->cp[0], w->poi[POI_BIKESHOP].x, w->poi[POI_BIKESHOP].z, 2.6f, NEED_FOOT, "Empty the register - stay close!");
    m->cp[0].hold = 8.0f; m->cp[0].alarm = 2;
    cp_set(&m->cp[1], w->poi[POI_GARAGE].x, w->poi[POI_GARAGE].z, 4.5f, NEED_ANY, "Lose the police, then hide at the garage");
    m->cp[1].clean = 1;
    m->ncp = 2;

    m = &g->mdef[16];
    m->title = "GETAWAY DRIVER";
    m->brief = "Pick up the crew at the bank, outrun the cops and drop them at the hideout.";
    m->giver = v3(road_c(4), 0, road_c(2) - 8.0f);
    m->reward = 2500; m->startVeh = 2;
    cp_set(&m->cp[0], road_c(4), block_x0(3) + 22.0f, 4.0f, NEED_CAR, "Pick up the crew at the bank");
    m->cp[0].hold = 3.0f; m->cp[0].alarm = 2;
    cp_set(&m->cp[1], road_c(7), road_c(6), 5.0f, NEED_CAR, "Drive to the east edge of town");
    cp_set(&m->cp[2], w->poi[POI_GARAGE].x, w->poi[POI_GARAGE].z, 4.5f, NEED_CAR, "Lose the police, then go to the garage");
    m->cp[2].clean = 1;
    m->ncp = 3;

    m = &g->mdef[17];
    m->title = "MOST WANTED";
    m->brief = "Three stars and the whole police force on your tail. Shake them off!";
    m->giver = v3(road_c(6), 0, road_c(3));
    m->reward = 1500; m->escape = 3; m->startVeh = 2; m->ncp = 0;

    m = &g->mdef[18];
    m->title = "TOUR DE STAD";
    m->brief = "The big bike race: a full lap of the city in four minutes.";
    m->giver = v3(road_c(0), 0, road_c(0) + 8.0f);
    m->reward = 800; m->time = 240; m->startVeh = 1;
    {
        static const int s[10][2] = {{2, 0}, {4, 0}, {7, 0}, {7, 3}, {7, 7}, {4, 7}, {0, 7}, {0, 4}, {2, 3}, {0, 1}};
        for (i = 0; i < 10; i++) cp_set(&m->cp[i], road_c(s[i][0]), road_c(s[i][1]), 4.0f, NEED_BIKE, "Race: next checkpoint");
        m->ncp = 10;
    }
    m = &g->mdef[19];
    m->title = "GOLD RESERVE HEIST";
    m->brief = "The hardest job in town: crack the gold reserve, drive it to the east edge and lose a full alarm.";
    m->giver = w->poi[POI_BANK];
    m->reward = 15000; m->repeatReward = 10000;
    m->requireWeapon = 1; m->time = 300;
    cp_set(&m->cp[0], w->poi[POI_VAULT].x, w->poi[POI_VAULT].z, 2.2f, NEED_FOOT, "Drill the gold reserve - stay close!");
    m->cp[0].hold = 30.0f;
    m->cp[0].alarm = 3;
    cp_set(&m->cp[1], road_c(7), road_c(0), 5.0f, NEED_CAR, "Grab a car and get the gold to the north-east corner");
    cp_set(&m->cp[2], road_c(0), road_c(7), 5.0f, NEED_CAR, "Switch route: south-west corner");
    cp_set(&m->cp[3], w->poi[POI_GARAGE].x, w->poi[POI_GARAGE].z, 4.5f, NEED_ANY, "Lose the police, then hide the gold at the garage");
    m->cp[3].clean = 1;
    m->ncp = 4;
    m = &g->mdef[20];
    m->title = "THE ULTIMATE HEIST";
    m->brief = "The job of a lifetime: hit the register, drill the bank vault, outrun the whole city and lose every cop.";
    m->giver = w->poi[POI_BANK];
    m->reward = 30000; m->repeatReward = 25000;
    m->requireWeapon = 1; m->time = 420;
    cp_set(&m->cp[0], w->poi[POI_BIKESHOP].x, w->poi[POI_BIKESHOP].z, 2.4f, NEED_FOOT, "Warm-up: empty the bike shop register");
    m->cp[0].hold = 8.0f; m->cp[0].alarm = 2;
    cp_set(&m->cp[1], w->poi[POI_VAULT].x, w->poi[POI_VAULT].z, 2.0f, NEED_FOOT, "Drill the main vault - this takes long, stay close!");
    m->cp[1].hold = 45.0f; m->cp[1].alarm = 3;
    cp_set(&m->cp[2], road_c(7), road_c(7), 5.0f, NEED_CAR, "Get a fast car - race to the south-east corner");
    cp_set(&m->cp[3], road_c(0), road_c(7), 5.0f, NEED_CAR, "South-west corner");
    cp_set(&m->cp[4], road_c(0), road_c(0), 5.0f, NEED_CAR, "North-west corner");
    cp_set(&m->cp[5], road_c(7), road_c(0), 5.0f, NEED_CAR, "North-east corner");
    cp_set(&m->cp[6], w->poi[POI_GARAGE].x, w->poi[POI_GARAGE].z, 4.5f, NEED_ANY, "Lose ALL the police, then hide the loot at the garage");
    m->cp[6].clean = 1;
    m->ncp = 7;

    m = &g->mdef[21];
    m->title = "THE IMPOSSIBLE JOB";
    m->brief = "Only legends finish this: two robberies, the biggest vault drill ever, a full city chase and a second run for the gold.";
    m->giver = w->poi[POI_BANK];
    m->reward = 100000; m->repeatReward = 80000;
    m->requireWeapon = 1; m->time = 540;
    cp_set(&m->cp[0], w->poi[POI_BIKESHOP].x, w->poi[POI_BIKESHOP].z, 2.2f, NEED_FOOT, "Step 1: rob the bike shop register");
    m->cp[0].hold = 10.0f; m->cp[0].alarm = 2;
    cp_set(&m->cp[1], w->poi[POI_VAULT].x, w->poi[POI_VAULT].z, 1.8f, NEED_FOOT, "Step 2: drill the main vault for 60 seconds - do NOT leave!");
    m->cp[1].hold = 60.0f; m->cp[1].alarm = 3;
    cp_set(&m->cp[2], road_c(7), road_c(7), 5.0f, NEED_CAR, "Step 3: steal a car - south-east corner");
    cp_set(&m->cp[3], road_c(4), road_c(4), 5.0f, NEED_CAR, "City centre");
    cp_set(&m->cp[4], road_c(0), road_c(7), 5.0f, NEED_CAR, "South-west corner");
    cp_set(&m->cp[5], road_c(0), road_c(0), 5.0f, NEED_CAR, "North-west corner");
    cp_set(&m->cp[6], road_c(7), road_c(0), 5.0f, NEED_CAR, "North-east corner");
    cp_set(&m->cp[7], road_c(3), road_c(5), 5.0f, NEED_CAR, "Back into town");
    cp_set(&m->cp[8], w->poi[POI_VAULT].x, w->poi[POI_VAULT].z, 1.8f, NEED_FOOT, "Step 4: go back IN for the gold bars (30 s)");
    m->cp[8].hold = 30.0f; m->cp[8].alarm = 3;
    cp_set(&m->cp[9], w->poi[POI_GARAGE].x, w->poi[POI_GARAGE].z, 4.5f, NEED_ANY, "Final step: lose EVERY cop, then hide at the garage");
    m->cp[9].clean = 1;
    m->ncp = 10;
#undef SW
#undef RC

    g->mdef[2].startVeh = 1;
    for (i = 0; i < NMISSIONS; i++) {
        int need = g->mdef[i].ncp > 0 ? g->mdef[i].cp[g->mdef[i].ncp - 1].need : NEED_CAR;
        g->mdef[i].tag = need == NEED_BIKE ? "BIKE" : (need == NEED_CAR ? "CAR" : (need == NEED_FOOT ? "ON FOOT" : "ANY"));
        if (g->mdef[i].requireWeapon) g->mdef[i].tag = "NEEDS A GUN";
    }
}

void game_start_mission(Game *g, int m) {
    if (m < 0 || m >= NMISSIONS) return;
    if (!g->noCutscenes && !g->nl && m < VO_MISSIONS && VO_MIS[m].nintro > 0) { cut_start(g, m); return; }
    game_mission_begin(g, m);
}

void game_mission_begin(Game *g, int m) {
    if (m < 0 || m >= NMISSIONS) return;
    g->mission = m;
    g->mstep = 0;
    g->mmask = 0;
    g->mclock = 0;
    g->holdT = 0;
    g->alarmDone = 0;
    g->mtime = g->mdef[m].time;
    banner(g, g->mdef[m].title, g->mdef[m].brief, 1, 4.5f);
    if (g->mdef[m].escape) { g->crimeCool = 0; g->wanted = 0; game_add_wanted(g, g->mdef[m].escape > 1 ? g->mdef[m].escape : 2); }
}

static void mission_end(Game *g, int ok) {
    int m = g->mission;
    if (m < 0) return;
    if (ok) {
        int rew = !g->completed[m] ? g->mdef[m].reward : (g->mdef[m].repeatReward ? g->mdef[m].repeatReward : g->mdef[m].reward / 2);
        char b[64];
        g->money += rew;
        g->moneyFlash = 2.0f;
        game_sfx(g, SFX_PASS, g->p.x, g->p.z);
        g->completed[m] = 1;
        g->statMissionsPassed++;
        snprintf(b, sizeof(b), "+$%d", rew);
        banner(g, "MISSION PASSED", b, 1, 4.0f);
        if (m < VO_MISSIONS) vo_say(g, VO_MIS[m].pass, VP_MISSION);
    } else {
        banner(g, "MISSION FAILED", "Out of time. Try again from the MISSIONS menu.", 0, 4.0f);
        if (m < VO_MISSIONS) vo_say(g, VO_MIS[m].fail, VP_MISSION);
        game_sfx(g, SFX_FAIL, g->p.x, g->p.z);
    }
    g->mission = -1;
    g->p.cool = 2.5f;
}

static int need_ok(const Game *g, int need) {
    int mode = game_mode(g);
    /* een gekochte motor telt als motorvoertuig (auto-opdrachten), niet als fiets */
    int moto = g->p.veh >= 0 && g->veh[g->p.veh].custom && g->veh[g->p.veh].type == VEH_BIKE;
    if (need == NEED_BIKE) return mode == MODE_BIKE && !moto;
    if (need == NEED_CAR) return mode == MODE_CAR || moto;
    if (need == NEED_FOOT) return mode == MODE_FOOT;
    return 1;
}

static void player_pos(const Game *g, float *x, float *z) {
    if (g->p.veh >= 0) { *x = g->veh[g->p.veh].x; *z = g->veh[g->p.veh].z; }
    else { *x = g->p.x; *z = g->p.z; }
    if (!g->nl && *x < HOME_ZONE_X) { home_door(g->homeLvl, x, z); return; }
    if (!g->nl && *z < STORE_ZONE_Z) { int s = store_at(*x, *z); if (s) { V3 d = g->w->poi[store_poi(s - 1)]; *x = d.x; *z = d.z; } }
}

static void update_missions(Game *g, float dt) {
    float px, pz;
    int i;
    player_pos(g, &px, &pz);
    (void)i;
    if (g->mission < 0) return;   /* missies start je alleen via het MISSIONS-menu */
    {
        MissionDef *m = &g->mdef[g->mission];
        g->mclock += dt;
        if (m->escape) {
            if (g->wanted == 0 && g->mclock > 2.0f) mission_end(g, 1);
            return;
        }
        if (m->time > 0) {
            g->mtime -= dt;
            if (g->mtime <= 0) { mission_end(g, 0); return; }
        }
        if (g->mstep < m->anyOrder) {
            for (i = 0; i < m->anyOrder; i++) {
                Checkpoint *c = &m->cp[i];
                float dx = px - c->x, dz = pz - c->z;
                if (g->mmask & (1u << i)) continue;
                if (dx * dx + dz * dz < c->r * c->r) {
                    if (!need_ok(g, c->need)) { if (g->msgT <= 0) game_message(g, c->need == NEED_BIKE ? "You need a bike for this!" : (c->need == NEED_FOOT ? "Get off your vehicle!" : "You need a car for this!"), 2.0f); continue; }
                    g->mmask |= 1u << i;
                    g->mstep++;
                    if (g->mission < VO_MISSIONS) vo_say(g, VO_MIS[g->mission].step[g->mstep], VP_MISSION);
                    game_sfx(g, SFX_CHECK, px, pz);
                    {
                        char b[64];
                        snprintf(b, sizeof(b), "Got %d of %d", g->mstep, m->anyOrder);
                        game_message(g, b, 2.0f);
                    }
                }
            }
        } else if (g->mstep < m->ncp) {
            Checkpoint *c = &m->cp[g->mstep];
            float dx = px - c->x, dz = pz - c->z;
            int inside = dx * dx + dz * dz < c->r * c->r;
            if (!inside && c->hold > 0 && g->holdT > 0) {
                g->holdT = fmaxf(0, g->holdT - dt * 0.5f);
                if (g->msgT <= 0) game_message(g, "Get back to the vault!", 1.5f);
            }
            if (inside && c->alarm && !g->alarmDone && need_ok(g, c->need)) {
                g->alarmDone = 1;
                g->crimeCool = 0;
                g->wanted = 0;
                game_add_wanted(g, c->alarm);
                game_message(g, "ALARM! The police are on their way!", 3.0f);
                if (g->mission < VO_MISSIONS) vo_say(g, VO_MIS[g->mission].alarm, VP_MISSION);
                game_sfx(g, SFX_ALARM, px, pz);
            }
            if (inside && c->clean && g->wanted > 0) {
                if (g->msgT <= 0) game_message(g, "Lose the police first!", 2.0f);
            } else if (inside && c->hold > 0 && need_ok(g, c->need)) {
                g->holdT += dt;
                if (g->holdT >= c->hold) {
                    g->holdT = 0;
                    g->mstep++;
                    g->alarmDone = 0;   /* volgende kluis/kassa kan weer alarm geven */
                    game_message(g, "Vault open - you got the cash!", 2.5f);
                    if (g->mission < VO_MISSIONS) {
                        int sl = g->mstep < MAX_CP + 1 ? VO_MIS[g->mission].step[g->mstep] : -1;
                        vo_say(g, VO_MIS[g->mission].open, VP_MISSION);
                        vo_queue(g, sl, VP_MISSION);
                    }
                    game_sfx(g, SFX_CASH, px, pz);
                    if (g->mstep >= m->ncp) mission_end(g, 1);
                }
            } else if (inside) {
                if (!need_ok(g, c->need)) {
                    if (g->msgT <= 0) game_message(g, c->need == NEED_BIKE ? "Get on a bike first!" : (c->need == NEED_FOOT ? "On foot only - get off!" : "Get in a car first!"), 2.0f);
                } else {
                    g->mstep++;
                    if (g->mstep < m->ncp) game_sfx(g, SFX_CHECK, px, pz);
                    if (g->mstep < m->ncp && g->mission < VO_MISSIONS) vo_say(g, VO_MIS[g->mission].step[g->mstep], VP_MISSION);
                    if (g->mstep >= m->ncp) mission_end(g, 1);
                }
            }
        }
    }
}

int game_targets(const Game *g, V3 *out, int max) {
    int n = 0, i;
    if (g->nl || g->mission < 0) return 0;   /* geen missie-cirkels in de stad */
    if (g->mission < 0) {
        for (i = 0; i < NMISSIONS && n < max; i++) {
            out[n] = g->mdef[i].giver;
            out[n].y = g->completed[i] ? 1.0f : 0.0f;  /* y = vlag: al gehaald */
            n++;
        }
        return n;
    }
    {
        const MissionDef *m = &g->mdef[g->mission];
        if (g->mstep < m->anyOrder) {
            for (i = 0; i < m->anyOrder && n < max; i++)
                if (!(g->mmask & (1u << i))) out[n++] = v3(m->cp[i].x, 2.0f, m->cp[i].z);
        } else if (g->mstep < m->ncp && n < max) {
            out[n++] = v3(m->cp[g->mstep].x, 2.0f, m->cp[g->mstep].z);
        }
    }
    return n;
}

/* ------------------------------------------------------------------ voertuigen */

int game_mode(const Game *g) {
    if (g->p.veh < 0) return MODE_FOOT;
    return g->veh[g->p.veh].type == VEH_BIKE ? MODE_BIKE : MODE_CAR;
}

static float veh_radius(const Vehicle *v) { return v->type == VEH_BIKE ? 0.55f : 1.05f; }

int game_near_vehicle(const Game *g) {
    int i, best = -1;
    float bd = 1e9f;
    if (g->p.veh >= 0) return -1;
    for (i = 0; i < g->nveh; i++) {
        const Vehicle *v = &g->veh[i];
        float dx, dz, d2, lim;
        if (!v->active || v->police) continue;
        if (v->traffic && fabsf(v->speed) > 5.0f) continue;
        dx = v->x - g->p.x; dz = v->z - g->p.z; d2 = dx * dx + dz * dz;
        lim = v->type == VEH_BIKE ? 2.0f : 3.2f;
        if (d2 < lim * lim && d2 < bd) { bd = d2; best = i; }
    }
    return best;
}

static void spawn_ped_at(Game *g, Ped *p, float x, float z, int flee);

static void enter_vehicle(Game *g, int idx) {
    Vehicle *v = &g->veh[idx];
    if (v->traffic) {
        /* bestuurder stapt geschrokken uit en loopt weg */
        int k;
        for (k = 0; k < g->wantPeds && k < MAX_PED; k++) {
            if (!g->ped[k].active) break;
        }
        if (k >= MAX_PED || k >= g->wantPeds) k = 0;
        {
            V3 r = right_of(v->h);
            spawn_ped_at(g, &g->ped[k], v->x + r.x * 1.8f, v->z + r.z * 1.8f, 1);
        }
        v->traffic = 0;
        v->speed *= 0.3f;
        game_add_wanted(g, 1);
        game_message(g, "Car stolen! The police are coming...", 2.5f);
    }
    g->p.anX = g->p.x; g->p.anZ = g->p.z; g->p.anH = g->p.h;
    g->p.anKind = v->type == VEH_BIKE ? 2 : 1;
    g->p.anDur = g->p.anT = v->type == VEH_BIKE ? 0.5f : 0.7f;
    g->p.anVeh = idx;
    g->p.veh = idx;
    g->p.speed = 0;
    g->p.vy = 0;
    g->statEnter++;
    game_sfx(g, SFX_DOOR, v->x, v->z);
    vo_bark(g, v->type == VEH_BIKE ? VB_BIKEIN : VB_CARIN, 0.3f);
}

static void exit_vehicle(Game *g) {
    Vehicle *v = &g->veh[g->p.veh];
    V3 r = right_of(v->h);
    float side = v->type == VEH_BIKE ? 0.9f : 1.7f;
    float nx, nz;
    if (fabsf(v->speed) > 6.0f) { game_message(g, "Slow down first!", 1.5f); return; }
    g->p.x = v->x - r.x * side;
    g->p.z = v->z - r.z * side;
    g->p.h = v->h;
    g->p.y = world_ground_y(g->w, g->p.x, g->p.z);
    world_push_circle(g->w, &g->p.x, &g->p.z, 0.35f, g->p.y, &nx, &nz);
    v->speed = 0;
    v->steer = 0;
    g->p.anX = v->x; g->p.anZ = v->z; g->p.anH = v->h;
    g->p.anKind = v->type == VEH_BIKE ? 4 : 3;
    g->p.anDur = g->p.anT = v->type == VEH_BIKE ? 0.45f : 0.6f;
    g->p.anVeh = g->p.veh;
    g->p.veh = -1;
    g->p.cool = 0.5f;
    game_sfx(g, SFX_DOOR, v->x, v->z);
}

static void collide_vehicles(Game *g, Vehicle *a) {
    int i;
    for (i = 0; i < g->nveh; i++) {
        Vehicle *b = &g->veh[i];
        float dx, dz, d2, rr;
        if (b == a || !b->active) continue;
        dx = a->x - b->x; dz = a->z - b->z; d2 = dx * dx + dz * dz;
        rr = veh_radius(a) + veh_radius(b) + (a->type == VEH_CAR && b->type == VEH_CAR ? 0.5f : 0.0f);
        if (d2 < rr * rr && d2 > 1e-6f) {
            float d = sqrtf(d2), push = (rr - d);
            float wa = b->traffic || b->speed != 0 ? 0.5f : 0.7f;
            dx /= d; dz /= d;
            a->x += dx * push * wa; a->z += dz * push * wa;
            b->x -= dx * push * (1 - wa); b->z -= dz * push * (1 - wa);
            if (a->hitcool <= 0) { a->speed *= 0.55f; a->hitcool = 0.4f; }
            if (g->p.veh >= 0 && g->ramCool <= 0 &&
                ((a == &g->veh[g->p.veh] && b->police) || (b == &g->veh[g->p.veh] && a->police))) {
                game_damage(g, 6.0f);
                g->ramCool = 0.8f;
            }
            if (b->traffic) b->speed *= 0.3f;
        }
    }
}

static void vehicle_world_collide(Game *g, Vehicle *v) {
    float nx = 0, nz = 0;
    int hit = 0;
    if (!g->nl) {
        float ox = v->x, oz = v->z;
        if (v->type == VEH_BIKE) hit = home_push(g->homeLvl, g->garLvl, &v->x, &v->z, 0.5f);
        else {
            V3 f = fwd_of(v->h);
            int k;
            for (k = -1; k <= 1; k += 2) {
                float cx = v->x + f.x * 1.15f * k, cz = v->z + f.z * 1.15f * k, px = cx, pz = cz;
                if (home_push(g->homeLvl, g->garLvl, &cx, &cz, 0.95f)) { v->x += cx - px; v->z += cz - pz; hit = 1; }
            }
        }
        if (hit) { nx = v->x - ox; nz = v->z - oz; { float l = sqrtf(nx * nx + nz * nz); if (l > 1e-5f) { nx /= l; nz /= l; } } }
    }
    if (v->type == VEH_BIKE) {
        hit |= world_push_circle(g->w, &v->x, &v->z, 0.5f, v->y + 0.2f, &nx, &nz);
    } else {
        V3 f = fwd_of(v->h);
        int k;
        for (k = -1; k <= 1; k += 2) {
            float cx = v->x + f.x * 1.15f * k, cz = v->z + f.z * 1.15f * k, ox = cx, oz = cz;
            float tx, tz;
            if (world_push_circle(g->w, &cx, &cz, 0.95f, v->y + 0.2f, &tx, &tz)) {
                v->x += cx - ox; v->z += cz - oz;
                nx += tx; nz += tz; hit = 1;
            }
        }
    }
    if (hit) {
        V3 f = fwd_of(v->h);
        float d = f.x * nx + f.z * nz;
        if (v->hitcool <= 0 && g->p.veh >= 0 && v == &g->veh[g->p.veh]) {
            float imp = fabsf(v->speed) * fabsf(d);
            if (imp > 7.0f) game_damage(g, (imp - 7.0f) * 2.5f);
            if (imp > 4.0f) { game_sfx(g, SFX_CRASH, v->x, v->z); if (v == &g->veh[g->p.veh] || g->p.veh < 0) vo_bark(g, VB_CRASH, 0.2f); }
        }
        if (v->hitcool <= 0) {
            if ((d < -0.6f && v->speed > 0) || (d > 0.6f && v->speed < 0)) v->speed = -v->speed * 0.25f;
            else v->speed *= 0.75f;
            v->hitcool = 0.25f;
        }
    }
}

static void drive_vehicle(Game *g, Vehicle *v, float thr, float brk, float steerIn, float dt) {
    int bike = v->type == VEH_BIKE;
    float maxF = bike ? 11.5f : 19.0f, acc = bike ? 4.8f : 6.5f, brake = bike ? 10.0f : 13.0f, maxR = bike ? -2.0f : -6.0f;
    float steerK = bike ? 1.35f : 0.95f;
    if (v->custom) {   /* gekochte auto of motor */
        const CarDef *cd = &CARS[v->model % DEALER_MODELS];
        maxF = cd->maxF * (1.0f + 0.08f * v->eng);
        acc = cd->acc * (1.0f + 0.15f * v->eng);
        brake = 13.0f + v->hand * 2.0f;
        steerK = cd->steer * (1.0f + 0.12f * v->hand);
    }
    float yaw;
    if (thr > 0) {
        if (v->speed < 0) v->speed += brake * dt;
        else if (v->speed < maxF) v->speed += acc * thr * dt * (1.0f - 0.5f * v->speed / maxF);
    }
    if (brk > 0) {
        if (v->speed > 0.2f) v->speed -= brake * dt;
        else if (v->speed > maxR) v->speed -= acc * 0.6f * dt;
    }
    if (thr <= 0 && brk <= 0) v->speed = approachf(v->speed, 0, (bike ? 1.4f : 2.0f) * dt);
    v->speed -= v->speed * 0.05f * dt;
    v->steer = approachf(v->steer, steerIn, 3.5f * dt);
    {
        float sp = fabsf(v->speed);
        float k = steerK;
        yaw = v->steer * k * clampf(sp / 5.0f, 0, 1) * (1.0f - 0.35f * clampf(sp / maxF, 0, 1));
        if (v->speed < 0) yaw = -yaw;
        v->h = wrap_angle(v->h - yaw * dt * 2.0f);
    }
    {
        V3 f = fwd_of(v->h);
        v->x += f.x * v->speed * dt;
        v->z += f.z * v->speed * dt;
    }
    if (bike) v->lean = approachf(v->lean, -v->steer * clampf(fabsf(v->speed) / 8.0f, 0, 1) * 0.45f, 2.0f * dt);
    v->wheel += v->speed * dt / 0.34f;
    if (v->hitcool > 0) v->hitcool -= dt;
    vehicle_world_collide(g, v);
    collide_vehicles(g, v);
    v->y = lerpf(v->y, world_ground_y(g->w, v->x, v->z), clampf(dt * 12.0f, 0, 1));
}

/* ------------------------------------------------------------------ verkeer */

static V3 node_pos(int i, int j) { return v3(road_c(i), 0, road_c(j)); }

static void traffic_set_target(Vehicle *v) {
    V3 n = node_pos(v->ti, v->tj);
    V3 r = right_of(dir_heading(v->dir));
    v->tx = n.x - DIRX[v->dir] * (ROADW * 0.5f + 1.5f) + r.x * LANE_OFF;
    v->tz = n.z - DIRZ[v->dir] * (ROADW * 0.5f + 1.5f) + r.z * LANE_OFF;
    v->phase = 0;
}

static void traffic_spawn(Game *g, Vehicle *v, float px, float pz, int nearOk) {
    int tries;
    for (tries = 0; tries < 30; tries++) {
        int i = rng_int(&g->rng, NB + 1), j = rng_int(&g->rng, NB + 1), d = rng_int(&g->rng, 4);
        int ni = i + DIRX[d], nj = j + DIRZ[d];
        V3 a, b, r, p;
        float dx, dz, dist;
        if (ni < 0 || nj < 0 || ni > NB || nj > NB) continue;
        a = node_pos(i, j); b = node_pos(ni, nj);
        r = right_of(dir_heading(d));
        p = v3add(v3lerp(a, b, rng_range(&g->rng, 0.3f, 0.7f)), v3scale(r, LANE_OFF));
        dx = p.x - px; dz = p.z - pz; dist = sqrtf(dx * dx + dz * dz);
        if (!nearOk && (dist < 60.0f || dist > 150.0f)) continue;
        if (nearOk && dist > 140.0f) continue;
        if (nearOk && dist < 12.0f) continue;
        v->active = 1; v->type = VEH_CAR; v->traffic = 1;
        v->variant = 1 + rng_int(&g->rng, 5);
        v->x = p.x; v->z = p.z; v->y = 0; v->h = dir_heading(d);
        v->speed = 6.0f; v->steer = 0; v->stuck = 0; v->ghost = 0;
        v->ti = ni; v->tj = nj; v->dir = d;
        traffic_set_target(v);
        return;
    }
}

static int blocked_ahead(Game *g, Vehicle *v, float *dist) {
    V3 f = fwd_of(v->h), r = right_of(v->h);
    float best = 1e9f;
    int i;
    float px, pz;
#define CHECK(ox, oz, lat) do { float rx = (ox) - v->x, rz = (oz) - v->z; \
        float al = rx * f.x + rz * f.z, la = fabsf(rx * r.x + rz * r.z); \
        if (al > 0.5f && al < 11.0f && la < (lat) && al < best) best = al; } while (0)
    player_pos(g, &px, &pz);
    CHECK(px, pz, 2.0f);
    for (i = 0; i < g->nveh; i++) {
        Vehicle *o = &g->veh[i];
        if (o == v || !o->active) continue;
        if (o->traffic || i == g->p.veh) continue; /* speler al gedaan */
        CHECK(o->x, o->z, 2.0f);
    }
    for (i = g->firstTraffic; i < g->nveh; i++) {
        Vehicle *o = &g->veh[i];
        if (o == v || !o->active || (!o->traffic && !o->police)) continue;
        CHECK(o->x, o->z, 1.8f);
    }
    for (i = 0; i < MAX_PED; i++) {
        Ped *p = &g->ped[i];
        if (!p->active) continue;
        CHECK(p->x, p->z, 1.6f);
    }
#undef CHECK
    *dist = best;
    return best < 1e8f;
}

static void update_traffic_car(Game *g, Vehicle *v, float dt) {
    float dx = v->tx - v->x, dz = v->tz - v->z, d = sqrtf(dx * dx + dz * dz);
    float want = v->phase == 1 ? 5.0f : 9.0f;
    float bd;
    if (d < 2.2f) {
        if (v->phase == 0) {
            /* op het kruispunt: kies nieuwe richting */
            int opts[4], no = 0, k, nd;
            for (k = 0; k < 4; k++) {
                int ni = v->ti + DIRX[k], nj = v->tj + DIRZ[k];
                if (k == (v->dir + 2) % 4) continue;
                if (ni < 0 || nj < 0 || ni > NB || nj > NB) continue;
                opts[no++] = k;
                if (k == v->dir) opts[no++] = k; /* rechtdoor vaker */
            }
            nd = no ? opts[rng_int(&g->rng, no)] : (v->dir + 2) % 4;
            {
                V3 n = node_pos(v->ti, v->tj);
                V3 r = right_of(dir_heading(nd));
                v->tx = n.x + DIRX[nd] * (ROADW * 0.5f + 1.5f) + r.x * LANE_OFF;
                v->tz = n.z + DIRZ[nd] * (ROADW * 0.5f + 1.5f) + r.z * LANE_OFF;
            }
            v->dir = nd;
            v->phase = 1;
        } else {
            v->ti += DIRX[v->dir];
            v->tj += DIRZ[v->dir];
            traffic_set_target(v);
        }
        dx = v->tx - v->x; dz = v->tz - v->z;
    }
    if (v->ghost > 0) v->ghost -= dt;
    else if (blocked_ahead(g, v, &bd)) {
        if (bd < 5.5f) want = 0;
        else want = fminf(want, 3.0f);
    }
    if (want < 0.5f && fabsf(v->speed) < 0.5f) {
        v->stuck += dt;
        if (v->stuck > 4.0f) { v->ghost = 2.5f; v->stuck = 0; }
    } else v->stuck = 0;
    v->speed = approachf(v->speed, want, (want < v->speed ? 12.0f : 4.0f) * dt);
    {
        float target = atan2f(dx, dz);
        v->h = approach_angle(v->h, target, 2.2f * dt * clampf(v->speed / 4.0f, 0.2f, 1.2f));
    }
    {
        V3 f = fwd_of(v->h);
        v->x += f.x * v->speed * dt;
        v->z += f.z * v->speed * dt;
    }
    v->wheel += v->speed * dt / 0.34f;
}

/* ------------------------------------------------------------------ voetgangers */

static void ped_corner(int bi, int bj, int c, float *x, float *z) {
    float x0 = block_x0(bi) + 1.3f, z0 = block_x0(bj) + 1.3f, x1 = block_x0(bi) + BLOCKW - 1.3f, z1 = block_x0(bj) + BLOCKW - 1.3f;
    c &= 3;
    *x = (c == 0 || c == 3) ? x0 : x1;
    *z = (c < 2) ? z0 : z1;
}

static void spawn_ped_at(Game *g, Ped *p, float x, float z, int flee) {
    p->active = 1;
    p->variant = 1 + rng_int(&g->rng, 7);
    p->bi = (int)clampf(floorf((x - ROADW) / PITCH), 0, NB - 1);
    p->bj = (int)clampf(floorf((z - ROADW) / PITCH), 0, NB - 1);
    p->corner = rng_int(&g->rng, 4);
    p->dsign = rng_f(&g->rng) < 0.5f ? 1 : -1;
    p->x = x; p->z = z; p->y = world_ground_y(g->w, x, z);
    p->speed = rng_range(&g->rng, 1.2f, 1.7f);
    p->state = flee ? PED_FLEE : PED_WALK;
    p->timer = flee ? 3.0f : 0;
    p->h = 0; p->anim = 0; p->lay = 0;
    p->cop = 0; p->shot = 0; p->hp = 60; p->shootCool = 0;
}

static void ped_respawn(Game *g, Ped *p, float px, float pz, int nearOk) {
    int tries;
    for (tries = 0; tries < 20; tries++) {
        int bi = rng_int(&g->rng, NB), bj = rng_int(&g->rng, NB), c = rng_int(&g->rng, 4);
        float x0, z0, x1, z1, t = rng_f(&g->rng), x, z, dx, dz, d;
        ped_corner(bi, bj, c, &x0, &z0);
        ped_corner(bi, bj, c + 1, &x1, &z1);
        x = lerpf(x0, x1, t); z = lerpf(z0, z1, t);
        dx = x - px; dz = z - pz; d = sqrtf(dx * dx + dz * dz);
        if (nearOk ? (d > 110.0f || d < 8.0f) : (d < 45.0f || d > 110.0f)) continue;
        spawn_ped_at(g, p, x, z, 0);
        p->bi = bi; p->bj = bj; p->corner = p->dsign > 0 ? c + 1 : c;
        return;
    }
}

static void update_ped(Game *g, Ped *p, float dt) {
    float px, pz;
    int i;
    player_pos(g, &px, &pz);
    if (p->state == PED_DOWN) {
        p->timer -= dt;
        p->lay = approachf(p->lay, p->timer > 0.6f ? 1.0f : 0.0f, dt * 3.0f);
        if (p->timer <= 0) {
            if (p->shot) { p->active = 0; return; }   /* verdwijnt; elders komt een nieuwe */
            p->state = PED_FLEE; p->timer = 2.5f; p->lay = 0;
        }
        return;
    }
    if (p->cop) { update_cop(g, p, dt); return; }
    /* gevaar: voertuigen die eraan komen */
    for (i = 0; i < g->nveh; i++) {
        Vehicle *v = &g->veh[i];
        float rx, rz, al, la, sp;
        V3 f, r;
        if (!v->active) continue;
        sp = fabsf(v->speed);
        if (sp < 2.5f) continue;
        f = fwd_of(v->h); r = right_of(v->h);
        rx = p->x - v->x; rz = p->z - v->z;
        al = rx * f.x + rz * f.z; la = rx * r.x + rz * r.z;
        if (v->speed < 0) al = -al;
        if (al > -1.0f && al < 1.6f + (v->type == VEH_CAR ? 2.0f : 0.6f) && fabsf(la) < (v->type == VEH_CAR ? 1.2f : 0.7f) && sp > 4.0f) {
            p->state = PED_DOWN; p->timer = 2.8f; p->h = v->h;
            game_sfx(g, SFX_SCREAM, p->x, p->z);
            if (i == g->p.veh) {
                game_message(g, "You hit someone! The police are after you!", 2.0f);
                game_add_wanted(g, 1);
            }
            return;
        }
        if (al > 0 && al < 9.0f && fabsf(la) < 2.5f && p->state != PED_DODGE) {
            p->state = PED_DODGE; p->timer = 0.8f;
            p->h = wrap_angle(v->h + (la > 0 ? -PI_F * 0.5f : PI_F * 0.5f));
        }
    }
    if (p->state == PED_DODGE || p->state == PED_FLEE) {
        float sp = p->state == PED_DODGE ? 4.5f : 3.5f;
        V3 f;
        float nx, nz;
        if (p->state == PED_FLEE) {
            float dx = p->x - px, dz = p->z - pz;
            p->h = approach_angle(p->h, atan2f(dx, dz), 4.0f * dt);
        }
        f = fwd_of(p->h);
        p->x += f.x * sp * dt; p->z += f.z * sp * dt;
        world_push_circle(g->w, &p->x, &p->z, 0.35f, p->y, &nx, &nz);
        p->anim += sp * dt * 2.4f;
        p->timer -= dt;
        if (p->timer <= 0) {
            /* terug naar dichtstbijzijnde stoep-lus */
            p->state = PED_WALK;
            p->bi = (int)clampf(floorf((p->x - ROADW) / PITCH), 0, NB - 1);
            p->bj = (int)clampf(floorf((p->z - ROADW) / PITCH), 0, NB - 1);
        }
        p->y = world_ground_y(g->w, p->x, p->z);
        return;
    }
    if (p->state == PED_IDLE) {
        p->timer -= dt;
        if (p->timer <= 0) p->state = PED_WALK;
        return;
    }
    {
        float tx, tz, dx, dz, d;
        ped_corner(p->bi, p->bj, p->corner, &tx, &tz);
        dx = tx - p->x; dz = tz - p->z; d = sqrtf(dx * dx + dz * dz);
        if (d < 0.4f) {
            p->corner = (p->corner + p->dsign + 4) & 3;
            if (rng_f(&g->rng) < 0.2f) { p->state = PED_IDLE; p->timer = rng_range(&g->rng, 1.0f, 3.0f); }
            return;
        }
        p->h = approach_angle(p->h, atan2f(dx, dz), 5.0f * dt);
        {
            float step = fminf(p->speed * dt, d);
            p->x += dx / d * step; p->z += dz / d * step;
        }
        p->anim += p->speed * dt * 2.6f;
        p->y = world_ground_y(g->w, p->x, p->z);
    }
}

/* ------------------------------------------------------------------ init */

void game_set_quality(Game *g, int q) {
    static const int peds[3] = {6, 10, 14}, traf[3] = {4, 7, 10};
    static const float dd[3] = {95.0f, 135.0f, 180.0f};
    int i;
    if (q < 0) q = 0; if (q > 2) q = 2;
    g->quality = q;
    g->wantPeds = peds[q];
    g->wantTraffic = traf[q];
    g->drawDist = dd[q];
    if (g->nl) { g->wantPeds = 0; g->wantTraffic = 0; g->drawDist = dd[q] + 40.0f; }
    for (i = g->wantPeds; i < MAX_PED - MAX_COPS; i++) g->ped[i].active = 0;
    for (i = 0; i < MAX_TRAFFIC; i++) {
        Vehicle *v = &g->veh[g->firstTraffic + i];
        if (i >= g->wantTraffic && v->traffic && (g->firstTraffic + i) != g->p.veh) v->active = 0;
    }
}

void game_init(Game *g, World *w) {
    int i;
    memset(g, 0, sizeof(*g));
    g->w = w;
    g->rng.s = 20261118u;
    g->mission = -1;
    g->voReq = -1; g->voLine = -1;
    g->camFov = 58.0f;
    g->nl = w->real;
    g->autoAim = 1;
    g->p.x = w->poi[POI_SPAWN].x;
    g->p.z = w->poi[POI_SPAWN].z;
    g->p.h = w->spawn_heading;
    g->p.y = world_ground_y(w, g->p.x, g->p.z);
    g->p.veh = -1;
    g->stamina = 100.0f;   /* (na het laden van de training zet de frontend dit op het maximum) */
    g->p.onGround = 1;
    g->camYaw = g->p.h;
    g->camPitch = 0.32f;
    g->camDist = 5.0f;
    g->camSens = 1.0f;
    for (i = 0; i < w->npark && g->nveh < MAX_VEH - MAX_TRAFFIC - MAX_POLICE - MAX_OWNED; i++) {
        Vehicle *v;
        int k = i;
        /* de Giveaway Bike (fiets, variant 0) altijd als eerste */
        if (i == 0) {
            int j;
            for (j = 0; j < w->npark; j++) if (w->park[j].type == VEH_BIKE && w->park[j].variant == 0) { k = j; break; }
        } else {
            k = i;
            if (w->park[k].type == VEH_BIKE && w->park[k].variant == 0) k = 0;
            if (k == 0 && w->park[0].type == VEH_BIKE && w->park[0].variant == 0) continue;
        }
        v = &g->veh[g->nveh++];
        memset(v, 0, sizeof(*v));
        v->active = 1;
        v->garageSlot = -1;
        v->type = w->park[k].type; v->variant = w->park[k].variant;
        v->x = w->park[k].x; v->z = w->park[k].z; v->h = w->park[k].h;
        v->y = world_ground_y(w, v->x, v->z);
    }
    g->firstTraffic = g->nveh;
    for (i = 0; i < MAX_TRAFFIC; i++) { memset(&g->veh[g->nveh], 0, sizeof(Vehicle)); g->nveh++; }
    g->firstPolice = g->nveh;
    for (i = 0; i < MAX_POLICE; i++) { memset(&g->veh[g->nveh], 0, sizeof(Vehicle)); g->nveh++; }
    g->firstOwned = g->nveh;
    for (i = 0; i < MAX_OWNED; i++) { memset(&g->veh[g->nveh], 0, sizeof(Vehicle)); g->veh[g->nveh].garageSlot = -1; g->nveh++; }
    g->health = 100.0f;
    setup_missions(g);
    game_set_quality(g, 1);
    g->camPos = v3(g->p.x, 3, g->p.z + 5);
}

void game_teleport(Game *g, float x, float z, float h) {
    if (g->p.veh >= 0) {
        Vehicle *v = &g->veh[g->p.veh];
        v->x = x; v->z = z; v->h = h; v->y = world_ground_y(g->w, x, z);
    }
    g->p.x = x; g->p.z = z; g->p.h = h; g->p.y = world_ground_y(g->w, x, z);
    g->camYaw = h;
}


/* ------------------------------------------------------------------ eigen huis */

int game_home_inside(const Game *g) {
    float x = g->p.veh >= 0 ? g->veh[g->p.veh].x : g->p.x, z = g->p.veh >= 0 ? g->veh[g->p.veh].z : g->p.z;
    float a, b, c, d;
    if (g->nl) return 0;
    return home_room_at(g->homeLvl, g->garLvl, x, z, &a, &b, &c, &d);
}

int game_home_upgrade(Game *g, int house) {
    int *lvl = house ? &g->homeLvl : &g->garLvl;
    int price = home_upgrade_price(house, *lvl);
    if (price < 0) return -1;
    if (g->money < price) return 0;
    g->money -= price;
    (*lvl)++;
    g->moneyFlash = 1.0f;
    game_sfx(g, SFX_BUY, g->p.x, g->p.z);
    if (!house) game_spawn_owned(g);
    else if (game_home_inside(g) == 1) {   /* nieuwe kamer: bij de voordeur binnen beginnen */
        float x0, z0, x1, z1;
        home_room(g->homeLvl, &x0, &z0, &x1, &z1);
        game_teleport(g, x1 - 2.2f, z1 - 1.8f, -PI_F * 0.5f);
    }
    return 1;
}

static void home_go(Game *g, float x, float z, float h, const char *msg) {
    if (g->p.veh >= 0) g->veh[g->p.veh].speed *= 0.3f;
    game_teleport(g, x, z, h);
    g->camPos = v3(x - sinf(h) * 3.0f, 2.2f, z - cosf(h) * 3.0f);
    g->homeCool = 0.8f;
    if (msg) game_message(g, msg, 2.0f);
}

static void home_action(Game *g, int kind) {
    float x0, z0, x1, z1, dx, dz;
    int foot = g->p.veh < 0;
    switch (kind) {
    case HM_FRONTDOOR:
        if (!foot) break;
        if (g->wanted > 0) { game_message(g, "The police are watching - lose them first!", 2.0f); break; }
        home_room(g->homeLvl, &x0, &z0, &x1, &z1);
        home_go(g, x0 + 2.0f, (z0 + z1) * 0.5f, PI_F * 0.5f, "Home sweet home");
        vo_bark(g, VB_HOME, 0.6f);
        break;
    case HM_RAMP:
        if (g->wanted > 0) { game_message(g, "The police are watching - lose them first!", 2.0f); break; }
        home_garage_room(g->garLvl, &x0, &z0, &x1, &z1);
        if (foot) home_go(g, x1 - 4.0f, z1 - 1.6f, -PI_F * 0.5f, "Your garage");
        else home_go(g, x1 - 6.5f, z1 - 3.5f, -PI_F * 0.5f, "Your garage - park on a free spot");
        break;
    case HM_EXIT:
        if (!foot) break;
        home_door(g->homeLvl, &dx, &dz);
        home_go(g, dx - 1.2f, dz, -PI_F * 0.5f, NULL);
        break;
    case HM_LAPTOP:
        if (foot) g->homeMenuOpen = 1;
        break;
    case HM_BED:
        if (!foot) break;
        g->health = 100.0f;
        g->stamina = game_stamina_max(g); g->tired = 0;
        game_message(g, "You took a nap. Health restored!", 2.5f);
        game_sfx(g, SFX_CASH, g->p.x, g->p.z);
        break;
    case HM_TOGARAGE:
        if (!foot) break;
        home_garage_room(g->garLvl, &x0, &z0, &x1, &z1);
        home_go(g, x0 + 2.6f, z1 - 1.5f, PI_F * 0.5f, "Your garage");
        break;
    case HM_GAR_EXIT:
        home_go(g, HOME_RAMP_X - 4.8f, HOME_RAMP_Z, -PI_F * 0.5f, NULL);
        break;
    case HM_GAR_HOUSE:
        if (!foot) break;
        home_room(g->homeLvl, &x0, &z0, &x1, &z1);
        home_go(g, x0 + 1.6f, z0 + 2.2f, 0.0f, NULL);
        break;
    default: break;
    }
}

static void home_tick(Game *g, float dt) {
    V3 pos[12];
    int kind[12], n, i, cur = HM_NONE;
    float rad[12], px, pz;
    if (g->nl) return;
    home_set_decor(g->decor);
    if (g->homeCool > 0) g->homeCool -= dt;
    px = g->p.veh >= 0 ? g->veh[g->p.veh].x : g->p.x;
    pz = g->p.veh >= 0 ? g->veh[g->p.veh].z : g->p.z;
    n = home_markers(g->homeLvl, g->garLvl, pos, kind, rad, 12);
    for (i = 0; i < n; i++) {
        float dx = px - pos[i].x, dz = pz - pos[i].z;
        if (dx * dx + dz * dz < rad[i] * rad[i]) { cur = kind[i]; break; }
    }
    if (cur != g->homeMarker) {
        g->homeMarker = cur;
        if (cur != HM_NONE && g->homeCool <= 0) home_action(g, cur);
    }
}

/* ------------------------------------------------------------------ v4.0: winkels van binnen */

int game_store_inside(const Game *g) {
    if (g->nl || g->p.veh >= 0) return 0;
    return store_at(g->p.x, g->p.z);
}

static void store_go(Game *g, float x, float z, float h) {
    game_teleport(g, x, z, h);
    g->camPos = v3(x - sinf(h) * 3.0f, 2.2f, z - cosf(h) * 3.0f);
    g->p.speed = 0;
    g->storeCool = 0.8f;
}

void game_store_enter(Game *g, int s) {
    float x, z, h;
    char b[96];
    store_entry(s, &x, &z, &h);
    store_go(g, x, z, h);
    g->storeMarker = SM_NONE;
    {
        static const int greet[STORE_COUNT] = {VB_STORE_GUN, VB_STORE_BURGER, VB_STORE_GYM, VB_STORE_DRIP, VB_STORE_BLING, VB_STORE_MART};
        vo_say(g, vo_pick(g, greet[s]), VP_MISSION);
    }
    if (s == STORE_MART) {
        g->clerkScare = 0;
        if (g->robCool > 0) {
            int t = (int)ceilf(g->robCool);
            snprintf(b, sizeof(b), "GIVEAWAY MART - the register refills in %d:%02d", t / 60, t % 60);
            game_message(g, b, 3.0f);
        } else game_message(g, "GIVEAWAY MART - hold a gun at the register to rob it ($10,000)", 3.0f);
    } else {
        snprintf(b, sizeof(b), "%s - walk to the counter", store_name(s));
        game_message(g, b, 2.0f);
    }
}

/* naar buiten: net voor de deur op de stoep, met je rug naar de winkel */
static void store_leave(Game *g, int s) {
    V3 d = g->w->poi[store_poi(s)];
    float side = fmodf(d.z - ROADW, PITCH) < BLOCKW * 0.5f ? -1.0f : 1.0f;
    store_go(g, d.x, d.z + side * 2.3f, side < 0 ? PI_F : 0.0f);
    g->inShop = 0; g->inBiz = 0;
}

/* announce = 1: net op de kassa gestapt (meldingen tonen); 0: je staat er al (alleen overvallen als het nu kan) */
static void store_rob(Game *g, int announce) {
    float x, z, h;
    char b[96];
    if (g->robCool > 0) {
        if (announce) {
            int t = (int)ceilf(g->robCool);
            snprintf(b, sizeof(b), "The register is empty - it refills in %d:%02d", t / 60, t % 60);
            game_message(g, b, 2.5f);
            vo_say(g, vo_pick(g, VB_ROB_EMPTY), VP_MISSION);
        }
        return;
    }
    if (g->weapon == WP_NONE || g->p.veh >= 0) {   /* v5.2: zonder wapen in je hand lacht de kassamedewerker je uit */
        if (announce) game_message(g, "You need a gun in your hand to rob the store!", 2.5f);
        return;
    }
    vo_say(g, vo_pick(g, VB_ROB_YOU), VP_MISSION);
    vo_queue(g, vo_pick(g, VB_ROB_CLERK), VP_MISSION);
    g->robCool = MART_COOLDOWN;
    g->clerkScare = 6.0f;
    g->money += MART_LOOT;
    g->moneyFlash = 1.5f;
    g->statRobs++;
    store_clerk(STORE_MART, &x, &z, &h);
    game_sfx(g, SFX_CASH, x, z);
    g->crimeCool = 0;
    game_add_wanted(g, 2);
    banner(g, "STORE ROBBED!", "+$10,000 - get out and lose the cops", 1, 3.0f);
}

static void store_tick(Game *g, float dt) {
    V3 pos[4];
    int kind[4], n, i, s, cur = SM_NONE;
    float rad[4];
    if (g->nl) return;
    if (g->storeCool > 0) g->storeCool -= dt;
    if (g->clerkScare > 0) g->clerkScare -= dt;
    s = game_store_inside(g);
    if (!s) {
        /* in de stad: door een winkeldeur lopen = naar binnen */
        g->storeMarker = SM_NONE;
        if (g->p.veh >= 0 || g->storeCool > 0) return;
        for (i = 0; i < STORE_COUNT; i++) {
            V3 d = g->w->poi[store_poi(i)];
            float dx = g->p.x - d.x, dz = g->p.z - d.z;
            if (dx * dx + dz * dz < 1.3f * 1.3f) { game_store_enter(g, i); return; }
        }
        return;
    }
    s--;
    if (g->storeCool > 0) return;
    n = store_markers(s, pos, kind, rad, 4);
    for (i = 0; i < n; i++) {
        float dx = g->p.x - pos[i].x, dz = g->p.z - pos[i].z;
        if (dx * dx + dz * dz < rad[i] * rad[i]) { cur = kind[i]; break; }
    }
    if (cur == SM_ROB && g->storeMarker == SM_ROB) store_rob(g, 0);
    if (cur != g->storeMarker) {
        g->storeMarker = cur;
        if (cur == SM_NONE) return;
        if (cur == SM_EXIT) store_leave(g, s);
        else if (cur == SM_ROB) store_rob(g, 1);
        else if (cur == SM_COUNTER) {
            if (s == STORE_GUN) g->shopOpen = 1;
            else g->bizOpen = s - STORE_BURGER + BIZ_BURGER + 1;
        }
    }
}

/* ------------------------------------------------------------------ update */

static void update_player_foot(Game *g, const Input *in, float dt) {
    Player *p = &g->p;
    V3 f = fwd_of(g->camYaw), r = right_of(g->camYaw);
    float mx = in->mx, my = in->my;
    float mag = sqrtf(mx * mx + my * my);
    float ground, nx, nz;
    int i;
    if (mag > 1) { mx /= mag; my /= mag; mag = 1; }
    /* rennen kost uithoudingsvermogen; leeg = even uitgeput tot 30 */
    g->running = in->run && mag > 0.3f && !g->tired && g->stamina > 0;
    if (g->boostT > 0) g->boostT -= dt;
    if (g->running) {
        if (g->boostT <= 0) g->stamina -= 20.0f * dt;
        g->staminaCool = 0.9f;
        if (g->stamina <= 0) { g->stamina = 0; g->tired = 1; game_message(g, "Out of breath!", 1.2f); }
    } else if (g->staminaCool > 0) g->staminaCool -= dt;
    else g->stamina = fminf(game_stamina_max(g), g->stamina + (mag > 0.3f ? 11.0f : 22.0f) * dt);
    if (g->tired && g->stamina >= 30.0f) g->tired = 0;
    if (mag > 0.12f) {
        V3 d = v3add(v3scale(f, my), v3scale(r, mx));
        p->h = approach_angle(p->h, atan2f(d.x, d.z), 11.0f * dt);
        p->speed = approachf(p->speed, mag * (g->running ? 7.4f : (g->tired ? 3.2f : 4.3f)), 22.0f * dt);
    } else {
        p->speed = approachf(p->speed, 0, 25.0f * dt);
    }
    {
        V3 hf = fwd_of(p->h);
        p->x += hf.x * p->speed * dt;
        p->z += hf.z * p->speed * dt;
    }
    if (in->jumpPressed && p->onGround) { p->vy = 6.0f; p->onGround = 0; g->statJumps++; game_sfx(g, SFX_JUMP, p->x, p->z); }
    p->vy -= 16.0f * dt;
    p->y += p->vy * dt;
    ground = world_ground_y(g->w, p->x, p->z);
    if (p->y <= ground) { p->y = ground; p->vy = 0; p->onGround = 1; }
    else if (p->y > ground + 0.05f) p->onGround = 0;
    world_push_circle(g->w, &p->x, &p->z, 0.35f, p->y, &nx, &nz);
    if (!g->nl) { home_push(g->homeLvl, g->garLvl, &p->x, &p->z, 0.35f); store_push(&p->x, &p->z, 0.35f); }
    for (i = 0; i < g->nveh; i++) {
        Vehicle *v = &g->veh[i];
        float dx, dz, d2, rr;
        if (!v->active) continue;
        dx = p->x - v->x; dz = p->z - v->z; d2 = dx * dx + dz * dz;
        rr = 0.35f + (v->type == VEH_BIKE ? 0.35f : 1.0f);
        if (v->type == VEH_CAR) {
            /* auto als twee cirkels */
            V3 vf = fwd_of(v->h);
            int k;
            for (k = -1; k <= 1; k += 2) {
                float cx = v->x + vf.x * 1.1f * k, cz = v->z + vf.z * 1.1f * k;
                dx = p->x - cx; dz = p->z - cz; d2 = dx * dx + dz * dz;
                if (d2 < rr * rr && d2 > 1e-6f) { float d = sqrtf(d2); p->x += dx / d * (rr - d); p->z += dz / d * (rr - d); }
            }
        } else if (d2 < rr * rr && d2 > 1e-6f) {
            float d = sqrtf(d2); p->x += dx / d * (rr - d); p->z += dz / d * (rr - d);
        }
    }
    for (i = 0; i < MAX_PED; i++) {
        Ped *q = &g->ped[i];
        float dx, dz, d2;
        if (!q->active || q->state == PED_DOWN) continue;
        dx = p->x - q->x; dz = p->z - q->z; d2 = dx * dx + dz * dz;
        if (d2 < 0.6f * 0.6f && d2 > 1e-6f) { float d = sqrtf(d2); p->x += dx / d * (0.6f - d) * 0.5f; p->z += dz / d * (0.6f - d) * 0.5f; q->x -= dx / d * (0.6f - d) * 0.5f; q->z -= dz / d * (0.6f - d) * 0.5f; }
    }
    p->anim += p->speed * dt * 2.3f;
}

static void update_camera(Game *g, const Input *in, float dt) {
    int mode = game_mode(g);
    V3 target;
    float wantDist = mode == MODE_CAR ? 8.0f : (mode == MODE_BIKE ? 5.8f : 4.6f);
    int indoors = !g->nl && game_home_inside(g);
    int inStore = game_store_inside(g);
    if (indoors || inStore) wantDist = mode == MODE_CAR ? 6.0f : 3.3f;
    float lx = in->lx, ly = in->ly;
    if (mode == MODE_FOOT) target = v3(g->p.x, g->p.y + 1.55f, g->p.z);
    else {
        Vehicle *v = &g->veh[g->p.veh];
        target = v3(v->x, v->y + (mode == MODE_CAR ? 1.5f : 1.6f), v->z);
    }
    g->camYaw = wrap_angle(g->camYaw - lx * 2.6f * g->camSens * dt);
    g->camPitch = clampf(g->camPitch + ly * 1.5f * g->camSens * dt * (g->invertY ? 1.0f : -1.0f), -0.15f, 1.15f);
    if (fabsf(lx) > 0.08f || fabsf(ly) > 0.08f) g->camIdle = 0; else g->camIdle += dt;
    if (mode != MODE_FOOT && g->camIdle > 0.8f) {
        Vehicle *v = &g->veh[g->p.veh];
        if (fabsf(v->speed) > 1.5f) {
            float behind = v->speed >= 0 ? v->h : wrap_angle(v->h + PI_F);
            g->camYaw = approach_angle(g->camYaw, behind, 2.2f * dt * clampf(fabsf(v->speed) / 6.0f, 0.3f, 1.5f));
            g->camPitch = approachf(g->camPitch, 0.28f, 0.5f * dt);
        }
    }
    g->camDist = lerpf(g->camDist, wantDist, clampf(dt * 3.0f, 0, 1));
    {
        V3 f = fwd_of(g->camYaw);
        float cp = cosf(g->camPitch), sp = sinf(g->camPitch);
        V3 want = v3(target.x - f.x * g->camDist * cp, target.y + g->camDist * sp, target.z - f.z * g->camDist * cp);
        float t = world_raycast(g->w, target, want);
        float gy;
        if (t < 1.0f) {
            float tt = fmaxf(0.08f, t - 0.6f / (g->camDist + 0.01f));
            want = v3lerp(target, want, tt);
        }
        gy = world_ground_y(g->w, want.x, want.z) + 0.35f;
        if (want.y < gy) want.y = gy;
        if (indoors) {
            float x0, z0, x1, z1;
            home_room_at(g->homeLvl, g->garLvl, target.x, target.z, &x0, &z0, &x1, &z1);
            want.x = clampf(want.x, x0 + 0.3f, x1 - 0.3f);
            want.z = clampf(want.z, z0 + 0.3f, z1 - 0.3f);
            want.y = fminf(want.y, 2.75f);
        } else if (inStore) {
            float x0, z0, x1, z1;
            store_room(inStore - 1, &x0, &z0, &x1, &z1);
            want.x = clampf(want.x, x0 + 0.3f, x1 - 0.3f);
            want.z = clampf(want.z, z0 + 0.3f, z1 - 0.3f);
            want.y = fminf(want.y, STORE_H - 0.5f);
        }
        {   /* v6.0 GTA IV-achtige camera: zacht verende volgcamera, wijdere blik en een klein beetje trilling bij hoge snelheid */
            float sp = mode == MODE_FOOT ? 0.0f : fabsf(g->veh[g->p.veh].speed);
            float fov = 58.0f + (mode == MODE_FOOT ? 0.0f : clampf(sp / 22.0f, 0, 1) * 11.0f);
            g->camFov = approachf(g->camFov > 1.0f ? g->camFov : 58.0f, fov, 12.0f * dt);
            if (!indoors && !inStore && !g->cut && v3len(v3sub(want, g->camPos)) < 10.0f) {
                float k = 1.0f - expf(-dt * (mode == MODE_FOOT ? 16.0f : 10.0f));
                want = v3lerp(g->camPos, want, k);
                if (sp > 14.0f) want.y += sinf(g->time * 37.0f) * 0.012f * clampf((sp - 14.0f) / 10.0f, 0, 1);
            }
        }
        g->camPos = want;
        g->camTarget = target;
    }
}

/* ------------------------------------------------------------------ wapens, schieten, agenten te voet */

const WeaponDef WEAPONS[WP_COUNT] = {
    {"FISTS", 0, 0, 0, 0, 0.5f, 0, 0, 0},
    {"PISTOL", 250, 40, 48, 34, 0.32f, 38, 0.10f, 1},
    {"SMG", 900, 80, 120, 18, 0.09f, 34, 0.16f, 1},
    {"SHOTGUN", 700, 60, 24, 22, 0.85f, 18, 0.22f, 6},
};

int game_has_weapon(const Game *g) {
    int i;
    for (i = 1; i < WP_COUNT; i++) if (g->owned[i]) return 1;
    return 0;
}

/* winkel: 0..2 wapen, 3 kogels voor alle wapens, 4 kogelwerend vest */
int game_buy(Game *g, int item) {
    int price;
    if (item >= 0 && item <= 2) {
        int w = item + 1;
        price = g->owned[w] ? WEAPONS[w].ammoPrice : WEAPONS[w].price;
        if (g->money < price) return 0;
        g->money -= price;
        g->owned[w] = 1;
        g->ammo[w] += WEAPONS[w].ammoPack;
        g->weapon = w;
        return 1;
    }
    if (item == 3) {
        int w, any = 0;
        price = 0;
        for (w = 1; w < WP_COUNT; w++) if (g->owned[w]) { price += WEAPONS[w].ammoPrice; any = 1; }
        if (!any || g->money < price) return 0;
        g->money -= price;
        for (w = 1; w < WP_COUNT; w++) if (g->owned[w]) g->ammo[w] += WEAPONS[w].ammoPack;
        return 1;
    }
    if (item == 4) {
        if (g->armor >= 99.0f || g->money < 300) return 0;
        g->money -= 300;
        g->armor = 100.0f;
        return 1;
    }
    return 0;
}

static void add_tracer(Game *g, V3 a, V3 b) {
    int k = g->trNext;
    g->tracer[k].a = a; g->tracer[k].b = b; g->tracer[k].t = 0.08f;
    g->trNext = (k + 1) % MAX_TRACERS;
}

static void peds_panic(Game *g, float x, float z) {
    int i;
    for (i = 0; i < MAX_PED - MAX_COPS; i++) {
        Ped *p = &g->ped[i];
        float dx, dz;
        if (!p->active || p->state == PED_DOWN) continue;
        dx = p->x - x; dz = p->z - z;
        if (dx * dx + dz * dz < 28.0f * 28.0f) {
            if (p->state != PED_FLEE && g->screamCool <= 0) { game_sfx(g, SFX_SCREAM, p->x, p->z); g->screamCool = 2.5f; vo_bark(g, VB_SHOOT, 0.3f); }
            p->state = PED_FLEE; p->timer = 5.0f; p->h = atan2f(dx, dz);
        }
    }
}

static void ped_hit(Game *g, Ped *p, float dmg) {
    p->hp -= dmg;
    if (p->hp > 0) return;
    game_sfx(g, SFX_SCREAM, p->x, p->z);
    p->state = PED_DOWN;
    p->shot = 1;
    p->timer = p->cop ? 15.0f : 25.0f;
    g->statHits++;
    if (g->crimeCool > 0) g->crimeCool = 0;
    game_add_wanted(g, 1);
}

static void spawn_cops(Game *g) {
    int k, n = 0;
    for (k = MAX_PED - MAX_COPS; k < MAX_PED; k++) if (g->ped[k].active) n++;
    if (g->wanted < 2 || n >= MAX_COPS) return;
    /* versterking te voet als de auto's je niet bereiken (bijv. achter de bank) */
    /* niet terwijl je ontsnapt (sterren knipperen): anders verschijnt er steeds een agent naast je
     * en begint de ontsnap-timer telkens opnieuw */
    if (n < 2 && g->evadeT < 0.5f && g->time - g->lastCopSpawn > 6.0f) {
        int c;
        g->lastCopSpawn = g->time;
        for (c = 0; c < 2; c++) {
            int s, tries;
            for (s = MAX_PED - MAX_COPS; s < MAX_PED && g->ped[s].active; s++) {}
            if (s >= MAX_PED) break;
            for (tries = 0; tries < 12; tries++) {
                float a = rng_f(&g->rng) * 2 * PI_F, x = g->p.x + cosf(a) * 28.0f, z = g->p.z + sinf(a) * 28.0f, nx, nz;
                world_push_circle(g->w, &x, &z, 0.5f, 0, &nx, &nz);
                if (world_raycast(g->w, v3(x, 1.5f, z), v3(x, 1.5f, z)) < 0.5f) continue;
                {
                    Ped *p = &g->ped[s];
                    memset(p, 0, sizeof(*p));
                    p->active = 1; p->cop = 1; p->variant = COP_VARIANT;
                    p->x = x; p->z = z; p->y = world_ground_y(g->w, x, z);
                    p->hp = 70; p->speed = 4.2f; p->shootCool = 1.5f + c;
                    p->h = atan2f(g->p.x - x, g->p.z - z);
                    n++;
                }
                break;
            }
        }
    }
    for (k = 0; k < MAX_POLICE && n < MAX_COPS; k++) {
        Vehicle *v = &g->veh[g->firstPolice + k];
        float dx, dz;
        int c, s;
        if (!v->active || v->copsOut || v->wrecked) continue;
        dx = v->x - g->p.x; dz = v->z - g->p.z;
        if (dx * dx + dz * dz > 32.0f * 32.0f || fabsf(v->speed) > 3.0f) continue;
        v->copsOut = 1;
        for (c = 0; c < 2 && n < MAX_COPS; c++) {
            V3 r = right_of(v->h);
            for (s = MAX_PED - MAX_COPS; s < MAX_PED && g->ped[s].active; s++) {}
            if (s >= MAX_PED) break;
            {
                Ped *p = &g->ped[s];
                float side = c ? 1.8f : -1.8f;
                memset(p, 0, sizeof(*p));
                p->active = 1; p->cop = 1; p->variant = COP_VARIANT;
                p->x = v->x + r.x * side; p->z = v->z + r.z * side;
                p->y = world_ground_y(g->w, p->x, p->z);
                p->hp = 70; p->speed = 4.0f; p->shootCool = 1.0f + c * 0.5f;
                p->h = v->h;
                n++;
            }
        }
    }
}

static int line_of_sight(Game *g, V3 a, V3 b) { return world_raycast(g->w, a, b) >= 0.98f; }

static void update_cop(Game *g, Ped *p, float dt) {
    float px, pz, dx, dz, d;
    V3 eye, tgt;
    player_pos(g, &px, &pz);
    dx = px - p->x; dz = pz - p->z; d = sqrtf(dx * dx + dz * dz);
    if (g->wanted <= 0) {           /* weer weg */
        p->timer += dt;
        p->h = approach_angle(p->h, atan2f(-dx, -dz), 4.0f * dt);
        p->x -= dx / (d + 0.01f) * 2.0f * dt; p->z -= dz / (d + 0.01f) * 2.0f * dt;
        p->anim += 4.0f * dt;
        if (p->timer > 6.0f || d > 80.0f) p->active = 0;
        return;
    }
    if (d > 120.0f) { p->active = 0; return; }
    eye = v3(p->x, p->y + 1.5f, p->z);
    tgt = v3(px, g->p.y + 1.2f, pz);
    p->h = approach_angle(p->h, atan2f(dx, dz), 6.0f * dt);
    if (d > 11.0f || (!line_of_sight(g, eye, tgt) && d > 6.0f)) {
        float nx, nz;
        p->x += dx / d * p->speed * dt; p->z += dz / d * p->speed * dt;
        world_push_circle(g->w, &p->x, &p->z, 0.35f, p->y, &nx, &nz);
        p->anim += p->speed * dt * 2.4f;
        p->state = PED_WALK;
    } else {
        p->state = PED_IDLE;
        p->shootCool -= dt;
        if (p->shootCool <= 0) {
            float chance = 0.36f;
            p->shootCool = 1.1f + rng_f(&g->rng) * 0.7f;
            if (g->p.veh >= 0 && fabsf(g->veh[g->p.veh].speed) > 6.0f) chance *= 0.5f;
            if (rng_f(&g->rng) < chance) {
                game_damage(g, 7.0f);
                add_tracer(g, v3(p->x, p->y + 1.4f, p->z), tgt);
            } else {
                add_tracer(g, v3(p->x, p->y + 1.4f, p->z), v3(tgt.x + rng_range(&g->rng, -1.5f, 1.5f), tgt.y + rng_range(&g->rng, -0.8f, 1.0f), tgt.z + rng_range(&g->rng, -1.5f, 1.5f)));
            }
            g->copShots++;
            game_sfx(g, SFX_COPSHOT, p->x, p->z);
        }
    }
    p->y = world_ground_y(g->w, p->x, p->z);
}

static void update_weapons(Game *g, const Input *in, float dt) {
    int k;
    const WeaponDef *wd;
    if (g->fireCool > 0) g->fireCool -= dt;
    if (g->muzzleT > 0) g->muzzleT -= dt;
    for (k = 0; k < MAX_TRACERS; k++) if (g->tracer[k].t > 0) g->tracer[k].t -= dt;
    if (in->switchPressed) {
        int w = g->weapon, tries;
        for (tries = 0; tries < WP_COUNT; tries++) {
            w = (w + 1) % WP_COUNT;
            if (w == WP_NONE || g->owned[w]) break;
        }
        g->weapon = w;
        game_message(g, WEAPONS[w].name, 1.0f);
    }
    if (g->p.veh >= 0 || g->weapon == WP_NONE || !in->fire || g->fireCool > 0) return;
    wd = &WEAPONS[g->weapon];
    if (g->ammo[g->weapon] <= 0) { game_message(g, "Out of ammo! Buy more at BLOCK ARMS.", 1.5f); g->fireCool = 0.6f; game_sfx(g, SFX_EMPTY, g->p.x, g->p.z); return; }
    g->ammo[g->weapon]--;
    g->fireCool = wd->delay;
    g->muzzleT = 0.06f;
    g->statShots++;
    game_sfx(g, g->weapon == WP_SMG ? SFX_SMG : (g->weapon == WP_SHOTGUN ? SFX_SHOTGUN : SFX_PISTOL), g->p.x, g->p.z);
    {
        /* Richten. Auto-aim AAN: eerst politie (agenten en politieauto's) in de kijkrichting, pas als die er
         * niet is gewone mensen. Auto-aim UIT: alleen wat precies in de schietlijn van de camera staat. */
        V3 cf = fwd_of(g->vrAim ? g->vrAimYaw : g->camYaw), muzzle = g->vrAim ? g->vrMuzzle : v3(g->p.x, g->p.y + 1.35f, g->p.z), aim = v3(0, 0, 0);
        float best = 1e9f;
        int bestPed = -1, bestVeh = -1, i, pel, pass, npass = g->autoAim ? 2 : 1;
        for (pass = 0; pass < npass && bestPed < 0 && bestVeh < 0; pass++) {
            int wantCops = g->autoAim ? (pass == 0) : -1;   /* -1 = alles */
            for (i = 0; i < MAX_PED; i++) {
                Ped *p = &g->ped[i];
                float dx, dz, d, score;
                if (!p->active || p->state == PED_DOWN) continue;
                if (wantCops == 1 && !p->cop) continue;
                if (wantCops == 0 && p->cop) continue;
                dx = p->x - g->p.x; dz = p->z - g->p.z; d = sqrtf(dx * dx + dz * dz);
                if (d > wd->range || d < 0.3f) continue;
                if (g->autoAim) {
                    float ang = acosf(clampf((dx * cf.x + dz * cf.z) / d, -1, 1));
                    if (ang > 0.55f) continue;
                    score = ang * 10.0f + d * 0.1f;
                } else {
                    float along = dx * cf.x + dz * cf.z, lat = fabsf(dx * cf.z - dz * cf.x);
                    if (along <= 0 || lat > 0.6f) continue;
                    score = along;
                }
                if (!line_of_sight(g, muzzle, v3(p->x, p->y + 1.2f, p->z))) continue;
                if (score < best) { best = score; bestPed = i; bestVeh = -1; }
            }
            if (wantCops == 0) continue;
            for (i = 0; i < MAX_POLICE; i++) {
                Vehicle *v = &g->veh[g->firstPolice + i];
                float dx, dz, d, score;
                if (!v->active || v->wrecked) continue;
                dx = v->x - g->p.x; dz = v->z - g->p.z; d = sqrtf(dx * dx + dz * dz);
                if (d > wd->range || d < 0.5f) continue;
                if (g->autoAim) {
                    float ang = acosf(clampf((dx * cf.x + dz * cf.z) / d, -1, 1));
                    if (ang > 0.45f) continue;
                    score = ang * 10.0f + d * 0.1f + 1.0f;
                } else {
                    float along = dx * cf.x + dz * cf.z, lat = fabsf(dx * cf.z - dz * cf.x);
                    if (along <= 0 || lat > 1.2f) continue;
                    score = along;
                }
                if (score < best) { best = score; bestVeh = g->firstPolice + i; bestPed = -1; }
            }
        }
        /* andere spelers (multiplayer): als er geen politie gekozen is, gaan zij voor gewone mensen */
        {
            int bestRem = -1, onlyIfNoCop = !(bestVeh >= 0 || (bestPed >= 0 && g->ped[bestPed].cop));
            float bestR = 1e9f;
            for (i = 0; i < MAX_REMOTE && onlyIfNoCop; i++) {
                RemotePlayer *rp = &g->remote[i];
                float dx, dz, d, score;
                if (!rp->active || rp->health <= 0) continue;
                dx = rp->x - g->p.x; dz = rp->z - g->p.z; d = sqrtf(dx * dx + dz * dz);
                if (d > wd->range || d < 0.3f) continue;
                if (g->autoAim) {
                    float ang = acosf(clampf((dx * cf.x + dz * cf.z) / d, -1, 1));
                    if (ang > 0.55f) continue;
                    score = ang * 10.0f + d * 0.1f;
                } else {
                    float along = dx * cf.x + dz * cf.z, lat = fabsf(dx * cf.z - dz * cf.x);
                    if (along <= 0 || lat > 0.8f) continue;
                    score = along;
                }
                if (!line_of_sight(g, muzzle, v3(rp->x, rp->y + 1.2f, rp->z))) continue;
                if (score < bestR) { bestR = score; bestRem = i; }
            }
            if (bestRem >= 0 && (bestPed < 0 || (g->autoAim ? bestR <= best + 3.0f : bestR <= best))) {
                RemotePlayer *rp = &g->remote[bestRem];
                aim = v3(rp->x, rp->y + 1.2f, rp->z);
                g->p.h = g->autoAim ? atan2f(aim.x - g->p.x, aim.z - g->p.z) : g->camYaw;
                for (pel = 0; pel < wd->pellets; pel++) {
                    float dist = v3len(v3sub(aim, muzzle)), hitP = 1.0f - wd->spread * (dist / wd->range) * 2.5f;
                    if (rng_f(&g->rng) < hitP && g->nhit < 8) {
                        g->hitq[g->nhit].target = rp->id;
                        g->hitq[g->nhit].dmg = wd->damage * 0.4f;
                        g->nhit++;
                        g->statHits++;
                    }
                    add_tracer(g, v3add(muzzle, v3scale(fwd_of(g->p.h), 0.6f)), aim);
                }
                return;
            }
        }
        if (bestPed >= 0) aim = v3(g->ped[bestPed].x, g->ped[bestPed].y + 1.2f, g->ped[bestPed].z);
        else if (bestVeh >= 0) aim = v3(g->veh[bestVeh].x, 0.9f, g->veh[bestVeh].z);
        else aim = v3add(muzzle, v3scale(cf, wd->range));
        g->p.h = g->autoAim ? atan2f(aim.x - g->p.x, aim.z - g->p.z) : g->camYaw;
        for (pel = 0; pel < wd->pellets; pel++) {
            V3 d = v3norm(v3sub(aim, muzzle));
            float dist = v3len(v3sub(aim, muzzle)), hitP = 1.0f - wd->spread * (dist / wd->range) * 2.5f;
            V3 end;
            int hit = (bestPed >= 0 || bestVeh >= 0) && rng_f(&g->rng) < hitP;
            V3 sd = v3norm(v3add(d, v3(rng_range(&g->rng, -wd->spread, wd->spread), rng_range(&g->rng, -wd->spread, wd->spread) * 0.5f, rng_range(&g->rng, -wd->spread, wd->spread))));
            if (hit) {
                end = aim;
                if (bestPed >= 0) ped_hit(g, &g->ped[bestPed], wd->damage);
                else {
                    Vehicle *v = &g->veh[bestVeh];
                    v->hp -= wd->damage * 0.5f;
                    if (v->hp <= 0 && !v->wrecked) { v->wrecked = 1; v->speed = 0; g->statHits++; }
                    if (g->crimeCool <= 0) game_add_wanted(g, 1);
                }
            } else {
                float t;
                end = v3add(muzzle, v3scale(sd, wd->range));
                t = world_raycast(g->w, muzzle, end);
                end = v3lerp(muzzle, end, t);
            }
            add_tracer(g, v3add(muzzle, v3scale(fwd_of(g->p.h), 0.6f)), end);
        }
        peds_panic(g, g->p.x, g->p.z);
        if (g->wanted == 0 && !g->nl) {
            int i2, near = 0;
            for (i2 = 0; i2 < MAX_PED; i2++) {
                float dx = g->ped[i2].x - g->p.x, dz = g->ped[i2].z - g->p.z;
                if (g->ped[i2].active && dx * dx + dz * dz < 30.0f * 30.0f) near = 1;
            }
            if (near) { game_add_wanted(g, 1); game_message(g, "Shots fired! The police are coming.", 2.0f); }
        }
    }
}

/* ------------------------------------------------------------------ autodealer en BLOCK CUSTOMS */

const CarDef CARS[DEALER_MODELS] = {
    {"CITY HATCH", 1500, 19.0f, 6.5f, 0.95f},
    {"RHINO V8", 6000, 25.0f, 9.5f, 0.90f},
    {"VORTEX GT", 9000, 28.0f, 10.5f, 1.05f},
    {"BOLT X", 20000, 34.0f, 12.5f, 1.10f},
    {"STREET 600", 4500, 27.0f, 11.0f, 1.30f},     /* motor */
    {"DRAGON 1000R", 14000, 36.0f, 14.0f, 1.35f},  /* superbike */
};
const int ENGINE_PRICE[3] = {1000, 2500, 5000};
const int HANDLING_PRICE[3] = {800, 1600, 3200};

float car_top_speed(const Vehicle *v) {
    return CARS[v->model % DEALER_MODELS].maxF * (1.0f + 0.08f * v->eng);
}

static void place_owned(Game *g, int slot, int atDealer) {
    Vehicle *v = &g->veh[g->firstOwned + slot];
    GarageCar *c = &g->garage[slot];
    memset(v, 0, sizeof(*v));
    if (!c->exists) return;
    v->active = 1; v->type = IS_MOTO(c->model) ? VEH_BIKE : VEH_CAR; v->variant = 1; v->custom = 1;
    v->model = c->model; v->paint = c->paint; v->eng = c->eng; v->hand = c->hand; v->neon = c->neon; v->spoiler = c->spoiler;
    v->garageSlot = slot;
    v->x = g->w->dealerSpot[slot].x; v->z = g->w->dealerSpot[slot].z; v->h = g->w->dealerSpotH;
    if (!atDealer && !g->nl) {   /* plekken in de ondergrondse garage van je huis */
        int k, idx = 0;
        for (k = 0; k < slot; k++) if (g->garage[k].exists) idx++;
        if (idx <= g->garLvl) home_garage_spot(g->garLvl, idx, &v->x, &v->z, &v->h);
    }
    v->y = world_ground_y(g->w, v->x, v->z);
}

void game_spawn_owned(Game *g) {
    int i;
    if (g->nl) return;
    for (i = 0; i < MAX_OWNED; i++) if (g->p.veh != g->firstOwned + i) place_owned(g, i, 0);
}

/* 1 = gekocht (speler zit erin), 0 = te weinig geld, -1 = garage vol */
int game_buy_car(Game *g, int model) {
    int slot;
    if (model < 0 || model >= DEALER_MODELS) return 0;
    for (slot = 0; slot < MAX_OWNED && g->garage[slot].exists; slot++) {}
    if (slot >= MAX_OWNED) return -1;
    if (g->money < CARS[model].price) return 0;
    g->money -= CARS[model].price;
    g->garage[slot].exists = 1;
    g->garage[slot].model = model;
    g->garage[slot].paint = model_default_paint(model);
    g->garage[slot].eng = g->garage[slot].hand = g->garage[slot].neon = g->garage[slot].spoiler = 0;
    place_owned(g, slot, 1);
    if (g->p.veh >= 0) { g->veh[g->p.veh].speed = 0; }
    g->p.veh = g->firstOwned + slot;
    g->p.x = g->veh[g->p.veh].x; g->p.z = g->veh[g->p.veh].z; g->p.h = g->veh[g->p.veh].h;
    g->camYaw = g->p.h;
    return 1;
}

void game_sell_car(Game *g, int slot) {
    if (slot < 0 || slot >= MAX_OWNED || !g->garage[slot].exists) return;
    if (g->p.veh == g->firstOwned + slot) return;
    g->money += CARS[g->garage[slot].model].price / 2;
    g->garage[slot].exists = 0;
    g->veh[g->firstOwned + slot].active = 0;
}

/* prijs van de volgende stap (of -1 als het niet kan) */
int game_custom_price(const Game *g, int item) {
    const Vehicle *v;
    if (g->p.veh < 0) return -1;
    v = &g->veh[g->p.veh];
    if (v->type != VEH_CAR || v->police) return -1;
    switch (item) {
    case 0: return 200;
    case 1: return v->eng < 3 ? ENGINE_PRICE[v->eng] : -1;
    case 2: return v->hand < 3 ? HANDLING_PRICE[v->hand] : -1;
    case 3: return 500;
    case 4: return 400;
    default: return -1;
    }
}

int game_custom(Game *g, int item) {
    Vehicle *v;
    int price = game_custom_price(g, item);
    if (price < 0 || g->money < price) return 0;
    v = &g->veh[g->p.veh];
    g->money -= price;
    if (!v->custom) { v->custom = 1; v->model = 0; v->paint = v->variant % 6; if (v->traffic) v->traffic = 0; }
    switch (item) {
    case 0: v->paint = (v->paint < 0 ? 0 : v->paint + 1) % NPAINTS; break;
    case 1: v->eng++; break;
    case 2: v->hand++; break;
    case 3: v->neon = (v->neon + 1) % NNEONS; break;
    case 4: v->spoiler = !v->spoiler; break;
    }
    if (v->garageSlot >= 0 && v->garageSlot < MAX_OWNED) {
        GarageCar *c = &g->garage[v->garageSlot];
        c->paint = v->paint; c->eng = v->eng; c->hand = v->hand; c->neon = v->neon; c->spoiler = v->spoiler;
    }
    return 1;
}

static void update_shops(Game *g) {
    float dx, dz;
    int in2;
    if (g->nl) return;
    /* dealer: te voet binnenlopen */
    dx = g->p.x - g->w->poi[POI_DEALER].x; dz = g->p.z - g->w->poi[POI_DEALER].z;
    in2 = g->p.veh < 0 && dx * dx + dz * dz < 2.4f * 2.4f;
    if (in2 && !g->inDealer) g->dealerOpen = 1;
    g->inDealer = in2;
    /* BLOCK CUSTOMS: met een auto langzaam naar binnen rijden */
    dx = g->p.x - g->w->poi[POI_CUSTOMS].x; dz = g->p.z - g->w->poi[POI_CUSTOMS].z;
    in2 = g->p.veh >= 0 && g->veh[g->p.veh].type == VEH_CAR && dx * dx + dz * dz < 3.5f * 3.5f;
    if (in2 && !g->inCustoms) {
        if (fabsf(g->veh[g->p.veh].speed) < 5.0f && !g->veh[g->p.veh].police) { g->customsOpen = 1; g->veh[g->p.veh].speed = 0; }
        else if (g->msgT <= 0) game_message(g, "Slow down to enter BLOCK CUSTOMS", 1.5f);
    }
    g->inCustoms = in2;
    if (g->p.veh < 0) {
        dx = g->p.x - g->w->poi[POI_CUSTOMS].x; dz = g->p.z - g->w->poi[POI_CUSTOMS].z;
        if (dx * dx + dz * dz < 3.0f * 3.0f && g->msgT <= 0) game_message(g, "BLOCK CUSTOMS: drive in with a car", 1.5f);
    }
}

/* ------------------------------------------------------------------ politie en gezondheid */

void game_add_wanted(Game *g, int n) {
    if (g->crimeCool > 0 && n < 2) return;
    if (g->wanted == 0) vo_bark(g, VB_WANTED, 0.8f);
    g->wanted += n;
    if (g->wanted > 3) g->wanted = 3;
    g->evadeT = 0;
    g->crimeCool = 2.0f;
}

void game_damage(Game *g, float amount) {
    if (g->health <= 0) return;
    amount *= 1.0f - 0.1f * (float)(g->tough < 0 ? 0 : (g->tough > 3 ? 3 : g->tough));   /* BLOCK GYM: taaiheid */
    if (g->armor > 0) {
        float a = fminf(g->armor, amount * 0.75f);
        g->armor -= a;
        amount -= a;
    }
    g->health -= amount;
    if (g->health < 0) g->health = 0;
    g->hurtT = 0;
    g->hurtFlash = 0.6f;
    if (g->hurtSndCool <= 0) { game_sfx(g, SFX_HURT, g->p.x, g->p.z); g->hurtSndCool = 0.5f; vo_bark(g, VB_HURT, 0.35f); }
}

static void respawn(Game *g, const char *title, const char *sub) {
    int i;
    if (g->p.veh >= 0) { g->veh[g->p.veh].speed = 0; g->p.veh = -1; }
    g->mission = -1;
    g->wanted = 0; g->bustT = 0; g->evadeT = 0;
    for (i = 0; i < MAX_POLICE; i++) g->veh[g->firstPolice + i].active = 0;
    for (i = MAX_PED - MAX_COPS; i < MAX_PED; i++) g->ped[i].active = 0;
    g->health = 100.0f;
    g->p.vy = 0; g->p.speed = 0;
    game_teleport(g, g->w->poi[POI_SPAWN].x, g->w->poi[POI_SPAWN].z, g->w->spawn_heading);
    banner(g, title, sub, 0, 4.5f);
    game_sfx(g, SFX_FAIL, g->p.x, g->p.z);
    g->p.cool = 3.0f;
}

static void police_spawn(Game *g, Vehicle *v, float px, float pz) {
    int tries;
    for (tries = 0; tries < 40; tries++) {
        int i = rng_int(&g->rng, NB + 1), j = rng_int(&g->rng, NB + 1);
        V3 n = node_pos(i, j);
        float dx = n.x - px, dz = n.z - pz, d = sqrtf(dx * dx + dz * dz);
        if (d < 40.0f || d > 100.0f) continue;
        memset(v, 0, sizeof(*v));
        v->active = 1; v->type = VEH_CAR; v->variant = POLICE_VARIANT; v->police = 1;
        v->x = n.x; v->z = n.z; v->y = 0;
        v->h = atan2f(px - n.x, pz - n.z);
        v->speed = 5.0f;
        v->hp = 100.0f;
        return;
    }
}

static void update_police(Game *g, Vehicle *v, float dt) {
    float px, pz, dx, dz, d, diff, thr = 1.0f, brk = 0.0f, steer;
    player_pos(g, &px, &pz);
    dx = px - v->x; dz = pz - v->z; d = sqrtf(dx * dx + dz * dz);
    if (v->wrecked) { v->speed = approachf(v->speed, 0, 10.0f * dt); if (g->wanted <= 0 && d > 40.0f) v->active = 0; return; }
    if (g->wanted <= 0) {
        v->ghost += dt;
        drive_vehicle(g, v, 0, 1, 0, dt);
        if (v->ghost > 5.0f || d > 45.0f) v->active = 0;
        return;
    }
    diff = wrap_angle(atan2f(dx, dz) - v->h);
    steer = clampf(-diff * 1.8f, -1, 1);
    if (v->tz > 0) {             /* even achteruit na vastzitten */
        v->tz -= dt;
        drive_vehicle(g, v, 0, 1, -steer, dt);
        return;
    }
    if (d < 12.0f) { thr = d > 7.0f && fabsf(v->speed) < 3.0f ? 0.4f : 0.0f; brk = fabsf(v->speed) > 3.0f || d < 7.0f ? 1.0f : 0.0f; }
    else if (fabsf(diff) > 1.7f && d < 14.0f) { thr = 0; brk = 1; }
    if (v->speed > 17.0f) thr = 0;
    drive_vehicle(g, v, thr, brk, steer, dt);
    if (thr > 0 && fabsf(v->speed) < 1.0f) {
        v->stuck += dt;
        if (v->stuck > 1.2f) { v->tz = 1.3f; v->stuck = 0; }
    } else v->stuck = 0;
}

static float player_speed(const Game *g) {
    if (g->p.veh >= 0) return fabsf(g->veh[g->p.veh].speed);
    return fabsf(g->p.speed);
}

static void update_police_and_health(Game *g, float dt) {
    float px, pz;
    int k, near = 0, close = 0;
    player_pos(g, &px, &pz);
    if (g->crimeCool > 0) g->crimeCool -= dt;
    if (g->screamCool > 0) g->screamCool -= dt;
    if (g->hurtSndCool > 0) g->hurtSndCool -= dt;
    if (g->ramCool > 0) g->ramCool -= dt;
    if (g->hurtFlash > 0) g->hurtFlash -= dt;
    if (g->p.hitCool > 0) g->p.hitCool -= dt;
    for (k = 0; k < MAX_POLICE; k++) {
        Vehicle *v = &g->veh[g->firstPolice + k];
        if (!v->active && k < g->wanted) {
            police_spawn(g, v, px, pz);
            if (v->active && k == 0) game_message(g, "POLICE! Lose them or get busted.", 2.5f);
        }
        if (v->active) {
            float dx, dz, d2;
            update_police(g, v, dt);
            dx = v->x - px; dz = v->z - pz; d2 = dx * dx + dz * dz;
            if (d2 > 170.0f * 170.0f) v->active = 0;
            if (v->active && !v->wrecked && d2 < 45.0f * 45.0f) near = 1;
            if (v->active && !v->wrecked && d2 < 5.5f * 5.5f) close = 1;
        }
    }
    spawn_cops(g);
    for (k = MAX_PED - MAX_COPS; k < MAX_PED; k++) {
        Ped *c = &g->ped[k];
        float dx, dz;
        if (!c->active || c->state == PED_DOWN) continue;
        dx = c->x - px; dz = c->z - pz;
        if (dx * dx + dz * dz < 40.0f * 40.0f) near = 1;
        if (dx * dx + dz * dz < 2.0f * 2.0f) close = 1;
    }
    if (game_store_inside(g)) close = 0;   /* binnen in een winkel: agenten wachten buiten bij de deur */
    if (g->wanted > 0) {
        if (near) g->evadeT = 0;
        else {
            g->evadeT += dt;
            if (g->evadeT > 10.0f) {
                g->wanted--;
                g->evadeT = 0;
                if (g->wanted == 0) { game_message(g, "You lost the police!", 2.5f); vo_bark(g, VB_LOSTCOPS, 1.0f); }
            }
        }
        /* gewapend te voet: de politie arresteert niet, maar schiet */
        if (close && player_speed(g) < 2.5f && !(g->p.veh < 0 && g->weapon != WP_NONE)) g->bustT += dt;
        else g->bustT = fmaxf(0, g->bustT - dt);
        if (g->bustT > 2.5f) {
            long fine = g->money / 10;
            char b[96];
            if (fine < 50) fine = 50;
            if (fine > g->money) fine = g->money;
            g->money -= fine;
            snprintf(b, sizeof(b), "The police caught you. Fine: -$%ld", fine);
            respawn(g, "BUSTED", b);
            vo_say(g, vo_pick(g, VB_BUSTED), VP_MISSION);
            return;
        }
    } else g->bustT = 0;

    /* aangereden worden als voetganger */
    if (g->p.veh < 0 && g->p.hitCool <= 0) {
        int i;
        for (i = 0; i < g->nveh; i++) {
            Vehicle *v = &g->veh[i];
            float sp, best = 1e9f, bx = 0, bz = 0, rr;
            int c, car;
            if (!v->active) continue;
            sp = fabsf(v->speed);
            if (sp < 4.0f) continue;
            car = v->type == VEH_CAR;
            rr = car ? 1.35f : 0.95f;
            for (c = -1; c <= 1; c++) {
                V3 f = fwd_of(v->h);
                float off = car ? 1.1f * c : 0.0f;
                float cx = v->x + f.x * off, cz = v->z + f.z * off;
                float dx = g->p.x - cx, dz = g->p.z - cz, d2 = dx * dx + dz * dz;
                if (d2 < best) { best = d2; bx = dx; bz = dz; }
            }
            if (best < rr * rr && g->p.y < 1.0f + v->y) {
                float d = sqrtf(best) + 1e-4f;
                game_damage(g, (10.0f + sp * 1.5f) * (v->police ? 0.6f : 1.0f));
                g->p.x += bx / d * 1.2f; g->p.z += bz / d * 1.2f;
                g->p.vy = 4.0f; g->p.onGround = 0;
                g->p.hitCool = 1.0f;
                game_message(g, "Ouch!", 1.0f);
                break;
            }
        }
    }

    g->hurtT += dt;
    if (g->hurtT > 5.0f && g->health < 100.0f) g->health = fminf(100.0f, g->health + 4.0f * dt);
    if (g->health <= 0) { respawn(g, "KNOCKED OUT", "You got hurt too badly. Back to the plaza."); vo_say(g, vo_pick(g, VB_WASTED), VP_MISSION); }
}

void game_start_mission_at(Game *g, int m) {
    MissionDef *md;
    int k;
    if (m < 0 || m >= NMISSIONS) return;
    md = &g->mdef[m];
    if (md->requireWeapon && !game_has_weapon(g)) {
        banner(g, "NEED A GUN", "Buy one at BLOCK ARMS first (red marker on the map).", 0, 3.5f);
        return;
    }
    if (g->p.veh >= 0) { g->veh[g->p.veh].speed = 0; g->p.veh = -1; }
    g->wanted = 0; g->bustT = 0;
    for (k = 0; k < MAX_POLICE; k++) g->veh[g->firstPolice + k].active = 0;
    game_teleport(g, md->giver.x, md->giver.z, g->p.h);
    g->p.speed = 0; g->p.vy = 0;
    if (md->startVeh) {
        int type = md->startVeh == 1 ? VEH_BIKE : VEH_CAR, i, best = -1;
        float bd = 1e18f;
        for (i = 0; i < g->firstTraffic; i++) {
            Vehicle *v = &g->veh[i];
            float dx, dz;
            if (!v->active || v->type != type) continue;
            if (v->variant == 0) continue;  /* Giveaway Bike en rode auto (Joyride) laten staan */
            dx = v->x - md->giver.x; dz = v->z - md->giver.z;
            if (dx * dx + dz * dz < bd) { bd = dx * dx + dz * dz; best = i; }
        }
        if (best >= 0) {
            Vehicle *v = &g->veh[best];
            float nx, nz;
            v->x = md->giver.x; v->z = md->giver.z; v->speed = 0;
            v->y = world_ground_y(g->w, v->x, v->z);
            world_push_circle(g->w, &v->x, &v->z, type == VEH_BIKE ? 0.6f : 1.6f, v->y + 0.2f, &nx, &nz);
            g->p.veh = best;
            g->p.x = v->x; g->p.z = v->z; g->p.h = v->h;
            g->camYaw = v->h;
        }
    }
    game_start_mission(g, m);
}

void game_update(Game *g, const Input *in, float dt) {
    int i;
    float px, pz;
    Input still;
    if (dt > 0.1f) dt = 0.1f;
    g->time += dt;
    vo_tick(g, dt);
    if (g->robCool > 0) g->robCool -= dt;   /* loopt altijd door, ook buiten de winkel */
    if (g->cut) {   /* v4.5 cutscene: de wereld loopt door, de speler staat stil */
        cut_update(g, in, dt);
        memset(&still, 0, sizeof(still));
        if (g->cut) in = &still;
    }
    if (g->p.cool > 0) g->p.cool -= dt;
    if (g->p.anT > 0) { g->p.anT -= dt; if (g->p.anT <= 0) { g->p.anT = 0; g->p.anKind = 0; } }
    if (g->msgT > 0) g->msgT -= dt;
    if (g->bannerT > 0) g->bannerT -= dt;
    if (g->moneyFlash > 0) g->moneyFlash -= dt;

    /* in-/uitstappen */
    if (in->actionPressed) {
        if (g->p.veh >= 0) exit_vehicle(g);
        else {
            int v = game_near_vehicle(g);
            if (v >= 0) enter_vehicle(g, v);
            else game_message(g, "No vehicle nearby", 1.2f);
        }
    }

    if (g->p.veh >= 0) {
        Vehicle *v = &g->veh[g->p.veh];
        float thr = in->gas ? 1.0f : 0.0f, brk = in->brake ? 1.0f : 0.0f;
        /* stick naar voren werkt ook als gas */
        if (!in->gas && in->my > 0.5f) thr = (in->my - 0.3f) / 0.7f;
        if (!in->brake && in->my < -0.5f) brk = 1.0f;
        drive_vehicle(g, v, thr, brk, clampf(in->mx * 1.15f, -1, 1), dt);
        g->p.x = v->x; g->p.z = v->z; g->p.y = v->y; g->p.h = v->h;
        g->p.anim += fabsf(v->speed) * dt * 1.2f;
    } else if (g->p.anT > 0 && g->p.anKind >= 3) {
        Input still = *in;   /* even stilstaan tijdens het uitstappen */
        still.mx = still.my = 0; still.jumpPressed = still.jump = 0; still.fire = 0;
        update_player_foot(g, &still, dt);
    } else {
        update_player_foot(g, in, dt);
    }

    if (g->nl && g->w->realSize > 0) {   /* niet van de echte kaart af (alleen bij een vaste kaart) */
        float lo = -30.0f, hi = g->w->realSize + 30.0f;
        if (g->p.veh >= 0) {
            Vehicle *v = &g->veh[g->p.veh];
            if (v->x < lo || v->x > hi || v->z < lo || v->z > hi) { v->x = clampf(v->x, lo, hi); v->z = clampf(v->z, lo, hi); v->speed *= 0.5f; game_message(g, "Edge of the map", 1.5f); }
        } else if (g->p.x < lo || g->p.x > hi || g->p.z < lo || g->p.z > hi) {
            g->p.x = clampf(g->p.x, lo, hi); g->p.z = clampf(g->p.z, lo, hi); game_message(g, "Edge of the map", 1.5f);
        }
    }
    home_tick(g, dt);
    player_pos(g, &px, &pz);

    /* verkeer: pooling rond de speler */
    for (i = 0; i < MAX_TRAFFIC; i++) {
        int idx = g->firstTraffic + i;
        Vehicle *v = &g->veh[idx];
        if (idx == g->p.veh) continue;
        if (i >= g->wantTraffic) { if (v->traffic) v->active = 0; continue; }
        if (!v->active || !v->traffic) {
            if (!v->active || (v->x - px) * (v->x - px) + (v->z - pz) * (v->z - pz) > 200.0f * 200.0f) {
                memset(v, 0, sizeof(*v));
                traffic_spawn(g, v, px, pz, g->time < 1.0f);
            }
            continue;
        }
        {
            float dx = v->x - px, dz = v->z - pz;
            if (dx * dx + dz * dz > 175.0f * 175.0f) { traffic_spawn(g, v, px, pz, 0); continue; }
        }
        update_traffic_car(g, v, dt);
    }

    /* voetgangers */
    for (i = MAX_PED - MAX_COPS; i < MAX_PED; i++) if (g->ped[i].active) update_ped(g, &g->ped[i], dt);
    for (i = 0; i < g->wantPeds && i < MAX_PED - MAX_COPS; i++) {
        Ped *p = &g->ped[i];
        if (!p->active) { ped_respawn(g, p, px, pz, g->time < 1.0f); continue; }
        {
            float dx = p->x - px, dz = p->z - pz;
            if (dx * dx + dz * dz > 125.0f * 125.0f) { ped_respawn(g, p, px, pz, 0); continue; }
        }
        update_ped(g, p, dt);
    }

    /* geld oppakken */
    for (i = 0; i < g->w->ncash; i++) {
        float dx, dz, lim;
        if (g->cashTimer[i] > 0) { g->cashTimer[i] -= dt; continue; }
        dx = g->w->cash[i].x - px; dz = g->w->cash[i].z - pz;
        lim = g->p.veh >= 0 ? 2.2f : 1.3f;
        if (dx * dx + dz * dz < lim * lim) {
            int val = 20 + (i * 7) % 31;
            char b[32];
            g->money += val;
            g->moneyFlash = 1.2f;
            g->cashTimer[i] = 120.0f;
            g->statCash++;
            vo_bark(g, VB_CASH, 0.5f);
            game_sfx(g, SFX_CASH, px, pz);
            snprintf(b, sizeof(b), "+$%d", val);
            game_message(g, b, 1.2f);
        }
    }

    update_weapons(g, in, dt);
    update_shops(g);
    update_police_and_health(g, dt);
    if (!g->nl) update_missions(g, dt);
    /* v4.0: wapenwinkel en winkels lopen via store_tick (deur -> binnen -> toonbank).
     * BLOCK SPRAY blijft een drive-in: inrijden opent het menu */
    store_tick(g, dt);
    if (!g->nl) {
        int cur = 0;
        float qx = g->p.veh >= 0 ? g->veh[g->p.veh].x : g->p.x, qz = g->p.veh >= 0 ? g->veh[g->p.veh].z : g->p.z;
        V3 pp = g->w->poi[biz_poi(BIZ_SPRAY)];
        float dx = qx - pp.x, dz = qz - pp.z;
        if (dx * dx + dz * dz < 3.2f * 3.2f) cur = BIZ_SPRAY + 1;
        if (cur && cur != g->inBiz) {
            g->bizOpen = cur;
            if (g->p.veh >= 0) g->veh[g->p.veh].speed = 0;
        }
        if (!game_store_inside(g)) g->inBiz = cur;
    }
    update_camera(g, in, dt);
    if (g->cut) cut_camera(g);

    /* geldteller loopt op */
    if (g->moneyShown < g->money) {
        long d = (g->money - g->moneyShown) / 8 + 1;
        g->moneyShown += d;
    } else g->moneyShown = g->money;
}
