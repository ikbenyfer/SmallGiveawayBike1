#include "render.h"
#include "../Game/home.h"
#include "../Game/biz.h"
#include "../Game/stores.h"
#include "../Game/voice.h"
#include "../Game/human.h"
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


/* ------------------------------------------------------------------ v6.0 HQ-shaders (kwaliteit Hoog) */

#define HQ_PREC "#ifdef GL_ES\n#ifdef GL_FRAGMENT_PRECISION_HIGH\nprecision highp float;\n#else\nprecision mediump float;\n#endif\n#endif\n"

static const char *VS_HQ =
    "attribute vec3 aPos;\n"
    "attribute vec4 aNrm;\n"
    "attribute vec4 aCol;\n"
    "uniform mat4 uVP;\n"
    "uniform mat4 uModel;\n"
    "uniform mat4 uShadowVP;\n"
    "uniform vec4 uTint;\n"
    "uniform vec3 uCam;\n"
    "uniform vec2 uFog;\n"
    "varying vec3 vCol;\n"
    "varying vec3 vN;\n"
    "varying vec3 vW;\n"
    "varying vec4 vSh;\n"
    "varying float vFog;\n"
    "varying float vMat;\n"
    "varying float vEm;\n"
    "void main() {\n"
    "  vec4 wp = uModel * vec4(aPos, 1.0);\n"
    "  vW = wp.xyz;\n"
    "  vN = (uModel * vec4(aNrm.xyz, 0.0)).xyz;\n"
    "  vCol = aCol.rgb * uTint.rgb;\n"
    "  vEm = aCol.a;\n"
    "  vMat = floor(aNrm.w * 127.0 + 0.5);\n"
    "  vSh = uShadowVP * wp;\n"
    "  float dist = length(wp.xyz - uCam);\n"
    "  vFog = clamp((dist - uFog.x) / (uFog.y - uFog.x), 0.0, 1.0);\n"
    "  gl_Position = uVP * wp;\n"
    "}\n";

