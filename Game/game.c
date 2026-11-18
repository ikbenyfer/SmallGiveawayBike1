#include "game.h"
#include "models.h"
#include <string.h>
#include <stdio.h>

static const int DIRX[4] = {1, 0, -1, 0};
static const int DIRZ[4] = {0, 1, 0, -1};

static float dir_heading(int d) { return atan2f((float)DIRX[d], (float)DIRZ[d]); }

void game_message(Game *g, const char *txt, float t) {
    snprintf(g->msg, sizeof(g->msg), "%s", txt);
    g->msgT = t;
}

static void banner(Game *g, const char *a, const char *b, int good, float t) {
    snprintf(g->banner, sizeof(g->banner), "%s", a);
    snprintf(g->sub, sizeof(g->sub), "%s", b ? b : "");
    g->bannerT = t;
    g->bannerGood = good;
}

/* ------------------------------------------------------------------ missies */

static void cp_set(Checkpoint *c, float x, float z, float r, int need, const char *text) {
    c->x = x; c->z = z; c->r = r; c->need = need; c->text = text;
}

static void setup_missions(Game *g) {
    World *w = g->w;
    MissionDef *m;
    int i;
    memset(g->mdef, 0, sizeof(g->mdef));

    m = &g->mdef[0];
    m->title = "WELCOME RIDE";
    m->brief = "Hop on the Giveaway Bike and ride to the markers.";
    m->giver = w->poi[POI_PLAZA];
    m->reward = 100;
    cp_set(&m->cp[0], w->poi[POI_GIVEAWAY_BIKE].x, w->poi[POI_GIVEAWAY_BIKE].z, 2.2f, NEED_BIKE, "Get on the Giveaway Bike (red)");
    cp_set(&m->cp[1], road_c(4), block_x0(3) + 22, 3.5f, NEED_BIKE, "Ride to the marker");
    cp_set(&m->cp[2], road_c(4), road_c(5), 3.5f, NEED_BIKE, "Ride to the next marker");
    cp_set(&m->cp[3], road_c(2), road_c(5), 3.5f, NEED_BIKE, "Almost there");
    cp_set(&m->cp[4], w->poi[POI_PLAZA].x, w->poi[POI_PLAZA].z + 3, 3.0f, NEED_BIKE, "Back to the plaza");
    m->ncp = 5;

    m = &g->mdef[1];
    m->title = "PIZZA PANIC";
    m->brief = "Deliver 3 hot pizzas to the suburb before they get cold!";
    m->giver = w->poi[POI_PIZZA];
    m->reward = 250;
    m->time = 170;
    for (i = 0; i < 3; i++) {
        V3 h = w->houses[(i * 5 + 1) % (w->nhouses > 0 ? w->nhouses : 1)];
        cp_set(&m->cp[i], h.x, h.z, 2.6f, NEED_ANY, i == 0 ? "Deliver pizza 1 of 3" : (i == 1 ? "Deliver pizza 2 of 3" : "Deliver the last pizza"));
    }
    m->ncp = 3;

    m = &g->mdef[2];
    m->title = "STREET SPRINT";
    m->brief = "Bike race! Hit all checkpoints before the clock runs out.";
    m->giver = w->poi[POI_BIKESHOP];
    m->reward = 300;
    m->time = 110;
    {
        static const int pts[8][2] = {{1, 5}, {1, 6}, {3, 6}, {3, 5}, {5, 5}, {5, 7}, {7, 7}, {7, 4}};
        for (i = 0; i < 8; i++)
            cp_set(&m->cp[i], road_c(pts[i][0]), road_c(pts[i][1]), 4.0f, NEED_BIKE, "Race: next checkpoint");
        m->ncp = 8;
    }

    m = &g->mdef[3];
    m->title = "JOYRIDE RETURN";
    m->brief = "Someone 'borrowed' the red car. Find it and bring it to the garage.";
    m->giver = w->poi[POI_GARAGE];
    m->giver.x -= 6;
    m->reward = 400;
    m->time = 140;
    cp_set(&m->cp[0], w->poi[POI_REDCAR].x, w->poi[POI_REDCAR].z, 3.0f, NEED_CAR, "Get in the red car (parking lot)");
    cp_set(&m->cp[1], w->poi[POI_GARAGE].x, w->poi[POI_GARAGE].z, 4.0f, NEED_CAR, "Drive it into the garage");
    m->ncp = 2;

    m = &g->mdef[4];
    m->title = "THE BIG GIVEAWAY";
    m->brief = "Collect 5 gift boxes around town and bring them to the park stage.";
    m->giver = w->poi[POI_STAGE];
    m->reward = 750;
    m->time = 300;
    for (i = 0; i < w->ngifts && i < 5; i++) cp_set(&m->cp[i], w->gifts[i].x, w->gifts[i].z, 2.0f, NEED_ANY, "Collect the gift boxes");
    cp_set(&m->cp[5], w->poi[POI_STAGE].x, w->poi[POI_STAGE].z, 3.5f, NEED_ANY, "Bring the gifts to the park stage");
    m->ncp = 6;
    m->anyOrder = 5;
    m->gifts = 1;
}

