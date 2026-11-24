/* Low-poly modellen (op de CPU opgebouwd). Alles kijkt naar +z, oorsprong op de grond. */
#ifndef SGB_MODELS_H
#define SGB_MODELS_H

#include "mesh.h"

#define CHAR_VARIANTS 9
#define COP_VARIANT 8
#define BIKE_VARIANTS 4
#define CAR_VARIANTS 7
#define POLICE_VARIANT 6

/* Personage-onderdelen. body: voeten op y=0. leg: heup op 0 (hangt naar -y). arm: schouder op 0. */
#define HIP_Y 0.92f
#define HIP_X 0.13f
#define SHOULDER_Y 1.50f
#define SHOULDER_X 0.33f

/* v3.0: gladde personages met knie en elleboog (dichtbij); de oude blokken (_lo, leg, arm) voor ver weg */
#define KNEE_Y 0.44f
#define ELBOW_Y 0.28f
void model_char_body(MeshB *m, int variant);
void model_char_thigh(MeshB *m, int variant);   /* heup op 0, knie op -KNEE_Y */
void model_char_shin(MeshB *m, int variant);    /* knie op 0, zool op -(0.92 - KNEE_Y) */
void model_char_uarm(MeshB *m, int variant);    /* schouder op 0, elleboog op -ELBOW_Y */
void model_char_farm(MeshB *m, int variant);    /* elleboog op 0, hand */
void model_char_body_lo(MeshB *m, int variant);
void model_char_leg(MeshB *m, int variant);
void model_char_arm(MeshB *m, int variant);

#define BIKE_SEAT_Y 0.98f
void model_bike(MeshB *m, int variant);
void model_car(MeshB *m, int variant);
void model_bike_lo(MeshB *m, int variant);
void model_car_lo(MeshB *m, int variant);
Col car_color(int variant);

void model_cash(MeshB *m);
void model_gift(MeshB *m);
void model_checkpoint(MeshB *m);   /* open cilinder r=1, h=1 (wordt geschaald) */
void model_arrow(MeshB *m);
void model_shadow(MeshB *m);       /* schijf r=1 */
void model_marker(MeshB *m);
void model_siren(MeshB *m);
#define CAR_MODELS 4      /* 0 CITY HATCH, 1 RHINO V8, 2 VORTEX GT, 3 BOLT X */
#define MOTO_MODELS 2     /* 4 STREET 600, 5 DRAGON 1000R (motoren bij de dealer) */
#define DEALER_MODELS (CAR_MODELS + MOTO_MODELS)
#define IS_MOTO(model) ((model) >= CAR_MODELS)
void model_moto(MeshB *m, int k);
extern int g_modelPart;   /* v5.1.1: 0 heel, 1 zonder voorkant, 2 alleen voorkant (fiets/motor) */   /* k = 0..MOTO_MODELS-1 */
void model_moto_lo(MeshB *m, int k);
#define NPAINTS 10
#define NNEONS 6
void model_car_body(MeshB *m, int model);     /* wit: wordt met de lakkleur getint */
void model_car_detail(MeshB *m, int model);   /* glas, wielen, lampen */
void model_car_body_lo(MeshB *m, int model);
void model_car_detail_lo(MeshB *m, int model);
void model_spoiler(MeshB *m);
void model_spoiler_lo(MeshB *m);
Col paint_color(int i);
Col neon_color(int i);
int model_default_paint(int model);
void model_gun(MeshB *m);       /* in de hand, loop langs -y (armrichting); oud blokmodel */
void model_weapon(MeshB *m, int weapon);   /* v3.0: 1 pistool, 2 SMG, 3 shotgun (zelfde coordinaten als model_gun) */
void model_tracer(MeshB *m);    /* lijntje van z=0 tot z=1 */       /* missie-start: ring + ster */

void model_char_colors(int v, Col *skin, Col *hair);   /* v6.2.0: voor automatische materialen */

void model_wheels4(MeshB *m, float wx, float wzf, float wzr, float r, float rr, Col rim);   /* v7.1.0: vier wielen (PRO-auto's) */

#endif
