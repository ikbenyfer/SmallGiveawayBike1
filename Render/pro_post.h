/* v7.0.0 PRO-engine (alleen pc, kwaliteit ULTRA+): nabewerking van het beeld, zoals in moderne games.
 * Werkt met gewone OpenGL ES 2.0 / OpenGL 2.1-framebuffers (geen extensies nodig):
 *   1. dieptepass vanuit de camera (halve resolutie, verpakte diepte in RGBA8)
 *   2. SSAO: ambient occlusion uit de diepte (hoeken, onder auto's, tussen gebouwen)
 *   3. bloom: heldere delen (zon, lampen, reflecties) gloeien over
 *   4. godrays: zonnestralen langs gebouwen en bomen
 *   5. eindbeeld: FXAA (randen glad), AO, bloom, stralen, vignet, kleur en filmkorrel
 * Wordt in render.c ingevoegd (gebruikt link_prog en de Renderer). */

#define PRO_QUAL_PREC "#ifdef GL_ES\n#ifdef GL_FRAGMENT_PRECISION_HIGH\nprecision highp float;\n#else\nprecision mediump float;\n#endif\n#endif\n"

static const char *VS_POST =
    "attribute vec2 aPos;\n"
    "varying vec2 vUv;\n"
    "void main() { vUv = aPos * 0.5 + 0.5; gl_Position = vec4(aPos, 0.0, 1.0); }\n";

#define PRO_COMMON \
    "varying vec2 vUv;\n" \
    "float unpackD(vec4 c) { return dot(c, vec4(1.0, 1.0 / 255.0, 1.0 / 65025.0, 1.0 / 16581375.0)); }\n" \
    "float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }\n"

/* 2: SSAO */
static const char *FS_AO =
    PRO_COMMON
    "uniform sampler2D uDepth;\n"
    "uniform vec2 uPx;\n"      /* 1 / grootte van de dieptekaart */
    "uniform vec2 uNF;\n"      /* near, far */
    "uniform vec2 uTan;\n"     /* tan(halve fov) in x en y */
    "float linZ(vec2 uv) { float d = unpackD(texture2D(uDepth, uv)); float z = d * 2.0 - 1.0;\n"
    "  return 2.0 * uNF.x * uNF.y / (uNF.y + uNF.x - z * (uNF.y - uNF.x)); }\n"
    "vec3 posAt(vec2 uv) { float z = linZ(uv); return vec3((uv * 2.0 - 1.0) * uTan * z, -z); }\n"
    "void main() {\n"
    "  float d0 = unpackD(texture2D(uDepth, vUv));\n"
    "  if (d0 > 0.9999) { gl_FragColor = vec4(1.0); return; }\n"
    "  vec3 P = posAt(vUv);\n"
    "  vec3 Pr = posAt(vUv + vec2(uPx.x, 0.0)), Pl = posAt(vUv - vec2(uPx.x, 0.0));\n"
    "  vec3 Pu = posAt(vUv + vec2(0.0, uPx.y)), Pd = posAt(vUv - vec2(0.0, uPx.y));\n"
    "  vec3 dx = abs(Pr.z - P.z) < abs(P.z - Pl.z) ? Pr - P : P - Pl;\n"
    "  vec3 dy = abs(Pu.z - P.z) < abs(P.z - Pd.z) ? Pu - P : P - Pd;\n"
    "  vec3 n = normalize(cross(dx, dy));\n"
    "  if (dot(n, -P) < 0.0) n = -n;\n"
    "  float R = 0.75, ao = 0.0, a0 = hash(vUv * 731.0) * 6.2832;\n"
    "  vec2 rs = vec2(R / (2.0 * uTan.x * -P.z), R / (2.0 * uTan.y * -P.z));\n"
    "  rs = min(rs, vec2(0.12));\n"
    "  for (int i = 0; i < 12; i++) {\n"
    "    float fi = float(i);\n"
    "    float ang = a0 + fi * 2.39996;\n"
    "    float rad = (fi + 0.5) / 12.0;\n"
    "    vec2 suv = vUv + vec2(cos(ang), sin(ang)) * rs * rad;\n"
    "    vec3 Q = posAt(suv);\n"
    "    vec3 v = Q - P;\n"
    "    float dist = length(v);\n"
    "    float w = 1.0 - smoothstep(R * 0.6, R * 2.2, dist);\n"
    "    ao += max(0.0, dot(n, v / max(dist, 0.0001)) - 0.08) * w;\n"
    "  }\n"
    "  ao = clamp(1.0 - ao * 1.5 / 12.0, 0.0, 1.0);\n"
    "  gl_FragColor = vec4(ao, ao, ao, 1.0);\n"
    "}\n";

