/* v7.1.1: PRO-modellen op achtergrondthreads bouwen (pc, ULTRA+). Dit duurde 25 s in een keer op de
 * hoofdthread; Windows sloot het spel dan af als "reageert niet". De renderer haalt de klare modellen op. */
#ifndef SGB_PRO_JOBS_H
#define SGB_PRO_JOBS_H

#include "mesh.h"

enum { PJ_BODY = 0, PJ_THIGH, PJ_SHIN, PJ_UARM, PJ_FARM, PJ_CAR, PJ_CARBODY, PJ_CARDETAIL, PJ_TREEHI, PJ_TREELO };

void pro_jobs_start(float detail);                     /* threads starten (meteen bij het laden) */
int pro_jobs_total(void);
int pro_jobs_left(void);                               /* nog niet opgehaald */
int pro_jobs_take(int *kind, int *idx, MeshB *out);    /* 1 = een klaar model (daarna mb_free), 0 = nog niets klaar */

#endif