void game_start_mission(Game *g, int m) {
    if (m < 0 || m >= NMISSIONS) return;
    g->mission = m;
    g->mstep = 0;
    g->mmask = 0;
    g->mclock = 0;
    g->mtime = g->mdef[m].time;
    banner(g, g->mdef[m].title, g->mdef[m].brief, 1, 4.5f);
}

static void mission_end(Game *g, int ok) {
    int m = g->mission;
    if (m < 0) return;
    if (ok) {
        int rew = g->completed[m] ? g->mdef[m].reward / 2 : g->mdef[m].reward;
        char b[64];
        g->money += rew;
        g->moneyFlash = 2.0f;
        g->completed[m] = 1;
        g->statMissionsPassed++;
        snprintf(b, sizeof(b), "+$%d", rew);
        banner(g, "MISSION PASSED", b, 1, 4.0f);
    } else {
        banner(g, "MISSION FAILED", "Out of time. Go back to the marker to retry.", 0, 4.0f);
    }
    g->mission = -1;
    g->p.cool = 2.5f;
}

static int need_ok(const Game *g, int need) {
    int mode = game_mode(g);
    if (need == NEED_BIKE) return mode == MODE_BIKE;
    if (need == NEED_CAR) return mode == MODE_CAR;
    return 1;
}

static void player_pos(const Game *g, float *x, float *z) {
    if (g->p.veh >= 0) { *x = g->veh[g->p.veh].x; *z = g->veh[g->p.veh].z; }
    else { *x = g->p.x; *z = g->p.z; }
}

static void update_missions(Game *g, float dt) {
    float px, pz;
    int i;
    player_pos(g, &px, &pz);
    if (g->mission < 0) {
        if (g->p.cool > 0) return;
        for (i = 0; i < NMISSIONS; i++) {
            MissionDef *m = &g->mdef[i];
            float dx = px - m->giver.x, dz = pz - m->giver.z;
            if (dx * dx + dz * dz < 2.4f * 2.4f) { game_start_mission(g, i); break; }
        }
        return;
    }
    {
        MissionDef *m = &g->mdef[g->mission];
        g->mclock += dt;
        if (m->time > 0) {
            g->mtime -= dt;
            if (g->mtime <= 0) { mission_end(g, 0); return; }
        }
        if (g->mstep < m->anyOrder) {
            for (i = 0; i < m->anyOrder; i++) {
                Checkpoint *c = &m->cp[i];
                float dx = px - c->x, dz = pz - c->z;
                if (g->mmask & (1u << i)) continue;
                if (dx * dx + dz * dz < c->r * c->r) {
                    if (!need_ok(g, c->need)) { if (g->msgT <= 0) game_message(g, c->need == NEED_BIKE ? "You need a bike for this!" : "You need a car for this!", 2.0f); continue; }
                    g->mmask |= 1u << i;
                    g->mstep++;
                    {
                        char b[64];
                        snprintf(b, sizeof(b), "Gift %d of %d", g->mstep, m->anyOrder);
                        game_message(g, b, 2.0f);
                    }
                }
            }
        } else if (g->mstep < m->ncp) {
            Checkpoint *c = &m->cp[g->mstep];
            float dx = px - c->x, dz = pz - c->z;
            if (dx * dx + dz * dz < c->r * c->r) {
                if (!need_ok(g, c->need)) {
                    if (g->msgT <= 0) game_message(g, c->need == NEED_BIKE ? "Get on a bike first!" : "Get in a car first!", 2.0f);
                } else {
                    g->mstep++;
                    if (g->mstep >= m->ncp) mission_end(g, 1);
                }
            }
        }
    }
}

int game_targets(const Game *g, V3 *out, int max) {
    int n = 0, i;
    if (g->mission < 0) {
        for (i = 0; i < NMISSIONS && n < max; i++) {
            out[n] = g->mdef[i].giver;
            out[n].y = g->completed[i] ? 1.0f : 0.0f;  /* y = vlag: al gehaald */
            n++;
        }
        return n;
    }
    {
        const MissionDef *m = &g->mdef[g->mission];
        if (g->mstep < m->anyOrder) {
            for (i = 0; i < m->anyOrder && n < max; i++)
                if (!(g->mmask & (1u << i))) out[n++] = v3(m->cp[i].x, 2.0f, m->cp[i].z);
        } else if (g->mstep < m->ncp && n < max) {
            out[n++] = v3(m->cp[g->mstep].x, 2.0f, m->cp[g->mstep].z);
        }
    }
    return n;
}

/* ------------------------------------------------------------------ voertuigen */

int game_mode(const Game *g) {
    if (g->p.veh < 0) return MODE_FOOT;
    return g->veh[g->p.veh].type == VEH_BIKE ? MODE_BIKE : MODE_CAR;
}

static float veh_radius(const Vehicle *v) { return v->type == VEH_BIKE ? 0.55f : 1.05f; }

