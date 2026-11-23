/* v6.0.1 PHYSICS-modus (niet voor de iPad): bouw je eigen voertuig uit onderdelen op een raster
 * (chassisblok, stoel, motor, wiel, vleugel, jet) en rijd/vlieg ermee volgens echte rigid-body-physics:
 * zwaartekracht, wielen met vering en bandengrip (je kunt driften), lift en weerstand van vleugels,
 * stuwkracht van jets, botsingen met de grond en gebouwen. Bij een harde klap breken onderdelen af. */
#ifndef SGB_PHYS_H
#define SGB_PHYS_H

#include "mathx.h"

#define PH_GX 7          /* breedte (x), midden = 3 */
#define PH_GZ 9          /* lengte (z), voorkant = 8 */
#define PH_GY 4          /* lagen (y) */
#define PH_CELL 0.6f     /* meter per hokje */
#define PH_MAXP (PH_GX * PH_GZ * PH_GY)
#define PH_DEBRIS 40

enum { PP_NONE = 0, PP_BLOCK, PP_SEAT, PP_MOTOR, PP_WHEEL, PP_WING, PP_JET, PP_COUNT };

typedef struct { unsigned char c[PH_GY][PH_GZ][PH_GX]; } PDesign;

typedef struct { float w, x, y, z; } Quat;

typedef struct {
    int type, alive;
    V3 local;            /* t.o.v. het zwaartepunt */
    float comp, spin;    /* wielen: indrukking van de vering, draaihoek */
    int front;           /* wielen voorin sturen */
} PPart;

typedef struct { int active, type; V3 pos, vel, angv; Quat q; float life; } PDebris;

typedef struct {
    int active;
    PDesign d;
    V3 pos, vel, angv;   /* wereld */
    Quat q;
    float mass, invMass;
    V3 inertia;          /* lokaal (diagonaal) */
    PPart part[PH_MAXP];
    int np, nmotor, njet, nwing, nwheel, seat, broken;
    float throttle, brake, steer, steerAng, hb, pitch, roll;
    float speed, alt, impact;
    int meshDirty;       /* renderer: model opnieuw bouwen */
    PDebris deb[PH_DEBRIS];
    int crashed;         /* stoel kapot */
    float crashT;
} Craft;

const char *ph_part_name(int t);
V3 ph_cell_pos(int gx, int gy, int gz);       /* midden van een hokje (bouwscherm en physics) */
void ph_default(PDesign *d);                   /* een simpele auto */
void ph_stats(const PDesign *d, float *mass, int *motors, int *wheels, int *wings, int *jets, int *seats);
void ph_spawn(Craft *c, const PDesign *d, V3 pos, float heading);
/* stap: gas/rem -1..1, sturen -1..1, handrem, pitch/rol (lucht) -1..1; ground: hoogte van de grond (x,z),
 * push: duwt een punt uit gebouwen (1 = geraakt); ctx wordt doorgegeven */
typedef float (*PhGround)(void *ctx, float x, float z);
typedef int (*PhPush)(void *ctx, float *x, float *z, float y);
void ph_step(Craft *c, float gas, float brk, float steer, int handbrake, float pitch, float roll, float dt,
             PhGround ground, PhPush push, void *ctx);
void ph_reset_upright(Craft *c, PhGround ground, void *ctx);
M4 ph_matrix(const Craft *c);                  /* lokaal -> wereld */
M4 ph_quat_matrix(Quat q, V3 pos);
V3 ph_forward(const Craft *c);
V3 ph_to_world(const Craft *c, V3 local);

/* testterrein met schansen (oost van de stad) */
#define PH_RAMPS 12
typedef struct { float x0, z0, x1, z1, h0, h1; } PRamp;   /* hoogte loopt van h0 (z0) naar h1 (z1) */
extern const PRamp PH_RAMP[PH_RAMPS];
float ph_ramp_height(float x, float z);       /* 0 = geen schans */

#endif
