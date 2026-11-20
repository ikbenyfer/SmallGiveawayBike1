#include "voice.h"
#include <math.h>

/* ------------------------------------------------------------------ stemmen */

static void vo_play(Game *g, int line, int prio) {
    g->voReq = line;
    g->voLine = line;
    g->voPrio = prio;
    g->voT = VO[line].dur + 0.35f;
}

void vo_say(Game *g, int line, int prio) {
    if (line < 0 || line >= VO_LINES) return;
    if (g->voT > 0 && prio < g->voPrio) return;
    if (prio > VP_BARK) g->voQn = 0;   /* belangrijke regel: wachtrij leeg */
    vo_play(g, line, prio);
}

void vo_queue(Game *g, int line, int prio) {
    if (line < 0 || line >= VO_LINES) return;
    if (g->voT <= 0) { vo_play(g, line, prio); return; }
    if (g->voQn < 4) { g->voQ[g->voQn] = line; g->voQP[g->voQn] = prio; g->voQn++; }
}

int vo_pick(Game *g, int bark) {
    int n = 0;
    while (n < VO_BARK_MAX && VO_BARK[bark][n] >= 0) n++;
    return n ? VO_BARK[bark][rng_int(&g->rng, n)] : -1;
}

void vo_bark(Game *g, int bark, float chance) {
    if (bark < 0 || bark >= VB_COUNT || g->cut) return;
    if (g->barkCool > 0 || g->voT > 0) return;
    if (chance < 1.0f && rng_f(&g->rng) > chance) return;
    vo_say(g, vo_pick(g, bark), VP_BARK);
    g->barkCool = 5.0f + rng_f(&g->rng) * 5.0f;
}

void vo_stop(Game *g) {
    g->voReq = -2;
    g->voT = 0;
    g->voQn = 0;
    g->voPrio = 0;
}

void vo_tick(Game *g, float dt) {
    if (g->barkCool > 0) g->barkCool -= dt;
    if (g->voT > 0) {
        g->voT -= dt;
        if (g->voT <= 0 && g->voQn > 0) {
            int k, line = g->voQ[0], pr = g->voQP[0];
            for (k = 1; k < g->voQn; k++) { g->voQ[k - 1] = g->voQ[k]; g->voQP[k - 1] = g->voQP[k]; }
            g->voQn--;
            vo_play(g, line, pr);
        }
    }
}

int vo_speaker_variant(int spk) {
    static const int v[VO_SPEAKERS] = {0, 2, 5, 3, 4, 6, 7, 1, 8, 6, 5, 7, 7, 3, 1};
    return spk >= 0 && spk < VO_SPEAKERS ? v[spk] : 1;
}

/* ------------------------------------------------------------------ cutscenes */

static void cut_player(const Game *g, float *x, float *y, float *z, float *h, float *head) {
    if (g->p.veh >= 0) {
        const Vehicle *v = &g->veh[g->p.veh];
        *x = v->x; *y = v->y; *z = v->z; *h = v->h;
        *head = v->type == VEH_CAR ? 1.25f : 1.7f;
    } else {
        *x = g->p.x; *y = g->p.y; *z = g->p.z; *h = g->p.h;
        *head = 1.6f;
    }
}

void cut_start(Game *g, int m) {
    float px, py, pz, ph, head, d, nx, nz;
    if (m < 0 || m >= VO_MISSIONS) return;
    cut_player(g, &px, &py, &pz, &ph, &head);
    d = g->p.veh >= 0 && g->veh[g->p.veh].type == VEH_CAR ? 3.6f : 2.3f;
    g->cutX = px + sinf(ph) * d;
    g->cutZ = pz + cosf(ph) * d;
    world_push_circle(g->w, &g->cutX, &g->cutZ, 0.4f, world_ground_y(g->w, g->cutX, g->cutZ) + 0.2f, &nx, &nz);
    g->cutH = atan2f(px - g->cutX, pz - g->cutZ);   /* naar de speler kijken */
    g->cutSpk = VO_MIS[m].who;
    g->cut = 1;
    g->cutMission = m;
    g->cutIdx = -1;
    g->cutT = 0;
    g->cutLineT = 0.9f;   /* eerst even een overzichtsshot */
    g->cutSkip = 0;
    g->p.speed = 0;
    if (g->p.veh >= 0) g->veh[g->p.veh].speed = 0;
    vo_stop(g);
}

void cut_end(Game *g) {
    int m = g->cutMission;
    float px, py, pz, ph, head;
    if (!g->cut) return;
    g->cut = 0;
    vo_stop(g);
    cut_player(g, &px, &py, &pz, &ph, &head);
    g->camYaw = ph;
    g->camPitch = 0.25f;
    game_mission_begin(g, m);
}

void cut_update(Game *g, const Input *in, float dt) {
    const VoMission *vm = &VO_MIS[g->cutMission];
    g->cutT += dt;
    g->cutLineT -= dt;
    g->cutShotT += dt;
    if (g->cutSkip || ((in->actionPressed || in->jumpPressed) && g->cutT > 0.4f)) { cut_end(g); return; }
    if (g->cutLineT <= 0) {
        int line;
        g->cutIdx++;
        if (g->cutIdx >= vm->nintro) { cut_end(g); return; }
        line = vm->intro[g->cutIdx];
        vo_say(g, line, VP_CUT);
        g->cutLineT = VO[line].dur + 0.45f;
        g->cutShotT = 0;
    }
}

/* camera: overzicht, over de schouder van de speler naar het contact, of andersom */
void cut_camera(Game *g) {
    float px, py, pz, ph, head;
    V3 f, r, cam, tgt, P, C;
    int spk = -1;
    float t = g->cutShotT;
    cut_player(g, &px, &py, &pz, &ph, &head);
    f = v3(sinf(ph), 0, cosf(ph));
    r = v3(cosf(ph), 0, -sinf(ph));
    P = v3(px, py + head, pz);
    C = v3(g->cutX, world_ground_y(g->w, g->cutX, g->cutZ) + 1.6f, g->cutZ);
    if (g->cutIdx >= 0 && g->cutIdx < VO_MIS[g->cutMission].nintro) spk = VO[VO_MIS[g->cutMission].intro[g->cutIdx]].spk;
    if (g->cutIdx <= 0 && spk != VS_YOU && g->cutIdx < 0) {
        V3 M = v3lerp(P, C, 0.5f);
        float a = ph + 1.2f + g->cutT * 0.12f;
        cam = v3(M.x + sinf(a) * 6.5f, M.y + 2.2f, M.z + cosf(a) * 6.5f);
        tgt = v3(M.x, M.y - 0.3f, M.z);
    } else if (spk == VS_YOU) {
        /* achter de schouder van het contact, kijkend naar de speler */
        cam = v3add(v3add(C, v3scale(f, 1.25f + 0.05f * t)), v3scale(r, -0.65f));
        cam.y += 0.2f;
        tgt = P;
    } else {
        /* achter de schouder van de speler, kijkend naar het contact; langzaam inzoomen */
        float back = (g->p.veh >= 0 && g->veh[g->p.veh].type == VEH_CAR ? 2.6f : 1.3f) - 0.06f * t;
        cam = v3add(v3add(P, v3scale(f, -back)), v3scale(r, 0.7f));
        cam.y += 0.25f;
        tgt = C;
    }
    {
        float k = world_raycast(g->w, tgt, cam);
        if (k < 1.0f) cam = v3lerp(tgt, cam, fmaxf(0.15f, k - 0.05f));
    }
    g->camPos = cam;
    g->camTarget = tgt;
}
