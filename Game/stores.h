/* v4.0: winkels waar je echt in kunt lopen.
 * Elke winkel in de stad (BLOCK ARMS, de v3.2-winkels en de nieuwe GIVEAWAY MART) heeft een deur-marker.
 * Daar loop je doorheen en je staat binnen: een interieur dat ver buiten de stad ligt (net als het huis).
 * Binnen staat een verkoper achter de toonbank; bij de toonbank opent het winkelmenu.
 * In de GIVEAWAY MART kun je de kassa overvallen ($10.000 + politie). */
#ifndef SGB_STORES_H
#define SGB_STORES_H

#include "mesh.h"

enum { STORE_GUN = 0, STORE_BURGER, STORE_GYM, STORE_DRIP, STORE_BLING, STORE_MART, STORE_COUNT };

#define STORE_X0 (-260.0f)       /* midden van winkel 0; elke volgende 40 m verder */
#define STORE_DX 40.0f
#define STORE_Z (-400.0f)
#define STORE_ZONE_Z (-300.0f)   /* z hieronder (en x > -300) = binnen in een winkel */
#define STORE_H 3.4f
#define MART_LOOT 10000

enum { SM_NONE = 0, SM_EXIT, SM_COUNTER, SM_ROB };

int store_poi(int s);                       /* POI_* van de deur in de stad */
const char *store_name(int s);
void store_color(int s, float *r, float *g, float *b);
void store_room(int s, float *x0, float *z0, float *x1, float *z1);
int store_at(float x, float z);             /* winkel + 1 als (x,z) binnen een winkel ligt, anders 0 */
void store_entry(int s, float *x, float *z, float *h);   /* waar je binnen begint */
void store_clerk(int s, float *x, float *z, float *h);   /* verkoper achter de toonbank */
int store_markers(int s, V3 *pos, int *kind, float *rad, int max);
int store_push(float *x, float *z, float r);             /* muren en meubels binnen */
void store_build(MeshB *m, int s);

#endif
