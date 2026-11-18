#include "biz.h"
#include "models.h"
#include <stdio.h>
#include <string.h>

static const char *NAMES[BIZ_COUNT] = {"GIVEAWAY BURGER", "BLOCK GYM", "DRIP", "BLING", "BLOCK SPRAY"};
static const char *TAGS[BIZ_COUNT] = {
    "Hot food, cold drinks - eat to heal",
    "Train hard: more stamina, less damage",
    "Fresh clothes - change your look",
    "Luxury for your home (shows up inside)",
    "Drive in: lose the cops, repair, new paint"};
static const float COLS[BIZ_COUNT][3] = {{0.94f, 0.59f, 0.12f}, {0.16f, 0.78f, 0.86f}, {0.9f, 0.24f, 0.63f}, {0.95f, 0.8f, 0.2f}, {0.25f, 0.4f, 0.9f}};

static const int CARDIO_PRICE[3] = {1500, 4000, 9000};
static const int TOUGH_PRICE[3] = {2000, 5000, 12000};
static const char *OUTFIT_NAMES[BIZ_OUTFITS] = {"CLASSIC + CAP", "BLUE CASUAL", "YELLOW TEE", "GREEN HOODIE", "ALL WHITE", "PURPLE STYLE", "ORANGE SPORT", "BLACK SUIT"};
static const int OUTFIT_PRICE[BIZ_OUTFITS] = {0, 300, 300, 400, 500, 600, 500, 1500};
static const char *DECOR_NAMES[BIZ_DECOR] = {"GOLD STATUE", "AQUARIUM", "ARCADE MACHINE", "NEON SIGN", "PARTY LIGHTS"};
static const char *DECOR_INFO[BIZ_DECOR] = {
    "A shiny golden statue of you, by the front door",
    "Big glowing fish tank next to your desk",
    "Retro arcade cabinet next to the TV",
    "Pink neon sign above the sofa",
    "Colourful disco lights on the ceiling"};
static const int DECOR_PRICE[BIZ_DECOR] = {5000, 8000, 3000, 2000, 1500};

const char *biz_name(int shop) { return NAMES[(shop % BIZ_COUNT + BIZ_COUNT) % BIZ_COUNT]; }
const char *biz_tagline(int shop) { return TAGS[(shop % BIZ_COUNT + BIZ_COUNT) % BIZ_COUNT]; }
void biz_color(int shop, float *r, float *g, float *b) {
    shop = (shop % BIZ_COUNT + BIZ_COUNT) % BIZ_COUNT;
    *r = COLS[shop][0]; *g = COLS[shop][1]; *b = COLS[shop][2];
}
int biz_poi(int shop) { return POI_BURGER + (shop % BIZ_COUNT + BIZ_COUNT) % BIZ_COUNT; }

float game_stamina_max(const Game *g) {
    static const float mx[4] = {100.0f, 130.0f, 165.0f, 200.0f};
    return mx[g->cardio < 0 ? 0 : (g->cardio > 3 ? 3 : g->cardio)];
}

int biz_outfit_variant(int outfit) {
    static const int v[BIZ_OUTFITS] = {0, 1, 2, 3, 4, 5, 6, 7};
    return v[(outfit % BIZ_OUTFITS + BIZ_OUTFITS) % BIZ_OUTFITS];
}

static void item(BizItem *it, const char *name, const char *info, int price, int state) {
    snprintf(it->name, sizeof(it->name), "%s", name);
    snprintf(it->info, sizeof(it->info), "%s", info);
    it->price = price; it->state = state;
}

int biz_items(const Game *g, int shop, BizItem *o) {
    int n = 0, k;
    char b[32], inf[80];
    switch (shop) {
    case BIZ_BURGER:
        item(&o[n++], "BURGER", "+25 health", 20, 0);
        item(&o[n++], "MEGA MEAL", "+60 health and full stamina", 60, 0);
        item(&o[n++], "ENERGY DRINK", "Run without getting tired for 30 seconds", 45, 0);
        item(&o[n++], "FAMILY BUCKET", "Full health, full stamina and 60 s of energy", 150, 0);
        break;
    case BIZ_GYM:
        if (g->cardio < 3) {
            snprintf(b, sizeof(b), "CARDIO LEVEL %d", g->cardio + 1);
            snprintf(inf, sizeof(inf), "Max stamina %.0f -> %.0f (forever)", game_stamina_max(g), g->cardio == 0 ? 130.0f : (g->cardio == 1 ? 165.0f : 200.0f));
            item(&o[n++], b, inf, CARDIO_PRICE[g->cardio], 0);
        } else item(&o[n++], "CARDIO MAXED", "Max stamina 200 - you are an athlete", 0, 2);
        if (g->tough < 3) {
            snprintf(b, sizeof(b), "TOUGHNESS LEVEL %d", g->tough + 1);
            snprintf(inf, sizeof(inf), "Take %d%% less damage from bullets and crashes (forever)", (g->tough + 1) * 10);
            item(&o[n++], b, inf, TOUGH_PRICE[g->tough], 0);
        } else item(&o[n++], "TOUGHNESS MAXED", "You take 30% less damage", 0, 2);
        item(&o[n++], "PERSONAL TRAINER", "Full stamina now and 60 s of energy", 250, 0);
        break;
    case BIZ_DRIP:
        for (k = 0; k < BIZ_OUTFITS; k++) {
            int owned = k == 0 || (g->outfits >> k) & 1;
            item(&o[n++], OUTFIT_NAMES[k], g->outfit == k ? "You are wearing this" : (owned ? "Yours - tap to wear it" : "New outfit"),
                 owned ? 0 : OUTFIT_PRICE[k], g->outfit == k ? 1 : 0);
        }
        break;
    case BIZ_BLING:
        for (k = 0; k < BIZ_DECOR; k++) item(&o[n++], DECOR_NAMES[k], DECOR_INFO[k], DECOR_PRICE[k], (g->decor >> k) & 1);
        break;
    case BIZ_SPRAY: {
        int inVeh = g->p.veh >= 0;
        if (g->wanted > 0) {
            snprintf(inf, sizeof(inf), "New look: the police lose you (%d star%s)", g->wanted, g->wanted > 1 ? "s" : "");
            item(&o[n++], "LOSE THE COPS", inVeh ? inf : "Drive in with a vehicle first", 400 * g->wanted, inVeh ? 0 : 2);
        } else item(&o[n++], "LOSE THE COPS", "Nobody is looking for you right now", 0, 2);
        item(&o[n++], "REPAIR + WASH", inVeh ? "Fix all damage on this vehicle" : "Drive in with a vehicle first", 150, inVeh ? 0 : 2);
        item(&o[n++], "NEW COLOUR", inVeh ? "Random fresh paint job" : "Drive in with a vehicle first", 300, inVeh ? 0 : 2);
        break;
    }
    default: break;
    }
    return n;
}