int game_near_vehicle(const Game *g) {
    int i, best = -1;
    float bd = 1e9f;
    if (g->p.veh >= 0) return -1;
    for (i = 0; i < g->nveh; i++) {
        const Vehicle *v = &g->veh[i];
        float dx, dz, d2, lim;
        if (!v->active) continue;
        if (v->traffic && fabsf(v->speed) > 5.0f) continue;
        dx = v->x - g->p.x; dz = v->z - g->p.z; d2 = dx * dx + dz * dz;
        lim = v->type == VEH_BIKE ? 2.0f : 3.2f;
        if (d2 < lim * lim && d2 < bd) { bd = d2; best = i; }
    }
    return best;
}

static void spawn_ped_at(Game *g, Ped *p, float x, float z, int flee);

static void enter_vehicle(Game *g, int idx) {
    Vehicle *v = &g->veh[idx];
    if (v->traffic) {
        /* bestuurder stapt geschrokken uit en loopt weg */
        int k;
        for (k = 0; k < g->wantPeds && k < MAX_PED; k++) {
            if (!g->ped[k].active) break;
        }
        if (k >= MAX_PED || k >= g->wantPeds) k = 0;
        {
            V3 r = right_of(v->h);
            spawn_ped_at(g, &g->ped[k], v->x + r.x * 1.8f, v->z + r.z * 1.8f, 1);
        }
        v->traffic = 0;
        v->speed *= 0.3f;
    }
    g->p.veh = idx;
    g->p.speed = 0;
    g->p.vy = 0;
    g->statEnter++;
}

static void exit_vehicle(Game *g) {
    Vehicle *v = &g->veh[g->p.veh];
    V3 r = right_of(v->h);
    float side = v->type == VEH_BIKE ? 0.9f : 1.7f;
    float nx, nz;
    if (fabsf(v->speed) > 6.0f) { game_message(g, "Slow down first!", 1.5f); return; }
    g->p.x = v->x - r.x * side;
    g->p.z = v->z - r.z * side;
    g->p.h = v->h;
    g->p.y = world_ground_y(g->w, g->p.x, g->p.z);
    world_push_circle(g->w, &g->p.x, &g->p.z, 0.35f, g->p.y, &nx, &nz);
    v->speed = 0;
    v->steer = 0;
    g->p.veh = -1;
    g->p.cool = 0.5f;
}

static void collide_vehicles(Game *g, Vehicle *a) {
    int i;
    for (i = 0; i < g->nveh; i++) {
        Vehicle *b = &g->veh[i];
        float dx, dz, d2, rr;
        if (b == a || !b->active) continue;
        dx = a->x - b->x; dz = a->z - b->z; d2 = dx * dx + dz * dz;
        rr = veh_radius(a) + veh_radius(b) + (a->type == VEH_CAR && b->type == VEH_CAR ? 0.5f : 0.0f);
        if (d2 < rr * rr && d2 > 1e-6f) {
            float d = sqrtf(d2), push = (rr - d);
            float wa = b->traffic || b->speed != 0 ? 0.5f : 0.7f;
            dx /= d; dz /= d;
            a->x += dx * push * wa; a->z += dz * push * wa;
            b->x -= dx * push * (1 - wa); b->z -= dz * push * (1 - wa);
            if (a->hitcool <= 0) { a->speed *= 0.55f; a->hitcool = 0.4f; }
            if (b->traffic) b->speed *= 0.3f;
        }
    }
}

static void vehicle_world_collide(Game *g, Vehicle *v) {
    float nx = 0, nz = 0;
    int hit = 0;
    if (v->type == VEH_BIKE) {
        hit = world_push_circle(g->w, &v->x, &v->z, 0.5f, v->y + 0.2f, &nx, &nz);
    } else {
        V3 f = fwd_of(v->h);
        int k;
        for (k = -1; k <= 1; k += 2) {
            float cx = v->x + f.x * 1.15f * k, cz = v->z + f.z * 1.15f * k, ox = cx, oz = cz;
            float tx, tz;
            if (world_push_circle(g->w, &cx, &cz, 0.95f, v->y + 0.2f, &tx, &tz)) {
                v->x += cx - ox; v->z += cz - oz;
                nx += tx; nz += tz; hit = 1;
            }
        }
    }
    if (hit) {
        V3 f = fwd_of(v->h);
        float d = f.x * nx + f.z * nz;
        if (v->hitcool <= 0) {
            if ((d < -0.6f && v->speed > 0) || (d > 0.6f && v->speed < 0)) v->speed = -v->speed * 0.25f;
            else v->speed *= 0.75f;
            v->hitcool = 0.25f;
        }
    }
}