static const char *FS_HQ_BODY =
    "varying vec3 vCol;\n"
    "varying vec3 vN;\n"
    "varying vec3 vW;\n"
    "varying vec4 vSh;\n"
    "varying float vFog;\n"
    "varying float vMat;\n"
    "varying float vEm;\n"
    "uniform vec3 uSun;\n"
    "uniform vec3 uSunCol;\n"
    "uniform vec3 uAmb;\n"
    "uniform vec3 uCam;\n"
    "uniform vec3 uFogCol;\n"
    "uniform vec3 uSkyTop;\n"
    "uniform vec3 uSkyHor;\n"
    "uniform float uEmis;\n"
    "uniform float uUnlit;\n"
    "uniform float uAlpha;\n"
    "uniform float uRain;\n"
    "uniform float uTime;\n"
    "uniform float uShadowOn;\n"
    "uniform float uTexel;\n"
    "uniform float uUltra;\n"
    "uniform float uEmis2;\n"
    "uniform sampler2D uShadow;\n"
    "float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }\n"
    "float noise(vec2 p) { vec2 i = floor(p); vec2 f = fract(p); f = f * f * (3.0 - 2.0 * f);\n"
    "  return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x), mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x), f.y); }\n"
    "float fbm(vec2 p) { return noise(p) * 0.5 + noise(p * 2.03) * 0.25 + noise(p * 4.01) * 0.125 + noise(p * 8.07) * 0.0625; }\n"
    "vec3 bumpN(vec3 n, vec3 p, float s, float a) {\n"
    "  vec3 an = abs(n), t, b; vec2 uv;\n"
    "  if (an.y > 0.6) { uv = p.xz; t = vec3(1.0, 0.0, 0.0); b = vec3(0.0, 0.0, 1.0); }\n"
    "  else if (an.x > an.z) { uv = p.zy; t = vec3(0.0, 0.0, 1.0); b = vec3(0.0, 1.0, 0.0); }\n"
    "  else { uv = p.xy; t = vec3(1.0, 0.0, 0.0); b = vec3(0.0, 1.0, 0.0); }\n"
    "  float e = 0.35 / s;\n"
    "  float h0 = fbm(uv * s), hx = fbm((uv + vec2(e, 0.0)) * s), hy = fbm((uv + vec2(0.0, e)) * s);\n"
    "  return normalize(n - (t * (hx - h0) + b * (hy - h0)) * (a / e));\n"
    "}\n"
    "float brickMortar(vec2 bp) { bp.x += mod(floor(bp.y), 2.0) * 0.5; vec2 f = fract(bp);\n"
    "  return max(1.0 - smoothstep(0.0, 0.07, f.x), 1.0 - smoothstep(0.0, 0.13, f.y)); }\n"
    "float tileJoint(vec2 tp) { vec2 tf = fract(tp); return max(1.0 - smoothstep(0.0, 0.05, tf.x), 1.0 - smoothstep(0.0, 0.05, tf.y)); }\n"
    "float unpackD(vec4 c) { return dot(c, vec4(1.0, 1.0 / 255.0, 1.0 / 65025.0, 1.0 / 16581375.0)); }\n"
    "float shadowAt(vec2 uv, float z) { return unpackD(texture2D(uShadow, uv)) < z ? 0.0 : 1.0; }\n"
    "float shadowF(float ndl) {\n"
    "  vec3 s = vSh.xyz / vSh.w * 0.5 + 0.5;\n"
    "  if (s.x < 0.0 || s.x > 1.0 || s.y < 0.0 || s.y > 1.0 || s.z > 1.0) return 1.0;\n"
    "  float b = 0.0012 + 0.003 * (1.0 - ndl);\n"
    "  float z = s.z - b;\n"
    "  float t = uTexel;\n"
    "  float sum = shadowAt(s.xy + vec2(-t, -t), z) + shadowAt(s.xy + vec2(t, -t), z) + shadowAt(s.xy + vec2(-t, t), z) + shadowAt(s.xy + vec2(t, t), z);\n"
    "  if (uUltra < 0.5) return sum * 0.25;\n"
    "  float a = hash(gl_FragCoord.xy) * 6.2832; mat2 rot = mat2(cos(a), sin(a), -sin(a), cos(a));\n"
    "  sum += shadowAt(s.xy + rot * vec2(-0.94, -0.40) * t * 2.2, z);\n"
    "  sum += shadowAt(s.xy + rot * vec2(0.95, -0.77) * t * 2.2, z);\n"
    "  sum += shadowAt(s.xy + rot * vec2(-0.09, -0.93) * t * 2.2, z);\n"
    "  sum += shadowAt(s.xy + rot * vec2(0.34, 0.29) * t * 2.2, z);\n"
    "  sum += shadowAt(s.xy + rot * vec2(-0.92, 0.46) * t * 2.2, z);\n"
    "  sum += shadowAt(s.xy + rot * vec2(-0.22, 0.03) * t * 2.2, z);\n"
    "  sum += shadowAt(s.xy + rot * vec2(0.49, -0.42) * t * 2.2, z);\n"
    "  sum += shadowAt(s.xy + rot * vec2(0.80, 0.53) * t * 2.2, z);\n"
    "  sum += shadowAt(s.xy + rot * vec2(-0.38, 0.86) * t * 2.2, z);\n"
    "  sum += shadowAt(s.xy + rot * vec2(0.19, 0.98) * t * 2.2, z);\n"
    "  sum += shadowAt(s.xy + rot * vec2(-0.70, -0.82) * t * 2.2, z);\n"
    "  sum += shadowAt(s.xy + rot * vec2(0.97, 0.06) * t * 2.2, z);\n"
    "  return sum / 16.0;\n"
    "}\n"
    "vec3 skyCol(vec3 d) { float t = clamp(d.y * 1.4 + 0.15, 0.0, 1.0); return mix(uSkyHor, uSkyTop, t); }\n"
    "vec3 envCity(vec3 r) {\n"
    "  vec3 e = skyCol(r);\n"
    "  if (r.y < 0.3) {\n"
    "    float ang = atan(r.z, r.x);\n"
    "    float cl = floor(ang * 16.0);\n"
    "    float bh = 0.04 + 0.24 * hash(vec2(cl, 3.1));\n"
    "    float bld = 1.0 - smoothstep(bh - 0.006, bh + 0.006, r.y);\n"
    "    vec3 bc = uFogCol * 0.42 + uAmb * 0.25 + uSunCol * 0.12 * hash(vec2(cl, 9.7));\n"
    "    float win = step(0.62, hash(vec2(floor(ang * 150.0), floor(r.y * 90.0)))) * uEmis2;\n"
    "    bc += vec3(1.0, 0.82, 0.5) * win * 0.7;\n"
    "    e = mix(e, bc, bld);\n"
    "  }\n"
    "  return e;\n"
    "}\n"
    "float ggx(vec3 n, vec3 h, float r) { float a = max(r * r, 0.002); float a2 = a * a; float nh = max(dot(n, h), 0.0);\n"
    "  float d = nh * nh * (a2 - 1.0) + 1.0; return a2 / (3.14159 * d * d); }\n"
    "vec3 filmic(vec3 x) { x *= 0.94; return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0); }\n"
    "void main() {\n"
    "  vec3 n = normalize(vN);\n"
    "  vec3 v = normalize(uCam - vW);\n"
    "  vec3 c = vCol;\n"
    "  float m = vMat, spec = 0.08, refl = 0.0, rough = 0.7, wetOk = 0.0, skin = 0.0, cloth = 0.0, paint = 0.0;\n"
    "  float bf = uUltra > 0.5 ? 1.0 - smoothstep(20.0, 55.0, length(uCam - vW)) : 0.0;\n"
    "  if (m < 0.5) {\n"
    "    c *= 0.94 + 0.12 * noise(vW.xz * 2.0 + vW.y * 1.7); wetOk = n.y > 0.7 ? 1.0 : 0.0;\n"
    "  } else if (m < 1.5) {\n"   /* asfalt: korrel, scheuren, reparatievlakken */
    "    float g = fbm(vW.xz * 0.9);\n"
    "    float crack = 1.0 - smoothstep(0.0, 0.018, abs(noise(vW.xz * 0.32) - 0.5));\n"
    "    crack *= smoothstep(0.55, 0.75, noise(vW.xz * 0.05));\n"
    "    float patch = smoothstep(0.70, 0.72, noise(vW.xz * 0.07 + 3.0));\n"
    "    c *= (0.78 + 0.38 * g) * mix(1.0, 0.8, patch);\n"
    "    c = mix(c, c * 0.45, crack);\n"
    "    c *= 0.92 + 0.16 * hash(floor(vW.xz * 18.0));\n"
    "    spec = 0.12; rough = 0.85; wetOk = 1.0;\n"
    "    if (bf > 0.0) n = bumpN(n, vW, 9.0, 0.006 * bf);\n"
    "  } else if (m < 2.5) {\n"   /* baksteen: stenen in halfsteens verband, voegen */
    "    vec2 bp = vec2(dot(vW.xz, vec2(-n.z, n.x)) * 4.4, vW.y * 13.0);\n"
    "    bp.x += mod(floor(bp.y), 2.0) * 0.5;\n"
    "    vec2 f = fract(bp);\n"
    "    float mortar = max(1.0 - smoothstep(0.0, 0.07, f.x), 1.0 - smoothstep(0.0, 0.13, f.y));\n"
    "    if (abs(n.y) > 0.7) mortar = 0.0;\n"
    "    c *= 0.82 + 0.3 * hash(floor(bp)) + 0.1 * noise(vW.xz * 3.0 + vW.y);\n"
    "    c = mix(c, vec3(0.72, 0.70, 0.66) * (0.6 + 0.4 * length(vCol)), mortar * 0.75);\n"
    "    spec = 0.04; rough = 0.9;\n"
    "    if (bf > 0.0 && abs(n.y) < 0.7) {\n"
    "      vec3 tw = normalize(vec3(-n.z, 0.0, n.x));\n"
    "      vec2 b0 = vec2(dot(vW.xz, vec2(-n.z, n.x)) * 4.4, vW.y * 13.0);\n"
    "      float m0 = brickMortar(b0), m1 = brickMortar(b0 + vec2(0.05, 0.0)), m2 = brickMortar(b0 + vec2(0.0, 0.05));\n"
    "      float gx = -(m1 - m0) * 0.008 / (0.05 / 4.4), gy = -(m2 - m0) * 0.008 / (0.05 / 13.0);\n"
    "      n = normalize(n - (tw * gx + vec3(0.0, gy, 0.0)) * bf * 0.5);\n"
    "      n = bumpN(n, vW, 14.0, 0.0025 * bf);\n"
    "    }\n"
    "  } else if (m < 3.5) {\n"   /* glas */
    "    spec = 1.0; refl = 0.55; rough = 0.05;\n"
    "  } else if (m < 4.5) {\n"   /* autolak met blanke laklaag */
    "    spec = 1.2; refl = 0.32; rough = 0.12; paint = 1.0;\n"
    "  } else if (m < 5.5) {\n"   /* glimmend: chroom, metaal, kunststof */
    "    spec = 0.9; refl = 0.4; rough = 0.25;\n"
    "  } else if (m < 6.5) {\n"   /* gras */
    "    c *= 0.72 + 0.5 * fbm(vW.xz * 1.6) + 0.12 * hash(floor(vW.xz * 9.0));\n"
    "    spec = 0.02; rough = 1.0;\n"
    "    if (bf > 0.0) n = bumpN(n, vW, 7.0, 0.02 * bf);\n"
    "  } else if (m < 7.5) {\n"   /* beton/stoeptegels: tegelvoegen */
    "    vec2 tp = vW.xz / 0.6; vec2 tf = fract(tp);\n"
    "    float joint = max(1.0 - smoothstep(0.0, 0.05, tf.x), 1.0 - smoothstep(0.0, 0.05, tf.y));\n"
    "    if (n.y < 0.7) joint = 0.0;\n"
    "    c *= (0.86 + 0.2 * hash(floor(tp))) * (0.92 + 0.12 * noise(vW.xz * 4.0));\n"
    "    c = mix(c, c * 0.62, joint);\n"
    "    spec = 0.06; rough = 0.8; wetOk = 1.0;\n"
    "    if (bf > 0.0 && n.y > 0.7) {\n"
    "      vec2 t0 = vW.xz / 0.6;\n"
    "      float j0 = tileJoint(t0), j1 = tileJoint(t0 + vec2(0.04, 0.0)), j2 = tileJoint(t0 + vec2(0.0, 0.04));\n"
    "      n = normalize(n + vec3((j1 - j0), 0.0, (j2 - j0)) * (0.006 / (0.04 * 0.6)) * bf);\n"
    "      n = bumpN(n, vW, 11.0, 0.003 * bf);\n"
    "    }\n"
    "  } else if (m < 8.5) {\n"   /* dakpannen: rijen */
    "    float row = fract(vW.y * 4.5 + dot(vW.xz, vec2(0.13, 0.07)));\n"
    "    c *= 0.78 + 0.25 * smoothstep(0.0, 0.8, row) + 0.08 * hash(floor(vW.xz * 4.0 + vW.y * 4.5));\n"
    "    spec = 0.12; rough = 0.6;\n"
    "    if (bf > 0.0) n = bumpN(n, vW, 10.0, 0.004 * bf);\n"
    "  } else if (m < 9.5) {\n"   /* water: golfjes */
    "    vec2 q = vW.xz * 0.6 + vec2(uTime * 0.25, uTime * 0.18);\n"
    "    n = normalize(n + vec3(noise(q) - 0.5, 0.0, noise(q + 7.3) - 0.5) * 0.35);\n"
    "    spec = 1.0; refl = 0.65; rough = 0.05;\n"
    "  } else if (m < 10.5) {\n"   /* v6.2.0 huid: kleurvariatie, porien, zachte glans */
    "    c *= 0.95 + 0.1 * noise(vW.xy * 30.0 + vW.z * 17.0);\n"
    "    c = mix(c, c * vec3(1.06, 0.93, 0.9), 0.5 * noise(vW.xz * 9.0 + vW.y * 7.0));\n"
    "    if (uUltra > 0.5) n = normalize(n + (vec3(noise(vW.xy * 260.0), noise(vW.yz * 260.0), noise(vW.zx * 260.0)) - 0.5) * 0.09);\n"
    "    spec = 0.22; rough = 0.42; skin = 1.0;\n"
    "  } else if (m < 11.5) {\n"   /* stof: weefsel en draad */
    "    float wv = noise(vec2(dot(vW.xz, vec2(0.7, 0.7)), vW.y) * 140.0);\n"
    "    float th = abs(fract((vW.y + vW.x * 0.3) * 120.0) - 0.5);\n"
    "    c *= 0.88 + 0.14 * wv + 0.06 * th;\n"
    "    c *= 0.93 + 0.12 * fbm(vW.xz * 6.0 + vW.y * 5.0);\n"
    "    spec = 0.03; rough = 0.95; cloth = 1.0;\n"
    "  } else if (m < 12.5) {\n"   /* haar: strengen */
    "    c *= 0.72 + 0.45 * noise(vec2(dot(vW.xz, vec2(1.0, 0.6)) * 160.0, vW.y * 9.0));\n"
    "    spec = 0.55; rough = 0.32;\n"
    "  } else {\n"   /* rubber: banden, bumpers */
    "    c *= 0.9 + 0.14 * noise(vW.xz * 40.0 + vW.y * 40.0);\n"
    "    spec = 0.1; rough = 0.78;\n"
    "  }\n"
    "  if (uRain > 0.01 && wetOk > 0.5 && n.y > 0.7) {\n"   /* nat: donkerder, glanzend, plassen */
    "    float puddle = smoothstep(0.55, 0.72, fbm(vW.xz * 0.22));\n"
    "    float w = uRain * (0.5 + 0.5 * puddle);\n"
    "    c *= 1.0 - 0.42 * w;\n"
    "    refl = max(refl, w * (0.35 + 0.45 * puddle));\n"
    "    spec = max(spec, w * 1.2);\n"
    "    rough = mix(rough, 0.04, w);\n"
    "    float rip = noise(vW.xz * 6.0 + uTime * 3.0);\n"
    "    n = normalize(n + vec3(rip - 0.5, 0.0, noise(vW.xz * 6.0 - uTime * 2.7) - 0.5) * 0.12 * puddle * uRain);\n"
    "  }\n"
    "  float ndr = dot(n, uSun);\n"
    "  float ndl = max(mix(ndr, (ndr + 0.4) / 1.4, skin), 0.0);\n"
    "  float sh = uShadowOn > 0.5 ? shadowF(max(ndr, 0.0)) : 1.0;\n"
    "  float ao = 1.0;\n"
    "  if (uUltra > 0.5 && n.y < 0.6) ao = mix(0.55, 1.0, smoothstep(0.0, 0.85, vW.y));\n"
    "  vec3 amb = uAmb * (0.7 + 0.3 * n.y);\n"
    "  if (uUltra > 0.5) amb = mix(uAmb * vec3(0.9, 0.85, 0.78) * 0.6, uAmb * 1.08, n.y * 0.5 + 0.5) + uSunCol * vec3(0.06, 0.05, 0.035) * (1.0 - n.y) * 0.5;\n"
    "  vec3 light = amb * ao + uSunCol * ndl * sh;\n"
    "  light += uSunCol * vec3(0.32, 0.08, 0.04) * skin * max(0.0, 0.35 - ndr) * (0.4 + 0.6 * sh);\n"
    "  vec3 lit = c * light + c * vEm * uEmis;\n"
    "  vec3 h = normalize(uSun + v);\n"
    "  float sp;\n"
    "  if (uUltra > 0.5) {\n"
    "    float fh = 0.04 + 0.96 * pow(1.0 - max(dot(h, v), 0.0), 5.0);\n"
    "    sp = min(ggx(n, h, rough) * fh * spec * max(ndr, 0.0) * 0.6, 24.0) * sh;\n"
    "    if (paint > 0.5) {\n"
    "      float fl = step(0.9, hash(floor(vW.xz * 260.0) + floor(vW.y * 260.0) * 7.0));\n"
    "      sp += fl * pow(max(dot(n, h), 0.0), 18.0) * 0.9 * sh;\n"
    "      sp += min(ggx(n, h, 0.035) * fh * 0.5 * max(ndr, 0.0), 30.0) * sh;\n"
    "    }\n"
    "    if (cloth > 0.5) sp += pow(1.0 - max(dot(n, v), 0.0), 4.0) * 0.12 * max(ndr + 0.3, 0.0);\n"
    "  } else sp = pow(max(dot(n, h), 0.0), mix(6.0, 140.0, 1.0 - rough)) * spec * sh;\n"
    "  lit += uSunCol * sp;\n"
    "  float fres = refl * (0.22 + 0.78 * pow(1.0 - max(dot(n, v), 0.0), uUltra > 0.5 ? 5.0 : 3.0));\n"
    "  vec3 r = reflect(-v, n);\n"
    "  vec3 env = (uUltra > 0.5 ? envCity(r) : skyCol(r)) * (0.55 + 0.45 * max(uSunCol.r, 0.2));\n"
    "  if (r.y < 0.0) env = mix(env, uFogCol * 0.5, clamp(-r.y * 3.0, 0.0, 1.0));\n"
    "  if (uUltra > 0.5) env *= mix(1.0, 0.75, rough);\n"
    "  lit = mix(lit, env, clamp(fres, 0.0, 1.0));\n"
    "  lit = mix(lit, c, uUnlit);\n"
    "  vec3 outc = mix(lit, uFogCol, vFog);\n"
    "  if (uUltra > 0.5 && uUnlit < 0.5) { vec3 f = filmic(outc); float l = dot(f, vec3(0.299, 0.587, 0.114)); outc = mix(outc, mix(vec3(l), f, 1.1), 0.92); }\n"
    "  gl_FragColor = vec4(outc, uAlpha);\n"
    "}\n";

static const char *VS_SH =
    "attribute vec3 aPos;\n"
    "attribute vec4 aNrm;\n"
    "attribute vec4 aCol;\n"
    "uniform mat4 uVP;\n"
    "uniform mat4 uModel;\n"
    "varying float vD;\n"
    "void main() { vec4 p = uVP * (uModel * vec4(aPos, 1.0)); vD = p.z / p.w * 0.5 + 0.5; gl_Position = p; }\n";

static const char *FS_SH_BODY =
    "varying float vD;\n"
    "void main() {\n"
    "  vec4 e = fract(vec4(1.0, 255.0, 65025.0, 16581375.0) * vD);\n"
    "  e -= e.yzww * vec4(1.0 / 255.0, 1.0 / 255.0, 1.0 / 255.0, 0.0);\n"
    "  gl_FragColor = e;\n"
    "}\n";

static const char *VS_SKY =
    "attribute vec3 aPos;\n"
    "attribute vec4 aNrm;\n"
    "attribute vec4 aCol;\n"
    "uniform mat4 uVP;\n"
    "uniform mat4 uModel;\n"
    "varying vec3 vDir;\n"
    "void main() { vDir = aPos; gl_Position = uVP * (uModel * vec4(aPos, 1.0)); }\n";

