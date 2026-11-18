#include "render.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* plaats van Resources/logo_cutout.png in bg_alley.jpg (zie tools/make_assets.py -> logo_cutout.txt) */
#define LOGO_X 297
#define LOGO_Y 139
#define LOGO_W 804
#define LOGO_H 458

static const char *VS_W =
    "attribute vec3 aPos;\n"
    "attribute vec4 aNrm;\n"
    "attribute vec4 aCol;\n"
    "uniform mat4 uVP;\n"
    "uniform mat4 uModel;\n"
    "uniform vec3 uSun;\n"
    "uniform vec3 uSunCol;\n"
    "uniform vec3 uAmb;\n"
    "uniform vec3 uCam;\n"
    "uniform vec2 uFog;\n"
    "uniform vec4 uTint;\n"
    "uniform float uEmis;\n"
    "uniform float uUnlit;\n"
    "varying vec3 vCol;\n"
    "varying float vFog;\n"
    "void main() {\n"
    "  vec4 wp = uModel * vec4(aPos, 1.0);\n"
    "  vec3 n = normalize((uModel * vec4(aNrm.xyz, 0.0)).xyz);\n"
    "  float d = max(dot(n, uSun), 0.0);\n"
    "  vec3 light = uAmb * (0.75 + 0.25 * n.y) + uSunCol * d;\n"
    "  vec3 c = aCol.rgb * uTint.rgb;\n"
    "  vec3 lit = c * light + c * aCol.a * uEmis;\n"
    "  vCol = mix(lit, c, uUnlit);\n"
    "  float dist = length(wp.xyz - uCam);\n"
    "  vFog = clamp((dist - uFog.x) / (uFog.y - uFog.x), 0.0, 1.0);\n"
    "  gl_Position = uVP * wp;\n"
    "}\n";

static const char *FS_W =
    "precision mediump float;\n"
    "varying vec3 vCol;\n"
    "varying float vFog;\n"
    "uniform vec3 uFogCol;\n"
    "uniform float uAlpha;\n"
    "void main() {\n"
    "  gl_FragColor = vec4(mix(vCol, uFogCol, vFog), uAlpha);\n"
    "}\n";

static const char *VS_U =
    "attribute vec2 aPos;\n"
    "attribute vec2 aUV;\n"
    "attribute vec4 aCol;\n"
    "uniform mat4 uProj;\n"
    "varying vec2 vUV;\n"
    "varying vec4 vCol;\n"
    "void main() { vUV = aUV; vCol = aCol; gl_Position = uProj * vec4(aPos, 0.0, 1.0); }\n";

static const char *FS_U =
    "precision mediump float;\n"
    "varying vec2 vUV;\n"
    "varying vec4 vCol;\n"
    "uniform sampler2D uTex;\n"
    "void main() { gl_FragColor = texture2D(uTex, vUV) * vCol; }\n";

static GLuint compile(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    GLint ok = 0;
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        plat_log("SGB shader-fout: %s", log);
    }
    return s;
}

static GLuint link_prog(const char *vs, const char *fs, int ui) {
    GLuint p = glCreateProgram();
    GLint ok = 0;
    glAttachShader(p, compile(GL_VERTEX_SHADER, vs));
    glAttachShader(p, compile(GL_FRAGMENT_SHADER, fs));
    glBindAttribLocation(p, 0, "aPos");
    glBindAttribLocation(p, 1, ui ? "aUV" : "aNrm");
    glBindAttribLocation(p, 2, "aCol");
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(p, sizeof(log), NULL, log);
        plat_log("SGB link-fout: %s", log);
    }
    return p;
}