static void drive_vehicle(Game *g, Vehicle *v, float thr, float brk, float steerIn, float dt) {
    int bike = v->type == VEH_BIKE;
    float maxF = bike ? 11.5f : 19.0f, acc = bike ? 4.8f : 6.5f, brake = bike ? 10.0f : 13.0f, maxR = bike ? -2.0f : -6.0f;
    float yaw;
    if (thr > 0) {
        if (v->speed < 0) v->speed += brake * dt;
        else if (v->speed < maxF) v->speed += acc * thr * dt * (1.0f - 0.5f * v->speed / maxF);
    }
    if (brk > 0) {
        if (v->speed > 0.2f) v->speed -= brake * dt;
        else if (v->speed > maxR) v->speed -= acc * 0.6f * dt;
    }
    if (thr <= 0 && brk <= 0) v->speed = approachf(v->speed, 0, (bike ? 1.4f : 2.0f) * dt);
    v->speed -= v->speed * 0.05f * dt;
    v->steer = approachf(v->steer, steerIn, 3.5f * dt);
    {
        float sp = fabsf(v->speed);
        float k = bike ? 1.35f : 0.95f;
        yaw = v->steer * k * clampf(sp / 5.0f, 0, 1) * (1.0f - 0.35f * clampf(sp / maxF, 0, 1));
        if (v->speed < 0) yaw = -yaw;
        v->h = wrap_angle(v->h - yaw * dt * 2.0f);
    }
    {
        V3 f = fwd_of(v->h);
        v->x += f.x * v->speed * dt;
        v->z += f.z * v->speed * dt;
    }
    if (bike) v->lean = approachf(v->lean, -v->steer * clampf(fabsf(v->speed) / 8.0f, 0, 1) * 0.45f, 2.0f * dt);
    v->wheel += v->speed * dt / 0.34f;
    if (v->hitcool > 0) v->hitcool -= dt;
    vehicle_world_collide(g, v);
    collide_vehicles(g, v);
    v->y = lerpf(v->y, world_ground_y(g->w, v->x, v->z), clampf(dt * 12.0f, 0, 1));
}

/* ------------------------------------------------------------------ verkeer */

static V3 node_pos(int i, int j) { return v3(road_c(i), 0, road_c(j)); }

static void traffic_set_target(Vehicle *v) {
    V3 n = node_pos(v->ti, v->tj);
    V3 r = right_of(dir_heading(v->dir));
    v->tx = n.x - DIRX[v->dir] * (ROADW * 0.5f + 1.5f) + r.x * LANE_OFF;
    v->tz = n.z - DIRZ[v->dir] * (ROADW * 0.5f + 1.5f) + r.z * LANE_OFF;
    v->phase = 0;
}

static void traffic_spawn(Game *g, Vehicle *v, float px, float pz, int nearOk) {
    int tries;
    for (tries = 0; tries < 30; tries++) {
        int i = rng_int(&g->rng, NB + 1), j = rng_int(&g->rng, NB + 1), d = rng_int(&g->rng, 4);
        int ni = i + DIRX[d], nj = j + DIRZ[d];
        V3 a, b, r, p;
        float dx, dz, dist;
        if (ni < 0 || nj < 0 || ni > NB || nj > NB) continue;
        a = node_pos(i, j); b = node_pos(ni, nj);
        r = right_of(dir_heading(d));
        p = v3add(v3lerp(a, b, rng_range(&g->rng, 0.3f, 0.7f)), v3scale(r, LANE_OFF));
        dx = p.x - px; dz = p.z - pz; dist = sqrtf(dx * dx + dz * dz);
        if (!nearOk && (dist < 60.0f || dist > 150.0f)) continue;
        if (nearOk && dist > 140.0f) continue;
        if (nearOk && dist < 12.0f) continue;
        v->active = 1; v->type = VEH_CAR; v->traffic = 1;
        v->variant = 1 + rng_int(&g->rng, CAR_VARIANTS - 1);
        v->x = p.x; v->z = p.z; v->y = 0; v->h = dir_heading(d);
        v->speed = 6.0f; v->steer = 0; v->stuck = 0; v->ghost = 0;
        v->ti = ni; v->tj = nj; v->dir = d;
        traffic_set_target(v);
        return;
    }
}

static int blocked_ahead(Game *g, Vehicle *v, float *dist) {
    V3 f = fwd_of(v->h), r = right_of(v->h);
    float best = 1e9f;
    int i;
    float px, pz;
#define CHECK(ox, oz, lat) do { float rx = (ox) - v->x, rz = (oz) - v->z; \
        float al = rx * f.x + rz * f.z, la = fabsf(rx * r.x + rz * r.z); \
        if (al > 0.5f && al < 11.0f && la < (lat) && al < best) best = al; } while (0)
    player_pos(g, &px, &pz);
    CHECK(px, pz, 2.0f);
    for (i = 0; i < g->nveh; i++) {
        Vehicle *o = &g->veh[i];
        if (o == v || !o->active) continue;
        if (o->traffic || i == g->p.veh) continue; /* speler al gedaan */
        CHECK(o->x, o->z, 2.0f);
    }
    for (i = g->firstTraffic; i < g->nveh; i++) {
        Vehicle *o = &g->veh[i];
        if (o == v || !o->active || !o->traffic) continue;
        CHECK(o->x, o->z, 1.8f);
    }
    for (i = 0; i < MAX_PED; i++) {
        Ped *p = &g->ped[i];
        if (!p->active) continue;
        CHECK(p->x, p->z, 1.6f);
    }
#undef CHECK
    *dist = best;
    return best < 1e8f;
}