static void boost(Game *g, float secs) {
    g->stamina = game_stamina_max(g);
    g->tired = 0;
    if (g->boostT < secs) g->boostT = secs;
}

int biz_buy(Game *g, int shop, int idx, char *msg, int cap) {
    BizItem it[BIZ_MAX_ITEMS];
    int n = biz_items(g, shop, it);
    if (idx < 0 || idx >= n) return 0;
    if (it[idx].state == 2) { snprintf(msg, (size_t)cap, "%s", it[idx].info); return 0; }
    if (shop == BIZ_BLING && it[idx].state == 1) { snprintf(msg, (size_t)cap, "You already have the %s.", it[idx].name); return 0; }
    if (shop == BIZ_DRIP && it[idx].state == 1) { snprintf(msg, (size_t)cap, "You are already wearing this."); return 0; }
    if (g->money < it[idx].price) { snprintf(msg, (size_t)cap, "Not enough money."); return 0; }
    g->money -= it[idx].price;
    if (it[idx].price > 0) g->moneyFlash = 1.0f;
    switch (shop) {
    case BIZ_BURGER:
        if (idx == 0) g->health = fminf(100.0f, g->health + 25.0f);
        else if (idx == 1) { g->health = fminf(100.0f, g->health + 60.0f); g->stamina = game_stamina_max(g); g->tired = 0; }
        else if (idx == 2) boost(g, 30.0f);
        else { g->health = 100.0f; boost(g, 60.0f); }
        snprintf(msg, (size_t)cap, "Yum! %s", it[idx].name);
        break;
    case BIZ_GYM:
        if (idx == 0) { g->cardio++; g->stamina = game_stamina_max(g); snprintf(msg, (size_t)cap, "Cardio up! Max stamina is now %.0f.", game_stamina_max(g)); }
        else if (idx == 1) { g->tough++; snprintf(msg, (size_t)cap, "Tougher! You now take %d%% less damage.", g->tough * 10); }
        else { boost(g, 60.0f); snprintf(msg, (size_t)cap, "Great workout - full of energy!"); }
        break;
    case BIZ_DRIP:
        g->outfits |= 1 << idx;
        g->outfit = idx;
        snprintf(msg, (size_t)cap, "Looking fresh in %s!", it[idx].name);
        break;
    case BIZ_BLING:
        g->decor |= 1 << idx;
        snprintf(msg, (size_t)cap, "The %s is delivered to your home.", it[idx].name);
        break;
    case BIZ_SPRAY: {
        Vehicle *v = &g->veh[g->p.veh];
        if (idx == 0) {
            int k;
            g->wanted = 0; g->evadeT = 0; g->bustT = 0;
            for (k = 0; k < MAX_POLICE; k++) g->veh[g->firstPolice + k].active = 0;
            for (k = MAX_PED - MAX_COPS; k < MAX_PED; k++) g->ped[k].active = 0;
            if (!v->custom && !v->police) v->variant = (v->variant + 1 + rng_int(&g->rng, CAR_VARIANTS - 2)) % CAR_VARIANTS;
            if (v->variant == POLICE_VARIANT && !v->police) v->variant = 0;
            snprintf(msg, (size_t)cap, "Fresh paint - the police lost you!");
        } else if (idx == 1) {
            v->hp = 100; v->wrecked = 0;
            g->health = fmaxf(g->health, 50.0f);
            snprintf(msg, (size_t)cap, "Repaired and washed - good as new.");
        } else {
            if (v->custom) v->paint = (v->paint + 1 + rng_int(&g->rng, NPAINTS - 1)) % NPAINTS;
            else if (!v->police) { v->variant = (v->variant + 1 + rng_int(&g->rng, CAR_VARIANTS - 1)) % CAR_VARIANTS; if (v->variant == POLICE_VARIANT) v->variant = 0; }
            if (v->custom && v->garageSlot >= 0 && v->garageSlot < MAX_OWNED) g->garage[v->garageSlot].paint = v->paint;
            snprintf(msg, (size_t)cap, "New colour - looking good!");
        }
        break;
    }
    default: break;
    }
    game_sfx(g, SFX_BUY, g->p.x, g->p.z);
    return 1;
}
