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

void model_char_body(MeshB *m, int variant);
void model_char_leg(MeshB *m, int variant);
void model_char_arm(MeshB *m, int variant);

#define BIKE_SEAT_Y 0.98f
void model_bike(MeshB *m, int variant);
void model_car(MeshB *m, int variant);
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
void model_moto(MeshB *m, int k);   /* k = 0..MOTO_MODELS-1 */
#define NPAINTS 10
#define NNEONS 6
void model_car_body(MeshB *m, int model);     /* wit: wordt met de lakkleur getint */
void model_car_detail(MeshB *m, int model);   /* glas, wielen, lampen */
void model_spoiler(MeshB *m);
Col paint_color(int i);
Col neon_color(int i);
int model_default_paint(int model);
void model_gun(MeshB *m);       /* in de hand, loop langs -y (armrichting) */
void model_tracer(MeshB *m);    /* lijntje van z=0 tot z=1 */       /* missie-start: ring + ster */

#endif