/* 3a: heldere delen (kwart resolutie) */
static const char *FS_BRIGHT =
    PRO_COMMON
    "uniform sampler2D uTex;\n"
    "uniform vec2 uPx;\n"      /* 1 / grootte van de bron */
    "void main() {\n"
    "  vec3 c = texture2D(uTex, vUv + vec2(-uPx.x, -uPx.y)).rgb + texture2D(uTex, vUv + vec2(uPx.x, -uPx.y)).rgb\n"
    "         + texture2D(uTex, vUv + vec2(-uPx.x, uPx.y)).rgb + texture2D(uTex, vUv + vec2(uPx.x, uPx.y)).rgb;\n"
    "  c *= 0.25;\n"
    "  float l = max(c.r, max(c.g, c.b));\n"
    "  vec3 b = c * smoothstep(0.62, 1.0, l) * 1.5;\n"
    "  gl_FragColor = vec4(b, 1.0);\n"
    "}\n";

/* 3b: gaussische vervaging (1 richting) */
static const char *FS_BLUR =
    PRO_COMMON
    "uniform sampler2D uTex;\n"
    "uniform vec2 uDir;\n"     /* texelstap in x of y */
    "void main() {\n"
    "  vec3 c = texture2D(uTex, vUv).rgb * 0.227;\n"
    "  c += (texture2D(uTex, vUv + uDir * 1.385).rgb + texture2D(uTex, vUv - uDir * 1.385).rgb) * 0.316;\n"
    "  c += (texture2D(uTex, vUv + uDir * 3.231).rgb + texture2D(uTex, vUv - uDir * 3.231).rgb) * 0.070;\n"
    "  gl_FragColor = vec4(c, 1.0);\n"
    "}\n";

/* 4: zonnestralen (kwart resolutie): lucht-pixels richting de zon uitsmeren */
static const char *FS_RAYS =
    PRO_COMMON
    "uniform sampler2D uTex;\n"
    "uniform sampler2D uDepth;\n"
    "uniform vec2 uSunUv;\n"
    "void main() {\n"
    "  vec2 dlt = (vUv - uSunUv) * (1.0 / 28.0) * 0.92;\n"
    "  vec2 uv = vUv;\n"
    "  float decay = 1.0, sum = 0.0, j = hash(vUv * 517.0);\n"
    "  uv -= dlt * j;\n"
    "  for (int i = 0; i < 28; i++) {\n"
    "    uv -= dlt;\n"
    "    float sky = step(0.9999, unpackD(texture2D(uDepth, uv)));\n"
    "    vec3 c = texture2D(uTex, uv).rgb;\n"
    "    sum += sky * smoothstep(0.55, 1.0, dot(c, vec3(0.33))) * decay;\n"
    "    decay *= 0.95;\n"
    "  }\n"
    "  sum /= 28.0;\n"
    "  gl_FragColor = vec4(vec3(sum), 1.0);\n"
    "}\n";

