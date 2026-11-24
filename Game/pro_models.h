/* v7.1.0 PRO-modellen (pc, ULTRA+): organische auto's en bomen, gebouwd uit signed distance fields
 * met surface nets (zoals de PRO-mensen in human.c). Zelfde maten en assen als de gewone modellen. */
#ifndef SGB_PRO_MODELS_H
#define SGB_PRO_MODELS_H

#include "mesh.h"

void pro_car_traffic(MeshB *m, int variant);   /* hele verkeersauto (vervangt model_car) */
void pro_car_body(MeshB *m, int model);        /* alleen de lak (wit, wordt gekleurd) - vervangt model_car_body */
void pro_car_detail(MeshB *m, int model);      /* ruiten, lampen, grille, bumpers, wielen - vervangt model_car_detail */
#define PRO_TREE_KINDS 3
void pro_tree(MeshB *m, int kind, int hi);     /* boom met de voet op (0,0,0); hi = veel detail (dichtbij) */

#endif
