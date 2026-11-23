#include "phys.h"
#include <math.h>
#include <string.h>

/* ------------------------------------------------------------------ onderdelen */

static const char *PNAME[PP_COUNT] = {"ERASE", "BLOCK", "SEAT", "MOTOR", "WHEEL", "WING", "JET"};
static const float PMASS[PP_COUNT] = {0, 70, 90, 140, 30, 20, 60};

const char *ph_part_name(int t) { return t >= 0 && t < PP_COUNT ? PNAME[t] : "?"; }

/* schansen in het testterrein oost van de stad (hoogte loopt langs z) */
const PRamp PH_RAMP[PH_RAMPS] = {
    {452, 90, 472, 112, 0, 2.5f},      /* kleine schans */
    {492, 90, 518, 130, 0, 6},         /* middel */
    {540, 80, 580, 150, 0, 12},        /* grote schans */
    {600, 70, 650, 190, 0, 26},        /* mega-schans */
    {600, 190, 650, 260, 26, 26},      /* plateau erachter */
    {600, 260, 650, 330, 26, 0},       /* afdaling */
    {452, 180, 474, 196, 0, 1.2f},     /* drempels */
    {452, 215, 474, 231, 0, 1.2f},
    {452, 250, 474, 266, 0, 1.2f},
    {500, 200, 530, 250, 0, 4},
    {500, 250, 530, 300, 4, 0},
    {540, 210, 580, 260, 0, 8},
};

float ph_ramp_height(float x, float z) {
    int i;
    float h = 0;
    for (i = 0; i < PH_RAMPS; i++) {
        const PRamp *r = &PH_RAMP[i];
        float t, y;
        if (x < r->x0 || x > r->x1 || z < r->z0 || z > r->z1) continue;
        t = (z - r->z0) / (r->z1 - r->z0);
        y = r->h0 + (r->h1 - r->h0) * t;
        if (y > h) h = y;
    }
    return h;
}

/* ------------------------------------------------------------------ quaternions */

