/* v7.1.1: zie pro_jobs.h */
#include "pro_jobs.h"

#if SGB_PRO_OK
#include "human.h"
#include "models.h"
#include "pro_models.h"
#include <pthread.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#undef near
#undef far
#else
#include <unistd.h>
#endif

#define PJ_MAX 96
#define PJ_THREADS 12

typedef struct { int kind, idx, state; MeshB m; } PJob;   /* state: 0 wacht, 1 bezig, 2 klaar, 3 opgehaald */

static PJob job[PJ_MAX];
static int njob, next, left_, started;
static float detail_ = 1.0f;
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;

static void build(PJob *j) {
    MeshB *m = &j->m;
    mb_init(m);
    g_meshMat = 0;
    g_autoMat = j->kind <= PJ_FARM ? 1 : (j->kind <= PJ_CARDETAIL ? 2 : 0);   /* zoals bij r_build_models */
    switch (j->kind) {
    case PJ_BODY: human_body(m, j->idx); break;
    case PJ_THIGH: human_thigh(m, j->idx); break;
    case PJ_SHIN: human_shin(m, j->idx); break;
    case PJ_UARM: human_uarm(m, j->idx); break;
    case PJ_FARM: human_farm(m, j->idx); break;
    case PJ_CAR: pro_car_traffic(m, j->idx); break;
    case PJ_CARBODY: pro_car_body(m, j->idx); break;
    case PJ_CARDETAIL: pro_car_detail(m, j->idx); break;
    case PJ_TREEHI: pro_tree(m, j->idx, 1); break;
    default: pro_tree(m, j->idx, 0); break;
    }
    g_meshMat = 0; g_autoMat = 0;
}

static void *worker(void *arg) {
    (void)arg;
    g_sqDetail = detail_;   /* per thread (SGB_TLS) */
    for (;;) {
        PJob *j = NULL;
        pthread_mutex_lock(&mu);
        if (next < njob) { j = &job[next++]; j->state = 1; }
        pthread_mutex_unlock(&mu);
        if (!j) break;
        build(j);
        pthread_mutex_lock(&mu);
        j->state = 2;
        pthread_mutex_unlock(&mu);
    }
    return NULL;
}

static void add(int kind, int n) {
    int i;
    for (i = 0; i < n && njob < PJ_MAX; i++) { job[njob].kind = kind; job[njob].idx = i; job[njob].state = 0; njob++; }
}

static int cores(void) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int)si.dwNumberOfProcessors;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int)n : 2;
#endif
}

void pro_jobs_start(float detail) {
    int n = cores() - 1, k, ok = 0;   /* een kern vrij voor het laadscherm */
    if (started) return;
    started = 1;
    detail_ = detail;
    /* de zwaarste eerst: dan zijn alle threads ongeveer tegelijk klaar */
    add(PJ_BODY, CHAR_VARIANTS);
    add(PJ_CAR, CAR_VARIANTS);
    add(PJ_CARBODY, CAR_MODELS);
    add(PJ_CARDETAIL, CAR_MODELS);
    add(PJ_FARM, CHAR_VARIANTS);
    add(PJ_SHIN, CHAR_VARIANTS);
    add(PJ_THIGH, CHAR_VARIANTS);
    add(PJ_UARM, CHAR_VARIANTS);
    add(PJ_TREEHI, PRO_TREE_KINDS);
    add(PJ_TREELO, PRO_TREE_KINDS);
    left_ = njob;
    if (n < 1) n = 1;
    if (n > PJ_THREADS) n = PJ_THREADS;
    for (k = 0; k < n; k++) {
        pthread_t th;
        if (pthread_create(&th, NULL, worker, NULL) == 0) { pthread_detach(th); ok++; }
    }
    if (!ok) {   /* geen threads: dan toch hier (traag, maar het werkt) */
        float keep = g_sqDetail;
        worker(NULL);
        g_sqDetail = keep;
    }
}

int pro_jobs_total(void) { return njob; }
int pro_jobs_left(void) { return left_; }

int pro_jobs_take(int *kind, int *idx, MeshB *out) {
    int i, got = 0;
    pthread_mutex_lock(&mu);
    for (i = 0; i < njob && !got; i++)
        if (job[i].state == 2) {
            job[i].state = 3;
            *kind = job[i].kind; *idx = job[i].idx; *out = job[i].m;
            left_--;
            got = 1;
        }
    pthread_mutex_unlock(&mu);
    return got;
}

#else
void pro_jobs_start(float detail) { (void)detail; }
int pro_jobs_total(void) { return 0; }
int pro_jobs_left(void) { return 0; }
int pro_jobs_take(int *kind, int *idx, MeshB *out) { (void)kind; (void)idx; (void)out; return 0; }
#endif
