/* Low-poly modellen (op de CPU opgebouwd). Alles kijkt naar +z, oorsprong op de grond. */
#ifndef SGB_MODELS_H
#define SGB_MODELS_H

#include "mesh.h"

#define CHAR_VARIANTS 8
#define BIKE_VARIANTS 4
#define CAR_VARIANTS 6

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
void model_marker(MeshB *m);       /* missie-start: ring + ster */

#endif
