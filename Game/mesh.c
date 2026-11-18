#include "mesh.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

void mb_init(MeshB *m) { memset(m, 0, sizeof(*m)); m->xf = m4_identity(); }
void mb_free(MeshB *m) { free(m->v); m->v = NULL; m->n = m->cap = 0; }
void mb_reset(MeshB *m) { m->n = 0; }
void mb_set_xf(MeshB *m, M4 xf) { m->xf = xf; m->has_xf = 1; }
void mb_clear_xf(MeshB *m) { m->xf = m4_identity(); m->has_xf = 0; }

static void grow(MeshB *m, int add) {
    if (m->n + add <= m->cap) return;
    int nc = m->cap ? m->cap * 2 : 1024;
    while (nc < m->n + add) nc *= 2;
    Vtx *nv = (Vtx *)realloc(m->v, (size_t)nc * sizeof(Vtx));
    if (!nv) return;
    m->v = nv; m->cap = nc;
}

static void put(MeshB *m, V3 p, V3 n, Col c) {
    if (m->n >= m->cap) { grow(m, 1); if (m->n >= m->cap) return; }
    Vtx *v = &m->v[m->n++];
    v->x = p.x; v->y = p.y; v->z = p.z;
    v->nx = (int8_t)clampf(n.x * 127.0f, -127, 127);
    v->ny = (int8_t)clampf(n.y * 127.0f, -127, 127);
    v->nz = (int8_t)clampf(n.z * 127.0f, -127, 127);
    v->pad = 0;
    v->r = c.r; v->g = c.g; v->b = c.b; v->a = c.a;
}

void mb_tri(MeshB *m, V3 a, V3 b, V3 c, V3 n, Col c0) {
    if (m->has_xf) {
        a = m4_point(&m->xf, a); b = m4_point(&m->xf, b); c = m4_point(&m->xf, c);
        n = v3norm(m4_dir(&m->xf, n));
    }
    grow(m, 3);
    V3 cr = v3cross(v3sub(b, a), v3sub(c, a));
    if (v3dot(cr, n) < 0) { V3 t = b; b = c; c = t; }
    put(m, a, n, c0); put(m, b, n, c0); put(m, c, n, c0);
}

void mb_quad4(MeshB *m, V3 a, V3 b, V3 c, V3 d, V3 n, Col ca, Col cb, Col cc, Col cd) {
    if (m->has_xf) {
        a = m4_point(&m->xf, a); b = m4_point(&m->xf, b);
        c = m4_point(&m->xf, c); d = m4_point(&m->xf, d);
        n = v3norm(m4_dir(&m->xf, n));
    }
    grow(m, 6);
    V3 cr = v3cross(v3sub(b, a), v3sub(c, a));
    if (v3dot(cr, n) < 0) {
        /* andere kant op: a d c b */
        V3 t = b; b = d; d = t;
        Col tc = cb; cb = cd; cd = tc;
    }
    put(m, a, n, ca); put(m, b, n, cb); put(m, c, n, cc);
    put(m, a, n, ca); put(m, c, n, cc); put(m, d, n, cd);
}

void mb_quad(MeshB *m, V3 a, V3 b, V3 c, V3 d, V3 n, Col c0) {
    mb_quad4(m, a, b, c, d, n, c0, c0, c0, c0);
}

void mb_box2(MeshB *m, V3 mn, V3 mx, Col top, Col side, int flags) {
    Col lo = m->ao > 0 ? col_mul(side, 1.0f - m->ao) : side;
    Col hi = side;
    /* top */
    if (!(flags & BOX_NOTOP))
        mb_quad(m, v3(mn.x, mx.y, mn.z), v3(mx.x, mx.y, mn.z), v3(mx.x, mx.y, mx.z), v3(mn.x, mx.y, mx.z), v3(0, 1, 0), top);
    if (!(flags & BOX_NOBOTTOM))
        mb_quad(m, v3(mn.x, mn.y, mn.z), v3(mx.x, mn.y, mn.z), v3(mx.x, mn.y, mx.z), v3(mn.x, mn.y, mx.z), v3(0, -1, 0), col_mul(side, 0.6f));
    /* +x */
    mb_quad4(m, v3(mx.x, mn.y, mn.z), v3(mx.x, mn.y, mx.z), v3(mx.x, mx.y, mx.z), v3(mx.x, mx.y, mn.z), v3(1, 0, 0), lo, lo, hi, hi);
    /* -x */
    mb_quad4(m, v3(mn.x, mn.y, mn.z), v3(mn.x, mn.y, mx.z), v3(mn.x, mx.y, mx.z), v3(mn.x, mx.y, mn.z), v3(-1, 0, 0), lo, lo, hi, hi);
    /* +z */
    mb_quad4(m, v3(mn.x, mn.y, mx.z), v3(mx.x, mn.y, mx.z), v3(mx.x, mx.y, mx.z), v3(mn.x, mx.y, mx.z), v3(0, 0, 1), lo, lo, hi, hi);
    /* -z */
    mb_quad4(m, v3(mn.x, mn.y, mn.z), v3(mx.x, mn.y, mn.z), v3(mx.x, mx.y, mn.z), v3(mn.x, mx.y, mn.z), v3(0, 0, -1), lo, lo, hi, hi);
}

