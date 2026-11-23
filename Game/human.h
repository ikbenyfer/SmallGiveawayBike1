/* v7.0.0 PRO-mensen: organische, hoge-detail personages (alleen pc, kwaliteit ULTRA+).
 * Elk lichaamsdeel is een signed distance field (vloeiend samengevoegde ellipsoiden, capsules en dozen)
 * dat met "surface nets" naar een fijn driehoeksnet wordt omgezet. Zelfde assenstelsels en scharnieren
 * als de gewone modellen (models.h), dus alle animaties blijven werken. */
#ifndef SGB_HUMAN_H
#define SGB_HUMAN_H

#include "mesh.h"

void human_body(MeshB *m, int variant);    /* romp, nek, hoofd, haar, gezicht */
void human_thigh(MeshB *m, int variant);   /* heup op 0, knie op -KNEE_Y */
void human_shin(MeshB *m, int variant);    /* knie op 0, zool op -(0.92 - KNEE_Y) */
void human_uarm(MeshB *m, int variant);    /* schouder op 0, elleboog op -ELBOW_Y */
void human_farm(MeshB *m, int variant);    /* elleboog op 0, hand rond -0.32 */

#endif