static void update_traffic_car(Game *g, Vehicle *v, float dt) {
    float dx = v->tx - v->x, dz = v->tz - v->z, d = sqrtf(dx * dx + dz * dz);
    float want = v->phase == 1 ? 5.0f : 9.0f;
    float bd;
    if (d < 2.2f) {
        if (v->phase == 0) {
            /* op het kruispunt: kies nieuwe richting */
            int opts[4], no = 0, k, nd;
            for (k = 0; k < 4; k++) {
                int ni = v->ti + DIRX[k], nj = v->tj + DIRZ[k];
                if (k == (v->dir + 2) % 4) continue;
                if (ni < 0 || nj < 0 || ni > NB || nj > NB) continue;
                opts[no++] = k;
                if (k == v->dir) opts[no++] = k; /* rechtdoor vaker */
            }
            nd = no ? opts[rng_int(&g->rng, no)] : (v->dir + 2) % 4;
            {
                V3 n = node_pos(v->ti, v->tj);
                V3 r = right_of(dir_heading(nd));
                v->tx = n.x + DIRX[nd] * (ROADW * 0.5f + 1.5f) + r.x * LANE_OFF;
                v->tz = n.z + DIRZ[nd] * (ROADW * 0.5f + 1.5f) + r.z * LANE_OFF;
            }
            v->dir = nd;
            v->phase = 1;
        } else {
            v->ti += DIRX[v->dir];
            v->tj += DIRZ[v->dir];
            traffic_set_target(v);
        }
        dx = v->tx - v->x; dz = v->tz - v->z;
    }
    if (v->ghost > 0) v->ghost -= dt;
    else if (blocked_ahead(g, v, &bd)) {
        if (bd < 5.5f) want = 0;
        else want = fminf(want, 3.0f);
    }
    if (want < 0.5f && fabsf(v->speed) < 0.5f) {
        v->stuck += dt;
        if (v->stuck > 4.0f) { v->ghost = 2.5f; v->stuck = 0; }
    } else v->stuck = 0;
    v->speed = approachf(v->speed, want, (want < v->speed ? 12.0f : 4.0f) * dt);
    {
        float target = atan2f(dx, dz);
        v->h = approach_angle(v->h, target, 2.2f * dt * clampf(v->speed / 4.0f, 0.2f, 1.2f));
    }
    {
        V3 f = fwd_of(v->h);
        v->x += f.x * v->speed * dt;
        v->z += f.z * v->speed * dt;
    }
    v->wheel += v->speed * dt / 0.34f;
}

/* ------------------------------------------------------------------ voetgangers */

static void ped_corner(int bi, int bj, int c, float *x, float *z) {
    float x0 = block_x0(bi) + 1.3f, z0 = block_x0(bj) + 1.3f, x1 = block_x0(bi) + BLOCKW - 1.3f, z1 = block_x0(bj) + BLOCKW - 1.3f;
    c &= 3;
    *x = (c == 0 || c == 3) ? x0 : x1;
    *z = (c < 2) ? z0 : z1;
}

static void spawn_ped_at(Game *g, Ped *p, float x, float z, int flee) {
    p->active = 1;
    p->variant = 1 + rng_int(&g->rng, 7);
    p->bi = (int)clampf(floorf((x - ROADW) / PITCH), 0, NB - 1);
    p->bj = (int)clampf(floorf((z - ROADW) / PITCH), 0, NB - 1);
    p->corner = rng_int(&g->rng, 4);
    p->dsign = rng_f(&g->rng) < 0.5f ? 1 : -1;
    p->x = x; p->z = z; p->y = world_ground_y(g->w, x, z);
    p->speed = rng_range(&g->rng, 1.2f, 1.7f);
    p->state = flee ? PED_FLEE : PED_WALK;
    p->timer = flee ? 3.0f : 0;
    p->h = 0; p->anim = 0; p->lay = 0;
}

static void ped_respawn(Game *g, Ped *p, float px, float pz, int nearOk) {
    int tries;
    for (tries = 0; tries < 20; tries++) {
        int bi = rng_int(&g->rng, NB), bj = rng_int(&g->rng, NB), c = rng_int(&g->rng, 4);
        float x0, z0, x1, z1, t = rng_f(&g->rng), x, z, dx, dz, d;
        ped_corner(bi, bj, c, &x0, &z0);
        ped_corner(bi, bj, c + 1, &x1, &z1);
        x = lerpf(x0, x1, t); z = lerpf(z0, z1, t);
        dx = x - px; dz = z - pz; d = sqrtf(dx * dx + dz * dz);
        if (nearOk ? (d > 110.0f || d < 8.0f) : (d < 45.0f || d > 110.0f)) continue;
        spawn_ped_at(g, p, x, z, 0);
        p->bi = bi; p->bj = bj; p->corner = p->dsign > 0 ? c + 1 : c;
        return;
    }
}

