/* Kaarttegels van 250 x 250 m (RD-raster) voor de echte NL-kaart.
 * TileData  = de ruwe kaartinhoud (lokale meters t.o.v. de zuidwest-hoek van de tegel, x oost, y noord)
 * TileBuilt = klaar om te tekenen: 4 deel-meshes, muren, straatnamen, minimap-raster.
 * Alles hier is zonder OpenGL en mag in een achtergrond-thread draaien (een tegel tegelijk). */
#ifndef SGB_NLTILE_H
#define SGB_NLTILE_H

#include "world.h"

#define TILE_M 250.0f
#define TILE_RASTER 128

enum { ROAD_CAR = 0, ROAD_BIKE = 1, ROAD_FOOT = 2, ROAD_MAIN = 3 };

/* v5.5 gebouwen: plat dak (doos), schuin dak als schilddak (3D BAG LoD 1.2 + nokhoogte), of een echt
 * dakvlak uit 3D BAG LoD 2.2 (helling + richting + laagste/hoogste punt) */
enum { BK_FLAT = 0, BK_HIP = 1, BK_FACE = 2 };

typedef struct {
    int n;
    float *x, *y;      /* lokaal, meters */
    float h;           /* gebouwen: hoogte (BK_HIP: nok, BK_FACE: hoogste punt van het vlak) */
    float w;           /* wegen: breedte */
    int code;          /* terrein: landgebruik; wegen: soort; gebouwen: BK_* */
    int name;          /* wegen: index in names, -1 = geen */
    float h0;          /* gebouwen: goothoogte (BK_HIP) of laagste punt van het vlak (BK_FACE) */
    float az, sl;      /* BK_FACE: richting (graden vanaf noord, met de klok mee) en helling (graden) */
    int year, grp;     /* bouwjaar (0 = onbekend) en pand-nummer (voor de kleur) */
} TFeat;

typedef struct {
    int nb, nr, nw, nt;
    TFeat *b, *r, *w, *t;
    char **names; int nnames;
    int heightsEstimated;
    int roofs;          /* 1 = nieuwe gebouwdata (v5.5: daken en bouwjaar) */
} TileData;

typedef struct {
    MeshB sub[4];
    WSeg *segs; int nsegs;
    RSeg *rsegs; int nrsegs;
    char *names; int nameslen;
    uint8_t *raster;           /* TILE_RASTER x TILE_RASTER RGBA, rij 0 = noord */
    int nb, nr, nw, nt, verts;
    float maxH;
} TileBuilt;

void td_free(TileData *td);
/* ruwe tegelbytes (na zlib) -> TileData */
int td_decode(const unsigned char *raw, long len, TileData *out);
/* TileData -> ruwe tegelbytes (malloc); geeft lengte */
long td_encode(const TileData *td, unsigned char **out);
/* JSON-antwoorden (3D BAG of PDOK BAG, TOP10NL) -> TileData voor de tegel met RD-hoek (tminx, tminy).
 * De buffers worden aangepast (in-place JSON). bagPdok = 1: gebouwen zonder hoogte (geschat). */
int td_from_json(double tminx, double tminy, char *bag, long bagLen, int bagPdok,
                 char *roads, long roadsLen, char *water, long waterLen, char *terrain, long terrainLen, TileData *out);

/* Bouwt meshes etc. wx0 = wereld-x van de westrand, wzS = wereld-z van de zuidrand. */
void tb_build(const TileData *td, float wx0, float wzS, TileBuilt *out);
void tb_free(TileBuilt *tb);

#endif