/* 5: eindbeeld */
static const char *FS_FINAL =
    PRO_COMMON
    "uniform sampler2D uTex;\n"
    "uniform sampler2D uAO;\n"
    "uniform sampler2D uBloom;\n"
    "uniform sampler2D uRays;\n"
    "uniform vec2 uPx;\n"
    "uniform vec2 uAoPx;\n"
    "uniform vec3 uRayCol;\n"
    "uniform float uTime;\n"
    "uniform float uAoOn;\n"
    "vec3 fxaa(vec2 uv) {\n"
    "  vec3 nw = texture2D(uTex, uv + vec2(-1.0, -1.0) * uPx).rgb;\n"
    "  vec3 ne = texture2D(uTex, uv + vec2(1.0, -1.0) * uPx).rgb;\n"
    "  vec3 sw = texture2D(uTex, uv + vec2(-1.0, 1.0) * uPx).rgb;\n"
    "  vec3 se = texture2D(uTex, uv + vec2(1.0, 1.0) * uPx).rgb;\n"
    "  vec3 m = texture2D(uTex, uv).rgb;\n"
    "  vec3 L = vec3(0.299, 0.587, 0.114);\n"
    "  float lnw = dot(nw, L), lne = dot(ne, L), lsw = dot(sw, L), lse = dot(se, L), lm = dot(m, L);\n"
    "  float lmin = min(lm, min(min(lnw, lne), min(lsw, lse)));\n"
    "  float lmax = max(lm, max(max(lnw, lne), max(lsw, lse)));\n"
    "  vec2 dir = vec2(-((lnw + lne) - (lsw + lse)), ((lnw + lsw) - (lne + lse)));\n"
    "  float red = max((lnw + lne + lsw + lse) * 0.03125, 1.0 / 128.0);\n"
    "  float rcp = 1.0 / (min(abs(dir.x), abs(dir.y)) + red);\n"
    "  dir = clamp(dir * rcp, vec2(-8.0), vec2(8.0)) * uPx;\n"
    "  vec3 a = 0.5 * (texture2D(uTex, uv + dir * (1.0 / 3.0 - 0.5)).rgb + texture2D(uTex, uv + dir * (2.0 / 3.0 - 0.5)).rgb);\n"
    "  vec3 b = a * 0.5 + 0.25 * (texture2D(uTex, uv + dir * -0.5).rgb + texture2D(uTex, uv + dir * 0.5).rgb);\n"
    "  float lb = dot(b, L);\n"
    "  return (lb < lmin || lb > lmax) ? a : b;\n"
    "}\n"
    "void main() {\n"
    "  vec3 c = fxaa(vUv);\n"
    "  float ao = texture2D(uAO, vUv).r * 0.4 + 0.15 * (texture2D(uAO, vUv + vec2(uAoPx.x, 0.0)).r + texture2D(uAO, vUv - vec2(uAoPx.x, 0.0)).r\n"
    "           + texture2D(uAO, vUv + vec2(0.0, uAoPx.y)).r + texture2D(uAO, vUv - vec2(0.0, uAoPx.y)).r);\n"
    "  c *= mix(1.0, ao, 0.9 * uAoOn);\n"
    "  c += texture2D(uBloom, vUv).rgb * 0.45;\n"
    "  c += texture2D(uRays, vUv).r * uRayCol;\n"
    "  float vg = smoothstep(0.92, 0.3, length(vUv - 0.5));\n"
    "  c *= mix(0.8, 1.0, vg);\n"
    "  float l = dot(c, vec3(0.299, 0.587, 0.114));\n"
    "  c = mix(vec3(l), c, 1.05);\n"
    "  c += (hash(vUv * 1937.0 + fract(uTime) * 61.0) - 0.5) * 0.014;\n"
    "  gl_FragColor = vec4(c, 1.0);\n"
    "}\n";

enum { PP_AO = 0, PP_BRIGHT, PP_BLUR, PP_RAYS, PP_FINAL, PP_NPROG };
enum { PT_SCENE = 0, PT_DEPTH, PT_AO, PT_BLA, PT_BLB, PT_RAYS, PT_N };

static GLuint pro_link(const char *fs) {
    char *f = cat2(PRO_QUAL_PREC, fs);
    GLuint p = link_prog(VS_POST, f, 0);
    free(f);
    return p;
}

