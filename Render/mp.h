/* Peer-to-peer multiplayer (UDP). Eén speler host, de rest verbindt met de host (ster-vorm).
 * LAN: hosts sturen elke seconde een baken; zoekers zien ze vanzelf.
 * Internet: de host probeert UPnP (poort openzetten op de router) en STUN (publiek adres)
 * en toont een uitnodigingscode (IP + poort). */
#ifndef SGB_MP_H
#define SGB_MP_H

#include "../Game/game.h"

#define MP_PORT 27016
#define MP_BEACON_PORT 27017
#define MP_MAXLAN 8

enum { MP_OFF = 0, MP_HOST = 1, MP_JOINING = 2, MP_CLIENT = 3 };

typedef struct { char name[16]; uint32_t ip; int port, players, world; double seen; } MPLan;

void mp_set_name(const char *name);
const char *mp_name(void);
int mp_host(int world, double ox, double oy, const char *label);
int mp_join(uint32_t ip, int port);
void mp_leave(void);
void mp_tick(Game *g, float dt);
int mp_mode(void);
int mp_players(void);                       /* inclusief jezelf */
const char *mp_status(void);
const char *mp_code_internet(void);         /* "" zolang onbekend */
const char *mp_code_lan(void);
const char *mp_internet_note(void);         /* uitleg over UPnP/doorsturen */
int mp_code_decode(const char *code, uint32_t *ip, int *port);
void mp_lan_listen(int on);
int mp_lan_count(void);
const MPLan *mp_lan(int i);
/* na WELCOME: de wereld van de host (0 stad, 1 echte NL met oorsprong) - geeft 1 één keer */
int mp_take_world(int *world, double *ox, double *oy, char *label, int n);
const char *mp_event(void);                 /* "X joined" e.d. (eenmalig), of NULL */

#endif