static const char *FS_SKY_BODY =
    "varying vec3 vDir;\n"
    "uniform vec3 uSkyTop;\n"
    "uniform vec3 uSkyHor;\n"
    "uniform vec3 uSun;\n"
    "uniform vec3 uSunCol;\n"
    "uniform vec3 uAmb;\n"
    "uniform vec3 uFogCol;\n"
    "uniform float uRain;\n"
    "uniform float uTime;\n"
    "uniform float uUltra;\n"
    "vec3 filmic(vec3 x) { x *= 0.94; return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0); }\n"
    "float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }\n"
    "float noise(vec2 p) { vec2 i = floor(p); vec2 f = fract(p); f = f * f * (3.0 - 2.0 * f);\n"
    "  return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x), mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x), f.y); }\n"
    "float fbm(vec2 p) { return noise(p) * 0.5 + noise(p * 2.03) * 0.25 + noise(p * 4.01) * 0.125 + noise(p * 8.07) * 0.0625; }\n"
    "void main() {\n"
    "  vec3 d = normalize(vDir);\n"
    "  float t = clamp(d.y, 0.0, 1.0);\n"
    "  vec3 c = mix(uSkyHor, uSkyTop, sqrt(t));\n"
    "  float sd = max(dot(d, uSun), 0.0);\n"
    "  c += uSunCol * (pow(sd, 600.0) * 3.0 + pow(sd, 10.0) * 0.3) * (1.0 - uRain * 0.8);\n"
    "  if (d.y > 0.0) {\n"
    "    vec2 p = d.xz / (d.y + 0.12) * 1.3 + vec2(uTime * 0.008, uTime * 0.003);\n"
    "    float cl = smoothstep(0.42 - uRain * 0.35, 0.82, fbm(p));\n"
    "    vec3 cc = (uAmb * 0.9 + uSunCol * 0.6) * mix(1.0, 0.5, uRain);\n"
    "    cc += uSunCol * pow(sd, 6.0) * 0.4;\n"
    "    c = mix(c, cc, cl * smoothstep(0.0, 0.18, d.y));\n"
    "  }\n"
    "  c = mix(c, uFogCol, (1.0 - smoothstep(-0.02, 0.18, d.y)) * 0.85);\n"
    "  if (uUltra > 0.5) { vec3 f = filmic(c); float l = dot(f, vec3(0.299, 0.587, 0.114)); c = mix(c, mix(vec3(l), f, 1.1), 0.92); }\n"
    "  gl_FragColor = vec4(c, 1.0);\n"
    "}\n";

static char *cat2(const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    char *r = (char *)malloc(la + lb + 1);
    memcpy(r, a, la); memcpy(r + la, b, lb + 1);
    return r;
}

