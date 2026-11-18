/* v3.2: winkels in de stad waar je geld kwijt kunt.
 * GIVEAWAY BURGER (eten), BLOCK GYM (training), DRIP (kleding), BLING (luxe voor je huis),
 * BLOCK SPRAY (drive-in: politie kwijt, repareren, nieuwe kleur). */
#ifndef SGB_BIZ_H
#define SGB_BIZ_H

#include "game.h"

enum { BIZ_BURGER = 0, BIZ_GYM, BIZ_DRIP, BIZ_BLING, BIZ_SPRAY, BIZ_COUNT };
#define BIZ_MAX_ITEMS 8
#define BIZ_OUTFITS 8          /* 0 = je eigen pet-look, 1..7 = andere kleding */
#define BIZ_DECOR 5            /* gouden beeld, aquarium, arcadekast, neonbord, discolampen */

typedef struct {
    char name[32];
    char info[80];
    int price;     /* 0 = gratis */
    int state;     /* 0 te koop, 1 actief/aan (gekocht of gedragen), 2 niet mogelijk (max of geen voertuig) */
} BizItem;

const char *biz_name(int shop);
const char *biz_tagline(int shop);
void biz_color(int shop, float *r, float *g, float *b);
int biz_poi(int shop);                                   /* POI_* in de wereld */
int biz_items(const Game *g, int shop, BizItem *out);    /* aantal items */
int biz_buy(Game *g, int shop, int item, char *msg, int cap);   /* 1 = gelukt */

float game_stamina_max(const Game *g);
int biz_outfit_variant(int outfit);                       /* personage-variant voor de speler */

#endif