static void update_ped(Game *g, Ped *p, float dt) {
    float px, pz;
    int i;
    player_pos(g, &px, &pz);
    if (p->state == PED_DOWN) {
        p->timer -= dt;
        p->lay = approachf(p->lay, p->timer > 0.6f ? 1.0f : 0.0f, dt * 3.0f);
        if (p->timer <= 0) { p->state = PED_FLEE; p->timer = 2.5f; p->lay = 0; }
        return;
    }
    /* gevaar: voertuigen die eraan komen */
    for (i = 0; i < g->nveh; i++) {
        Vehicle *v = &g->veh[i];
        float rx, rz, al, la, sp;
        V3 f, r;
        if (!v->active) continue;
        sp = fabsf(v->speed);
        if (sp < 2.5f) continue;
        f = fwd_of(v->h); r = right_of(v->h);
        rx = p->x - v->x; rz = p->z - v->z;
        al = rx * f.x + rz * f.z; la = rx * r.x + rz * r.z;
        if (v->speed < 0) al = -al;
        if (al > -1.0f && al < 1.6f + (v->type == VEH_CAR ? 2.0f : 0.6f) && fabsf(la) < (v->type == VEH_CAR ? 1.2f : 0.7f) && sp > 4.0f) {
            p->state = PED_DOWN; p->timer = 2.8f; p->h = v->h;
            if (i == g->p.veh) game_message(g, "Watch out for pedestrians!", 1.5f);
            return;
        }
        if (al > 0 && al < 9.0f && fabsf(la) < 2.5f && p->state != PED_DODGE) {
            p->state = PED_DODGE; p->timer = 0.8f;
            p->h = wrap_angle(v->h + (la > 0 ? -PI_F * 0.5f : PI_F * 0.5f));
        }
    }
    if (p->state == PED_DODGE || p->state == PED_FLEE) {
        float sp = p->state == PED_DODGE ? 4.5f : 3.5f;
        V3 f;
        float nx, nz;
        if (p->state == PED_FLEE) {
            float dx = p->x - px, dz = p->z - pz;
            p->h = approach_angle(p->h, atan2f(dx, dz), 4.0f * dt);
        }
        f = fwd_of(p->h);
        p->x += f.x * sp * dt; p->z += f.z * sp * dt;
        world_push_circle(g->w, &p->x, &p->z, 0.35f, p->y, &nx, &nz);
        p->anim += sp * dt * 2.4f;
        p->timer -= dt;
        if (p->timer <= 0) {
            /* terug naar dichtstbijzijnde stoep-lus */
            p->state = PED_WALK;
            p->bi = (int)clampf(floorf((p->x - ROADW) / PITCH), 0, NB - 1);
            p->bj = (int)clampf(floorf((p->z - ROADW) / PITCH), 0, NB - 1);
        }
        p->y = world_ground_y(g->w, p->x, p->z);
        return;
    }
    if (p->state == PED_IDLE) {
        p->timer -= dt;
        if (p->timer <= 0) p->state = PED_WALK;
        return;
    }
    {
        float tx, tz, dx, dz, d;
        ped_corner(p->bi, p->bj, p->corner, &tx, &tz);
        dx = tx - p->x; dz = tz - p->z; d = sqrtf(dx * dx + dz * dz);
        if (d < 0.4f) {
            p->corner = (p->corner + p->dsign + 4) & 3;
            if (rng_f(&g->rng) < 0.2f) { p->state = PED_IDLE; p->timer = rng_range(&g->rng, 1.0f, 3.0f); }
            return;
        }
        p->h = approach_angle(p->h, atan2f(dx, dz), 5.0f * dt);
        {
            float step = fminf(p->speed * dt, d);
            p->x += dx / d * step; p->z += dz / d * step;
        }
        p->anim += p->speed * dt * 2.6f;
        p->y = world_ground_y(g->w, p->x, p->z);
    }
}

/* ------------------------------------------------------------------ init */

void game_set_quality(Game *g, int q) {
    static const int peds[3] = {6, 10, 14}, traf[3] = {4, 7, 10};
    static const float dd[3] = {95.0f, 135.0f, 180.0f};
    int i;
    if (q < 0) q = 0; if (q > 2) q = 2;
    g->quality = q;
    g->wantPeds = peds[q];
    g->wantTraffic = traf[q];
    g->drawDist = dd[q];
    for (i = g->wantPeds; i < MAX_PED; i++) g->ped[i].active = 0;
    for (i = 0; i < MAX_TRAFFIC; i++) {
        Vehicle *v = &g->veh[g->firstTraffic + i];
        if (i >= g->wantTraffic && v->traffic && (g->firstTraffic + i) != g->p.veh) v->active = 0;
    }
}

void game_init(Game *g, World *w) {
    int i;
    memset(g, 0, sizeof(*g));
    g->w = w;
    g->rng.s = 20261118u;
    g->mission = -1;
    g->p.x = w->poi[POI_SPAWN].x;
    g->p.z = w->poi[POI_SPAWN].z;
    g->p.h = w->spawn_heading;
    g->p.y = world_ground_y(w, g->p.x, g->p.z);
    g->p.veh = -1;
    g->p.onGround = 1;
    g->camYaw = g->p.h;
    g->camPitch = 0.32f;
    g->camDist = 5.0f;
    g->camSens = 1.0f;
    for (i = 0; i < w->npark && g->nveh < MAX_VEH - MAX_TRAFFIC; i++) {
        Vehicle *v = &g->veh[g->nveh++];
        memset(v, 0, sizeof(*v));
        v->active = 1; v->type = w->park[i].type; v->variant = w->park[i].variant;
        v->x = w->park[i].x; v->z = w->park[i].z; v->h = w->park[i].h;
        v->y = world_ground_y(w, v->x, v->z);
    }
    g->firstTraffic = g->nveh;
    for (i = 0; i < MAX_TRAFFIC; i++) { memset(&g->veh[g->nveh], 0, sizeof(Vehicle)); g->nveh++; }
    setup_missions(g);
    game_set_quality(g, 1);
    g->camPos = v3(g->p.x, 3, g->p.z + 5);
}