static GLuint compile(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    GLint ok = 0;
#ifdef SGB_DESKTOP_GL
    /* desktop-OpenGL 2.1: GLSL 1.20 kent geen precision-regel */
    static char buf[65536];
    if (strncmp(src, "precision", 9) == 0) { const char *nl = strchr(src, '\n'); if (nl) src = nl + 1; }
    snprintf(buf, sizeof(buf), "#version 120\n%s", src);
    src = buf;
#endif
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

/* v5.0 VR-hand in de ruimte van de controller (OpenXR aim-pose: -Z wijst naar voren, +Y omhoog).
 * side -1 = links, +1 = rechts. Handpalm met vingers en een duim, plus een stukje mouw. */
static void model_vr_hand(MeshB *m, float side) {
    Col skin = col(196, 140, 104), sleeve = col(60, 140, 80);
    int k;
    SQ s = sq(v3(0, -0.01f, 0.03f), v3(0.042f, 0.024f, 0.055f), 0.5f, 0.6f, skin);
    s.su = 12; s.sv = 8; mb_sq(m, &s);
    for (k = 0; k < 4; k++) {
        float x = (-0.03f + k * 0.02f) * side, len = k == 1 || k == 2 ? 0.075f : 0.062f;
        mb_capsule(m, v3(x, -0.005f, -0.02f), v3(x, -0.03f, -0.02f - len), 0.0105f, 0.0095f, 8, skin);
    }
    mb_capsule(m, v3(-0.04f * side, -0.008f, 0.04f), v3(-0.06f * side, -0.0f, -0.005f), 0.012f, 0.011f, 8, skin);   /* duim */
    mb_capsule(m, v3(0, -0.01f, 0.09f), v3(0, -0.01f, 0.2f), 0.045f, 0.05f, 12, sleeve);
}

/* dichte hand om een greep (de greep loopt van onder naar boven door de vuist, bij de oorsprong) */
static void model_vr_fist(MeshB *m, float side) {
    Col skin = col(196, 140, 104), sleeve = col(60, 140, 80);
    int k;
    SQ s = sq(v3(0.035f * side, 0.0f, 0.045f), v3(0.028f, 0.045f, 0.05f), 0.5f, 0.6f, skin);
    s.su = 12; s.sv = 8; mb_sq(m, &s);   /* handpalm naast de greep */
    for (k = 0; k < 4; k++) {   /* vingers om de greep heen gekruld */
        float y = 0.03f - k * 0.02f;
        mb_capsule(m, v3(0.035f * side, y, 0.02f), v3(0.0f, y, -0.03f), 0.0105f, 0.0105f, 8, skin);
        mb_capsule(m, v3(0.0f, y, -0.03f), v3(-0.03f * side, y - 0.005f, -0.005f), 0.0105f, 0.0095f, 8, skin);
    }
    mb_capsule(m, v3(0.02f * side, 0.045f, 0.03f), v3(-0.025f * side, 0.05f, -0.01f), 0.012f, 0.011f, 8, skin);   /* duim */
    mb_capsule(m, v3(0.035f * side, 0.0f, 0.1f), v3(0.035f * side, -0.01f, 0.21f), 0.045f, 0.05f, 12, sleeve);
}

/* stuur (in het XY-vlak, naar de bestuurder toe = +Z) */
static void model_vr_wheel(MeshB *m) {
    Col rim = col(30, 30, 34), spoke = col(70, 70, 76);
    int k;
    const float R = 0.19f;
    for (k = 0; k < 24; k++) {
        float a0 = k * 2 * PI_F / 24, a1 = (k + 1) * 2 * PI_F / 24;
        mb_capsule(m, v3(cosf(a0) * R, sinf(a0) * R, 0), v3(cosf(a1) * R, sinf(a1) * R, 0), 0.016f, 0.016f, 8, rim);
    }
    for (k = 0; k < 3; k++) {
        float a = -PI_F * 0.5f + k * PI_F * 2.0f / 3.0f + PI_F;
        mb_capsule(m, v3(0, 0, 0), v3(cosf(a) * R, sinf(a) * R, 0), 0.012f, 0.012f, 6, spoke);
    }
    {
        SQ s = sq(v3(0, 0, 0.01f), v3(0.05f, 0.05f, 0.03f), 0.5f, 0.8f, spoke);
        mb_sq(m, &s);
    }
    mb_box(m, v3(-0.012f, R - 0.02f, -0.005f), v3(0.012f, R + 0.02f, 0.02f), col_e(220, 40, 40, 80), 0);   /* streep bovenop */
}

int r_init(Renderer *r, int w, int h) {
    uint8_t wpx[4] = {255, 255, 255, 255};
    memset(r, 0, sizeof(*r));
    r->W = w; r->H = h;
    r->progW = link_prog(VS_W, FS_W, 0);
    {   /* v6.0: HQ-, schaduw- en lucht-programma's */
        static const char *names[12] = {"uVP", "uModel", "uSun", "uSunCol", "uAmb", "uCam", "uFog", "uFogCol", "uTint", "uEmis", "uAlpha", "uUnlit"};
        char *fhq = cat2(HQ_PREC, FS_HQ_BODY), *fsh = cat2(HQ_PREC, FS_SH_BODY), *fsk = cat2(HQ_PREC, FS_SKY_BODY);
        int pi, k;
        r->prog[0] = r->progW;
        r->prog[1] = link_prog(VS_HQ, fhq, 0);
        r->prog[2] = link_prog(VS_SH, fsh, 0);
        r->progSky = link_prog(VS_SKY, fsk, 0);
        free(fhq); free(fsh); free(fsk);
        for (pi = 0; pi < 3; pi++)
            for (k = 0; k < 12; k++) r->loc[pi][k] = glGetUniformLocation(r->prog[pi], names[k]);
        r->hqShadowVP = glGetUniformLocation(r->prog[1], "uShadowVP");
        r->hqShadowTex = glGetUniformLocation(r->prog[1], "uShadow");
        r->hqShadowOn = glGetUniformLocation(r->prog[1], "uShadowOn");
        r->hqTexel = glGetUniformLocation(r->prog[1], "uTexel");
        r->hqRain = glGetUniformLocation(r->prog[1], "uRain");
        r->hqTime = glGetUniformLocation(r->prog[1], "uTime");
        r->hqUltra = glGetUniformLocation(r->prog[1], "uUltra");
        r->hqEmis2 = glGetUniformLocation(r->prog[1], "uEmis2");
        r->hqSkyTop = glGetUniformLocation(r->prog[1], "uSkyTop");
        r->hqSkyHor = glGetUniformLocation(r->prog[1], "uSkyHor");
        r->skVP = glGetUniformLocation(r->progSky, "uVP");
        r->skModel = glGetUniformLocation(r->progSky, "uModel");
        r->skTop = glGetUniformLocation(r->progSky, "uSkyTop");
        r->skHor = glGetUniformLocation(r->progSky, "uSkyHor");
        r->skSun = glGetUniformLocation(r->progSky, "uSun");
        r->skSunCol = glGetUniformLocation(r->progSky, "uSunCol");
        r->skAmb = glGetUniformLocation(r->progSky, "uAmb");
        r->skFog = glGetUniformLocation(r->progSky, "uFogCol");
        r->skRain = glGetUniformLocation(r->progSky, "uRain");
        r->skTime = glGetUniformLocation(r->progSky, "uTime");
        r->skUltra = glGetUniformLocation(r->progSky, "uUltra");
    }
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
    int i, hd = SGB_PRO_OK && g_sqDetail > 2.0f;   /* v7.0.0: ULTRA+ op de pc = PRO-mensen */
    double t0 = plat_time();
    mb_init(&m);
#define BUILD(dst, call) do { mb_reset(&m); g_meshMat = 0; call; g_meshMat = 0; upload(&(dst), &m); } while (0)
    for (i = 0; i < CHAR_VARIANTS; i++) {
        model_char_colors(i, &g_amSkin, &g_amHair);
        g_autoMat = 1;   /* v6.2.0: huid, haar, stof en ogen krijgen hun eigen materiaal */
        if (hd) {   /* v7.0.0 PRO-mensen (organisch, hoge detail) */
            BUILD(r->body[i], human_body(&m, i));
            BUILD(r->thigh[i], human_thigh(&m, i));
            BUILD(r->shin[i], human_shin(&m, i));
            BUILD(r->uarm[i], human_uarm(&m, i));
            BUILD(r->farm[i], human_farm(&m, i));
        } else {
            BUILD(r->body[i], model_char_body(&m, i));
            BUILD(r->thigh[i], model_char_thigh(&m, i));
            BUILD(r->shin[i], model_char_shin(&m, i));
            BUILD(r->uarm[i], model_char_uarm(&m, i));
            BUILD(r->farm[i], model_char_farm(&m, i));
        }
        BUILD(r->bodyLo[i], model_char_body_lo(&m, i));
        BUILD(r->leg[i], model_char_leg(&m, i));
        BUILD(r->arm[i], model_char_arm(&m, i));
        g_autoMat = 0;
    }
    g_autoMat = 2;   /* v6.2.0: banden van rubber */
    for (i = 0; i < BIKE_VARIANTS; i++) { BUILD(r->bike[i], model_bike(&m, i)); BUILD(r->bikeLo[i], model_bike_lo(&m, i)); }
    for (i = 0; i < CAR_VARIANTS; i++) { BUILD(r->car[i], model_car(&m, i)); BUILD(r->carLo[i], model_car_lo(&m, i)); }
    g_autoMat = 0;
    for (i = 1; i < 4; i++) BUILD(r->weapon[i], model_weapon(&m, i));
    BUILD(r->cash, model_cash(&m));
    BUILD(r->gift, model_gift(&m));
    BUILD(r->cp, model_checkpoint(&m));
    BUILD(r->arrow, model_arrow(&m));
    BUILD(r->shadow, model_shadow(&m));
    BUILD(r->marker, model_marker(&m));
    BUILD(r->siren, model_siren(&m));
    BUILD(r->gun, model_gun(&m));
    BUILD(r->spoiler, model_spoiler(&m));
    g_autoMat = 2;
    for (i = 0; i < CAR_MODELS; i++) {
        BUILD(r->carBody[i], model_car_body(&m, i)); BUILD(r->carDetail[i], model_car_detail(&m, i));
        BUILD(r->carBodyLo[i], model_car_body_lo(&m, i)); BUILD(r->carDetailLo[i], model_car_detail_lo(&m, i));
    }
    for (i = 0; i < MOTO_MODELS; i++) { BUILD(r->moto[i], model_moto(&m, i)); BUILD(r->motoLo[i], model_moto_lo(&m, i)); }
    g_autoMat = 0;
    {
        int tv = 0, k;
        for (k = 0; k < CHAR_VARIANTS; k++) tv += r->body[k].count + r->thigh[k].count + r->shin[k].count + r->uarm[k].count + r->farm[k].count;
        for (k = 0; k < CAR_VARIANTS; k++) tv += r->car[k].count;
        for (k = 0; k < CAR_MODELS; k++) tv += r->carBody[k].count + r->carDetail[k].count;
        plat_log("SGB modellen: PRO-mensen %s, bouwtijd %.2f s", hd ? "AAN" : "uit", plat_time() - t0);
        plat_log("SGB modellen v3: personage %d + %d + %d + %d + %d hoekpunten, auto %d, sportauto %d, motor %d, fiets %d (totaal hoog %d)",
                 r->body[0].count, r->thigh[0].count, r->shin[0].count, r->uarm[0].count, r->farm[0].count,
                 r->car[0].count, r->carBody[3].count + r->carDetail[3].count, r->moto[1].count, r->bike[0].count, tv);
    }
    BUILD(r->tracer, model_tracer(&m));
    for (i = 0; i < 3; i++) { BUILD(r->homeExt[i], home_build_exterior(&m, i)); BUILD(r->homeInt[i], home_build_interior(&m, i)); }
    for (i = 0; i < 4; i++) BUILD(r->homeGar[i], home_build_garage(&m, i));
    for (i = 0; i < STORE_COUNT; i++) BUILD(r->store[i], store_build(&m, i));
    for (i = 0; i < 2; i++) BUILD(r->handMesh[i], model_vr_hand(&m, i ? 1.0f : -1.0f));
    for (i = 0; i < 2; i++) BUILD(r->handMesh[2 + i], model_vr_fist(&m, i ? 1.0f : -1.0f));
    BUILD(r->wheelMesh, model_vr_wheel(&m));
    {   /* v6.0.1: onderdelen voor de PHYSICS-modus (gecentreerd, 0.6 m) */
        int t, k;
        for (t = 1; t < PP_COUNT; t++) {
            mb_reset(&m);
            switch (t) {
            case PP_BLOCK: g_meshMat = MAT_PAINT; mb_box(&m, v3(-0.29f, -0.29f, -0.29f), v3(0.29f, 0.29f, 0.29f), col(80, 108, 150), 0);
                mb_box(&m, v3(-0.3f, 0.2f, -0.3f), v3(0.3f, 0.3f, 0.3f), col(60, 84, 120), 0); break;
            case PP_SEAT: g_meshMat = MAT_PAINT; mb_box(&m, v3(-0.29f, -0.29f, -0.29f), v3(0.29f, -0.05f, 0.29f), col(40, 40, 44), 0);
                mb_box(&m, v3(-0.25f, -0.05f, -0.29f), v3(0.25f, 0.45f, -0.17f), col(200, 40, 40), 0);
                mb_box(&m, v3(-0.25f, -0.05f, -0.2f), v3(0.25f, 0.05f, 0.2f), col(200, 40, 40), 0); break;
            case PP_MOTOR: g_meshMat = MAT_SHINY; mb_box(&m, v3(-0.29f, -0.29f, -0.29f), v3(0.29f, 0.12f, 0.29f), col(60, 60, 66), 0);
                for (k = 0; k < 3; k++) mb_cyl(&m, v3(-0.18f + k * 0.18f, 0.2f, 0.0f), 0.07f, 0.18f, 8, 1, col(150, 150, 156), col(190, 190, 196), 1);
                mb_cyl(&m, v3(0.22f, 0.0f, -0.32f), 0.05f, 0.12f, 8, 2, col(120, 120, 126), col(20, 20, 20), 1); break;
            case PP_WING: g_meshMat = MAT_PAINT; mb_box(&m, v3(-0.3f, -0.04f, -0.3f), v3(0.3f, 0.04f, 0.3f), col(236, 236, 232), 0);
                mb_box(&m, v3(-0.3f, 0.04f, -0.3f), v3(0.3f, 0.05f, -0.2f), col(200, 40, 40), 0); break;
            default: g_meshMat = MAT_SHINY; mb_cyl(&m, v3(0, 0, 0), 0.24f, 0.58f, 12, 2, col(70, 72, 80), col(40, 40, 44), 1);
                mb_cyl(&m, v3(0, 0, -0.3f), 0.17f, 0.03f, 12, 2, col_e(255, 140, 40, 255), col_e(255, 180, 60, 255), 1); break;
            }
            g_meshMat = 0;
            upload(&r->phPart[t], &m);
        }
        mb_reset(&m);
        mb_cyl(&m, v3(0, 0, 0), 0.42f, 0.28f, 18, 0, col(24, 24, 26), col(30, 30, 32), 1);
        g_meshMat = MAT_SHINY;
        mb_cyl(&m, v3(0, 0, 0), 0.24f, 0.3f, 12, 0, col(170, 172, 178), col(200, 202, 208), 1);
        g_meshMat = 0;
        upload(&r->phWheel, &m);
        mb_reset(&m);
        g_meshMat = MAT_CONCRETE;
        for (k = 0; k < PH_RAMPS; k++) {   /* schansen: schuin vlak + zijkanten */
            const PRamp *rp = &PH_RAMP[k];
            V3 n = v3norm(v3(0, rp->z1 - rp->z0, -(rp->h1 - rp->h0)));
            Col c = col(178, 176, 170), sd = col(140, 138, 132);
            mb_quad(&m, v3(rp->x0, rp->h0 + 0.02f, rp->z0), v3(rp->x1, rp->h0 + 0.02f, rp->z0), v3(rp->x1, rp->h1 + 0.02f, rp->z1), v3(rp->x0, rp->h1 + 0.02f, rp->z1), n, c);
            mb_quad(&m, v3(rp->x0, 0, rp->z0), v3(rp->x0, 0, rp->z1), v3(rp->x0, rp->h1, rp->z1), v3(rp->x0, rp->h0, rp->z0), v3(-1, 0, 0), sd);
            mb_quad(&m, v3(rp->x1, 0, rp->z0), v3(rp->x1, 0, rp->z1), v3(rp->x1, rp->h1, rp->z1), v3(rp->x1, rp->h0, rp->z0), v3(1, 0, 0), sd);
            if (rp->h1 > 0.1f) mb_quad(&m, v3(rp->x0, 0, rp->z1), v3(rp->x1, 0, rp->z1), v3(rp->x1, rp->h1, rp->z1), v3(rp->x0, rp->h1, rp->z1), v3(0, 0, 1), sd);
            if (rp->h0 > 0.1f) mb_quad(&m, v3(rp->x0, 0, rp->z0), v3(rp->x1, 0, rp->z0), v3(rp->x1, rp->h0, rp->z0), v3(rp->x0, rp->h0, rp->z0), v3(0, 0, -1), sd);
            mb_quad(&m, v3(rp->x0 + 1.0f, rp->h0 + 0.03f, rp->z0), v3(rp->x0 + 1.4f, rp->h0 + 0.03f, rp->z0), v3(rp->x0 + 1.4f, rp->h1 + 0.03f, rp->z1), v3(rp->x0 + 1.0f, rp->h1 + 0.03f, rp->z1), n, col_e(250, 200, 40, 60));
            mb_quad(&m, v3(rp->x1 - 1.4f, rp->h0 + 0.03f, rp->z0), v3(rp->x1 - 1.0f, rp->h0 + 0.03f, rp->z0), v3(rp->x1 - 1.0f, rp->h1 + 0.03f, rp->z1), v3(rp->x1 - 1.4f, rp->h1 + 0.03f, rp->z1), n, col_e(250, 200, 40, 60));
        }
        g_meshMat = 0;
        upload(&r->rampMesh, &m);
        /* bouwstudio: vloer met raster van de bouwruimte */
        mb_reset(&m);
        g_meshMat = MAT_CONCRETE;
        mb_flat(&m, -20, -20, 20, 20, 0.0f, col(120, 124, 132));
        g_meshMat = 0;
        mb_flat(&m, -PH_GX * PH_CELL * 0.5f, -PH_GZ * PH_CELL * 0.5f, PH_GX * PH_CELL * 0.5f, PH_GZ * PH_CELL * 0.5f, 0.01f, col(70, 84, 110));
        for (k = 0; k <= PH_GX; k++) { float x = (k - PH_GX * 0.5f) * PH_CELL; mb_flat(&m, x - 0.012f, -PH_GZ * PH_CELL * 0.5f, x + 0.012f, PH_GZ * PH_CELL * 0.5f, 0.02f, col_e(150, 190, 255, 120)); }
        for (k = 0; k <= PH_GZ; k++) { float z = (k - PH_GZ * 0.5f) * PH_CELL; mb_flat(&m, -PH_GX * PH_CELL * 0.5f, z - 0.012f, PH_GX * PH_CELL * 0.5f, z + 0.012f, 0.02f, col_e(150, 190, 255, 120)); }
        mb_box(&m, v3(-0.15f, 0.0f, PH_GZ * PH_CELL * 0.5f + 0.3f), v3(0.15f, 0.04f, PH_GZ * PH_CELL * 0.5f + 0.9f), col_e(255, 200, 40, 200), 0);   /* pijl: voorkant */
        mb_box(&m, v3(-0.4f, 0.0f, PH_GZ * PH_CELL * 0.5f + 0.75f), v3(0.4f, 0.04f, PH_GZ * PH_CELL * 0.5f + 0.9f), col_e(255, 200, 40, 200), 0);
        upload(&r->buildFloor, &m);
    }
    {   /* v6.0: luchtkoepel (eenheidsbol) en regendruppels */
        int a, b2;
        mb_reset(&m);
        for (b2 = 0; b2 < 10; b2++)
            for (a = 0; a < 24; a++) {
                float t0 = -0.25f + b2 * 0.175f, t1 = t0 + 0.175f, p0 = a * 2 * PI_F / 24, p1 = (a + 1) * 2 * PI_F / 24;
                V3 A = v3(cosf(t0) * cosf(p0), sinf(t0), cosf(t0) * sinf(p0)), B = v3(cosf(t0) * cosf(p1), sinf(t0), cosf(t0) * sinf(p1));
                V3 C = v3(cosf(t1) * cosf(p1), sinf(t1), cosf(t1) * sinf(p1)), D = v3(cosf(t1) * cosf(p0), sinf(t1), cosf(t1) * sinf(p0));
                mb_quad(&m, A, B, C, D, v3scale(v3norm(v3add(A, C)), -1.0f), col(255, 255, 255));
            }
        upload(&r->skyDome, &m);
        mb_reset(&m);
        {
            Rng rr; rr.s = 777u;
            for (a = 0; a < 700; a++) {
                float x = rng_range(&rr, -22, 22), y = rng_range(&rr, 0, 30), z = rng_range(&rr, -22, 22);
                mb_quad(&m, v3(x - 0.012f, y, z), v3(x + 0.012f, y, z), v3(x + 0.012f, y + 0.55f, z), v3(x - 0.012f, y + 0.55f, z), v3(0, 0, 1), col(200, 210, 225));
                mb_quad(&m, v3(x, y, z - 0.012f), v3(x, y, z + 0.012f), v3(x, y + 0.55f, z + 0.012f), v3(x, y + 0.55f, z - 0.012f), v3(1, 0, 0), col(200, 210, 225));
            }
        }
        upload(&r->rainMesh, &m);
    }
    for (i = 0; i < BIKE_VARIANTS; i++) {
        g_modelPart = 1; BUILD(r->bikeBody[i], model_bike(&m, i));
        g_modelPart = 2; BUILD(r->bikeFront[i], model_bike(&m, i));
    }
    for (i = 0; i < MOTO_MODELS; i++) {
        g_modelPart = 1; BUILD(r->motoBody[i], model_moto(&m, i));
        g_modelPart = 2; BUILD(r->motoFront[i], model_moto(&m, i));
    }
    g_modelPart = 0;
    for (i = 0; i < 15; i++) BUILD(r->decor[i / 5][i % 5], home_build_decor(&m, i / 5, i % 5));
#undef BUILD
    mb_free(&m);
}

static void set3(float *d, float a, float b, float c) { d[0] = a; d[1] = b; d[2] = c; }

void r_set_quality(Renderer *r, int q) {
    {
        GLint ok = 0;
        if (r->prog[1]) glGetProgramiv(r->prog[1], GL_LINK_STATUS, &ok);
        r->hq = q >= 2 && ok;
#ifdef __APPLE__
        r->hq = 0;   /* v6.1.0: iPad 2: geen HQ-shaders/schaduwen (te zwaar), alleen LOW/MEDIUM/HIGH detail */
#endif
        (void)q;
    }
    r->ultra = r->hq && q >= 4;   /* v6.2.0 ULTRA+ */
    r->pro = SGB_PRO_OK && r->ultra;   /* v7.0.0: PRO-nabewerking op de pc */
#ifdef SGB_HAVE_FBO
    if (!SGB_HAVE_FBO()) r->pro = 0;
#endif
    r->shadowsOn = r->hq;
#ifdef SGB_HAVE_FBO
    if (!SGB_HAVE_FBO()) { r->shadowsOn = 0; return; }
#endif
    if (r->hq && !r->shFbo) {   /* schaduwkaart (RGBA met verpakte diepte, werkt overal met GLES 2) */
        GLint mx = 0, prev = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &mx);
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev);
        r->shSize = q >= 3 && mx >= 4096 ? 4096 : (mx >= 2048 ? 2048 : 1024);
        glGenTextures(1, &r->shTex);
        glBindTexture(GL_TEXTURE_2D, r->shTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, r->shSize, r->shSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenRenderbuffers(1, &r->shDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, r->shDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, r->shSize, r->shSize);
        glGenFramebuffers(1, &r->shFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, r->shFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r->shTex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, r->shDepth);
        r->shOk = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev);
        plat_log("SGB-HQ: schaduwkaart %dx%d %s", r->shSize, r->shSize, r->shOk ? "ok" : "MISLUKT");
    }
    if (!r->shOk) r->shadowsOn = 0;
    r->shArea = q >= 4 ? 190.0f : (q >= 3 ? 220.0f : 150.0f);   /* ULTRA+: scherpere schaduwen dichtbij */
}