static void pro_free_targets(Renderer *r) {
    int i;
    for (i = 0; i < PT_N; i++) {
        if (r->pFbo[i]) glDeleteFramebuffers(1, &r->pFbo[i]);
        if (r->pTex[i]) glDeleteTextures(1, &r->pTex[i]);
        if (r->pRb[i]) glDeleteRenderbuffers(1, &r->pRb[i]);
        r->pFbo[i] = r->pTex[i] = r->pRb[i] = 0;
    }
    r->proW = r->proH = 0;
}

static int pro_target(Renderer *r, int i, int w, int h, int depth) {
    GLint ok;
    glGenTextures(1, &r->pTex[i]);
    glBindTexture(GL_TEXTURE_2D, r->pTex[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, i == PT_DEPTH ? GL_NEAREST : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, i == PT_DEPTH ? GL_NEAREST : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &r->pFbo[i]);
    glBindFramebuffer(GL_FRAMEBUFFER, r->pFbo[i]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r->pTex[i], 0);
    if (depth) {
        glGenRenderbuffers(1, &r->pRb[i]);
        glBindRenderbuffer(GL_RENDERBUFFER, r->pRb[i]);
        glRenderbufferStorage(GL_RENDERBUFFER, 0x81A6 /* DEPTH_COMPONENT24 */, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, r->pRb[i]);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {   /* geen 24 bits: dan 16 */
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, w, h);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, r->pRb[i]);
        }
    }
    ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    return ok;
}

/* doelen (her)maken voor deze schermgrootte */
static int pro_setup(Renderer *r, int w, int h) {
    int ok = 1, hw = w / 2 > 1 ? w / 2 : 1, hh = h / 2 > 1 ? h / 2 : 1, qw = w / 4 > 1 ? w / 4 : 1, qh = h / 4 > 1 ? h / 4 : 1;
    GLint prev = 0;
    if (r->proOk && r->proW == w && r->proH == h) return 1;
    if (r->proFailed) return 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev);
    if (!r->pProg[0]) {
        static const float q[12] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};
        GLint lk = 0;
        int k;
        r->pProg[PP_AO] = pro_link(FS_AO);
        r->pProg[PP_BRIGHT] = pro_link(FS_BRIGHT);
        r->pProg[PP_BLUR] = pro_link(FS_BLUR);
        r->pProg[PP_RAYS] = pro_link(FS_RAYS);
        r->pProg[PP_FINAL] = pro_link(FS_FINAL);
        for (k = 0; k < PP_NPROG; k++) {
            glGetProgramiv(r->pProg[k], GL_LINK_STATUS, &lk);
            if (!lk) { plat_log("SGB-PRO: nabewerking %d werkt niet (shader)", k); r->proFailed = 1; return 0; }
        }
        glGenBuffers(1, &r->pQuad);
        glBindBuffer(GL_ARRAY_BUFFER, r->pQuad);
        glBufferData(GL_ARRAY_BUFFER, sizeof(q), q, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    pro_free_targets(r);
    ok &= pro_target(r, PT_SCENE, w, h, 1);
    ok &= pro_target(r, PT_DEPTH, hw, hh, 1);
    ok &= pro_target(r, PT_AO, hw, hh, 0);
    ok &= pro_target(r, PT_BLA, qw, qh, 0);
    ok &= pro_target(r, PT_BLB, qw, qh, 0);
    ok &= pro_target(r, PT_RAYS, qw, qh, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev);
    if (!ok) { plat_log("SGB-PRO: framebuffers niet compleet, PRO-nabewerking uit"); pro_free_targets(r); r->proFailed = 1; r->proOk = 0; return 0; }
    r->proW = w; r->proH = h; r->proOk = 1;
    plat_log("SGB-PRO: nabewerking aan (%dx%d): SSAO, bloom, zonnestralen, FXAA", w, h);
    return 1;
}