static void tex_from_rgba(Tex *t, int w, int h, const uint8_t *px, int linear, int repeat) {
    if (!t->id) glGenTextures(1, &t->id);
    glBindTexture(GL_TEXTURE_2D, t->id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    t->w = w; t->h = h;
}

static int tex_load(Tex *t, const char *name) {
    PlatImage img;
    memset(&img, 0, sizeof(img));
    if (!plat_load_image(name, &img) || !img.rgba) {
        uint8_t px[4] = {255, 0, 255, 255};
        plat_log("SGB: plaatje %s niet gevonden", name);
        tex_from_rgba(t, 1, 1, px, 0, 0);
        return 0;
    }
    tex_from_rgba(t, img.w, img.h, img.rgba, 1, 0);
    plat_free_image(&img);
    return 1;
}

static void make_soft(Tex *t, int ring) {
    enum { N = 64 };
    static uint8_t px[N * N * 4];
    int x, y;
    for (y = 0; y < N; y++)
        for (x = 0; x < N; x++) {
            float dx = (x + 0.5f) / N * 2 - 1, dy = (y + 0.5f) / N * 2 - 1;
            float d = sqrtf(dx * dx + dy * dy), a;
            if (ring) a = 1.0f - smoothstepf(0.0f, 1.0f, d);       /* gloed */
            else a = 1.0f - smoothstepf(0.93f, 1.0f, d);           /* cirkel */
            if (ring) a = a * a;
            px[(y * N + x) * 4 + 0] = px[(y * N + x) * 4 + 1] = px[(y * N + x) * 4 + 2] = px[(y * N + x) * 4 + 3] = (uint8_t)(a * 255);
        }
    tex_from_rgba(t, N, N, px, 1, 0);
}

void r_resize(Renderer *r, int w, int h) { r->W = w; r->H = h; }

int r_init(Renderer *r, int w, int h) {
    uint8_t wpx[4] = {255, 255, 255, 255};
    memset(r, 0, sizeof(*r));
    r->W = w; r->H = h;
    r->progW = link_prog(VS_W, FS_W, 0);
    r->progU = link_prog(VS_U, FS_U, 1);
    r->wVP = glGetUniformLocation(r->progW, "uVP");
    r->wModel = glGetUniformLocation(r->progW, "uModel");
    r->wSun = glGetUniformLocation(r->progW, "uSun");
    r->wSunCol = glGetUniformLocation(r->progW, "uSunCol");
    r->wAmb = glGetUniformLocation(r->progW, "uAmb");
    r->wCam = glGetUniformLocation(r->progW, "uCam");
    r->wFog = glGetUniformLocation(r->progW, "uFog");
    r->wFogCol = glGetUniformLocation(r->progW, "uFogCol");
    r->wTint = glGetUniformLocation(r->progW, "uTint");
    r->wEmis = glGetUniformLocation(r->progW, "uEmis");
    r->wAlpha = glGetUniformLocation(r->progW, "uAlpha");
    r->wUnlit = glGetUniformLocation(r->progW, "uUnlit");
    r->uProj = glGetUniformLocation(r->progU, "uProj");
    r->uTex = glGetUniformLocation(r->progU, "uTex");
    tex_from_rgba(&r->white, 1, 1, wpx, 0, 0);
    make_soft(&r->circle, 0);
    make_soft(&r->glow, 1);
    tex_load(&r->bg, "bg_alley.jpg");
    tex_load(&r->logo, "logo_cutout.png");
    tex_load(&r->icons, "icons.png");
    r->logoX = LOGO_X; r->logoY = LOGO_Y; r->logoW = LOGO_W; r->logoH = LOGO_H;
    r_set_tod(r, TOD_DUSK);
    return 1;
}

int r_load_font(Renderer *r) {
    if (!plat_make_font(&r->fd, 48.0f) || !r->fd.img.rgba) {
        plat_log("SGB: geen lettertype");
        return 0;
    }
    tex_from_rgba(&r->font, r->fd.img.w, r->fd.img.h, r->fd.img.rgba, 1, 0);
    plat_free_image(&r->fd.img);
    return 1;
}

static void upload(GMesh *g, MeshB *m) {
    if (!g->vbo) glGenBuffers(1, &g->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(m->n * (int)sizeof(Vtx)), m->v, GL_STATIC_DRAW);
    g->count = m->n;
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void r_upload_world(Renderer *r, World *w, int slot) {
    int i;
    struct RWorld *rw = &r->rw[slot];
    for (i = 0; i < CHN * CHN; i++) {
        Chunk *c = &w->chunks[i];
        upload(&rw->chunk[i], &c->mb);
        rw->chb[i][0] = c->x0; rw->chb[i][1] = c->y0; rw->chb[i][2] = c->z0;
        rw->chb[i][3] = c->x1; rw->chb[i][4] = c->y1; rw->chb[i][5] = c->z1;
        mb_free(&c->mb);
    }
    upload(&rw->ground, &w->ground);
    mb_free(&w->ground);
    if (w->minimap) tex_from_rgba(&rw->minimap, w->mm, w->mm, w->minimap, 1, 0);
}

void r_slot_upload(Renderer *r, int world, int slot, MeshB *m) {
    struct RWorld *rw = &r->rw[world];
    float *b = rw->chb[slot];
    int i;
    b[0] = b[2] = 1e9f; b[3] = b[5] = -1e9f; b[1] = 0; b[4] = 1;
    for (i = 0; i < m->n; i++) {
        Vtx *v = &m->v[i];
        if (v->x < b[0]) b[0] = v->x;
        if (v->x > b[3]) b[3] = v->x;
        if (v->z < b[2]) b[2] = v->z;
        if (v->z > b[5]) b[5] = v->z;
        if (v->y > b[4]) b[4] = v->y;
    }
    upload(&rw->chunk[slot], m);
}

void r_slot_clear(Renderer *r, int world, int slot) {
    r->rw[world].chunk[slot].count = 0;
}

void r_tex_upload(Tex *t, int w, int h, const uint8_t *rgba) { tex_from_rgba(t, w, h, rgba, 1, 0); }

void r_build_models(Renderer *r) {
    MeshB m;
    int i;
    mb_init(&m);
#define BUILD(dst, call) do { mb_reset(&m); call; upload(&(dst), &m); } while (0)
    for (i = 0; i < CHAR_VARIANTS; i++) {
        BUILD(r->body[i], model_char_body(&m, i));
        BUILD(r->leg[i], model_char_leg(&m, i));
        BUILD(r->arm[i], model_char_arm(&m, i));
    }
    for (i = 0; i < BIKE_VARIANTS; i++) BUILD(r->bike[i], model_bike(&m, i));
    for (i = 0; i < CAR_VARIANTS; i++) BUILD(r->car[i], model_car(&m, i));
    BUILD(r->cash, model_cash(&m));
    BUILD(r->gift, model_gift(&m));
    BUILD(r->cp, model_checkpoint(&m));
    BUILD(r->arrow, model_arrow(&m));
    BUILD(r->shadow, model_shadow(&m));
    BUILD(r->marker, model_marker(&m));
    BUILD(r->siren, model_siren(&m));
    BUILD(r->gun, model_gun(&m));
    BUILD(r->spoiler, model_spoiler(&m));
    for (i = 0; i < CAR_MODELS; i++) { BUILD(r->carBody[i], model_car_body(&m, i)); BUILD(r->carDetail[i], model_car_detail(&m, i)); }
    BUILD(r->tracer, model_tracer(&m));
#undef BUILD
    mb_free(&m);
}

static void set3(float *d, float a, float b, float c) { d[0] = a; d[1] = b; d[2] = c; }

void r_set_tod(Renderer *r, int tod) {
    r->tod = tod;
    if (tod == TOD_NIGHT) {
        set3(r->skyTop, 0.03f, 0.04f, 0.10f); set3(r->skyHor, 0.14f, 0.14f, 0.26f);
        set3(r->fogCol, 0.10f, 0.10f, 0.19f);
        set3(r->sun, 0.35f, 0.75f, -0.45f); set3(r->sunCol, 0.22f, 0.26f, 0.40f);
        set3(r->amb, 0.36f, 0.38f, 0.56f); r->emis = 1.25f;
    } else if (tod == TOD_DAY) {
        set3(r->skyTop, 0.34f, 0.56f, 0.88f); set3(r->skyHor, 0.80f, 0.86f, 0.92f);
        set3(r->fogCol, 0.74f, 0.80f, 0.88f);
        set3(r->sun, -0.45f, 0.78f, 0.42f); set3(r->sunCol, 0.78f, 0.74f, 0.64f);
        set3(r->amb, 0.62f, 0.66f, 0.74f); r->emis = 0.2f;
    } else {
        set3(r->skyTop, 0.14f, 0.16f, 0.36f); set3(r->skyHor, 0.95f, 0.58f, 0.42f);
        set3(r->fogCol, 0.66f, 0.47f, 0.47f);
        set3(r->sun, -0.62f, 0.38f, 0.68f); set3(r->sunCol, 0.95f, 0.66f, 0.45f);
        set3(r->amb, 0.62f, 0.56f, 0.70f); r->emis = 0.75f;
    }
    {
        V3 s = v3norm(v3(r->sun[0], r->sun[1], r->sun[2]));
        set3(r->sun, s.x, s.y, s.z);
    }
}

/* ------------------------------------------------------------------ 3D */

static void bind_mesh(const GMesh *g) {
    glBindBuffer(GL_ARRAY_BUFFER, g->vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (const void *)0);
    glVertexAttribPointer(1, 4, GL_BYTE, GL_TRUE, sizeof(Vtx), (const void *)12);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vtx), (const void *)16);
}