static void lerp3(float *o, const float *a, const float *b, float t) { int i; for (i = 0; i < 3; i++) o[i] = a[i] + (b[i] - a[i]) * t; }

/* vloeiende tijd van de dag: nacht -> dageraad -> dag -> avond -> nacht, plus regen (grijzer, minder zon) */
void r_set_time(Renderer *r, float hours, float rain) {
    static Renderer d, k, n;
    float h = fmodf(hours, 24.0f), t;
    const Renderer *a, *b;
    r_set_tod(&d, TOD_DAY); r_set_tod(&k, TOD_DUSK); r_set_tod(&n, TOD_NIGHT);
    if (h < 5.0f) { a = &n; b = &n; t = 0; }
    else if (h < 6.5f) { a = &n; b = &k; t = (h - 5.0f) / 1.5f; }
    else if (h < 8.0f) { a = &k; b = &d; t = (h - 6.5f) / 1.5f; }
    else if (h < 17.5f) { a = &d; b = &d; t = 0; }
    else if (h < 19.5f) { a = &d; b = &k; t = (h - 17.5f) / 2.0f; }
    else if (h < 21.0f) { a = &k; b = &n; t = (h - 19.5f) / 1.5f; }
    else { a = &n; b = &n; t = 0; }
    lerp3(r->skyTop, a->skyTop, b->skyTop, t); lerp3(r->skyHor, a->skyHor, b->skyHor, t);
    lerp3(r->fogCol, a->fogCol, b->fogCol, t); lerp3(r->sunCol, a->sunCol, b->sunCol, t);
    lerp3(r->amb, a->amb, b->amb, t);
    r->emis = a->emis + (b->emis - a->emis) * t;
    {   /* zon (of maan) loopt mee met de klok */
        float ang = (h - 6.0f) / 12.0f * PI_F;
        V3 sd = h >= 6.0f && h <= 18.0f ? v3(cosf(ang) * 0.75f, sinf(ang) * 0.9f + 0.12f, 0.45f) : v3(-cosf(ang) * 0.5f, 0.75f, -0.45f);
        sd = v3norm(sd);
        r->sun[0] = sd.x; r->sun[1] = sd.y; r->sun[2] = sd.z;
    }
    if (rain > 0) {   /* bewolkt: grijzer, minder direct zonlicht */
        static const float gt[3] = {0.42f, 0.45f, 0.5f}, gh[3] = {0.6f, 0.62f, 0.66f};
        float g = clampf(rain, 0, 1) * (r->emis > 0.9f ? 0.35f : 0.75f);
        float top[3], hor[3];
        int i;
        for (i = 0; i < 3; i++) { top[i] = gt[i] * (0.25f + 0.75f * r->amb[i] / 0.66f); hor[i] = gh[i] * (0.25f + 0.75f * r->amb[i] / 0.66f); }
        lerp3(r->skyTop, r->skyTop, top, g); lerp3(r->skyHor, r->skyHor, hor, g); lerp3(r->fogCol, r->fogCol, hor, g);
        for (i = 0; i < 3; i++) r->sunCol[i] *= 1.0f - 0.65f * clampf(rain, 0, 1);
    }
    r->rain = clampf(rain, 0, 1);
    r->hours = h;
    r->tod = h >= 8.0f && h < 17.5f ? TOD_DAY : (h >= 21.0f || h < 5.0f ? TOD_NIGHT : TOD_DUSK);
}

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
/* ---- personages: houding met knieen en ellebogen (v3.0) ---- */

typedef struct { float lean, bob, seatY, seatZ, th[2], kn[2], sh[2], el[2]; } CPose;   /* [0] = links (k=-1), [1] = rechts */

static float g_animT;   /* speltijd, voor ademhalen in stilstand */

static CPose pose_lerp(CPose a, CPose b, float t) {
    CPose o;
    int i;
    o.lean = lerpf(a.lean, b.lean, t); o.bob = lerpf(a.bob, b.bob, t);
    o.seatY = lerpf(a.seatY, b.seatY, t); o.seatZ = lerpf(a.seatZ, b.seatZ, t);
    for (i = 0; i < 2; i++) {
        o.th[i] = lerpf(a.th[i], b.th[i], t); o.kn[i] = lerpf(a.kn[i], b.kn[i], t);
        o.sh[i] = lerpf(a.sh[i], b.sh[i], t); o.el[i] = lerpf(a.el[i], b.el[i], t);
    }
    return o;
}

/* lopen/rennen (speed in m/s), of stilstaan met ademhaling */
static CPose pose_walk(float w, float speed, int idSeed) {
    CPose p;
    float run = clampf((speed - 4.6f) / 2.4f, 0, 1), mv = clampf(speed / 1.2f, 0, 1);
    float A = lerpf(0.5f, 0.92f, run) * mv, K = lerpf(0.6f, 1.4f, run) * mv;
    int i;
    memset(&p, 0, sizeof(p));
    for (i = 0; i < 2; i++) {
        float ph = w + (i ? 0.0f : PI_F);
        p.th[i] = A * sinf(ph) - 0.12f * run;
        p.kn[i] = 0.06f + K * fmaxf(0.0f, -cosf(ph)) + 0.12f * run;
        p.sh[i] = -A * 0.85f * sinf(ph);
        p.el[i] = -(0.15f + 1.1f * run) - 0.15f * mv * fmaxf(0.0f, -sinf(ph));
    }
    p.lean = 0.2f * run + 0.03f * mv;
    p.bob = -(0.025f + 0.04f * run) * fabsf(cosf(w)) * mv;
    if (mv < 0.05f) {   /* stilstaan: rustig ademen, armen iets los */
        float b = sinf(g_animT * 1.8f + idSeed * 1.7f);
        p.sh[0] = 0.03f + 0.03f * b; p.sh[1] = 0.03f - 0.03f * b;
        p.el[0] = p.el[1] = -0.12f - 0.03f * b;
        p.bob = 0.006f * b;
        p.kn[0] = p.kn[1] = 0.04f;
    }
    return p;
}

static CPose pose_air(void) {
    CPose p;
    memset(&p, 0, sizeof(p));
    p.th[1] = -0.75f; p.kn[1] = 1.15f; p.th[0] = 0.25f; p.kn[0] = 0.55f;
    p.sh[0] = p.sh[1] = -2.5f; p.el[0] = p.el[1] = -0.35f;
    p.lean = 0.06f;
    return p;
}

static CPose pose_ride(float pedal, int moto) {
    CPose p;
    int i;
    memset(&p, 0, sizeof(p));
    p.seatY = BIKE_SEAT_Y - HIP_Y + 0.02f;
    p.seatZ = moto ? -0.38f : -0.24f;
    p.lean = moto ? 0.42f : 0.38f;
    for (i = 0; i < 2; i++) {
        float ps = sinf(pedal + (i ? 0.0f : PI_F));
        if (moto) { p.th[i] = -1.05f; p.kn[i] = 1.55f; }
        else { p.th[i] = -1.25f + 0.45f * ps; p.kn[i] = 1.45f - 0.45f * ps; }
        p.sh[i] = moto ? -1.15f : -1.1f;
        p.el[i] = moto ? -0.45f : -0.3f;
    }
    return p;
}

static CPose pose_sit(void) {
    CPose p;
    int i;
    memset(&p, 0, sizeof(p));
    p.seatY = -0.38f;
    p.lean = -0.12f;
    for (i = 0; i < 2; i++) { p.th[i] = -1.45f; p.kn[i] = 1.45f; p.sh[i] = -0.9f; p.el[i] = -0.6f; }
    return p;
}

static int near_cam(Renderer *r, const M4 *m, float d) {
    float dx = m->m[12] - r->cam.x, dy = m->m[13] - r->cam.y, dz = m->m[14] - r->cam.z;
    return dx * dx + dy * dy + dz * dz < d * d;
}

#define LOD_CHAR 34.0f
#define LOD_VEH 48.0f