static void pro_quad(Renderer *r) {
    glBindBuffer(GL_ARRAY_BUFFER, r->pQuad);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, (const void *)0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    r->drawCalls++;
}

static void pro_bind_tex(int unit, GLuint t) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, t);
}

static void pro_pass(Renderer *r, int target, int w, int h, int prog) {
    glBindFramebuffer(GL_FRAMEBUFFER, r->pFbo[target]);
    glViewport(0, 0, w, h);
    glUseProgram(r->pProg[prog]);
}

/* alles na het tekenen van de 3D-wereld in PT_SCENE en PT_DEPTH */
static void pro_post(Renderer *r, Game *g, const GLint *vp, GLuint outFb) {
    int w = r->proW, h = r->proH, hw = w / 2 > 1 ? w / 2 : 1, hh = h / 2 > 1 ? h / 2 : 1, qw = w / 4 > 1 ? w / 4 : 1, qh = h / 4 > 1 ? h / 4 : 1;
    float fov = (g->camFov > 1.0f ? g->camFov : 58.0f) * PI_F / 180.0f, ty = tanf(fov * 0.5f), aspect = (float)r->W / (float)(r->H > 0 ? r->H : 1);
    float sunUv[2] = {-10.0f, -10.0f}, rayK = 0;
    GLuint P;
    int k;
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    /* SSAO */
    pro_pass(r, PT_AO, hw, hh, PP_AO); P = r->pProg[PP_AO];
    pro_bind_tex(0, r->pTex[PT_DEPTH]);
    glUniform1i(glGetUniformLocation(P, "uDepth"), 0);
    glUniform2f(glGetUniformLocation(P, "uPx"), 1.0f / hw, 1.0f / hh);
    glUniform2f(glGetUniformLocation(P, "uNF"), 0.3f, g->drawDist + 40.0f);
    glUniform2f(glGetUniformLocation(P, "uTan"), ty * aspect, ty);
    pro_quad(r);
    /* bloom: helder deel, dan twee keer horizontaal + verticaal vervagen */
    pro_pass(r, PT_BLA, qw, qh, PP_BRIGHT); P = r->pProg[PP_BRIGHT];
    pro_bind_tex(0, r->pTex[PT_SCENE]);
    glUniform1i(glGetUniformLocation(P, "uTex"), 0);
    glUniform2f(glGetUniformLocation(P, "uPx"), 1.0f / w, 1.0f / h);
    pro_quad(r);
    for (k = 0; k < 2; k++) {
        pro_pass(r, PT_BLB, qw, qh, PP_BLUR); P = r->pProg[PP_BLUR];
        pro_bind_tex(0, r->pTex[PT_BLA]);
        glUniform1i(glGetUniformLocation(P, "uTex"), 0);
        glUniform2f(glGetUniformLocation(P, "uDir"), (1.0f + k) / qw, 0.0f);
        pro_quad(r);
        pro_pass(r, PT_BLA, qw, qh, PP_BLUR);
        pro_bind_tex(0, r->pTex[PT_BLB]);
        glUniform2f(glGetUniformLocation(P, "uDir"), 0.0f, (1.0f + k) / qh);
        pro_quad(r);
    }
    /* zonnestralen: waar staat de zon op het scherm? */
    {
        V3 sp = v3add(r->cam, v3scale(v3(r->sun[0], r->sun[1], r->sun[2]), 1000.0f));
        float cx = r->vp.m[0] * sp.x + r->vp.m[4] * sp.y + r->vp.m[8] * sp.z + r->vp.m[12];
        float cy = r->vp.m[1] * sp.x + r->vp.m[5] * sp.y + r->vp.m[9] * sp.z + r->vp.m[13];
        float cw = r->vp.m[3] * sp.x + r->vp.m[7] * sp.y + r->vp.m[11] * sp.z + r->vp.m[15];
        if (cw > 0.01f && r->sun[1] > 0.02f) {
            sunUv[0] = cx / cw * 0.5f + 0.5f; sunUv[1] = cy / cw * 0.5f + 0.5f;
            rayK = clampf(1.4f - 0.9f * sqrtf((sunUv[0] - 0.5f) * (sunUv[0] - 0.5f) + (sunUv[1] - 0.5f) * (sunUv[1] - 0.5f)), 0, 1);
            rayK *= clampf((r->sunCol[0] + r->sunCol[1]) - 0.4f, 0, 1) * (1.0f - r->rain);
        }
    }
    pro_pass(r, PT_RAYS, qw, qh, PP_RAYS); P = r->pProg[PP_RAYS];
    if (rayK > 0.01f) {
        pro_bind_tex(0, r->pTex[PT_SCENE]);
        pro_bind_tex(1, r->pTex[PT_DEPTH]);
        glUniform1i(glGetUniformLocation(P, "uTex"), 0);
        glUniform1i(glGetUniformLocation(P, "uDepth"), 1);
        glUniform2f(glGetUniformLocation(P, "uSunUv"), sunUv[0], sunUv[1]);
        pro_quad(r);
    } else {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    /* eindbeeld naar het scherm */
    glBindFramebuffer(GL_FRAMEBUFFER, outFb);
    glViewport(vp[0], vp[1], vp[2], vp[3]);
    P = r->pProg[PP_FINAL];
    glUseProgram(P);
    pro_bind_tex(0, r->pTex[PT_SCENE]);
    pro_bind_tex(1, r->pTex[PT_AO]);
    pro_bind_tex(2, r->pTex[PT_BLA]);
    pro_bind_tex(3, r->pTex[PT_RAYS]);
    glUniform1i(glGetUniformLocation(P, "uTex"), 0);
    glUniform1i(glGetUniformLocation(P, "uAO"), 1);
    glUniform1i(glGetUniformLocation(P, "uBloom"), 2);
    glUniform1i(glGetUniformLocation(P, "uRays"), 3);
    glUniform2f(glGetUniformLocation(P, "uPx"), 1.0f / w, 1.0f / h);
    glUniform2f(glGetUniformLocation(P, "uAoPx"), 1.0f / hw, 1.0f / hh);
    glUniform3f(glGetUniformLocation(P, "uRayCol"), r->sunCol[0] * 0.55f * rayK, r->sunCol[1] * 0.5f * rayK, r->sunCol[2] * 0.4f * rayK);
    glUniform1f(glGetUniformLocation(P, "uTime"), g->time);
    glUniform1f(glGetUniformLocation(P, "uAoOn"), 1.0f);
    pro_quad(r);
    /* staat terug zoals de UI het verwacht */
    pro_bind_tex(3, 0); pro_bind_tex(2, 0); pro_bind_tex(1, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glDepthMask(GL_TRUE);
}

/* hele frame met de PRO-engine: 0 = lukte niet (dan gewoon tekenen) */
static int pro_draw(Renderer *r, Game *g) {
    GLint vp[4], prevFb = 0;
    glGetIntegerv(GL_VIEWPORT, vp);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFb);
    if (vp[2] < 16 || vp[3] < 16) return 0;
    if (!pro_setup(r, vp[2], vp[3])) return 0;
    /* dieptepass (halve resolutie) */
    glBindFramebuffer(GL_FRAMEBUFFER, r->pFbo[PT_DEPTH]);
    glViewport(0, 0, r->proW / 2 > 1 ? r->proW / 2 : 1, r->proH / 2 > 1 ? r->proH / 2 : 1);
    glClearColor(1, 1, 1, 1);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    r->pass = 2;
    draw_scene_one(r, g);
    r->pass = 0;
    /* de wereld zelf */
    glBindFramebuffer(GL_FRAMEBUFFER, r->pFbo[PT_SCENE]);
    glViewport(0, 0, r->proW, r->proH);
    glClearColor(r->fogCol[0], r->fogCol[1], r->fogCol[2], 1);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    draw_scene_one(r, g);
    pro_post(r, g, vp, (GLuint)prevFb);
    return 1;
}
