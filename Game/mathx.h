/* Kleine wiskundebibliotheek: vectoren, 4x4-matrices (kolom-major, zoals OpenGL). */
#ifndef SGB_MATHX_H
#define SGB_MATHX_H

#include <math.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define PI_F 3.14159265f

typedef struct { float x, y, z; } V3;
typedef struct { float m[16]; } M4;

static inline V3 v3(float x, float y, float z) { V3 r; r.x = x; r.y = y; r.z = z; return r; }
static inline V3 v3add(V3 a, V3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline V3 v3sub(V3 a, V3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline V3 v3scale(V3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static inline float v3dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline V3 v3cross(V3 a, V3 b) {
    return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
static inline float v3len(V3 a) { return sqrtf(v3dot(a, a)); }
static inline V3 v3norm(V3 a) { float l = v3len(a); return l > 1e-6f ? v3scale(a, 1.0f / l) : v3(0, 1, 0); }
static inline V3 v3lerp(V3 a, V3 b, float t) { return v3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t); }

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float signf(float v) { return v < 0 ? -1.0f : 1.0f; }
static inline float wrap_angle(float a) {
    while (a > PI_F) a -= 2 * PI_F;
    while (a < -PI_F) a += 2 * PI_F;
    return a;
}
/* Draai hoek a richting b met maximaal step radialen. */
static inline float approach_angle(float a, float b, float step) {
    float d = wrap_angle(b - a);
    if (d > step) d = step;
    if (d < -step) d = -step;
    return wrap_angle(a + d);
}
static inline float approachf(float a, float b, float step) {
    if (a < b) { a += step; if (a > b) a = b; }
    else { a -= step; if (a < b) a = b; }
    return a;
}
static inline float smoothstepf(float e0, float e1, float x) {
    float t = clampf((x - e0) / (e1 - e0), 0, 1);
    return t * t * (3 - 2 * t);
}

/* Richting: heading h, model kijkt naar +z. vooruit = (sin h, 0, cos h). */
static inline V3 fwd_of(float h) { return v3(sinf(h), 0, cosf(h)); }
static inline V3 right_of(float h) { return v3(-cosf(h), 0, sinf(h)); }

/* --- matrices --- */
static inline M4 m4_identity(void) {
    M4 r; int i; for (i = 0; i < 16; i++) r.m[i] = (i % 5 == 0) ? 1.0f : 0.0f; return r;
}
static inline M4 m4_mul(M4 a, M4 b) {
    M4 r; int c, rr, k;
    for (c = 0; c < 4; c++)
        for (rr = 0; rr < 4; rr++) {
            float s = 0;
            for (k = 0; k < 4; k++) s += a.m[k * 4 + rr] * b.m[c * 4 + k];
            r.m[c * 4 + rr] = s;
        }
    return r;
}
static inline M4 m4_translate(float x, float y, float z) {
    M4 r = m4_identity(); r.m[12] = x; r.m[13] = y; r.m[14] = z; return r;
}
static inline M4 m4_scale(float x, float y, float z) {
    M4 r = m4_identity(); r.m[0] = x; r.m[5] = y; r.m[10] = z; return r;
}
static inline M4 m4_rot_y(float a) {
    M4 r = m4_identity(); float c = cosf(a), s = sinf(a);
    r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c; return r;
}
static inline M4 m4_rot_x(float a) {
    M4 r = m4_identity(); float c = cosf(a), s = sinf(a);
    r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c; return r;
}
static inline M4 m4_rot_z(float a) {
    M4 r = m4_identity(); float c = cosf(a), s = sinf(a);
    r.m[0] = c; r.m[1] = s; r.m[4] = -s; r.m[5] = c; return r;
}
static inline M4 m4_perspective(float fovy, float aspect, float n, float f) {
    M4 r; int i; float t = 1.0f / tanf(fovy * 0.5f);
    for (i = 0; i < 16; i++) r.m[i] = 0;
    r.m[0] = t / aspect; r.m[5] = t;
    r.m[10] = (f + n) / (n - f); r.m[11] = -1;
    r.m[14] = 2 * f * n / (n - f);
    return r;
}
static inline M4 m4_ortho(float l, float rr, float b, float t, float n, float f) {
    M4 r = m4_identity();
    r.m[0] = 2 / (rr - l); r.m[5] = 2 / (t - b); r.m[10] = -2 / (f - n);
    r.m[12] = -(rr + l) / (rr - l); r.m[13] = -(t + b) / (t - b); r.m[14] = -(f + n) / (f - n);
    return r;
}
static inline M4 m4_lookat(V3 eye, V3 at, V3 up) {
    V3 f = v3norm(v3sub(at, eye));
    V3 s = v3norm(v3cross(f, up));
    V3 u = v3cross(s, f);
    M4 r = m4_identity();
    r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -v3dot(s, eye); r.m[13] = -v3dot(u, eye); r.m[14] = v3dot(f, eye);
    return r;
}
static inline V3 m4_point(const M4 *m, V3 p) {
    return v3(m->m[0] * p.x + m->m[4] * p.y + m->m[8] * p.z + m->m[12],
              m->m[1] * p.x + m->m[5] * p.y + m->m[9] * p.z + m->m[13],
              m->m[2] * p.x + m->m[6] * p.y + m->m[10] * p.z + m->m[14]);
}
static inline V3 m4_dir(const M4 *m, V3 p) {
    return v3(m->m[0] * p.x + m->m[4] * p.y + m->m[8] * p.z,
              m->m[1] * p.x + m->m[5] * p.y + m->m[9] * p.z,
              m->m[2] * p.x + m->m[6] * p.y + m->m[10] * p.z);
}
/* Model: verplaatsen + draaien om y (meest gebruikt). */
static inline M4 m4_trs_y(float x, float y, float z, float h) {
    M4 r = m4_rot_y(h); r.m[12] = x; r.m[13] = y; r.m[14] = z; return r;
}

/* --- deterministische random (xorshift) --- */
typedef struct { uint32_t s; } Rng;
static inline uint32_t rng_u32(Rng *r) {
    uint32_t x = r->s ? r->s : 0x9E3779B9u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5; r->s = x; return x;
}
static inline float rng_f(Rng *r) { return (rng_u32(r) >> 8) * (1.0f / 16777216.0f); }
static inline float rng_range(Rng *r, float a, float b) { return a + (b - a) * rng_f(r); }
static inline int rng_int(Rng *r, int n) { return n > 0 ? (int)(rng_u32(r) % (uint32_t)n) : 0; }

#endif