static void draw_char_pose(Renderer *r, int v, M4 base, const CPose *p, float lay, int weapon) {
    M4 seat, bodyM, m, m2;
    int k, hi;
    v = ((v % CHAR_VARIANTS) + CHAR_VARIANTS) % CHAR_VARIANTS;
    if (lay > 0) base = m4_mul(base, m4_mul(m4_translate(0, 0.25f * lay, 0), m4_rot_x(-lay * 1.45f)));
    hi = near_cam(r, &base, LOD_CHAR);
    seat = m4_mul(base, m4_translate(0, p->seatY + p->bob, p->seatZ));
    bodyM = m4_mul(seat, m4_mul(m4_translate(0, HIP_Y, 0), m4_mul(m4_rot_x(p->lean), m4_translate(0, -HIP_Y, 0))));
    draw_mesh(r, hi ? &r->body[v] : &r->bodyLo[v], &bodyM);
    for (k = -1; k <= 1; k += 2) {
        int i = k > 0;
        float th = p->th[i], kn = p->kn[i];
        if (lay > 0) { th = lerpf(th, 0.1f * k, clampf(lay, 0, 1)); kn = lerpf(kn, 0.15f, clampf(lay, 0, 1)); }
        m = m4_mul(seat, m4_mul(m4_translate(HIP_X * k, HIP_Y, 0), m4_rot_x(th)));
        if (hi) {
            draw_mesh(r, &r->thigh[v], &m);
            m2 = m4_mul(m, m4_mul(m4_translate(0, -KNEE_Y, 0), m4_rot_x(kn)));
            draw_mesh(r, &r->shin[v], &m2);
        } else draw_mesh(r, &r->leg[v], &m);
    }
    for (k = -1; k <= 1; k += 2) {
        int i = k > 0;
        float sh = p->sh[i], el = p->el[i];
        if (weapon && k < 0 && lay <= 0) { sh = -1.5f; el = 0; }   /* rechterarm gestrekt naar voren: richten */
        if (lay > 0) { sh = lerpf(sh, -0.3f * k, clampf(lay, 0, 1)); el = lerpf(el, -0.2f, clampf(lay, 0, 1)); }
        m = m4_mul(bodyM, m4_mul(m4_translate(SHOULDER_X * k, SHOULDER_Y, 0), m4_mul(m4_rot_z(k * 0.06f), m4_rot_x(sh))));
        if (hi) {
            draw_mesh(r, &r->uarm[v], &m);
            m2 = m4_mul(m, m4_mul(m4_translate(0, -ELBOW_Y, 0), m4_rot_x(el)));
            draw_mesh(r, &r->farm[v], &m2);
            if (weapon && k < 0) draw_mesh(r, &r->weapon[weapon % WP_COUNT], &m);
        } else {
            draw_mesh(r, &r->arm[v], &m);
            if (weapon && k < 0) draw_mesh(r, &r->gun, &m);
        }
    }
}

/* pose: 0 lopen/staan, 1 fietsen, 2 in de lucht/opzij springen, 3 motorrijden */
static void draw_char(Renderer *r, int v, M4 base, float walk, int pose, float lay, int weapon, float speed) {
    CPose p;
    if (pose == 1) p = pose_ride(walk, 0);
    else if (pose == 3) p = pose_ride(0, 1);
    else if (pose == 2) p = pose_air();
    else p = pose_walk(walk, speed, v);
    draw_char_pose(r, v, base, &p, lay, weapon);
}

/* in- en uitstappen van de speler: lopen naar de deur/het zadel en gaan zitten (of andersom) */
static void draw_player_transition(Renderer *r, Game *g) {
    const Player *pl = &g->p;
    const Vehicle *v = &g->veh[pl->anVeh];
    int entering = pl->anKind <= 2, bike = pl->anKind == 2 || pl->anKind == 4;
    int moto = bike && v->custom && IS_MOTO(v->model);
    float k = clampf(1.0f - pl->anT / (pl->anDur > 0 ? pl->anDur : 0.5f), 0, 1);
    float e = entering ? k : 1.0f - k;   /* 0 = buiten staan, 1 = zitten */
    V3 rr = right_of(v->h);   /* uitstappen gaat naar -rechts (zie exit_vehicle) */
    float ox = entering ? pl->anX : pl->x, oz = entering ? pl->anZ : pl->z;
    float side = bike ? 0.55f : 1.15f;
    float dx = v->x - rr.x * side, dz = v->z - rr.z * side;   /* deur / naast het zadel */
    float sx = bike ? v->x : v->x - rr.x * 0.38f, sz = bike ? v->z : v->z - rr.z * 0.38f;
    float x, z, h, y, split = bike ? 0.35f : 0.5f;
    CPose p;
    M4 m;
    if (e < split) {
        float t = e / split;
        x = lerpf(ox, dx, t); z = lerpf(oz, dz, t);
        h = atan2f(dx - ox, dz - oz);
        if ((dx - ox) * (dx - ox) + (dz - oz) * (dz - oz) < 0.04f) h = v->h;
        p = pose_walk(t * 6.0f, 2.0f, 0);
    } else {
        float t = (e - split) / (1.0f - split), ts = t * t * (3 - 2 * t);
        x = lerpf(dx, sx, ts); z = lerpf(dz, sz, ts);
        h = v->h;
        if (bike) {
            p = pose_lerp(pose_walk(0, 0, 0), pose_ride(0, moto), ts);
            p.th[1] -= sinf(t * PI_F) * 1.3f;   /* been over het zadel zwaaien */
            p.kn[1] += sinf(t * PI_F) * 0.6f;
        } else {
            p = pose_lerp(pose_walk(0, 0, 0), pose_sit(), ts);
            p.lean += sinf(t * PI_F) * 0.35f;     /* bukken om in te stappen */
        }
    }
    y = v->y;
    m = m4_trs_y(x, y, z, h);
    if (bike) m = m4_mul(m, m4_rot_z(v->lean * clampf((e - split) / (1.0f - split), 0, 1)));
    draw_char_pose(r, biz_outfit_variant(g->outfit), m, &p, 0, 0);
}

static void draw_shadow(Renderer *r, float x, float y, float z, float sx, float sz, float h) {
    M4 m = m4_mul(m4_trs_y(x, y + 0.05f, z, h), m4_scale(sx, 1, sz));
    draw_mesh(r, &r->shadow, &m);
}

static void draw_scene_one(Renderer *r, Game *g);

/* v6.0: schaduwkaart vanuit de zon, rond de speler (orthografisch, 150 x 150 m) */
static void shadow_pass(Renderer *r, Game *g) {
    GLint prevFb = 0, vp[4];
    V3 c = g->camTarget, sd = v3(r->sun[0], r->sun[1], r->sun[2]);
    float S = r->shArea > 1.0f ? r->shArea : 150.0f, texel;
    M4 view, proj;
    if (sd.y < 0.1f || r->sunCol[0] + r->sunCol[1] < 0.35f) { r->shVP = m4_identity(); return; }
    texel = S / (float)r->shSize;
    c.x = floorf(c.x / texel) * texel; c.z = floorf(c.z / texel) * texel;
    view = m4_lookat(v3add(c, v3scale(sd, 200.0f)), c, fabsf(sd.y) > 0.95f ? v3(0, 0, 1) : v3(0, 1, 0));
    proj = m4_ortho(-S * 0.5f, S * 0.5f, -S * 0.5f, S * 0.5f, 1.0f, 420.0f);
    r->shVP = m4_mul(proj, view);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFb);
    glGetIntegerv(GL_VIEWPORT, vp);
    glBindFramebuffer(GL_FRAMEBUFFER, r->shFbo);
    glViewport(0, 0, r->shSize, r->shSize);
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    r->pass = 1;
    draw_scene_one(r, g);
    r->pass = 0;
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFb);
    glViewport(vp[0], vp[1], vp[2], vp[3]);
}

#include "pro_post.h"

/* v6.0: luchtkoepel met verloop, zon en bewegende wolken (ook goed in VR) */
static void draw_sky(Renderer *r, Game *g) {
    float rad = g->drawDist * 0.85f;
    M4 m = m4_mul(m4_translate(r->cam.x, r->cam.y, r->cam.z), m4_scale(rad, rad, rad));
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glUseProgram(r->progSky);
    glEnableVertexAttribArray(0); glEnableVertexAttribArray(1); glEnableVertexAttribArray(2);
    glUniformMatrix4fv(r->skVP, 1, GL_FALSE, r->vp.m);
    glUniformMatrix4fv(r->skModel, 1, GL_FALSE, m.m);
    glUniform3f(r->skTop, r->skyTop[0], r->skyTop[1], r->skyTop[2]);
    glUniform3f(r->skHor, r->skyHor[0], r->skyHor[1], r->skyHor[2]);
    glUniform3f(r->skSun, r->sun[0], r->sun[1], r->sun[2]);
    glUniform3f(r->skSunCol, r->sunCol[0], r->sunCol[1], r->sunCol[2]);
    glUniform3f(r->skAmb, r->amb[0], r->amb[1], r->amb[2]);
    glUniform3f(r->skFog, r->fogCol[0], r->fogCol[1], r->fogCol[2]);
    glUniform1f(r->skRain, r->rain);
    glUniform1f(r->skTime, g->time);
    glUniform1f(r->skUltra, r->ultra ? 1.0f : 0.0f);
    bind_mesh(&r->skyDome);
    glDrawArrays(GL_TRIANGLES, 0, r->skyDome.count);
    r->drawCalls++;
}

void r_draw_scene(Renderer *r, Game *g) {
    if (r->hq && r->shadowsOn && r->shOk && !r->pass) shadow_pass(r, g);
    if (r->vr.active) {
        int e;
        if (r->vr.begin) r->vr.begin(r, g, r->vr.u);
        for (e = 0; e < 2; e++) {
            r->vr.bind_eye(e, r->vr.u);
            r->vr.eye = e + 1;
            draw_scene_one(r, g);
        }
        r->vr.eye = 0;
        r->vr.drew = 1;
        if (r->vr.bind_ui) r->vr.bind_ui(r->vr.u);
        return;
    }
    if (r->pro && pro_draw(r, g)) return;   /* v7.0.0 PRO-engine */
    draw_scene_one(r, g);
}