void game_teleport(Game *g, float x, float z, float h) {
    if (g->p.veh >= 0) {
        Vehicle *v = &g->veh[g->p.veh];
        v->x = x; v->z = z; v->h = h; v->y = world_ground_y(g->w, x, z);
    }
    g->p.x = x; g->p.z = z; g->p.h = h; g->p.y = world_ground_y(g->w, x, z);
    g->camYaw = h;
}

/* ------------------------------------------------------------------ update */

static void update_player_foot(Game *g, const Input *in, float dt) {
    Player *p = &g->p;
    V3 f = fwd_of(g->camYaw), r = right_of(g->camYaw);
    float mx = in->mx, my = in->my;
    float mag = sqrtf(mx * mx + my * my);
    float ground, nx, nz;
    int i;
    if (mag > 1) { mx /= mag; my /= mag; mag = 1; }
    if (mag > 0.12f) {
        V3 d = v3add(v3scale(f, my), v3scale(r, mx));
        p->h = approach_angle(p->h, atan2f(d.x, d.z), 11.0f * dt);
        p->speed = approachf(p->speed, mag * 6.2f, 22.0f * dt);
    } else {
        p->speed = approachf(p->speed, 0, 25.0f * dt);
    }
    {
        V3 hf = fwd_of(p->h);
        p->x += hf.x * p->speed * dt;
        p->z += hf.z * p->speed * dt;
    }
    if (in->jumpPressed && p->onGround) { p->vy = 6.0f; p->onGround = 0; g->statJumps++; }
    p->vy -= 16.0f * dt;
    p->y += p->vy * dt;
    ground = world_ground_y(g->w, p->x, p->z);
    if (p->y <= ground) { p->y = ground; p->vy = 0; p->onGround = 1; }
    else if (p->y > ground + 0.05f) p->onGround = 0;
    world_push_circle(g->w, &p->x, &p->z, 0.35f, p->y, &nx, &nz);
    for (i = 0; i < g->nveh; i++) {
        Vehicle *v = &g->veh[i];
        float dx, dz, d2, rr;
        if (!v->active) continue;
        dx = p->x - v->x; dz = p->z - v->z; d2 = dx * dx + dz * dz;
        rr = 0.35f + (v->type == VEH_BIKE ? 0.35f : 1.0f);
        if (v->type == VEH_CAR) {
            /* auto als twee cirkels */
            V3 vf = fwd_of(v->h);
            int k;
            for (k = -1; k <= 1; k += 2) {
                float cx = v->x + vf.x * 1.1f * k, cz = v->z + vf.z * 1.1f * k;
                dx = p->x - cx; dz = p->z - cz; d2 = dx * dx + dz * dz;
                if (d2 < rr * rr && d2 > 1e-6f) { float d = sqrtf(d2); p->x += dx / d * (rr - d); p->z += dz / d * (rr - d); }
            }
        } else if (d2 < rr * rr && d2 > 1e-6f) {
            float d = sqrtf(d2); p->x += dx / d * (rr - d); p->z += dz / d * (rr - d);
        }
    }
    for (i = 0; i < MAX_PED; i++) {
        Ped *q = &g->ped[i];
        float dx, dz, d2;
        if (!q->active || q->state == PED_DOWN) continue;
        dx = p->x - q->x; dz = p->z - q->z; d2 = dx * dx + dz * dz;
        if (d2 < 0.6f * 0.6f && d2 > 1e-6f) { float d = sqrtf(d2); p->x += dx / d * (0.6f - d) * 0.5f; p->z += dz / d * (0.6f - d) * 0.5f; q->x -= dx / d * (0.6f - d) * 0.5f; q->z -= dz / d * (0.6f - d) * 0.5f; }
    }
    p->anim += p->speed * dt * 2.3f;
}

static void update_camera(Game *g, const Input *in, float dt) {
    int mode = game_mode(g);
    V3 target;
    float wantDist = mode == MODE_CAR ? 8.0f : (mode == MODE_BIKE ? 5.8f : 4.6f);
    float lx = in->lx, ly = in->ly;
    if (mode == MODE_FOOT) target = v3(g->p.x, g->p.y + 1.55f, g->p.z);
    else {
        Vehicle *v = &g->veh[g->p.veh];
        target = v3(v->x, v->y + (mode == MODE_CAR ? 1.5f : 1.6f), v->z);
    }
    g->camYaw = wrap_angle(g->camYaw - lx * 2.6f * g->camSens * dt);
    g->camPitch = clampf(g->camPitch + ly * 1.5f * g->camSens * dt * (g->invertY ? 1.0f : -1.0f), -0.15f, 1.15f);
    if (fabsf(lx) > 0.08f || fabsf(ly) > 0.08f) g->camIdle = 0; else g->camIdle += dt;
    if (mode != MODE_FOOT && g->camIdle > 0.8f) {
        Vehicle *v = &g->veh[g->p.veh];
        if (fabsf(v->speed) > 1.5f) {
            float behind = v->speed >= 0 ? v->h : wrap_angle(v->h + PI_F);
            g->camYaw = approach_angle(g->camYaw, behind, 2.2f * dt * clampf(fabsf(v->speed) / 6.0f, 0.3f, 1.5f));
            g->camPitch = approachf(g->camPitch, 0.28f, 0.5f * dt);
        }
    }
    g->camDist = lerpf(g->camDist, wantDist, clampf(dt * 3.0f, 0, 1));
    {
        V3 f = fwd_of(g->camYaw);
        float cp = cosf(g->camPitch), sp = sinf(g->camPitch);
        V3 want = v3(target.x - f.x * g->camDist * cp, target.y + g->camDist * sp, target.z - f.z * g->camDist * cp);
        float t = world_raycast(g->w, target, want);
        float gy;
        if (t < 1.0f) {
            float tt = fmaxf(0.08f, t - 0.6f / (g->camDist + 0.01f));
            want = v3lerp(target, want, tt);
        }
        gy = world_ground_y(g->w, want.x, want.z) + 0.35f;
        if (want.y < gy) want.y = gy;
        g->camPos = want;
        g->camTarget = target;
    }
}

