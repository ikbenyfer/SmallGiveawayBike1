/* Het huis van de speler (alleen in de verzonnen stad): een huis aan de oostrand met een ondergrondse
 * garage. Met geld wordt het huis groter (3 niveaus) en de garage ook (1-4 plekken).
 * Het interieur en de garage liggen ver buiten de stad (je wordt erheen "geteleporteerd" via de deur/oprit). */
#ifndef SGB_HOME_H
#define SGB_HOME_H

#include "mesh.h"

#define HOME_LEVELS 3
#define GARAGE_LEVELS 4          /* 1, 2, 3 of 4 plekken */

/* buiten */
#define HOME_PLOT_X0 405.0f
#define HOME_PLOT_X1 433.0f
#define HOME_PLOT_Z0 176.0f
#define HOME_PLOT_Z1 222.0f
#define HOME_RAMP_X 409.5f       /* marker voor de garage-ingang (rijden of lopen) */
#define HOME_RAMP_Z 212.0f

/* binnen (ver weg, buiten de stad) */
#define HOME_INT_X (-600.0f)
#define HOME_INT_Z (-600.0f)
#define HOME_GAR_Z (-700.0f)
#define HOME_ZONE_X (-300.0f)    /* x hieronder = binnen in huis of garage */

typedef struct { float x0, z0, x1, z1; } HBox;

/* prijs om naar het volgende niveau te gaan, of -1 als het maximum bereikt is */
int home_upgrade_price(int house, int level);   /* house: 1 huis, 0 garage */

/* gevel en voordeur van het huis */
void home_door(int lvl, float *x, float *z);    /* marker voor de voordeur (buiten) */
void home_room(int lvl, float *x0, float *z0, float *x1, float *z1);
void home_garage_room(int glvl, float *x0, float *z0, float *x1, float *z1);
void home_garage_spot(int glvl, int k, float *x, float *z, float *h);

/* binnen-markers (in wereldcoordinaten) */
enum { HM_NONE = 0, HM_FRONTDOOR, HM_RAMP, HM_EXIT, HM_LAPTOP, HM_BED, HM_TOGARAGE, HM_GAR_EXIT, HM_GAR_HOUSE };
int home_markers(int lvl, int glvl, V3 *pos, int *kind, float *rad, int max);

/* botsing met muren en meubels (buiten, in huis en in de garage) */
int home_push(int lvl, int glvl, float *x, float *z, float r);
void home_set_decor(int mask);   /* luxe spullen (BLING) die in huis staan: ook botsen */
/* binnen een kamer: camera binnen de muren houden */
int home_room_at(int lvl, int glvl, float x, float z, float *x0, float *z0, float *x1, float *z1);

/* modellen (wereldcoordinaten) */
void home_build_exterior(MeshB *m, int lvl);
void home_build_interior(MeshB *m, int lvl);
void home_build_garage(MeshB *m, int glvl);
void home_build_decor(MeshB *m, int lvl, int item);   /* 0 beeld, 1 aquarium, 2 arcade, 3 neon, 4 discolampen */

#endif
