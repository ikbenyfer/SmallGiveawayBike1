/* De stad: indeling, botsingsvormen, geometrie per chunk en de minimap. */
#ifndef SGB_WORLD_H
#define SGB_WORLD_H

#include "mesh.h"

#define NB 7                 /* blokken per richting */
#define ROADW 12.0f          /* breedte weg incl. niets (stoep hoort bij blok) */
#define BLOCKW 44.0f         /* blok incl. stoep */
#define PITCH 56.0f
#define MAPW (NB * PITCH + ROADW) /* 404 m */
#define BORDER 60.0f
#define SIDEWALK 2.6f
#define CURB 0.15f
#define CHN 8
#define LANE_OFF 2.9f        /* rijstrook-midden vanaf weg-midden */

enum { BOX_BUILDING = 0, BOX_PROP = 1, BOX_WALL = 2, BOX_TREE = 3, BOX_FENCE = 4 };

typedef struct { float x0, z0, x1, z1, y1; int kind; } Box;
typedef struct { float x0, z0, x1, z1, h; } WSeg;        /* muur (echte gebouwen) */
typedef struct { float x0, z0, x1, z1; int name; } RSeg; /* wegstuk met straatnaam */

typedef struct {
    MeshB mb;
    float x0, z0, x1, z1, y0, y1; /* echte grenzen (na bouwen) */
    int vbo;                      /* gevuld door renderer */
    int count;
} Chunk;

enum { VEH_BIKE = 0, VEH_CAR = 1 };

typedef struct { float x, z, h; int type, variant; } ParkSpot;

enum {
    POI_SPAWN = 0, POI_PLAZA, POI_PIZZA, POI_GARAGE, POI_STAGE, POI_BIKESHOP,
    POI_REDCAR, POI_GIVEAWAY_BIKE, POI_BANK, POI_VAULT, POI_GUNSHOP, POI_DEALER, POI_CUSTOMS,
    POI_BURGER, POI_GYM, POI_DRIP, POI_BLING, POI_SPRAY,   /* v3.2: winkels (zelfde volgorde als BIZ_*) */
    POI_MART,                                              /* v4.0: GIVEAWAY MART (supermarkt) */
    POI_COUNT
};

typedef struct {
    char type[NB][NB];       /* [j][i]  j = rij (z), i = kolom (x) */
    Box *boxes; int nboxes, capboxes;
    /* raster voor snelle botsingen */
    int gn; float gcell, gorg;
    int *gstart; int *gitems;
    int *stamp; int curstamp;

    Chunk chunks[CHN * CHN];
    MeshB ground;           /* grote grondplaat + horizon-heuvels */
    int ground_vbo, ground_count;

    uint8_t *minimap; int mm;  /* RGBA mm x mm */

    V3 poi[POI_COUNT];
    float spawn_heading;

    ParkSpot park[40]; int npark;
    V3 cash[24]; int ncash;
    V3 houses[16]; int nhouses;   /* voordeuren voor bezorgmissies */
    V3 gifts[8]; int ngifts;
    V3 dealerSpot[4]; float dealerSpotH;

    int verts_total;
    float mmOrg, mmOrgZ, mmSpan;     /* minimap: wereldgebied (x, z, breedte) */

    /* echte NL-kaart */
    int real;
    float realSize;
    WSeg *segs; int nsegs, capsegs;
    int *sstart, *sitems, *sstamp; int sstampc;
    RSeg *rsegs; int nrsegs, caprsegs;
    char *names; int nameslen, namescap;
} World;

/* Bouwt alles op de CPU. step: 0..N (voor laadbalk), geeft 1 terug als klaar. */
void world_init(World *w);
int world_build_step(World *w, int step);   /* roep met step=0,1,2,... tot 1 */
int world_build_steps(void);
void world_free(World *w);

float world_ground_y(const World *w, float x, float z);
void world_build_grid(World *w, float org, float size);
/* Cirkel uit de muren duwen. Geeft 1 als er botsing was; normaal in nx,nz. */
int world_push_circle(World *w, float *x, float *z, float r, float y, float *nx, float *nz);
/* Straal (van a naar b) tegen gebouwen; geeft fractie 0..1 van eerste raak. */
float world_raycast(World *w, V3 a, V3 b);
int world_on_road(const World *w, float x, float z);
/* Blok-coordinaten */
static inline float block_x0(int i) { return i * PITCH + ROADW; }
static inline float road_c(int k) { return k * PITCH + ROADW * 0.5f; }

#endif