void game_update(Game *g, const Input *in, float dt) {
    int i;
    float px, pz;
    if (dt > 0.1f) dt = 0.1f;
    g->time += dt;
    if (g->p.cool > 0) g->p.cool -= dt;
    if (g->msgT > 0) g->msgT -= dt;
    if (g->bannerT > 0) g->bannerT -= dt;
    if (g->moneyFlash > 0) g->moneyFlash -= dt;

    /* in-/uitstappen */
    if (in->actionPressed) {
        if (g->p.veh >= 0) exit_vehicle(g);
        else {
            int v = game_near_vehicle(g);
            if (v >= 0) enter_vehicle(g, v);
            else game_message(g, "No vehicle nearby", 1.2f);
        }
    }

    if (g->p.veh >= 0) {
        Vehicle *v = &g->veh[g->p.veh];
        float thr = in->gas ? 1.0f : 0.0f, brk = in->brake ? 1.0f : 0.0f;
        /* stick naar voren werkt ook als gas */
        if (!in->gas && in->my > 0.5f) thr = (in->my - 0.3f) / 0.7f;
        if (!in->brake && in->my < -0.5f) brk = 1.0f;
        drive_vehicle(g, v, thr, brk, clampf(in->mx * 1.15f, -1, 1), dt);
        g->p.x = v->x; g->p.z = v->z; g->p.y = v->y; g->p.h = v->h;
        g->p.anim += fabsf(v->speed) * dt * 1.2f;
    } else {
        update_player_foot(g, in, dt);
    }

    player_pos(g, &px, &pz);

    /* verkeer: pooling rond de speler */
    for (i = 0; i < MAX_TRAFFIC; i++) {
        int idx = g->firstTraffic + i;
        Vehicle *v = &g->veh[idx];
        if (idx == g->p.veh) continue;
        if (i >= g->wantTraffic) { if (v->traffic) v->active = 0; continue; }
        if (!v->active || !v->traffic) {
            if (!v->active || (v->x - px) * (v->x - px) + (v->z - pz) * (v->z - pz) > 200.0f * 200.0f) {
                memset(v, 0, sizeof(*v));
                traffic_spawn(g, v, px, pz, g->time < 1.0f);
            }
            continue;
        }
        {
            float dx = v->x - px, dz = v->z - pz;
            if (dx * dx + dz * dz > 175.0f * 175.0f) { traffic_spawn(g, v, px, pz, 0); continue; }
        }
        update_traffic_car(g, v, dt);
    }

    /* voetgangers */
    for (i = 0; i < g->wantPeds && i < MAX_PED; i++) {
        Ped *p = &g->ped[i];
        if (!p->active) { ped_respawn(g, p, px, pz, g->time < 1.0f); continue; }
        {
            float dx = p->x - px, dz = p->z - pz;
            if (dx * dx + dz * dz > 125.0f * 125.0f) { ped_respawn(g, p, px, pz, 0); continue; }
        }
        update_ped(g, p, dt);
    }

    /* geld oppakken */
    for (i = 0; i < g->w->ncash; i++) {
        float dx, dz, lim;
        if (g->cashTimer[i] > 0) { g->cashTimer[i] -= dt; continue; }
        dx = g->w->cash[i].x - px; dz = g->w->cash[i].z - pz;
        lim = g->p.veh >= 0 ? 2.2f : 1.3f;
        if (dx * dx + dz * dz < lim * lim) {
            int val = 20 + (i * 7) % 31;
            char b[32];
            g->money += val;
            g->moneyFlash = 1.2f;
            g->cashTimer[i] = 120.0f;
            g->statCash++;
            snprintf(b, sizeof(b), "+$%d", val);
            game_message(g, b, 1.2f);
        }
    }

    update_missions(g, dt);
    update_camera(g, in, dt);

    /* geldteller loopt op */
    if (g->moneyShown < g->money) {
        long d = (g->money - g->moneyShown) / 8 + 1;
        g->moneyShown += d;
    } else g->moneyShown = g->money;
}
