/* Echte Nederlandse kaart rond een postcode.
 * Bronnen (open data, CC BY 4.0):
 *   - PDOK Locatieserver (postcode -> RD-coordinaten)
 *   - 3D BAG (TU Delft / 3DGI): gebouwen met hoogte (LoD 1.2)
 *   - PDOK BAG (Kadaster): reserve-plattegronden zonder hoogte
 *   - PDOK TOP10NL (Kadaster): wegen met straatnamen, water, terrein */
#ifndef SGB_NLMAP_H
#define SGB_NLMAP_H

#include "world.h"

#define NL_HALF 250.0f
#define NL_SIZE (2.0f * NL_HALF)

typedef struct { double cx, cy; char name[128]; } NLGeo;

typedef struct {
    int buildings, roads, water, terrain, verts, segs;
    int heightsReal;   /* 1 = hoogtes uit 3D BAG, 0 = geschat */
    float maxH;
} NLStats;

/* "1012 js" -> "1012JS"; geeft 1 als het een geldige postcode is */
int nl_normalize_postcode(const char *in, char *out);
void nl_url_geocode(char *buf, int n, const char *pc);
void nl_url_bag3d(char *buf, int n, double cx, double cy);
void nl_url_bagpdok(char *buf, int n, double cx, double cy);
void nl_url_top10(char *buf, int n, const char *collection, double cx, double cy);
int nl_parse_geocode(char *json, long len, NLGeo *out);

/* Bouwt de wereld (CPU) uit de JSON-antwoorden. Buffers mogen NULL zijn.
 * bagPdok: 1 als de gebouwen van PDOK BAG komen (zonder hoogte). */
int nl_build_world(World *w, double cx, double cy,
                   char *bag, long bagLen, int bagPdok,
                   char *roads, long roadsLen, char *water, long waterLen, char *terrain, long terrainLen,
                   NLStats *st);
/* Straatnaam bij een punt (of NULL) */
const char *nl_street_name(const World *w, float x, float z);

#endif