void mb_box(MeshB *m, V3 mn, V3 mx, Col c, int flags) { mb_box2(m, mn, mx, c, c, flags); }

static V3 axis_pt(int axis, float along, float u, float v) {
    if (axis == 0) return v3(along, u, v);
    if (axis == 1) return v3(u, along, v);
    return v3(u, v, along);
}

void mb_cyl(MeshB *m, V3 c, float r, float len, int seg, int axis, Col side, Col cap, int caps) {
    int i;
    float a0, a1;
    float lo = (axis == 1) ? 0 : -len * 0.5f, hi = (axis == 1) ? len : len * 0.5f;
    for (i = 0; i < seg; i++) {
        a0 = 2 * PI_F * i / seg; a1 = 2 * PI_F * (i + 1) / seg;
        float u0 = cosf(a0) * r, v0 = sinf(a0) * r, u1 = cosf(a1) * r, v1 = sinf(a1) * r;
        float am = (a0 + a1) * 0.5f;
        V3 n = axis_pt(axis, 0, cosf(am), sinf(am));
        V3 p0 = v3add(c, axis_pt(axis, lo, u0, v0)), p1 = v3add(c, axis_pt(axis, lo, u1, v1));
        V3 p2 = v3add(c, axis_pt(axis, hi, u1, v1)), p3 = v3add(c, axis_pt(axis, hi, u0, v0));
        mb_quad(m, p0, p1, p2, p3, n, side);
        if (caps) {
            V3 nh = axis_pt(axis, 1, 0, 0), nl = axis_pt(axis, -1, 0, 0);
            mb_tri(m, v3add(c, axis_pt(axis, hi, 0, 0)), p3, p2, nh, cap);
            mb_tri(m, v3add(c, axis_pt(axis, lo, 0, 0)), p0, p1, nl, cap);
        }
    }
}

void mb_cone(MeshB *m, V3 base, float r, float h, int seg, Col c) {
    int i;
    V3 top = v3add(base, v3(0, h, 0));
    for (i = 0; i < seg; i++) {
        float a0 = 2 * PI_F * i / seg, a1 = 2 * PI_F * (i + 1) / seg, am = (a0 + a1) * 0.5f;
        V3 p0 = v3add(base, v3(cosf(a0) * r, 0, sinf(a0) * r));
        V3 p1 = v3add(base, v3(cosf(a1) * r, 0, sinf(a1) * r));
        V3 n = v3norm(v3(cosf(am) * h, r, sinf(am) * h));
        mb_tri(m, p0, p1, top, n, c);
        mb_tri(m, p0, p1, base, v3(0, -1, 0), col_mul(c, 0.6f));
    }
}