static void draw_mesh(Renderer *r, const GMesh *g, const M4 *model) {
    if (!g->vbo || g->count <= 0) return;
    bind_mesh(g);
    glUniformMatrix4fv(r->wModel, 1, GL_FALSE, model->m);
    glDrawArrays(GL_TRIANGLES, 0, g->count);
    r->drawCalls++;
    r->drawnTris += g->count / 3;
}

static void tint(Renderer *r, float a, float b, float c) { glUniform4f(r->wTint, a, b, c, 1.0f); }

static void extract_planes(Renderer *r) {
    const float *m = r->vp.m;
    int i;
    for (i = 0; i < 3; i++) {
        int s;
        for (s = 0; s < 2; s++) {
            float *p = r->planes[i * 2 + s];
            float sg = s ? -1.0f : 1.0f;
            float l;
            p[0] = m[3] + sg * m[i];
            p[1] = m[7] + sg * m[4 + i];
            p[2] = m[11] + sg * m[8 + i];
            p[3] = m[15] + sg * m[12 + i];
            l = sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
            if (l > 0) { p[0] /= l; p[1] /= l; p[2] /= l; p[3] /= l; }
        }
    }
}

static int box_visible(Renderer *r, const float *b) {
    int i;
    for (i = 0; i < 6; i++) {
        const float *p = r->planes[i];
        float x = p[0] >= 0 ? b[3] : b[0];
        float y = p[1] >= 0 ? b[4] : b[1];
        float z = p[2] >= 0 ? b[5] : b[2];
        if (p[0] * x + p[1] * y + p[2] * z + p[3] < 0) return 0;
    }
    return 1;
}

static int sphere_visible(Renderer *r, float x, float y, float z, float rad, float maxd) {
    int i;
    float dx = x - r->cam.x, dz = z - r->cam.z;
    if (dx * dx + dz * dz > maxd * maxd) return 0;
    for (i = 0; i < 6; i++) {
        const float *p = r->planes[i];
        if (p[0] * x + p[1] * y + p[2] * z + p[3] < -rad) return 0;
    }
    return 1;
}

/* pose: 0 lopen/staan, 1 fiets, 2 sprong */
static void draw_char(Renderer *r, int v, M4 base, float walk, int pose, float lay, int armed) {
    M4 bodyM, m;
    float legA = sinf(walk) * 0.75f, armA = -sinf(walk) * 0.6f;
    int k;
    v = ((v % CHAR_VARIANTS) + CHAR_VARIANTS) % CHAR_VARIANTS;
    if (lay > 0) base = m4_mul(base, m4_mul(m4_translate(0, 0.25f * lay, 0), m4_rot_x(-lay * 1.45f)));
    if (pose == 1) {
        base = m4_mul(base, m4_translate(0, BIKE_SEAT_Y - HIP_Y + 0.02f, -0.24f));
        bodyM = m4_mul(base, m4_mul(m4_translate(0, HIP_Y, 0), m4_mul(m4_rot_x(0.38f), m4_translate(0, -HIP_Y, 0))));
    } else bodyM = base;
    draw_mesh(r, &r->body[v], &bodyM);
    for (k = -1; k <= 1; k += 2) {
        float a;
        if (pose == 1) a = -1.25f + sinf(walk + (k > 0 ? 0 : PI_F)) * 0.45f;
        else if (pose == 2) a = k > 0 ? -0.7f : 0.2f;
        else a = legA * k;
        m = m4_mul(base, m4_mul(m4_translate(HIP_X * k, HIP_Y, 0), m4_rot_x(a)));
        draw_mesh(r, &r->leg[v], &m);
    }
    for (k = -1; k <= 1; k += 2) {
        float a;
        if (pose == 1) a = -1.15f;
        else if (pose == 2) a = -2.6f;
        else a = armA * k;
        if (armed && k < 0 && lay <= 0) a = -1.5f;   /* rechterarm naar voren: wapen richten */
        m = m4_mul(bodyM, m4_mul(m4_translate(SHOULDER_X * k, SHOULDER_Y, 0), m4_mul(m4_rot_z(k * 0.06f), m4_rot_x(a))));
        draw_mesh(r, &r->arm[v], &m);
        if (armed && k < 0) draw_mesh(r, &r->gun, &m);
    }
}

