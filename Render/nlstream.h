/* Streaming van de echte NL-kaart in tegels van 250 m.
 * Bron per tegel (in deze volgorde): ingebouwd pakket (Heemskerk + Beverwijk) -> opgeslagen tegel
 * (Documents/nl/tiles) -> internet (3D BAG + PDOK TOP10NL). Bouwen gebeurt in een achtergrond-thread. */
#ifndef SGB_NLSTREAM_H
#define SGB_NLSTREAM_H

#include "render.h"
#include "../Game/nltile.h"

/* ingebouwd pakket */
int ns_pack_load(void);
int ns_pack_postcode(const char *pc, double *rx, double *ry);
int ns_pack_nstarts(void);
const char *ns_pack_start(int i, double *rx, double *ry);
int ns_pack_contains(double rx, double ry);

/* streaming-wereld */
void ns_begin(Renderer *r, World *w, double ox, double oy);
void ns_end(void);
void ns_update(float px, float pz);
int ns_active(void);
int ns_ready_at(float x, float z);      /* tegel onder dit punt klaar (of zeker leeg) */
int ns_busy(void);                      /* aantal tegels dat nog laadt */
int ns_offline_hole(float x, float z);  /* tegel heeft geen data (geen internet / niet gedownload) */
void ns_to_rd(float x, float z, double *rx, double *ry);
void ns_from_rd(double rx, double ry, float *x, float *z);
int ns_find_road(float x, float z, float *ox, float *oz, float *heading);
void ns_stats(int *tiles, int *buildings, int *verts, int *fromPack, int *fromCache, int *fromNet);

/* grote kaart: bezochte tegels met kleine textuur */
int ns_map_count(void);
const Tex *ns_map_tile(int i, double *rx0, double *ry0);  /* zuidwest-hoek in RD */

/* downloaden voor offline (los van de speelwereld) */
void ns_dl_start(double rx, double ry, int radius);
void ns_dl_tick(void);
int ns_dl_busy(void);
void ns_dl_progress(int *done, int *total, long *bytes, int *failed);

#endif