void mb_roof(MeshB *m, float x0, float z0, float x1, float z1, float y, float h, int ridge_x, Col roof, Col gable) {
    if (ridge_x) {
        float zm = (z0 + z1) * 0.5f;
        V3 n1 = v3norm(v3(0, z1 - zm, -h)), n2 = v3norm(v3(0, zm - z0, h));
        mb_quad(m, v3(x0, y, z0), v3(x1, y, z0), v3(x1, y + h, zm), v3(x0, y + h, zm), n1, roof);
        mb_quad(m, v3(x0, y, z1), v3(x1, y, z1), v3(x1, y + h, zm), v3(x0, y + h, zm), n2, roof);
        mb_tri(m, v3(x0, y, z0), v3(x0, y, z1), v3(x0, y + h, zm), v3(-1, 0, 0), gable);
        mb_tri(m, v3(x1, y, z0), v3(x1, y, z1), v3(x1, y + h, zm), v3(1, 0, 0), gable);
    } else {
        float xm = (x0 + x1) * 0.5f;
        V3 n1 = v3norm(v3(-h, x1 - xm, 0)), n2 = v3norm(v3(h, xm - x0, 0));
        mb_quad(m, v3(x0, y, z0), v3(x0, y, z1), v3(xm, y + h, z1), v3(xm, y + h, z0), n1, roof);
        mb_quad(m, v3(x1, y, z0), v3(x1, y, z1), v3(xm, y + h, z1), v3(xm, y + h, z0), n2, roof);
        mb_tri(m, v3(x0, y, z0), v3(x1, y, z0), v3(xm, y + h, z0), v3(0, 0, -1), gable);
        mb_tri(m, v3(x0, y, z1), v3(x1, y, z1), v3(xm, y + h, z1), v3(0, 0, 1), gable);
    }
}

void mb_flat(MeshB *m, float x0, float z0, float x1, float z1, float y, Col c) {
    mb_quad(m, v3(x0, y, z0), v3(x1, y, z0), v3(x1, y, z1), v3(x0, y, z1), v3(0, 1, 0), c);
}

void mb_disc(MeshB *m, V3 c, float r, int seg, Col c0) {
    int i;
    for (i = 0; i < seg; i++) {
        float a0 = 2 * PI_F * i / seg, a1 = 2 * PI_F * (i + 1) / seg;
        mb_tri(m, c, v3add(c, v3(cosf(a0) * r, 0, sinf(a0) * r)), v3add(c, v3(cosf(a1) * r, 0, sinf(a1) * r)), v3(0, 1, 0), c0);
    }
}

/* ------------------------------------------------------------------ gladde vormen (v3.0) */

static void put_n(MeshB *m, V3 p, V3 n, Col c) {
    if (m->has_xf) { p = m4_point(&m->xf, p); n = v3norm(m4_dir(&m->xf, n)); }
    put(m, p, n, c);
}

void mb_tri_n(MeshB *m, V3 a, V3 b, V3 c, V3 na, V3 nb, V3 nc, Col ca, Col cb, Col cc) {
    V3 cr = v3cross(v3sub(b, a), v3sub(c, a));
    V3 avg = v3add(v3add(na, nb), nc);
    grow(m, 3);
    if (v3dot(cr, avg) < 0) {
        V3 t = b; b = c; c = t;
        t = nb; nb = nc; nc = t;
        { Col tc = cb; cb = cc; cc = tc; }
    }
    put_n(m, a, na, ca); put_n(m, b, nb, cb); put_n(m, c, nc, cc);
}

static void quad_n(MeshB *m, const V3 *p, const V3 *n, const Col *c) {
    mb_tri_n(m, p[0], p[1], p[2], n[0], n[1], n[2], c[0], c[1], c[2]);
    mb_tri_n(m, p[0], p[2], p[3], n[0], n[2], n[3], c[0], c[2], c[3]);
}

static float spow(float x, float e) { return x < 0 ? -powf(-x, e) : powf(x, e); }

SQ sq(V3 c, V3 h, float roundV, float roundH, Col col) {
    SQ s;
    memset(&s, 0, sizeof(s));
    s.c = c; s.h = h; s.roundV = roundV; s.roundH = roundH; s.col = col;
    s.su = 16; s.sv = 10;
    s.bx = s.bz = s.tx = s.tz = 1.0f;
    return s;
}

static V3 sq_pt(const SQ *s, float u, float v) {
    float cv = cosf(v), sv = sinf(v);
    float x = s->h.x * spow(cv, s->roundV) * spow(cosf(u), s->roundH);
    float y = s->h.y * spow(sv, s->roundV);
    float z = s->h.z * spow(cv, s->roundV) * spow(sinf(u), s->roundH);
    float t = s->h.y > 0 ? y / s->h.y : 0, k = (t + 1) * 0.5f;
    x *= lerpf(s->bx, s->tx, k);
    z *= lerpf(s->bz, s->tz, k);
    z += s->shear * t;
    return v3(s->c.x + x, s->c.y + y, s->c.z + z);
}