static void draw_shadow(Renderer *r, float x, float y, float z, float sx, float sz, float h) {
    M4 m = m4_mul(m4_trs_y(x, y + 0.05f, z, h), m4_scale(sx, 1, sz));
    draw_mesh(r, &r->shadow, &m);
}

void r_draw_scene(Renderer *r, Game *g) {
    float aspect = (float)r->W / (float)(r->H > 0 ? r->H : 1);
    float fogEnd = g->drawDist, fogStart = g->drawDist * 0.45f;
    float t = g->time;
    int i, mode = game_mode(g);
    V3 shadows[MAX_PED + MAX_VEH + 1];
    float shs[MAX_PED + MAX_VEH + 1][3];
    int nsh = 0;

    r->drawnChunks = 0; r->drawnTris = 0;
    r->cam = g->camPos;
    r->proj = m4_perspective(58.0f * PI_F / 180.0f, aspect, 0.3f, g->drawDist + 40.0f);
    r->view = m4_lookat(g->camPos, g->camTarget, v3(0, 1, 0));
    r->vp = m4_mul(r->proj, r->view);
    extract_planes(r);

    /* lucht als verloop (2D) */
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    ui_begin(r);
    {
        V3 f = v3sub(g->camTarget, g->camPos);
        V3 far;
        float hy;
        f.y = 0; f = v3norm(f);
        far = v3add(g->camPos, v3scale(f, 1000.0f));
        far.y = 0;
        {
            float cx = r->vp.m[0] * far.x + r->vp.m[4] * far.y + r->vp.m[8] * far.z + r->vp.m[12];
            float cy = r->vp.m[1] * far.x + r->vp.m[5] * far.y + r->vp.m[9] * far.z + r->vp.m[13];
            float cw = r->vp.m[3] * far.x + r->vp.m[7] * far.y + r->vp.m[11] * far.z + r->vp.m[15];
            (void)cx;
            hy = cw > 0.01f ? (0.5f - 0.5f * cy / cw) * r->H : r->H * 0.5f;
        }
        hy = clampf(hy, -200.0f, (float)r->H + 200.0f);
        if (hy > 0) ui_grad(r, 0, hy - r->H * 0.9f, (float)r->W, r->H * 0.9f, r->skyTop, r->skyHor, 1);
        if (hy - r->H * 0.9f > 0) ui_rect(r, 0, 0, (float)r->W, hy - r->H * 0.9f + 1, r->skyTop[0], r->skyTop[1], r->skyTop[2], 1);
        ui_rect(r, 0, hy, (float)r->W, (float)r->H - hy + 1, r->fogCol[0], r->fogCol[1], r->fogCol[2], 1);
        if (r->tod != TOD_DAY) {
            /* maan/zon-gloed laag aan de horizon */
            V3 sd = v3(r->sun[0], 0, r->sun[2]);
            float dotf = v3dot(v3norm(sd), f);
            if (dotf > 0.2f) {
                V3 sv = v3(f.z, 0, -f.x);
                float side = v3dot(v3norm(sd), sv);
                float sx = r->W * 0.5f - side * r->W * 0.8f;
                ui_additive(r, 1);
                ui_quad(r, &r->glow, sx - 260, hy - 300, 520, 520, 0, 0, 1, 1, r->sunCol[0] * 0.35f, r->sunCol[1] * 0.25f, r->sunCol[2] * 0.2f, 0.0f);
                ui_additive(r, 0);
            }
        }
    }
    ui_flush(r);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glDisable(GL_BLEND);

    glUseProgram(r->progW);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glUniformMatrix4fv(r->wVP, 1, GL_FALSE, r->vp.m);
    glUniform3f(r->wSun, r->sun[0], r->sun[1], r->sun[2]);
    glUniform3f(r->wSunCol, r->sunCol[0], r->sunCol[1], r->sunCol[2]);
    glUniform3f(r->wAmb, r->amb[0], r->amb[1], r->amb[2]);
    glUniform3f(r->wCam, g->camPos.x, g->camPos.y, g->camPos.z);
    glUniform2f(r->wFog, fogStart, fogEnd);
    glUniform3f(r->wFogCol, r->fogCol[0], r->fogCol[1], r->fogCol[2]);
    glUniform1f(r->wEmis, r->emis);
    glUniform1f(r->wAlpha, 1.0f);
    glUniform1f(r->wUnlit, 0.0f);
    tint(r, 1, 1, 1);

    {
        M4 id = m4_identity();
        struct RWorld *rw = &r->rw[r->cur];
        draw_mesh(r, &rw->ground, &id);
        for (i = 0; i < RW_SLOTS; i++) {
            const float *b = rw->chb[i];
            float cx = clampf(g->camPos.x, b[0], b[3]), cz = clampf(g->camPos.z, b[2], b[5]);
            float dx = cx - g->camPos.x, dz = cz - g->camPos.z;
            if (rw->chunk[i].count <= 0) continue;
            if (dx * dx + dz * dz > (fogEnd + 10) * (fogEnd + 10)) continue;
            if (!box_visible(r, b)) continue;
            draw_mesh(r, &rw->chunk[i], &id);
            r->drawnChunks++;
        }
    }

    /* voertuigen */
    for (i = 0; i < g->nveh; i++) {
        Vehicle *v = &g->veh[i];
        M4 m;
        if (!v->active) continue;
        if (!sphere_visible(r, v->x, v->y + 0.8f, v->z, 2.5f, fogEnd)) continue;
        m = m4_trs_y(v->x, v->y, v->z, v->h);
        if (v->type == VEH_BIKE) {
            m = m4_mul(m, m4_rot_z(v->lean));
            draw_mesh(r, &r->bike[v->variant % BIKE_VARIANTS], &m);
            if (i == g->p.veh) draw_char(r, 0, m, g->p.anim * 2.0f, 1, 0, 0);
        } else {
            if (v->custom) {
                Col pc = paint_color(v->paint);
                tint(r, pc.r / 255.0f, pc.g / 255.0f, pc.b / 255.0f);
                draw_mesh(r, &r->carBody[v->model % CAR_MODELS], &m);
                if (v->spoiler && v->model != 3) {
                    M4 sm = m4_mul(m, m4_translate(0, v->model == 2 ? 0.72f - 0.0f : 0.95f, v->model == 0 ? -1.85f : -2.0f));
                    sm = m4_mul(sm, m4_scale(1, v->model == 0 ? 0.35f : 0.3f, 1));
                    draw_mesh(r, &r->spoiler, &sm);
                }
                tint(r, 1, 1, 1);
                draw_mesh(r, &r->carDetail[v->model % CAR_MODELS], &m);
            } else {
                if (v->wrecked) tint(r, 0.3f, 0.3f, 0.32f);
                draw_mesh(r, &r->car[v->variant % CAR_VARIANTS], &m);
                if (v->wrecked) tint(r, 1, 1, 1);
            }
            if (v->police && !v->wrecked) {
                /* zwaailicht: links rood, rechts blauw, om en om */
                int ph = ((int)(g->time * 6.0f)) & 1;
                M4 s1 = m4_mul(m, m4_translate(0, 1.62f, -0.2f));
                M4 s2 = m4_mul(m, m4_translate(0.26f, 1.62f, -0.2f));
                glUniform1f(r->wUnlit, 1.0f);
                tint(r, ph ? 1.0f : 0.35f, 0.05f, 0.05f);
                draw_mesh(r, &r->siren, &s1);
                tint(r, 0.05f, 0.15f, ph ? 0.35f : 1.0f);
                draw_mesh(r, &r->siren, &s2);
                tint(r, 1, 1, 1);
                glUniform1f(r->wUnlit, 0.0f);
            }
        }
        if (nsh < (int)(sizeof(shadows) / sizeof(shadows[0]))) {
            shadows[nsh] = v3(v->x, v->y, v->z);
            shs[nsh][0] = v->type == VEH_BIKE ? 0.45f : 1.15f;
            shs[nsh][1] = v->type == VEH_BIKE ? 1.0f : 2.3f;
            shs[nsh][2] = v->h;
            nsh++;
        }
    }
    /* voetgangers */
    for (i = 0; i < MAX_PED; i++) {
        Ped *p = &g->ped[i];
        M4 m;
        if (!p->active) continue;
        if (!sphere_visible(r, p->x, p->y + 1.0f, p->z, 1.2f, fogEnd * 0.8f)) continue;
        m = m4_trs_y(p->x, p->y, p->z, p->h);
        draw_char(r, p->variant, m, p->state == PED_IDLE ? 0 : p->anim, p->state == PED_DODGE ? 2 : 0, p->lay, p->cop);
        if (nsh < (int)(sizeof(shadows) / sizeof(shadows[0]))) {
            shadows[nsh] = v3(p->x, p->y, p->z); shs[nsh][0] = 0.45f; shs[nsh][1] = 0.45f; shs[nsh][2] = 0; nsh++;
        }
    }
    /* speler te voet */
    if (mode == MODE_FOOT) {
        M4 m = m4_trs_y(g->p.x, g->p.y, g->p.z, g->p.h);
        float ground = world_ground_y(g->w, g->p.x, g->p.z);
        draw_char(r, 0, m, g->p.speed > 0.2f ? g->p.anim : 0, g->p.onGround ? 0 : 2, 0, g->weapon != WP_NONE);
        if (nsh < (int)(sizeof(shadows) / sizeof(shadows[0]))) {
            float s = clampf(0.5f - (g->p.y - ground) * 0.15f, 0.2f, 0.5f);
            shadows[nsh] = v3(g->p.x, ground, g->p.z); shs[nsh][0] = s; shs[nsh][1] = s; shs[nsh][2] = 0; nsh++;
        }
    }
    /* geld */
    glUniform1f(r->wUnlit, 0.35f);
    for (i = 0; i < g->w->ncash; i++) {
        V3 c = g->w->cash[i];
        M4 m;
        if (g->cashTimer[i] > 0) continue;
        if (!sphere_visible(r, c.x, c.y + 0.6f, c.z, 0.8f, fogEnd * 0.7f)) continue;
        m = m4_trs_y(c.x, c.y + 0.5f + sinf(t * 3 + i) * 0.12f, c.z, t * 2.0f + i);
        draw_mesh(r, &r->cash, &m);
    }
    /* missie-dingen */
    {
        V3 tg[12];
        int n = game_targets(g, tg, 12), k;
        if (g->mission >= 0 && g->mdef[g->mission].gifts && g->mstep < g->mdef[g->mission].anyOrder) {
            for (k = 0; k < n; k++) {
                M4 m = m4_trs_y(tg[k].x, CURB + 0.2f + sinf(t * 2 + k) * 0.1f, tg[k].z, t + k);
                if (g->mdef[g->mission].gifts == 2) { m = m4_mul(m, m4_scale(1.6f, 1.6f, 1.6f)); draw_mesh(r, &r->cash, &m); }
                else draw_mesh(r, &r->gift, &m);
            }
        }
        glUniform1f(r->wUnlit, 1.0f);
        for (k = 0; k < n; k++) {
            float bob = sinf(t * 3.0f) * 0.25f;
            float y = (g->mission < 0 ? 3.2f : 5.0f) + bob;
            M4 m = m4_mul(m4_trs_y(tg[k].x, y, tg[k].z, t * 1.5f), m4_scale(0.8f, 0.8f, 0.8f));
            if (g->mission < 0 && tg[k].y > 0.5f) tint(r, 0.4f, 1.0f, 0.45f);
            else tint(r, 1.0f, 0.82f, 0.15f);
            if (!sphere_visible(r, tg[k].x, y, tg[k].z, 2.0f, fogEnd * 1.5f)) continue;
            draw_mesh(r, &r->arrow, &m);
        }
        tint(r, 1, 1, 1);
        glUniform1f(r->wUnlit, 0.0f);

        /* doorzichtig: schaduwen, ringen, checkpoint-zuilen */
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUniform1f(r->wAlpha, 0.38f);
        tint(r, 1, 1, 1);
        glUniform1f(r->wUnlit, 1.0f);
        for (k = 0; k < nsh; k++) draw_shadow(r, shadows[k].x, shadows[k].y, shadows[k].z, shs[k][0], shs[k][1], shs[k][2]);
        glBlendFunc(GL_ONE, GL_ONE);
        glUniform1f(r->wAlpha, 1.0f);
        glDisable(GL_CULL_FACE);
        for (k = 0; k < MAX_TRACERS; k++) {
            V3 a = g->tracer[k].a, d = v3sub(g->tracer[k].b, a);
            float len = v3len(d), yaw, pitch;
            M4 m;
            if (g->tracer[k].t <= 0 || len < 0.1f) continue;
            yaw = atan2f(d.x, d.z);
            pitch = asinf(clampf(d.y / len, -1, 1));
            m = m4_mul(m4_translate(a.x, a.y, a.z), m4_mul(m4_rot_y(yaw), m4_mul(m4_rot_x(-pitch), m4_scale(1, 1, len))));
            tint(r, g->tracer[k].t * 10.0f, g->tracer[k].t * 8.0f, g->tracer[k].t * 4.0f);
            draw_mesh(r, &r->tracer, &m);
        }
        if (g->muzzleT > 0 && g->p.veh < 0) {
            V3 f = fwd_of(g->p.h), rt = right_of(g->p.h);
            M4 m = m4_mul(m4_trs_y(g->p.x + f.x * 0.95f + rt.x * 0.33f, g->p.y + 1.4f, g->p.z + f.z * 0.95f + rt.z * 0.33f, g->p.h + t * 20.0f), m4_scale(4, 4, 0.3f));
            tint(r, 1.0f, 0.8f, 0.3f);
            draw_mesh(r, &r->tracer, &m);
        }
        for (k = 0; k < g->nveh; k++) {   /* neon onder de auto */
            Vehicle *v = &g->veh[k];
            Col nc;
            M4 m;
            if (!v->active || !v->custom || v->neon <= 0) continue;
            if (!sphere_visible(r, v->x, 0.2f, v->z, 3.0f, fogEnd)) continue;
            nc = neon_color(v->neon);
            m = m4_mul(m4_trs_y(v->x, v->y + 0.07f, v->z, v->h), m4_scale(1.45f, 1, 2.7f));
            tint(r, nc.r / 255.0f * 0.7f, nc.g / 255.0f * 0.7f, nc.b / 255.0f * 0.7f);
            draw_mesh(r, &r->shadow, &m);
        }
        if (!g->nl) {
            static const float mc[2][3] = {{1.0f, 0.55f, 0.1f}, {0.15f, 0.45f, 1.0f}};
            int q;
            for (q = 0; q < 2; q++) {
                V3 pp = g->w->poi[q == 0 ? POI_DEALER : POI_CUSTOMS];
                float pulse = 0.75f + 0.25f * sinf(t * 4.0f + q);
                float rs = q ? 3.0f : 2.0f;
                M4 m = m4_mul(m4_trs_y(pp.x, world_ground_y(g->w, pp.x, pp.z) + 0.06f, pp.z, t), m4_scale(rs, 1, rs));
                if (!sphere_visible(r, pp.x, 1, pp.z, 4, fogEnd)) continue;
                tint(r, mc[q][0] * pulse, mc[q][1] * pulse, mc[q][2] * pulse);
                draw_mesh(r, &r->marker, &m);
            }
        }
        if (!g->nl && g->mission < 0) {
            V3 gs = g->w->poi[POI_GUNSHOP];
            float pulse = 0.75f + 0.25f * sinf(t * 4.0f);
            M4 m = m4_mul(m4_trs_y(gs.x, world_ground_y(g->w, gs.x, gs.z) + 0.06f, gs.z, t), m4_scale(2.0f, 1, 2.0f));
            if (sphere_visible(r, gs.x, 1, gs.z, 3, fogEnd)) {
                tint(r, 1.0f * pulse, 0.12f * pulse, 0.1f * pulse);
                draw_mesh(r, &r->marker, &m);
                m = m4_mul(m4_trs_y(gs.x, 0, gs.z, 0), m4_scale(2.0f, 2.0f, 2.0f));
                tint(r, 0.3f, 0.03f, 0.03f);
                draw_mesh(r, &r->cp, &m);
            }
        }
        for (k = 0; k < n; k++) {
            float pulse = 0.75f + 0.25f * sinf(t * 4.0f);
            float rad = 2.4f;
            M4 m;
            if (g->mission >= 0) {
                const MissionDef *md = &g->mdef[g->mission];
                int idx = g->mstep < md->anyOrder ? -1 : g->mstep;
                rad = idx >= 0 ? md->cp[idx].r : 2.0f;
            }
            if (!sphere_visible(r, tg[k].x, 2, tg[k].z, rad + 3, fogEnd * 1.4f)) continue;
            if (g->mission < 0) {
                float gr = tg[k].y > 0.5f ? 1.0f : 0.8f, rr = tg[k].y > 0.5f ? 0.3f : 1.0f;
                m = m4_mul(m4_trs_y(tg[k].x, world_ground_y(g->w, tg[k].x, tg[k].z) + 0.06f, tg[k].z, t), m4_scale(2.4f, 1, 2.4f));
                tint(r, rr * pulse, gr * 0.85f * pulse, 0.15f * pulse);
                draw_mesh(r, &r->marker, &m);
                m = m4_mul(m4_trs_y(tg[k].x, 0, tg[k].z, 0), m4_scale(2.4f, 2.2f, 2.4f));
                tint(r, rr * 0.25f, gr * 0.2f, 0.04f);
                draw_mesh(r, &r->cp, &m);
            } else {
                float cdx = g->camPos.x - tg[k].x, cdz = g->camPos.z - tg[k].z;
                float pdx = g->p.x - tg[k].x, pdz = g->p.z - tg[k].z;
                if (pdx * pdx + pdz * pdz < rad * rad) { cdx = 0; cdz = 0; }   /* speler staat erin: geen zuil */
                m = m4_mul(m4_trs_y(tg[k].x, 0, tg[k].z, 0), m4_scale(rad, 4.5f, rad));
                tint(r, 0.55f * pulse, 0.42f * pulse, 0.06f);
                if (cdx * cdx + cdz * cdz > (rad + 0.8f) * (rad + 0.8f)) draw_mesh(r, &r->cp, &m);   /* niet als de camera erin staat */
                m = m4_mul(m4_trs_y(tg[k].x, world_ground_y(g->w, tg[k].x, tg[k].z) + 0.06f, tg[k].z, t), m4_scale(rad, 1, rad));
                tint(r, 0.9f, 0.7f, 0.1f);
                draw_mesh(r, &r->marker, &m);
            }
        }
        tint(r, 1, 1, 1);
        glUniform1f(r->wUnlit, 0.0f);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisableVertexAttribArray(1);
}

/* ------------------------------------------------------------------ 2D batch */

void ui_begin(Renderer *r) {
    M4 p = m4_ortho(0, (float)r->W, (float)r->H, 0, -1, 1);
    glUseProgram(r->progU);
    glUniformMatrix4fv(r->uProj, 1, GL_FALSE, p.m);
    glUniform1i(r->uTex, 0);
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    r->un = 0;
    r->curTex = 0;
    r->additive = 0;
}

void ui_flush(Renderer *r) {
    if (r->un == 0) return;
    glUseProgram(r->progU);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, r->curTex);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UIVtx), &r->ub[0].x);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(UIVtx), &r->ub[0].u);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(UIVtx), &r->ub[0].r);
    glDrawArrays(GL_TRIANGLES, 0, r->un);
    r->drawCalls++;
    r->un = 0;
}

