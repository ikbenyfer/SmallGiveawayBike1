#include "mesh.h"
#include <stdlib.h>
#include <string.h>

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