void mb_sq(MeshB *m, const SQ *s) {
    int i, j, su = s->su < 4 ? 4 : s->su, sv = s->sv < 2 ? 2 : s->sv;
    float v0 = -PI_F * 0.5f, v1 = PI_F * 0.5f, lo = m->ao;
    V3 *P, *N;
    if (s->part > 0) v0 = 0;
    if (s->part < 0) v1 = 0;
    P = (V3 *)malloc(sizeof(V3) * (size_t)(su + 1) * (size_t)(sv + 1));
    N = (V3 *)malloc(sizeof(V3) * (size_t)(su + 1) * (size_t)(sv + 1));
    if (!P || !N) { free(P); free(N); return; }
    for (j = 0; j <= sv; j++) {
        float v = v0 + (v1 - v0) * j / sv;
        for (i = 0; i <= su; i++) {
            float u = -PI_F + 2 * PI_F * i / su, e = 0.002f;
            float va = v - e, vb = v + e;
            V3 p = sq_pt(s, u, v), du, dv, n, out;
            if (va < -PI_F * 0.5f) va = -PI_F * 0.5f;
            if (vb > PI_F * 0.5f) vb = PI_F * 0.5f;
            du = v3sub(sq_pt(s, u + e, v), sq_pt(s, u - e, v));
            dv = v3sub(sq_pt(s, u, vb), sq_pt(s, u, va));
            n = v3cross(dv, du);
            if (v3len(n) < 1e-10f || fabsf(fabsf(v) - PI_F * 0.5f) < 1e-3f) n = v3(0, v > 0 ? 1.0f : -1.0f, 0);
            n = v3norm(n);
            /* naar buiten wijzen */
            out = v3sub(p, v3(s->c.x, s->c.y, s->c.z + s->shear * (s->h.y > 0 ? (p.y - s->c.y) / s->h.y : 0)));
            if (v3dot(n, out) < 0) n = v3scale(n, -1);
            P[j * (su + 1) + i] = p;
            N[j * (su + 1) + i] = n;
        }
    }
    for (j = 0; j < sv; j++)
        for (i = 0; i < su; i++) {
            int a = j * (su + 1) + i, b = a + 1, c = a + su + 2, d = a + su + 1, k;
            V3 p[4], n[4];
            Col cc[4];
            p[0] = P[a]; p[1] = P[b]; p[2] = P[c]; p[3] = P[d];
            n[0] = N[a]; n[1] = N[b]; n[2] = N[c]; n[3] = N[d];
            for (k = 0; k < 4; k++) {
                float t = s->h.y > 0 ? clampf((p[k].y - (s->c.y - s->h.y)) / (2 * s->h.y), 0, 1) : 1;
                cc[k] = lo > 0 ? col_mul(s->col, 1.0f - lo * (1.0f - t)) : s->col;
            }
            if (v3len(v3sub(p[0], p[1])) < 1e-6f) mb_tri_n(m, p[0], p[2], p[3], n[0], n[2], n[3], cc[0], cc[2], cc[3]);
            else if (v3len(v3sub(p[2], p[3])) < 1e-6f) mb_tri_n(m, p[0], p[1], p[2], n[0], n[1], n[2], cc[0], cc[1], cc[2]);
            else quad_n(m, p, n, cc);
        }
    if (s->part != 0) {   /* halve vorm: dichtmaken met een plat vlak */
        V3 nn = v3(0, s->part > 0 ? -1.0f : 1.0f, 0), ctr = v3(s->c.x, s->c.y, s->c.z);
        Col dc = col_mul(s->col, 0.7f);
        int row = s->part > 0 ? 0 : sv;
        for (i = 0; i < su; i++) {
            V3 a = P[row * (su + 1) + i], b = P[row * (su + 1) + i + 1];
            mb_tri_n(m, ctr, a, b, nn, nn, nn, dc, dc, dc);
        }
    }
    free(P); free(N);
}

static void frame_of(V3 d, V3 *s, V3 *u) {
    V3 f = v3norm(d);
    V3 up = fabsf(f.y) > 0.9f ? v3(1, 0, 0) : v3(0, 1, 0);
    *s = v3norm(v3cross(f, up));
    *u = v3cross(*s, f);
}