static Quat q_mul(Quat a, Quat b) {
    Quat r;
    r.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
    r.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
    r.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
    r.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
    return r;
}
static Quat q_norm(Quat q) {
    float l = sqrtf(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    if (l < 1e-6f) { q.w = 1; q.x = q.y = q.z = 0; return q; }
    q.w /= l; q.x /= l; q.y /= l; q.z /= l;
    return q;
}
static V3 q_rot(Quat q, V3 v) {
    V3 u = v3(q.x, q.y, q.z), t = v3scale(v3cross(u, v), 2.0f);
    return v3add(v3add(v, v3scale(t, q.w)), v3cross(u, t));
}
static V3 q_rot_inv(Quat q, V3 v) { Quat c = q; c.x = -c.x; c.y = -c.y; c.z = -c.z; return q_rot(c, v); }
static Quat q_yaw(float h) { Quat q; q.w = cosf(h * 0.5f); q.x = 0; q.y = sinf(h * 0.5f); q.z = 0; return q; }

M4 ph_quat_matrix(Quat q, V3 pos) {
    M4 m = m4_identity();
    float x = q.x, y = q.y, z = q.z, w = q.w;
    m.m[0] = 1 - 2 * (y * y + z * z); m.m[1] = 2 * (x * y + z * w);     m.m[2] = 2 * (x * z - y * w);
    m.m[4] = 2 * (x * y - z * w);     m.m[5] = 1 - 2 * (x * x + z * z); m.m[6] = 2 * (y * z + x * w);
    m.m[8] = 2 * (x * z + y * w);     m.m[9] = 2 * (y * z - x * w);     m.m[10] = 1 - 2 * (x * x + y * y);
    m.m[12] = pos.x; m.m[13] = pos.y; m.m[14] = pos.z;
    return m;
}
M4 ph_matrix(const Craft *c) { return ph_quat_matrix(c->q, c->pos); }
M4 ph_model_matrix(const Craft *c) { return m4_mul(ph_quat_matrix(c->q, c->pos), m4_translate(-c->com0.x, -c->com0.y, -c->com0.z)); }
V3 ph_forward(const Craft *c) { return q_rot(c->q, v3(0, 0, 1)); }
V3 ph_to_world(const Craft *c, V3 l) { return v3add(c->pos, q_rot(c->q, l)); }

/* ------------------------------------------------------------------ ontwerp */

V3 ph_cell_pos(int gx, int gy, int gz) { return v3((gx - PH_GX / 2) * PH_CELL, gy * PH_CELL + 0.45f, (gz - PH_GZ / 2) * PH_CELL); }
#define cell_pos ph_cell_pos

void ph_default(PDesign *d) {
    int z;
    memset(d, 0, sizeof(*d));
    for (z = 2; z <= 6; z++) { d->c[0][z][2] = PP_BLOCK; d->c[0][z][3] = PP_BLOCK; d->c[0][z][4] = PP_BLOCK; }
    d->c[1][4][3] = PP_SEAT;
    d->c[0][3][3] = PP_MOTOR;
    d->c[0][2][1] = PP_WHEEL; d->c[0][2][5] = PP_WHEEL;
    d->c[0][6][1] = PP_WHEEL; d->c[0][6][5] = PP_WHEEL;
}

/* v6.0.3: presets (x 0..6 links-rechts, z 0..8 achter-voor, y = laag) */
const char *ph_preset(PDesign *d, int idx) {
    int x, z;
    memset(d, 0, sizeof(*d));
    switch ((idx % PH_NPRESETS + PH_NPRESETS) % PH_NPRESETS) {
    case 0:
        ph_default(d);
        return "SIMPLE CAR";
    case 1:   /* vliegende motor: smal lijf, 4 smalle wielen, vleugels opzij, jet achter */
        for (z = 1; z <= 7; z++) d->c[0][z][3] = PP_BLOCK;
        d->c[0][3][3] = PP_MOTOR;
        d->c[1][4][3] = PP_SEAT;
        d->c[1][6][3] = PP_BLOCK;
        d->c[0][1][2] = PP_WHEEL; d->c[0][1][4] = PP_WHEEL;
        d->c[0][7][2] = PP_WHEEL; d->c[0][7][4] = PP_WHEEL;
        for (x = 0; x <= 1; x++) { d->c[0][4][x] = PP_WING; d->c[0][5][x] = PP_WING; d->c[0][4][6 - x] = PP_WING; d->c[0][5][6 - x] = PP_WING; }
        d->c[0][0][3] = PP_JET;
        return "FLYING MOTOR";
    case 2:   /* vliegtuig: romp, grote vleugels, staart, twee jets, driewielig landingsgestel */
        for (z = 1; z <= 8; z++) d->c[0][z][3] = PP_BLOCK;
        d->c[1][6][3] = PP_SEAT;
        for (x = 0; x <= 2; x++) { d->c[0][4][x] = PP_WING; d->c[0][5][x] = PP_WING; d->c[0][4][6 - x] = PP_WING; d->c[0][5][6 - x] = PP_WING; }
        d->c[0][1][2] = PP_WING; d->c[0][1][4] = PP_WING;
        d->c[1][1][3] = PP_WING;
        d->c[0][0][2] = PP_JET; d->c[0][0][4] = PP_JET;
        d->c[0][7][2] = PP_WHEEL; d->c[0][2][2] = PP_WHEEL; d->c[0][2][4] = PP_WHEEL;
        d->c[0][3][3] = PP_MOTOR;
        return "PLANE";
    case 3:   /* jet-auto: snel, motor + twee jets */
        ph_default(d);
        d->c[0][4][3] = PP_MOTOR;
        d->c[1][2][2] = PP_JET; d->c[1][2][4] = PP_JET;
        return "JET CAR";
    case 4:   /* zes wielen, twee motoren */
        for (z = 1; z <= 7; z++) for (x = 2; x <= 4; x++) d->c[0][z][x] = PP_BLOCK;
        d->c[0][3][3] = PP_MOTOR; d->c[0][4][3] = PP_MOTOR;
        d->c[1][6][3] = PP_SEAT;
        for (z = 1; z <= 7; z += 3) { d->c[0][z][1] = PP_WHEEL; d->c[0][z][5] = PP_WHEEL; }
        for (z = 1; z <= 4; z++) for (x = 2; x <= 4; x++) d->c[1][z][x] = PP_BLOCK;
        return "6-WHEEL TRUCK";
    default:  /* zweefplatform: geen wielen, opstijgen/dalen met de knoppen */
        for (z = 2; z <= 6; z++) for (x = 1; x <= 5; x++) d->c[0][z][x] = PP_BLOCK;
        d->c[1][4][3] = PP_SEAT;
        d->c[0][2][0] = PP_WING; d->c[0][6][0] = PP_WING; d->c[0][2][6] = PP_WING; d->c[0][6][6] = PP_WING;
        d->c[0][1][3] = PP_JET;
        return "HOVER PLATFORM";
    }
}

void ph_stats(const PDesign *d, float *mass, int *motors, int *wheels, int *wings, int *jets, int *seats) {
    int x, y, z;
    *mass = 0; *motors = *wheels = *wings = *jets = *seats = 0;
    for (y = 0; y < PH_GY; y++) for (z = 0; z < PH_GZ; z++) for (x = 0; x < PH_GX; x++) {
        int t = d->c[y][z][x];
        if (t <= 0 || t >= PP_COUNT) continue;
        *mass += PMASS[t];
        if (t == PP_MOTOR) (*motors)++;
        if (t == PP_WHEEL) (*wheels)++;
        if (t == PP_WING) (*wings)++;
        if (t == PP_JET) (*jets)++;
        if (t == PP_SEAT) (*seats)++;
    }
}

/* massa, zwaartepunt en traagheid opnieuw berekenen (ook na afbreken) */
static void mass_props(Craft *c) {
    int i;
    V3 com = v3(0, 0, 0);
    float M = 0;
#define PM(pp) ((pp)->m > 0 ? (pp)->m : PMASS[(pp)->type])
    for (i = 0; i < c->np; i++) if (c->part[i].alive) { M += PM(&c->part[i]); com = v3add(com, v3scale(c->part[i].local, PM(&c->part[i]))); }
    if (M < 1) M = 1;
    com = v3scale(com, 1.0f / M);
    /* het lichaam verschuift naar het nieuwe zwaartepunt */
    c->pos = v3add(c->pos, q_rot(c->q, com));
    c->inertia = v3(0, 0, 0);
    c->nmotor = c->njet = c->nwing = c->nwheel = 0;
    for (i = 0; i < c->np; i++) {
        PPart *p = &c->part[i];
        float m = PM(p), b = m * (p->half.x > 0 ? (p->half.x * p->half.x + p->half.z * p->half.z) * 0.6f : PH_CELL * PH_CELL / 6.0f);
        if (!p->alive) continue;
        p->local = v3sub(p->local, com);
        c->inertia.x += m * (p->local.y * p->local.y + p->local.z * p->local.z) + b;
        c->inertia.y += m * (p->local.x * p->local.x + p->local.z * p->local.z) + b;
        c->inertia.z += m * (p->local.x * p->local.x + p->local.y * p->local.y) + b;
        if (p->type == PP_MOTOR) c->nmotor++;
        if (p->type == PP_JET) c->njet++;
        if (p->type == PP_WING) c->nwing++;
        if (p->type == PP_WHEEL) c->nwheel++;
    }
    for (i = 0; i < c->np; i++) if (c->part[i].type == PP_WHEEL) c->part[i].front = c->part[i].local.z > 0.15f;
    c->mass = M; c->invMass = 1.0f / M;
}

void ph_spawn(Craft *c, const PDesign *d, V3 pos, float heading) {
    int x, y, z;
    float miny = 1e9f;
    memset(c, 0, sizeof(*c));
    c->d = *d;
    for (y = 0; y < PH_GY; y++) for (z = 0; z < PH_GZ; z++) for (x = 0; x < PH_GX; x++) {
        int t = d->c[y][z][x];
        if (t <= 0 || t >= PP_COUNT || c->np >= PH_MAXP) continue;
        c->part[c->np].type = t;
        c->part[c->np].alive = 1;
        c->part[c->np].local = cell_pos(x, y, z);
        if (c->part[c->np].local.y < miny) miny = c->part[c->np].local.y;
        if (t == PP_SEAT && !c->seat) c->seat = c->np + 1;
        c->np++;
    }
    c->q = q_yaw(heading);
    c->pos = v3(pos.x, pos.y, pos.z);
    mass_props(c);   /* verschuift pos met het zwaartepunt */
    c->pos.y = pos.y + 1.0f + (c->pos.y - pos.y) - miny;
    c->active = 1;
    c->meshDirty = 1;
}

static void add_part(Craft *c, int type, V3 at, V3 half, float m) {
    PPart *p;
    if (c->np >= PH_MAXP) return;
    p = &c->part[c->np++];
    memset(p, 0, sizeof(*p));
    p->type = type; p->alive = 1; p->local = at; p->half = half; p->m = m;
}

void ph_spawn_vehicle(Craft *c, int kind, int fly, V3 pos, float heading, float driveF, float vmax) {
    V3 before;
    int k;
    memset(c, 0, sizeof(*c));
    if (kind == PV_CAR) {
        add_part(c, PP_BLOCK, v3(0, 0.66f, 0.0f), v3(0.9f, 0.3f, 2.0f), 1050);
        add_part(c, PP_BLOCK, v3(0, 1.22f, -0.2f), v3(0.78f, 0.26f, 0.95f), 150);
        for (k = 0; k < 4; k++) add_part(c, PP_WHEEL, v3(k & 1 ? 0.84f : -0.84f, 0.64f, k < 2 ? 1.3f : -1.3f), v3(0.14f, 0.34f, 0.34f), 25);
        c->susp = 0.3f;
        if (fly) {
            add_part(c, PP_WING, v3(-1.45f, 0.8f, -0.1f), v3(0.6f, 0.04f, 0.45f), 30);
            add_part(c, PP_WING, v3(1.45f, 0.8f, -0.1f), v3(0.6f, 0.04f, 0.45f), 30);
            add_part(c, PP_JET, v3(0, 0.85f, -2.2f), v3(0.25f, 0.25f, 0.3f), 40);
        }
    } else {
        int bike = kind == PV_BIKE;
        float r = bike ? 0.34f : 0.33f, wz = bike ? 0.55f : 0.71f;
        add_part(c, PP_BLOCK, v3(0, 0.62f, 0.0f), v3(bike ? 0.1f : 0.18f, 0.28f, bike ? 0.5f : 0.75f), bike ? 15.0f : 190.0f);
        add_part(c, PP_SEAT, v3(0, 1.25f, -0.15f), v3(0.18f, 0.4f, 0.18f), 75);
        add_part(c, PP_WHEEL, v3(0, r + 0.25f, wz), v3(0.06f, r, r), bike ? 3.0f : 15.0f);
        add_part(c, PP_WHEEL, v3(0, r + 0.25f, -wz), v3(0.06f, r, r), bike ? 3.0f : 15.0f);
        c->susp = 0.25f;
        c->balance = 1;
        if (fly) {
            add_part(c, PP_WING, v3(-0.9f, 0.7f, -0.05f), v3(0.5f, 0.04f, 0.35f), 15);
            add_part(c, PP_WING, v3(0.9f, 0.7f, -0.05f), v3(0.5f, 0.04f, 0.35f), 15);
            add_part(c, PP_JET, v3(0, 0.65f, bike ? -0.75f : -1.0f), v3(0.18f, 0.18f, 0.25f), 25);
        }
    }
    c->noBreak = 1;
    c->driveF = driveF; c->vmax = vmax;
    c->q = q_yaw(heading);
    c->pos = pos;
    before = c->pos;
    mass_props(c);
    c->com0 = q_rot_inv(c->q, v3sub(c->pos, before));
    c->active = 1;
}

void ph_reset_upright(Craft *c, PhGround ground, void *ctx) {
    V3 f = ph_forward(c);
    float h = atan2f(f.x, f.z);
    c->q = q_yaw(h);
    c->vel = v3(0, 0, 0); c->angv = v3(0, 0, 0);
    c->pos.y = ground(ctx, c->pos.x, c->pos.z) + 2.5f;
}

/* ------------------------------------------------------------------ simulatie */

static void add_force(V3 *F, V3 *T, V3 f, V3 r) { *F = v3add(*F, f); *T = v3add(*T, v3cross(r, f)); }

static void break_part(Craft *c, int i, V3 vel) {
    int k;
    PPart *p = &c->part[i];
    if (!p->alive) return;
    p->alive = 0;
    c->broken++;
    for (k = 0; k < PH_DEBRIS; k++) if (!c->deb[k].active) {
        PDebris *d = &c->deb[k];
        d->active = 1; d->type = p->type;
        d->pos = ph_to_world(c, p->local);
        d->vel = v3add(vel, v3(0, 2.5f, 0));
        d->angv = v3(3.0f * ((i * 7) % 5 - 2), 4.0f, 2.0f * ((i * 3) % 5 - 2));
        d->q = c->q; d->life = 12.0f;
        break;
    }
    if (p->type == PP_SEAT) c->crashed = 1;
    mass_props(c);
    c->meshDirty = 1;
}

void ph_step(Craft *c, float gas, float brk, float steer, int handbrake, float pitch, float roll, float lift, float dt,
             PhGround ground, PhPush push, void *ctx) {
    int sub, ns = 6, i, k;
    float h;
    if (!c->active) return;
    if (dt > 0.05f) dt = 0.05f;
    h = dt / ns;
    c->throttle = gas; c->brake = brk; c->hb = (float)handbrake; c->pitch = pitch; c->roll = roll;
    c->steerAng = approachf(c->steerAng, steer, 3.0f * dt);
    c->impact = 0;
    for (sub = 0; sub < ns; sub++) {
        V3 F = v3(0, -9.81f * c->mass, 0), T = v3(0, 0, 0);
        V3 up = q_rot(c->q, v3(0, 1, 0)), fwd = q_rot(c->q, v3(0, 0, 1)), rt = q_rot(c->q, v3(1, 0, 0));
        float wheelK = c->nwheel ? c->mass * 9.81f / c->nwheel / 0.12f : 0;
        float wheelC = c->nwheel ? 2.0f * sqrtf(wheelK * c->mass / c->nwheel) * 0.55f : 0;
        float drive = c->driveF > 0 ? c->driveF : c->nmotor * 5200.0f, vmax = c->vmax > 0 ? c->vmax : 26.0f + c->nmotor * 9.0f;
        int motorOk = c->driveF > 0 || c->nmotor > 0;
        int ndrive = 0, breakIdx = -1;
        float breakV = 0;
        V3 breakVel = v3(0, 0, 0);
        for (i = 0; i < c->np; i++) if (c->part[i].alive && c->part[i].type == PP_WHEEL) ndrive++;
        /* --- onderdelen: wielen, contact, vleugels, jets */
        for (i = 0; i < c->np; i++) {
            PPart *p = &c->part[i];
            V3 r, wp, vp;
            if (!p->alive) continue;
            r = q_rot(c->q, p->local);
            wp = v3add(c->pos, r);
            vp = v3add(c->vel, v3cross(c->angv, r));
            if (p->type == PP_WHEEL) {
                const float Ls = c->susp > 0 ? c->susp : 0.35f, rw = p->half.y > 0 ? p->half.y : 0.42f;
                float gy = ground(ctx, wp.x, wp.z), d, comp;
                p->comp = approachf(p->comp, 0, h * 2.0f);
                if (up.y < 0.25f) continue;
                d = (wp.y - gy) / up.y;
                comp = Ls + rw - d;
                if (comp > 0) {
                    float vup = vp.y, N = wheelK * comp - wheelC * vup;
                    float a = p->front ? -c->steerAng * 0.55f : 0.0f;
                    V3 wd = v3add(v3scale(fwd, cosf(a)), v3scale(rt, sinf(a))), ld;
                    float vlong, vlat, mu, Flat, Flong;
                    if (N < 0) N = 0;
                    if (comp > Ls + rw) comp = Ls + rw;
                    p->comp = comp;
                    wd.y = 0; wd = v3norm(wd);
                    ld = v3(wd.z, 0, -wd.x);
                    vlong = v3dot(vp, wd); vlat = v3dot(vp, ld);
                    mu = (!p->front && handbrake) ? 0.32f : 1.15f;   /* handrem: achterwielen glijden (driften) */
                    Flat = clampf(-vlat * 14.0f * N / 1000.0f * 100.0f, -mu * N, mu * N);
                    Flong = 0;
                    if (gas > 0 && motorOk && (!c->balance || !p->front)) Flong += gas * drive / (c->balance ? 1 : ndrive) * clampf(1.0f - vlong / vmax, 0, 1);
                    if (gas < 0 && motorOk && (!c->balance || !p->front)) Flong += gas * drive * 0.5f / (c->balance ? 1 : ndrive) * (vlong > -6.0f ? 1.0f : 0.0f);
                    if (brk > 0 || (handbrake && !p->front)) Flong -= clampf(vlong * N * 3.0f, -0.9f * N, 0.9f * N) * (handbrake && !p->front ? 1.0f : brk);
                    Flong -= vlong * 0.015f * N;
                    Flong = clampf(Flong, -mu * N, mu * N);
                    add_force(&F, &T, v3add(v3add(v3(0, N, 0), v3scale(wd, Flong)), v3scale(ld, Flat)), v3sub(v3(wp.x, gy, wp.z), c->pos));
                    p->spin += vlong / rw * h;
                } else p->spin += v3dot(vp, fwd) / rw * h * 0.98f;
                continue;
            }
            /* --- contactpunten van het blok (grond + gebouwen) */
            {
                static const float cx[8] = {-1, 1, -1, 1, -1, 1, -1, 1}, cy[8] = {-1, -1, 1, 1, -1, -1, 1, 1}, cz[8] = {-1, -1, -1, -1, 1, 1, 1, 1};
                float hs = 0.3f, hy = p->type == PP_WING ? 0.05f : 0.3f, hz = 0.3f;
                if (p->half.x > 0) { hs = p->half.x; hy = p->half.y; hz = p->half.z; }
                for (k = 0; k < 8; k++) {
                    V3 lc = v3add(p->local, v3(cx[k] * hs, cy[k] * hy, cz[k] * hz));
                    V3 rc = q_rot(c->q, lc), pc = v3add(c->pos, rc), vc = v3add(c->vel, v3cross(c->angv, rc)), n = v3(0, 1, 0);
                    float gy = ground(ctx, pc.x, pc.z), pen = gy - pc.y;
                    if (pen <= 0) {
                        float px = pc.x, pz = pc.z;
                        if (push && push(ctx, &px, &pz, pc.y)) {
                            V3 dd = v3(px - pc.x, 0, pz - pc.z);
                            pen = v3len(dd);
                            if (pen < 1e-4f) continue;
                            n = v3scale(dd, 1.0f / pen);
                        } else continue;
                    }
                    {
                        float vn = v3dot(vc, n), Fn = c->mass * (320.0f * pen) - c->mass * 22.0f * vn / 8.0f;
                        V3 vt = v3sub(vc, v3scale(n, vn));
                        float vtl = v3len(vt);
                        if (Fn < 0) Fn = 0;
                        add_force(&F, &T, v3scale(n, Fn), rc);
                        if (vtl > 1e-3f) add_force(&F, &T, v3scale(vt, -fminf(vtl * c->mass * 2.5f, 0.75f * Fn) / vtl), rc);
                        if (-vn > c->impact) c->impact = -vn;
                        /* crash: harde klap = onderdeel breekt af (de stoel houdt het langer) */
                        if (!c->noBreak && -vn > (p->type == PP_SEAT ? 17.0f : 11.0f) && -vn > breakV) { breakV = -vn; breakIdx = i; breakVel = c->vel; }
                    }
                }
            }
            if (p->type == PP_WING) {   /* lift (loodrecht op de vleugel) en weerstand */
                float vf = v3dot(vp, fwd), sp = v3len(vp), aoa, Cl, L;
                if (sp > 1.0f && vf > 0) {
                    aoa = asinf(clampf(-v3dot(vp, up) / sp, -1, 1));
                    Cl = clampf(0.35f + 5.0f * aoa, -1.2f, 1.4f);
                    L = 0.5f * 1.2f * vf * vf * Cl * 0.36f * 4.4f;
                    add_force(&F, &T, v3add(v3scale(up, L), v3scale(fwd, -fabsf(L) * 0.06f)), r);
                }
            }
            if (p->type == PP_JET && gas > 0) add_force(&F, &T, v3scale(fwd, gas * 9000.0f), r);
        }
        /* --- luchtweerstand + roerwerking van vleugels (pitch/rol) */
        {
            float sp = v3len(c->vel);
            F = v3add(F, v3scale(c->vel, -0.5f * 1.2f * 0.9f * sp));
            if (c->nwing > 0) {
                float vf = fmaxf(v3dot(c->vel, fwd), 0), q = 0.5f * 1.2f * vf * vf * 0.36f * c->nwing * 0.35f;
                T = v3add(T, v3scale(rt, -pitch * q));
                T = v3add(T, v3scale(fwd, -roll * q * 0.8f));
                T = v3add(T, v3scale(up, -steer * q * 0.25f));
            }
        }
        /* --- v6.0.3 vleugels als zweefmotor: opstijgen/dalen, langzaam zakken, rechtop blijven */
        if (c->nwing > 0) {
            float g = c->mass * 9.81f, gy = ground(ctx, c->pos.x, c->pos.z), alt = c->pos.y - gy;
            if (lift > 0.01f) F.y += lift * fmaxf(c->nwing * 2600.0f, g * 1.4f);
            else if (lift < -0.01f) F.y += lift * g * 0.8f;
            else if (alt > 1.2f) F.y += g * 0.88f;   /* zweven: langzaam zakken */
            if (alt > 0.8f || lift > 0.01f) {   /* gyro: houdt het voertuig rechtop */
                V3 ax = v3cross(up, v3(0, 1, 0));
                float k = (c->inertia.x + c->inertia.z) * 0.5f * 14.0f;
                T = v3add(T, v3scale(ax, k));
                T = v3sub(T, v3scale(v3(c->angv.x, 0, c->angv.z), (c->inertia.x + c->inertia.z) * 0.5f * 3.0f));
            }
        }
        if (c->balance) {   /* motor/fiets: rechtop blijven, in de bocht hellen */
            float sp = v3len(c->vel), lean = -c->steerAng * 0.5f * clampf(sp / 9.0f, 0, 1);
            V3 rh = v3norm(v3(rt.x, 0, rt.z)), tu = v3add(v3scale(v3(0, 1, 0), cosf(lean)), v3scale(rh, sinf(lean)));
            V3 ax = v3cross(up, tu);
            float kI = (c->inertia.x + c->inertia.z) * 0.5f;
            T = v3add(T, v3scale(ax, kI * 60.0f));
            T = v3sub(T, v3scale(v3sub(c->angv, v3scale(up, v3dot(c->angv, up))), kI * 9.0f));
        }
        /* --- integreren */
        c->vel = v3add(c->vel, v3scale(F, c->invMass * h));
        {
            V3 tl = q_rot_inv(c->q, T), al = v3(tl.x / c->inertia.x, tl.y / c->inertia.y, tl.z / c->inertia.z);
            c->angv = v3add(c->angv, v3scale(q_rot(c->q, al), h));
        }
        c->angv = v3scale(c->angv, 1.0f - 0.5f * h);
        c->pos = v3add(c->pos, v3scale(c->vel, h));
        {
            Quat w; w.w = 0; w.x = c->angv.x; w.y = c->angv.y; w.z = c->angv.z;
            w = q_mul(w, c->q);
            c->q.w += 0.5f * h * w.w; c->q.x += 0.5f * h * w.x; c->q.y += 0.5f * h * w.y; c->q.z += 0.5f * h * w.z;
            c->q = q_norm(c->q);
        }
        if (breakIdx >= 0) break_part(c, breakIdx, breakVel);
    }
    c->speed = v3len(c->vel);
    c->alt = c->pos.y - ground(ctx, c->pos.x, c->pos.z);
    if (c->crashed) c->crashT += dt;
    /* losse brokstukken: vallen, stuiteren, verdwijnen */
    for (k = 0; k < PH_DEBRIS; k++) {
        PDebris *d = &c->deb[k];
        float gy;
        if (!d->active) continue;
        d->vel.y -= 9.81f * dt;
        d->pos = v3add(d->pos, v3scale(d->vel, dt));
        gy = ground(ctx, d->pos.x, d->pos.z) + 0.3f;
        if (d->pos.y < gy) { d->pos.y = gy; d->vel = v3(d->vel.x * 0.6f, -d->vel.y * 0.3f, d->vel.z * 0.6f); d->angv = v3scale(d->angv, 0.7f); }
        {
            Quat w; w.w = 0; w.x = d->angv.x; w.y = d->angv.y; w.z = d->angv.z;
            w = q_mul(w, d->q);
            d->q.w += 0.5f * dt * w.w; d->q.x += 0.5f * dt * w.x; d->q.y += 0.5f * dt * w.y; d->q.z += 0.5f * dt * w.z;
            d->q = q_norm(d->q);
        }
        d->life -= dt;
        if (d->life <= 0) d->active = 0;
    }
}