static void draw_scene_one(Renderer *r, Game *g) {
    float aspect = (float)r->W / (float)(r->H > 0 ? r->H : 1);
    float fogEnd = g->drawDist * (1.0f - 0.35f * r->rain), fogStart = fogEnd * 0.45f;   /* regen: minder zicht */
    float t = g->time;
    int i, mode = game_mode(g);
    V3 shadows[MAX_PED + MAX_VEH + 1];
    float shs[MAX_PED + MAX_VEH + 1][3];
    int nsh = 0;

    r->drawnChunks = 0; r->drawnTris = 0;
    g_animT = g->time;
    if (r->vr.eye) {   /* VR: matrices van het oog; de lucht is de wiskleur van bind_eye */
        r->cam = r->vr.pos[r->vr.eye - 1];
        r->proj = r->vr.proj[r->vr.eye - 1];
        r->view = r->vr.view[r->vr.eye - 1];
    } else {
        r->cam = g->camPos;
        r->proj = m4_perspective((g->camFov > 1.0f ? g->camFov : 58.0f) * PI_F / 180.0f, aspect, 0.3f, g->drawDist + 40.0f);
        r->view = m4_lookat(g->camPos, g->camTarget, v3(0, 1, 0));
    }
    r->vp = m4_mul(r->proj, r->view);
    if (r->pass == 1) { r->vp = r->shVP; r->cam = g->camTarget; }
    extract_planes(r);

    /* lucht als verloop (2D); in VR niet (de horizon kantelt dan mee met je hoofd) */
    if (!r->vr.eye && !r->pass && !r->hq) {
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
    }
    if (r->hq && !r->pass) draw_sky(r, g);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glDisable(GL_BLEND);

    {   /* v6.0: programma kiezen: gewoon, HQ of schaduw-pass */
        int pi = r->pass ? 2 : (r->hq ? 1 : 0);   /* pass 1 schaduw, pass 2 diepte (PRO) */
        GLint *L = r->loc[pi];
        r->progW = r->prog[pi] ? r->prog[pi] : r->prog[0];
        r->wVP = L[0]; r->wModel = L[1]; r->wSun = L[2]; r->wSunCol = L[3]; r->wAmb = L[4]; r->wCam = L[5];
        r->wFog = L[6]; r->wFogCol = L[7]; r->wTint = L[8]; r->wEmis = L[9]; r->wAlpha = L[10]; r->wUnlit = L[11];
    }
    glUseProgram(r->progW);
    if (r->pass == 1) glDisable(GL_CULL_FACE);
    if (r->hq && !r->pass) {
        int sh = r->shadowsOn && r->shOk;
        glUniformMatrix4fv(r->hqShadowVP, 1, GL_FALSE, r->shVP.m);
        glUniform1f(r->hqShadowOn, sh ? 1.0f : 0.0f);
        glUniform1f(r->hqTexel, r->shSize > 0 ? 1.0f / (float)r->shSize : 0.0f);
        glUniform1f(r->hqRain, r->rain);
        glUniform1f(r->hqTime, g->time);
        glUniform1f(r->hqUltra, r->ultra ? 1.0f : 0.0f);
        glUniform1f(r->hqEmis2, r->emis);
        glUniform3f(r->hqSkyTop, r->skyTop[0], r->skyTop[1], r->skyTop[2]);
        glUniform3f(r->hqSkyHor, r->skyHor[0], r->skyHor[1], r->skyHor[2]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, sh ? r->shTex : r->white.id);
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(r->hqShadowTex, 1);
    }
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glUniformMatrix4fv(r->wVP, 1, GL_FALSE, r->vp.m);
    glUniform3f(r->wSun, r->sun[0], r->sun[1], r->sun[2]);
    glUniform3f(r->wSunCol, r->sunCol[0], r->sunCol[1], r->sunCol[2]);
    glUniform3f(r->wAmb, r->amb[0], r->amb[1], r->amb[2]);
    glUniform3f(r->wCam, r->cam.x, r->cam.y, r->cam.z);
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
            float cx = clampf(r->cam.x, b[0], b[3]), cz = clampf(r->cam.z, b[2], b[5]);
            float dx = cx - r->cam.x, dz = cz - r->cam.z;
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
        m = v->phys ? v->pm : m4_trs_y(v->x, v->y, v->z, v->h);
        if (v->fly) {   /* v6.0.4: vliegend: vleugels en een jet */
            int bk = v->type == VEH_BIKE;
            float wx = bk ? 0.9f : 1.45f, wy = bk ? 0.7f : 0.8f;
            M4 wm = m4_mul(m, m4_mul(m4_translate(-wx, wy, -0.1f), m4_scale(bk ? 1.6f : 2.0f, 1.0f, 1.5f)));
            draw_mesh(r, &r->phPart[PP_WING], &wm);
            wm = m4_mul(m, m4_mul(m4_translate(wx, wy, -0.1f), m4_scale(bk ? 1.6f : 2.0f, 1.0f, 1.5f)));
            draw_mesh(r, &r->phPart[PP_WING], &wm);
            wm = m4_mul(m, m4_translate(0, bk ? 0.65f : 0.85f, bk ? -1.0f : -2.2f));
            draw_mesh(r, &r->phPart[PP_JET], &wm);
        }
        if (v->type == VEH_BIKE && r->vr.eye && r->vr.hidePlayer && i == g->p.veh) {
            /* VR first person (Quest): niet schuin hangen; voorwiel, vork en stuur draaien mee met het stuur */
            int moto = v->custom && IS_MOTO(v->model);
            float piv = moto ? 0.6f : 0.48f, ang = -v->steer * 0.55f;
            M4 fm = m4_mul(m, m4_mul(m4_translate(0, 0, piv), m4_mul(m4_rot_y(ang), m4_translate(0, 0, -piv))));
            if (moto) {
                int k = (v->model - CAR_MODELS) % MOTO_MODELS;
                draw_mesh(r, &r->motoBody[k], &m);
                draw_mesh(r, &r->motoFront[k], &fm);
            } else {
                draw_mesh(r, &r->bikeBody[v->variant % BIKE_VARIANTS], &m);
                draw_mesh(r, &r->bikeFront[v->variant % BIKE_VARIANTS], &fm);
            }
        } else if (v->type == VEH_BIKE) {
            if (!v->phys) m = m4_mul(m, m4_rot_z(v->lean));
            int rider = i == g->p.veh && !(g->p.anT > 0 && g->p.anKind == 2) && !r->vr.hidePlayer;
            int hiV = near_cam(r, &m, LOD_VEH);
            if (v->custom && IS_MOTO(v->model)) {
                int k = (v->model - CAR_MODELS) % MOTO_MODELS;
                draw_mesh(r, hiV ? &r->moto[k] : &r->motoLo[k], &m);
                if (rider) draw_char(r, biz_outfit_variant(g->outfit), m, 0, 3, 0, 0, 0);   /* motor: niet trappen */
            } else {
                draw_mesh(r, hiV ? &r->bike[v->variant % BIKE_VARIANTS] : &r->bikeLo[v->variant % BIKE_VARIANTS], &m);
                if (rider) draw_char(r, biz_outfit_variant(g->outfit), m, g->p.anim * 2.0f, 1, 0, 0, 0);
            }
        } else {
            int hiV = near_cam(r, &m, LOD_VEH);
            if (v->custom) {
                Col pc = paint_color(v->paint);
                tint(r, pc.r / 255.0f, pc.g / 255.0f, pc.b / 255.0f);
                draw_mesh(r, hiV ? &r->carBody[v->model % CAR_MODELS] : &r->carBodyLo[v->model % CAR_MODELS], &m);
                if (v->spoiler && v->model != 3) {
                    M4 sm = m4_mul(m, m4_translate(0, v->model == 2 ? 0.72f - 0.0f : 0.95f, v->model == 0 ? -1.85f : -2.0f));
                    sm = m4_mul(sm, m4_scale(1, v->model == 0 ? 0.35f : 0.3f, 1));
                    draw_mesh(r, &r->spoiler, &sm);
                }
                tint(r, 1, 1, 1);
                draw_mesh(r, hiV ? &r->carDetail[v->model % CAR_MODELS] : &r->carDetailLo[v->model % CAR_MODELS], &m);
            } else {
                if (v->wrecked) tint(r, 0.3f, 0.3f, 0.32f);
                draw_mesh(r, hiV ? &r->car[v->variant % CAR_VARIANTS] : &r->carLo[v->variant % CAR_VARIANTS], &m);
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
    /* eigen huis: buitenkant in de stad; interieur en garage liggen ver weg (alleen als je daar bent) */
    if (!g->nl) {
        M4 id = m4_identity();
        int hl = g->homeLvl < 0 ? 0 : (g->homeLvl > 2 ? 2 : g->homeLvl), gl = g->garLvl < 0 ? 0 : (g->garLvl > 3 ? 3 : g->garLvl);
        if (sphere_visible(r, (HOME_PLOT_X0 + HOME_PLOT_X1) * 0.5f, 3, (HOME_PLOT_Z0 + HOME_PLOT_Z1) * 0.5f, 30, fogEnd))
            draw_mesh(r, &r->homeExt[hl], &id);
        if (r->cam.x < HOME_ZONE_X) {
            int d;
            draw_mesh(r, &r->homeInt[hl], &id);
            draw_mesh(r, &r->homeGar[gl], &id);
            for (d = 0; d < 5; d++) if ((g->decor >> d) & 1) draw_mesh(r, &r->decor[hl][d], &id);
        }
        /* v4.5: het contact uit de missie-cutscene */
        if (g->cut) {
            float cy = world_ground_y(g->w, g->cutX, g->cutZ);
            draw_char(r, vo_speaker_variant(g->cutSpk), m4_trs_y(g->cutX, cy, g->cutZ, g->cutH), g->time * 0.3f, 0, 0, 0, 0);
            if (nsh < (int)(sizeof(shadows) / sizeof(shadows[0]))) {
                shadows[nsh] = v3(g->cutX, cy, g->cutZ); shs[nsh][0] = 0.45f; shs[nsh][1] = 0.45f; shs[nsh][2] = 0; nsh++;
            }
        }
        /* v4.0: winkel van binnen + verkoper achter de toonbank */
        if (r->cam.z < STORE_ZONE_Z) {
            int s = store_at(r->cam.x, r->cam.z);
            if (!s) s = game_store_inside(g);
            if (s) {
                float cx, cz, ch;
                int scared = s - 1 == STORE_MART && g->clerkScare > 0;
                draw_mesh(r, &r->store[s - 1], &id);
                store_clerk(s - 1, &cx, &cz, &ch);
                draw_char(r, (s * 4 + 1) % CHAR_VARIANTS, m4_trs_y(cx, 0, cz, ch), scared ? 0 : g->time * 0.3f, scared ? 2 : 0, 0,
                          s - 1 == STORE_GUN ? WP_SHOTGUN : 0, 0);
                if (nsh < (int)(sizeof(shadows) / sizeof(shadows[0]))) {
                    shadows[nsh] = v3(cx, 0, cz); shs[nsh][0] = 0.45f; shs[nsh][1] = 0.45f; shs[nsh][2] = 0; nsh++;
                }
            }
        }
    }
    /* voetgangers */
    for (i = 0; i < MAX_PED; i++) {
        Ped *p = &g->ped[i];
        M4 m;
        if (!p->active) continue;
        if (!sphere_visible(r, p->x, p->y + 1.0f, p->z, 1.2f, fogEnd * 0.8f)) continue;
        m = m4_trs_y(p->x, p->y, p->z, p->h);
        draw_char(r, p->variant, m, p->state == PED_IDLE ? 0 : p->anim, p->state == PED_DODGE ? 2 : 0, p->lay, p->cop ? WP_PISTOL : 0,
                  p->state == PED_IDLE ? 0 : fabsf(p->speed));
        if (nsh < (int)(sizeof(shadows) / sizeof(shadows[0]))) {
            shadows[nsh] = v3(p->x, p->y, p->z); shs[nsh][0] = 0.45f; shs[nsh][1] = 0.45f; shs[nsh][2] = 0; nsh++;
        }
    }
    /* andere spelers (multiplayer) */
    for (i = 0; i < MAX_REMOTE; i++) {
        static const int shirt[4] = {6, 1, 2, 3};
        RemotePlayer *rp = &g->remote[i];
        M4 m;
        if (!rp->active) continue;
        if (!sphere_visible(r, rp->x, rp->y + 1.0f, rp->z, 3.0f, fogEnd)) continue;
        m = m4_trs_y(rp->x, rp->y, rp->z, rp->h);
        if (rp->mode == 2) {
            if (rp->vcustom) {
                Col pc = paint_color(rp->vpaint);
                tint(r, pc.r / 255.0f, pc.g / 255.0f, pc.b / 255.0f);
                draw_mesh(r, &r->carBody[rp->vmodel % CAR_MODELS], &m);
                tint(r, 1, 1, 1);
                draw_mesh(r, &r->carDetail[rp->vmodel % CAR_MODELS], &m);
            } else draw_mesh(r, &r->car[rp->vvariant % CAR_VARIANTS], &m);
        } else if (rp->mode == 1) {
            if (rp->vcustom && IS_MOTO(rp->vmodel)) {
                draw_mesh(r, &r->moto[(rp->vmodel - CAR_MODELS) % MOTO_MODELS], &m);
                draw_char(r, shirt[rp->id & 3], m, 0, 3, 0, 0, 0);
            } else {
                draw_mesh(r, &r->bike[rp->vvariant % BIKE_VARIANTS], &m);
                draw_char(r, shirt[rp->id & 3], m, rp->animLocal * 2.0f, 1, 0, 0, 0);
            }
        } else {
            draw_char(r, shirt[rp->id & 3], m, rp->animLocal, 0, rp->health <= 0 ? 1.0f : 0, rp->weapon % WP_COUNT, fabsf(rp->speed));
        }
        if (nsh < (int)(sizeof(shadows) / sizeof(shadows[0]))) {
            shadows[nsh] = v3(rp->x, rp->y, rp->z);
            shs[nsh][0] = rp->mode == 2 ? 1.15f : 0.45f; shs[nsh][1] = rp->mode == 2 ? 2.3f : 0.45f; shs[nsh][2] = rp->h;
            nsh++;
        }
    }
    /* speler te voet */
    if (r->vr.hidePlayer) {
        /* first person: geen eigen lichaam */
    } else if (g->p.anT > 0 && g->p.anKind > 0 && g->p.anVeh >= 0 && g->p.anVeh < g->nveh && g->veh[g->p.anVeh].active) {
        draw_player_transition(r, g);
    } else if (mode == MODE_FOOT && !r->vr.hidePlayer) {
        M4 m = m4_trs_y(g->p.x, g->p.y, g->p.z, g->p.h);
        float ground = world_ground_y(g->w, g->p.x, g->p.z);
        draw_char(r, biz_outfit_variant(g->outfit), m, g->p.anim, g->p.onGround ? 0 : 2, 0, g->weapon % WP_COUNT, g->p.onGround ? fabsf(g->p.speed) : 0);
        if (nsh < (int)(sizeof(shadows) / sizeof(shadows[0]))) {
            float s = clampf(0.5f - (g->p.y - ground) * 0.15f, 0.2f, 0.5f);
            shadows[nsh] = v3(g->p.x, ground, g->p.z); shs[nsh][0] = s; shs[nsh][1] = s; shs[nsh][2] = 0; nsh++;
        }
    }
    if (r->pass) {   /* schaduw-/dieptepass: alleen gebouwen, voertuigen en mensen */
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glEnable(GL_CULL_FACE);
        return;
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
        if (!g->nl) {
            static const float hc[9][3] = {{0, 0, 0}, {0.75f, 0.35f, 1.0f}, {0.75f, 0.35f, 1.0f}, {0.3f, 1.0f, 0.4f}, {0.2f, 0.6f, 1.0f},
                                           {1.0f, 0.85f, 0.3f}, {1.0f, 0.75f, 0.1f}, {0.3f, 1.0f, 0.4f}, {1.0f, 0.75f, 0.1f}};
            V3 hp[12];
            int hk[12], q, nhm;
            float hr[12];
            nhm = home_markers(g->homeLvl, g->garLvl, hp, hk, hr, 12);
            for (q = 0; q < nhm; q++) {
                float pulse = 0.75f + 0.25f * sinf(t * 4.0f + q), rs = hr[q] * 0.9f;
                M4 m = m4_mul(m4_trs_y(hp[q].x, 0.06f, hp[q].z, t), m4_scale(rs, 1, rs));
                if (!sphere_visible(r, hp[q].x, 1, hp[q].z, 3, fogEnd)) continue;
                tint(r, hc[hk[q]][0] * pulse, hc[hk[q]][1] * pulse, hc[hk[q]][2] * pulse);
                draw_mesh(r, &r->marker, &m);
            }
        }
        if (!g->nl) {
            int q;
            for (q = 0; q < BIZ_COUNT; q++) {
                V3 pp = g->w->poi[biz_poi(q)];
                float cr, cg, cb, pulse = 0.75f + 0.25f * sinf(t * 4.0f + q * 1.3f), rs = q == BIZ_SPRAY ? 3.0f : 1.8f;
                M4 m;
                if (!sphere_visible(r, pp.x, 1, pp.z, 3.5f, fogEnd)) continue;
                biz_color(q, &cr, &cg, &cb);
                m = m4_mul(m4_trs_y(pp.x, world_ground_y(g->w, pp.x, pp.z) + 0.06f, pp.z, t), m4_scale(rs, 1, rs));
                tint(r, cr * pulse, cg * pulse, cb * pulse);
                draw_mesh(r, &r->marker, &m);
                m = m4_mul(m4_trs_y(pp.x, 0, pp.z, 0), m4_scale(rs, 1.6f, rs));
                tint(r, cr * 0.22f, cg * 0.22f, cb * 0.22f);
                draw_mesh(r, &r->cp, &m);
            }
        }
        if (!g->nl) {   /* v4.0: markers binnen in een winkel, en de deur van de GIVEAWAY MART */
            V3 sp[4];
            int sk[4], q, ns, si = game_store_inside(g);
            float sr[4], pulse = 0.75f + 0.25f * sinf(t * 4.0f);
            M4 m;
            if (si) {
                ns = store_markers(si - 1, sp, sk, sr, 4);
                for (q = 0; q < ns; q++) {
                    float cr, cg, cb;
                    if (sk[q] == SM_EXIT) { cr = 0.3f; cg = 1.0f; cb = 0.4f; }
                    else if (sk[q] == SM_ROB) { int ok = g->robCool <= 0; cr = ok ? 1.0f : 0.4f; cg = ok ? 0.15f : 0.4f; cb = ok ? 0.1f : 0.4f; }
                    else store_color(si - 1, &cr, &cg, &cb);
                    m = m4_mul(m4_trs_y(sp[q].x, 0.06f, sp[q].z, t), m4_scale(sr[q] * 0.9f, 1, sr[q] * 0.9f));
                    tint(r, cr * pulse, cg * pulse, cb * pulse);
                    draw_mesh(r, &r->marker, &m);
                }
            } else {
                V3 gm = g->w->poi[POI_MART];
                if (sphere_visible(r, gm.x, 1, gm.z, 3, fogEnd)) {
                    m = m4_mul(m4_trs_y(gm.x, world_ground_y(g->w, gm.x, gm.z) + 0.06f, gm.z, t), m4_scale(1.8f, 1, 1.8f));
                    tint(r, 0.25f * pulse, 0.85f * pulse, 0.35f * pulse);
                    draw_mesh(r, &r->marker, &m);
                    m = m4_mul(m4_trs_y(gm.x, 0, gm.z, 0), m4_scale(1.8f, 1.6f, 1.8f));
                    tint(r, 0.05f, 0.2f, 0.08f);
                    draw_mesh(r, &r->cp, &m);
                }
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
                float cdx = r->cam.x - tg[k].x, cdz = r->cam.z - tg[k].z;
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
    if (r->vr.eye && r->vr.hands) {   /* v5.0: handen (en het wapen in de rechterhand) */
        int k;
        /* in een voertuig: handen en stuur altijd zichtbaar (niet achter de motorkap of het frame) */
        if (g->p.veh >= 0) glClear(GL_DEPTH_BUFFER_BIT);
        glUseProgram(r->progW);
        glUniform1f(r->wUnlit, 0.0f);
        tint(r, 1, 1, 1);
        glDisable(GL_CULL_FACE);
        for (k = 0; k < 2; k++) draw_mesh(r, &r->handMesh[(r->vr.fist[k] ? 2 : 0) + k], &r->vr.hand[k]);
        if (r->vr.weaponOn && g->weapon > WP_NONE && g->weapon < WP_COUNT) {
            /* wapenmodel: loop langs -Y, greep op y = -0.62 -> loop langs -Z, greep in de vuist */
            M4 wm = m4_mul(r->vr.weapon, m4_mul(m4_rot_x(PI_F * 0.5f), m4_translate(0, 0.615f, -0.045f)));
            draw_mesh(r, &r->weapon[g->weapon], &wm);
        }
        if (r->vr.wheelOn) draw_mesh(r, &r->wheelMesh, &r->vr.wheel);
        glEnable(GL_CULL_FACE);
    }
    if (r->rain > 0.02f && !r->pass) {   /* v6.0: regendruppels rond de camera */
        float fall = fmodf(g->time * 16.0f, 30.0f);
        int k;
        glUseProgram(r->progW);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        glUniform1f(r->wUnlit, 1.0f);
        glUniform1f(r->wAlpha, 0.3f * r->rain);
        tint(r, 0.85f, 0.88f, 0.95f);
        for (k = 0; k < 2; k++) {
            M4 m = m4_translate(floorf(r->cam.x), r->cam.y - 12.0f - fall + k * 30.0f, floorf(r->cam.z));
            draw_mesh(r, &r->rainMesh, &m);
        }
        glUniform1f(r->wUnlit, 0.0f);
        glUniform1f(r->wAlpha, 1.0f);
        tint(r, 1, 1, 1);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisableVertexAttribArray(1);
}

/* ------------------------------------------------------------------ 2D batch */

void ui_begin(Renderer *r) {
    M4 p = m4_ortho(-r->uiOff, (float)r->W + r->uiOff, (float)r->H, 0, -1, 1);
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
    if (r->uiOff > 0 && x <= 0.5f && x + w >= (float)r->W - 0.5f) { x -= r->uiOff; w += 2.0f * r->uiOff; }
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
    if (r->uiOff > 0 && x <= 0.5f && x + w >= (float)r->W - 0.5f) { x -= r->uiOff; w += 2.0f * r->uiOff; }   /* schermbreed blijft schermbreed */
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

/* v6.0.2: 3D-bouwscherm (vloer, onderdelen, spookonderdeel) */
void r_draw_build(Renderer *r, const PDesign *d, V3 cam, V3 tgt, float fov, int gx, int gy, int gz, int ghostType, float t) {
    float aspect = (float)r->W / (float)(r->H > 0 ? r->H : 1);
    GLint *L = r->loc[0];
    M4 id = m4_identity();
    int x, y, z;
    r->cam = cam;
    r->proj = m4_perspective(fov * PI_F / 180.0f, aspect, 0.1f, 120.0f);
    r->view = m4_lookat(cam, tgt, v3(0, 1, 0));
    r->vp = m4_mul(r->proj, r->view);
    extract_planes(r);
    glClearColor(0.16f, 0.18f, 0.24f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE); glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glFrontFace(GL_CCW); glDisable(GL_BLEND);
    r->progW = r->prog[0];
    r->wVP = L[0]; r->wModel = L[1]; r->wSun = L[2]; r->wSunCol = L[3]; r->wAmb = L[4]; r->wCam = L[5];
    r->wFog = L[6]; r->wFogCol = L[7]; r->wTint = L[8]; r->wEmis = L[9]; r->wAlpha = L[10]; r->wUnlit = L[11];
    glUseProgram(r->progW);
    glEnableVertexAttribArray(0); glEnableVertexAttribArray(1); glEnableVertexAttribArray(2);
    glUniformMatrix4fv(r->wVP, 1, GL_FALSE, r->vp.m);
    glUniform3f(r->wSun, -0.45f, 0.8f, 0.4f);
    glUniform3f(r->wSunCol, 0.75f, 0.72f, 0.66f);
    glUniform3f(r->wAmb, 0.55f, 0.58f, 0.66f);
    glUniform3f(r->wCam, cam.x, cam.y, cam.z);
    glUniform2f(r->wFog, 60.0f, 110.0f);
    glUniform3f(r->wFogCol, 0.16f, 0.18f, 0.24f);
    glUniform1f(r->wEmis, 0.6f);
    glUniform1f(r->wAlpha, 1.0f);
    glUniform1f(r->wUnlit, 0.0f);
    tint(r, 1, 1, 1);
    draw_mesh(r, &r->buildFloor, &id);
    for (y = 0; y < PH_GY; y++) for (z = 0; z < PH_GZ; z++) for (x = 0; x < PH_GX; x++) {
        int tp = d->c[y][z][x];
        V3 c;
        M4 m;
        if (tp <= 0 || tp >= PP_COUNT) continue;
        c = ph_cell_pos(x, y, z);
        m = m4_translate(c.x, c.y, c.z);
        draw_mesh(r, tp == PP_WHEEL ? &r->phWheel : &r->phPart[tp], &m);
        if (tp == PP_SEAT) draw_char(r, 0, m4_translate(c.x, c.y - 0.6f, c.z - 0.05f), 0, 3, 0, 0, 0);
    }
    if (gx >= 0) {   /* spookonderdeel: waar het komt (of rood: wat er weggaat) */
        V3 c = ph_cell_pos(gx, gy, gz);
        M4 m = m4_translate(c.x, c.y, c.z);
        float pulse = 0.35f + 0.15f * sinf(t * 6.0f);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glUniform1f(r->wUnlit, 0.6f);
        glUniform1f(r->wAlpha, pulse + 0.15f);
        if (ghostType > 0 && ghostType < PP_COUNT) { tint(r, 0.6f, 1.0f, 0.6f); draw_mesh(r, ghostType == PP_WHEEL ? &r->phWheel : &r->phPart[ghostType], &m); }
        else { m = m4_mul(m, m4_scale(1.08f, 1.08f, 1.08f)); tint(r, 1.0f, 0.2f, 0.15f); draw_mesh(r, &r->phPart[PP_BLOCK], &m); }
        tint(r, 1, 1, 1);
        glUniform1f(r->wUnlit, 0.0f);
        glUniform1f(r->wAlpha, 1.0f);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisableVertexAttribArray(1);
}