static void ring_tube(MeshB *m, V3 a, V3 b, float ra, float rb, int seg, Col c, int caps) {
    V3 d = v3sub(b, a), s, u, f;
    float len = v3len(d), slope;
    Col cc[4];
    int i, k;
    if (len < 1e-5f) return;
    if (seg < 3) seg = 3;
    cc[0] = cc[1] = cc[2] = cc[3] = c;
    f = v3scale(d, 1.0f / len);
    frame_of(d, &s, &u);
    slope = (ra - rb) / len;
    for (i = 0; i < seg; i++) {
        float a0 = 2 * PI_F * i / seg, a1 = 2 * PI_F * (i + 1) / seg;
        V3 r0 = v3add(v3scale(s, cosf(a0)), v3scale(u, sinf(a0)));
        V3 r1 = v3add(v3scale(s, cosf(a1)), v3scale(u, sinf(a1)));
        V3 n0 = v3norm(v3add(r0, v3scale(f, slope))), n1 = v3norm(v3add(r1, v3scale(f, slope)));
        V3 p[4], n[4];
        p[0] = v3add(a, v3scale(r0, ra)); p[1] = v3add(a, v3scale(r1, ra));
        p[2] = v3add(b, v3scale(r1, rb)); p[3] = v3add(b, v3scale(r0, rb));
        n[0] = n0; n[1] = n1; n[2] = n1; n[3] = n0;
        quad_n(m, p, n, cc);
        if (caps) {   /* halve bollen op de uiteinden */
            int st = seg / 4 < 2 ? 2 : seg / 4, e;
            for (k = 0; k < st; k++) {
                float b0 = PI_F * 0.5f * k / st, b1 = PI_F * 0.5f * (k + 1) / st;
                for (e = 0; e < 2; e++) {
                    V3 ctr = e ? b : a, ax = e ? f : v3scale(f, -1), pp[4], nn[4];
                    float rr = e ? rb : ra;
                    nn[0] = v3add(v3scale(r0, cosf(b0)), v3scale(ax, sinf(b0)));
                    nn[1] = v3add(v3scale(r1, cosf(b0)), v3scale(ax, sinf(b0)));
                    nn[2] = v3add(v3scale(r1, cosf(b1)), v3scale(ax, sinf(b1)));
                    nn[3] = v3add(v3scale(r0, cosf(b1)), v3scale(ax, sinf(b1)));
                    pp[0] = v3add(ctr, v3scale(nn[0], rr)); pp[1] = v3add(ctr, v3scale(nn[1], rr));
                    pp[2] = v3add(ctr, v3scale(nn[2], rr)); pp[3] = v3add(ctr, v3scale(nn[3], rr));
                    if (k == st - 1) mb_tri_n(m, pp[0], pp[1], pp[2], nn[0], nn[1], nn[2], c, c, c);
                    else quad_n(m, pp, nn, cc);
                }
            }
        }
    }
}

void mb_capsule(MeshB *m, V3 a, V3 b, float ra, float rb, int seg, Col c) { ring_tube(m, a, b, ra, rb, seg, c, 1); }
void mb_rtube(MeshB *m, V3 a, V3 b, float ra, float rb, int seg, Col c) { ring_tube(m, a, b, ra, rb, seg, c, 0); }

/* Profiel loopt over de buitenkant van begin naar eind; de normaal staat rechts van de looprichting
 * (straal-component = dd, as-component = -dr). */