void ui_additive(Renderer *r, int on) {
    if (r->additive == on) return;
    ui_flush(r);
    r->additive = on;
    if (on) glBlendFunc(GL_ONE, GL_ONE);
    else glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

static void ui_tex(Renderer *r, const Tex *t) {
    if (t->id != r->curTex) { ui_flush(r); r->curTex = t->id; }
    if (r->un + 6 > UI_MAX) ui_flush(r);
}

static void uv_put(UIVtx *v, float x, float y, float u, float vv, uint8_t cr, uint8_t cg, uint8_t cb, uint8_t ca) {
    v->x = x; v->y = y; v->u = u; v->v = vv; v->r = cr; v->g = cg; v->b = cb; v->a = ca;
}

static uint8_t c8(float f) { return (uint8_t)(clampf(f, 0, 1) * 255.0f + 0.5f); }

void ui_quad4(Renderer *r, const Tex *t, const float *xy, const float *uv, float cr, float cg, float cb, float ca) {
    UIVtx *v;
    uint8_t R = c8(cr * ca), G = c8(cg * ca), B = c8(cb * ca), A = c8(ca);
    if (r->additive) A = 0;
    ui_tex(r, t);
    v = &r->ub[r->un];
    uv_put(&v[0], xy[0], xy[1], uv[0], uv[1], R, G, B, A);
    uv_put(&v[1], xy[2], xy[3], uv[2], uv[3], R, G, B, A);
    uv_put(&v[2], xy[4], xy[5], uv[4], uv[5], R, G, B, A);
    uv_put(&v[3], xy[0], xy[1], uv[0], uv[1], R, G, B, A);
    uv_put(&v[4], xy[4], xy[5], uv[4], uv[5], R, G, B, A);
    uv_put(&v[5], xy[6], xy[7], uv[6], uv[7], R, G, B, A);
    r->un += 6;
}

void ui_quad(Renderer *r, const Tex *t, float x, float y, float w, float h, float u0, float v0, float u1, float v1, float cr, float cg, float cb, float ca) {
    float xy[8] = {x, y, x + w, y, x + w, y + h, x, y + h};
    float uv[8] = {u0, v0, u1, v0, u1, v1, u0, v1};
    if (r->additive && ca <= 0.0f) {
        /* additief: alfa 0 betekent "volle kleur toevoegen" */
        UIVtx *v;
        ui_tex(r, t);
        v = &r->ub[r->un];
        uv_put(&v[0], xy[0], xy[1], uv[0], uv[1], c8(cr), c8(cg), c8(cb), 0);
        uv_put(&v[1], xy[2], xy[3], uv[2], uv[3], c8(cr), c8(cg), c8(cb), 0);
        uv_put(&v[2], xy[4], xy[5], uv[4], uv[5], c8(cr), c8(cg), c8(cb), 0);
        uv_put(&v[3], xy[0], xy[1], uv[0], uv[1], c8(cr), c8(cg), c8(cb), 0);
        uv_put(&v[4], xy[4], xy[5], uv[4], uv[5], c8(cr), c8(cg), c8(cb), 0);
        uv_put(&v[5], xy[6], xy[7], uv[6], uv[7], c8(cr), c8(cg), c8(cb), 0);
        r->un += 6;
        return;
    }
    ui_quad4(r, t, xy, uv, cr, cg, cb, ca);
}

void ui_grad(Renderer *r, float x, float y, float w, float h, const float *top, const float *bot, float a) {
    UIVtx *v;
    uint8_t tr = c8(top[0] * a), tg = c8(top[1] * a), tb = c8(top[2] * a);
    uint8_t br = c8(bot[0] * a), bg = c8(bot[1] * a), bb = c8(bot[2] * a), A = c8(a);
    ui_tex(r, &r->white);
    v = &r->ub[r->un];
    uv_put(&v[0], x, y, 0.5f, 0.5f, tr, tg, tb, A);
    uv_put(&v[1], x + w, y, 0.5f, 0.5f, tr, tg, tb, A);
    uv_put(&v[2], x + w, y + h, 0.5f, 0.5f, br, bg, bb, A);
    uv_put(&v[3], x, y, 0.5f, 0.5f, tr, tg, tb, A);
    uv_put(&v[4], x + w, y + h, 0.5f, 0.5f, br, bg, bb, A);
    uv_put(&v[5], x, y + h, 0.5f, 0.5f, br, bg, bb, A);
    r->un += 6;
}

void ui_rect(Renderer *r, float x, float y, float w, float h, float cr, float cg, float cb, float ca) {
    ui_quad(r, &r->white, x, y, w, h, 0.5f, 0.5f, 0.5f, 0.5f, cr, cg, cb, ca);
}

void ui_circle(Renderer *r, float x, float y, float rad, float cr, float cg, float cb, float ca) {
    ui_quad(r, &r->circle, x - rad, y - rad, rad * 2, rad * 2, 0, 0, 1, 1, cr, cg, cb, ca);
}

void ui_ring(Renderer *r, float x, float y, float rad, float thick, float cr, float cg, float cb, float ca) {
    int i, seg = 32;
    for (i = 0; i < seg; i++) {
        float a0 = 2 * PI_F * i / seg, a1 = 2 * PI_F * (i + 1) / seg;
        float ri = rad - thick;
        float xy[8] = {x + cosf(a0) * ri, y + sinf(a0) * ri, x + cosf(a0) * rad, y + sinf(a0) * rad,
                       x + cosf(a1) * rad, y + sinf(a1) * rad, x + cosf(a1) * ri, y + sinf(a1) * ri};
        float uv[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
        ui_quad4(r, &r->white, xy, uv, cr, cg, cb, ca);
    }
}

void ui_icon(Renderer *r, int idx, float cx, float cy, float size, float alpha) {
    float u0 = (float)idx / ICON_COUNT, u1 = (float)(idx + 1) / ICON_COUNT;
    ui_quad(r, &r->icons, cx - size * 0.5f, cy - size * 0.5f, size, size, u0, 0, u1, 1, 1, 1, 1, alpha);
}

float ui_text_w(Renderer *r, const char *s, float size) {
    float sc = size / (r->fd.size > 0 ? r->fd.size : 48.0f), w = 0;
    if (!r->font.id) return 0;
    for (; *s; s++) {
        int c = (unsigned char)*s;
        if (c < 32 || c > 127) c = '?';
        w += r->fd.g[c - 32].adv * sc;
    }
    return w;
}

void ui_text(Renderer *r, const char *s, float x, float y, float size, float cr, float cg, float cb, float ca, int align) {
    float sc = size / (r->fd.size > 0 ? r->fd.size : 48.0f);
    if (!r->font.id) return;
    if (align == 1) x -= ui_text_w(r, s, size) * 0.5f;
    else if (align == 2) x -= ui_text_w(r, s, size);
    for (; *s; s++) {
        int c = (unsigned char)*s;
        const Glyph *g;
        if (c < 32 || c > 127) c = '?';
        g = &r->fd.g[c - 32];
        if (c != ' ')
            ui_quad(r, &r->font, x + g->xoff * sc, y + g->yoff * sc, g->w * sc, g->h * sc, g->u0, g->v0, g->u1, g->v1, cr, cg, cb, ca);
        x += g->adv * sc;
    }
}