void mb_lathe(MeshB *m, V3 c, int axis, const float *r, const float *d, const Col *cols, int n, int seg) {
    int i, k, q;
    for (k = 0; k + 1 < n; k++) {
        float dr = r[k + 1] - r[k], dd = d[k + 1] - d[k];
        float nr = dd, na = -dr, l = sqrtf(nr * nr + na * na);
        if (l < 1e-7f) continue;
        nr /= l; na /= l;
        for (i = 0; i < seg; i++) {
            float a0 = 2 * PI_F * i / seg, a1 = 2 * PI_F * (i + 1) / seg;
            float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
            V3 p[4], nn[4];
            Col cc[4];
            if (axis == 0) {
                p[0] = v3(d[k], r[k] * c0, r[k] * s0); p[1] = v3(d[k], r[k] * c1, r[k] * s1);
                p[2] = v3(d[k + 1], r[k + 1] * c1, r[k + 1] * s1); p[3] = v3(d[k + 1], r[k + 1] * c0, r[k + 1] * s0);
                nn[0] = nn[3] = v3(na, nr * c0, nr * s0); nn[1] = nn[2] = v3(na, nr * c1, nr * s1);
            } else if (axis == 1) {
                p[0] = v3(r[k] * c0, d[k], r[k] * s0); p[1] = v3(r[k] * c1, d[k], r[k] * s1);
                p[2] = v3(r[k + 1] * c1, d[k + 1], r[k + 1] * s1); p[3] = v3(r[k + 1] * c0, d[k + 1], r[k + 1] * s0);
                nn[0] = nn[3] = v3(nr * c0, na, nr * s0); nn[1] = nn[2] = v3(nr * c1, na, nr * s1);
            } else {
                p[0] = v3(r[k] * c0, r[k] * s0, d[k]); p[1] = v3(r[k] * c1, r[k] * s1, d[k]);
                p[2] = v3(r[k + 1] * c1, r[k + 1] * s1, d[k + 1]); p[3] = v3(r[k + 1] * c0, r[k + 1] * s0, d[k + 1]);
                nn[0] = nn[3] = v3(nr * c0, nr * s0, na); nn[1] = nn[2] = v3(nr * c1, nr * s1, na);
            }
            for (q = 0; q < 4; q++) p[q] = v3add(p[q], c);
            cc[0] = cc[1] = cols[k]; cc[2] = cc[3] = cols[k + 1];
            if (r[k] < 1e-6f) mb_tri_n(m, p[0], p[2], p[3], nn[0], nn[2], nn[3], cc[0], cc[2], cc[3]);
            else if (r[k + 1] < 1e-6f) mb_tri_n(m, p[0], p[1], p[2], nn[0], nn[1], nn[2], cc[0], cc[1], cc[2]);
            else quad_n(m, p, nn, cc);
        }
    }
}

void mb_wheel(MeshB *m, V3 c, float r, float w, Col tire, Col rim, int spokes) {
    float hw = w * 0.5f, rr = r * 0.66f;
    int i, seg = 18;
    {   /* band: binnenwand -> afgerond loopvlak -> buitenwand */
        float pr[8] = {rr, r * 0.86f, r * 0.96f, r, r, r * 0.96f, r * 0.86f, rr};
        float pd[8] = {-hw, -hw, -hw * 0.82f, -hw * 0.45f, hw * 0.45f, hw * 0.82f, hw, hw};
        Col tc[8];
        for (i = 0; i < 8; i++) tc[i] = tire;
        tc[3] = tc[4] = col_mul(tire, 1.15f);
        mb_lathe(m, c, 0, pr, pd, tc, 8, seg);
    }
    {   /* velg aan beide kanten: rand -> verzonken schotel -> naaf */
        float pr[4] = {rr, rr * 0.9f, r * 0.2f, 0};
        float pdp[4] = {hw * 0.95f, hw * 0.6f, hw * 0.7f, hw * 0.78f};
        float pdn[4] = {-hw * 0.95f, -hw * 0.6f, -hw * 0.7f, -hw * 0.78f};
        float rn[4] = {0, r * 0.2f, rr * 0.9f, rr};
        float dn[4] = {-hw * 0.78f, -hw * 0.7f, -hw * 0.6f, -hw * 0.95f};
        Col rc[4] = {col_mul(rim, 0.75f), rim, rim, col_mul(rim, 1.1f)};
        Col rcn[4] = {col_mul(rim, 1.1f), rim, rim, col_mul(rim, 0.75f)};
        (void)pdn;
        mb_lathe(m, c, 0, pr, pdp, rc, 4, seg);
        mb_lathe(m, c, 0, rn, dn, rcn, 4, seg);
    }
    for (i = 0; i < spokes; i++) {
        float a = 2 * PI_F * i / spokes;
        int sd;
        for (sd = -1; sd <= 1; sd += 2) {
            V3 h0 = v3add(c, v3(hw * 0.72f * sd, 0, 0));
            V3 o = v3add(c, v3(hw * 0.62f * sd, cosf(a) * rr * 0.92f, sinf(a) * rr * 0.92f));
            mb_rtube(m, h0, o, r * 0.045f, r * 0.03f, 5, col_mul(rim, 0.9f));
        }
    }
}
