/* v5.0 Meta Quest-versie (VR via OpenXR, OpenGL ES).
 *
 * - De wereld wordt per oog getekend (Renderer.vr); HUD en menu's tekenen in een eigen textuur die als
 *   zwevend scherm (OpenXR quad-laag) in beeld komt: in menu's vast in de ruimte en te bedienen met een
 *   laserpointer (rechtertrekker of A), tijdens het spelen als HUD voor je ogen.
 * - Third person (standaard): je kijkt vanaf de gewone volgcamera, je hoofd kijkt vrij rond.
 * - First person: je zit in het hoofd van je personage; je handen volgen de controllers, het wapen zit
 *   in je rechterhand en je richt ermee. Rechterstick links/rechts = draaien in stappen.
 * - Wisselen: rechter stick indrukken (R3).
 *
 * Knoppen (Xbox-indeling voor het spel): A springen, B rennen, X wapen wisselen, Y in/uitstappen,
 * rechtertrekker schieten/gas, linkertrekker remmen, linker stick indrukken kaart, menuknop pauze.
 *
 * Geluid en internet lopen via Java (QuestActivity). Bestanden komen uit de APK-assets. */
#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <jni.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "../Render/gl_inc.h"
#define XR_USE_PLATFORM_ANDROID 1
#define XR_USE_GRAPHICS_API_OPENGL_ES 1
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../Render/platform.h"
#include "../Render/frontend.h"
#include "../Render/render.h"
#include "../Game/game.h"

#define TAG "SGB-VR"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

#ifndef GL_SRGB8_ALPHA8
#define GL_SRGB8_ALPHA8 0x8C43
#endif
#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif
#ifndef GL_FRAMEBUFFER_SRGB_EXT
#define GL_FRAMEBUFFER_SRGB_EXT 0x8DB9
#endif
#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24 0x81A6
#endif

static struct android_app *g_app;
static char BASE[512];

/* ================================================================== platform: bestanden, instellingen */

static void mkdirs(const char *path) {
    char tmp[512];
    size_t i;
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (i = 1; tmp[i]; i++)
        if (tmp[i] == '/') { tmp[i] = 0; mkdir(tmp, 0755); tmp[i] = '/'; }
}

static int read_all(const char *path, char **data, long *len) {
    FILE *f = fopen(path, "rb");
    long n;
    if (!f) return 0;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    *data = (char *)malloc((size_t)n + 1);
    if (!*data || fread(*data, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(*data); *data = NULL; return 0; }
    (*data)[n] = 0; *len = n;
    fclose(f);
    return 1;
}

int plat_read_resource(const char *name, char **data, long *len) {
    AAsset *a = AAssetManager_open(g_app->activity->assetManager, name, AASSET_MODE_BUFFER);
    off_t n;
    if (!a) return 0;
    n = AAsset_getLength(a);
    *data = (char *)malloc((size_t)n + 1);
    if (!*data) { AAsset_close(a); return 0; }
    if (n > 0 && AAsset_read(a, *data, (size_t)n) != n) { free(*data); *data = NULL; AAsset_close(a); return 0; }
    (*data)[n] = 0; *len = (long)n;
    AAsset_close(a);
    return 1;
}

int plat_read_file(const char *name, char **data, long *len) {
    char p[512];
    snprintf(p, sizeof(p), "%s/%s", BASE, name);
    return read_all(p, data, len);
}

int plat_write_file(const char *name, const char *data, long len) {
    char p[512];
    FILE *f;
    snprintf(p, sizeof(p), "%s/%s", BASE, name);
    mkdirs(p);
    f = fopen(p, "wb");
    if (!f) return 0;
    fwrite(data, 1, (size_t)len, f);
    fclose(f);
    return 1;
}

void plat_delete_file(const char *name) { char p[512]; snprintf(p, sizeof(p), "%s/%s", BASE, name); remove(p); }
void plat_write_text(const char *name, const char *text) { plat_write_file(name, text, (long)strlen(text)); }

int plat_load_image(const char *name, PlatImage *out) {
    char *d = NULL, n2[260];
    long len = 0;
    int wh[2];
    snprintf(n2, sizeof(n2), "%s.raw", name);
    if (!plat_read_resource(n2, &d, &len) || len < 8) return 0;
    memcpy(wh, d, 8);
    if ((long)wh[0] * wh[1] * 4 + 8 > len) { free(d); return 0; }
    out->w = wh[0]; out->h = wh[1];
    out->rgba = (uint8_t *)malloc((size_t)wh[0] * wh[1] * 4);
    memcpy(out->rgba, d + 8, (size_t)wh[0] * wh[1] * 4);
    free(d);
    return 1;
}

void plat_free_image(PlatImage *img) { free(img->rgba); img->rgba = NULL; }

int plat_make_font(FontData *out, float size) {
    char *d = NULL, *p;
    long len = 0;
    int i;
    (void)size;
    if (!plat_load_image("font", &out->img)) return 0;
    if (!plat_read_resource("font.txt", &d, &len)) return 0;
    p = d;
    out->size = (float)strtod(p, &p); out->lineh = (float)strtod(p, &p);
    for (i = 0; i < 96; i++) {
        Glyph *g = &out->g[i];
        g->u0 = (float)strtod(p, &p); g->v0 = (float)strtod(p, &p); g->u1 = (float)strtod(p, &p); g->v1 = (float)strtod(p, &p);
        g->w = (float)strtod(p, &p); g->h = (float)strtod(p, &p); g->xoff = (float)strtod(p, &p); g->yoff = (float)strtod(p, &p);
        g->adv = (float)strtod(p, &p);
    }
    free(d);
    return 1;
}

void plat_log(const char *fmt, ...) {
    va_list ap;
    char buf[1024];
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    __android_log_print(ANDROID_LOG_INFO, "SGB", "%s", buf);
}

double plat_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec * 1e-9;
}

long plat_memory_kb(void) {
    long pages = 0, rss = 0;
    FILE *f = fopen("/proc/self/statm", "r");
    if (!f) return -1;
    if (fscanf(f, "%ld %ld", &pages, &rss) != 2) rss = -1;
    fclose(f);
    return rss < 0 ? -1 : rss * (sysconf(_SC_PAGESIZE) / 1024);
}

#define MAXKEYS 256
static char keys[MAXKEYS][32];
static int vals[MAXKEYS], nkeys;

static void settings_load(void) {
    char *d = NULL, *line, *save = NULL;
    long len = 0;
    if (!plat_read_file("settings.txt", &d, &len)) return;
    for (line = strtok_r(d, "\r\n", &save); line && nkeys < MAXKEYS; line = strtok_r(NULL, "\r\n", &save)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        snprintf(keys[nkeys], sizeof(keys[0]), "%s", line);
        vals[nkeys++] = atoi(eq + 1);
    }
    free(d);
}

int plat_load_int(const char *key, int def) {
    int i;
    for (i = 0; i < nkeys; i++) if (strcmp(keys[i], key) == 0) return vals[i];
    return def;
}

void plat_save_int(const char *key, int val) {
    int i;
    for (i = 0; i < nkeys; i++) if (strcmp(keys[i], key) == 0) { vals[i] = val; return; }
    if (nkeys < MAXKEYS) { snprintf(keys[nkeys], sizeof(keys[0]), "%s", key); vals[nkeys++] = val; }
}

void plat_save_flush(void) {
    static char buf[MAXKEYS * 48];
    int i, n = 0;
    for (i = 0; i < nkeys; i++) n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s=%d\n", keys[i], vals[i]);
    plat_write_file("settings.txt", buf, n);
}

int plat_autotest_requested(void) { return 0; }
void plat_autotest_done(void) {}
void plat_screenshot(const char *name, int w, int h) { (void)name; (void)w; (void)h; }
const char *plat_key_name(int code) { (void)code; return ""; }

/* ================================================================== Java: geluid en internet */

static JavaVM *g_vm;
static jclass g_cls;
static jmethodID m_audioInit, m_sfx, m_loop, m_music, m_voice, m_http;

static JNIEnv *jni_env(void) {
    JNIEnv *env = NULL;
    if ((*g_vm)->GetEnv(g_vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) (*g_vm)->AttachCurrentThread(g_vm, &env, NULL);
    return env;
}

static void jni_check(JNIEnv *env) { if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionDescribe(env); (*env)->ExceptionClear(env); } }

static void java_init(void) {
    JNIEnv *env;
    jclass c;
    g_vm = g_app->activity->vm;
    env = jni_env();
    c = (*env)->GetObjectClass(env, g_app->activity->clazz);
    g_cls = (jclass)(*env)->NewGlobalRef(env, c);
    m_audioInit = (*env)->GetStaticMethodID(env, g_cls, "audioInit", "([Ljava/lang/String;[Ljava/lang/String;)V");
    m_sfx = (*env)->GetStaticMethodID(env, g_cls, "sfx", "(IF)V");
    m_loop = (*env)->GetStaticMethodID(env, g_cls, "loop", "(IFF)V");
    m_music = (*env)->GetStaticMethodID(env, g_cls, "music", "(Z)V");
    m_voice = (*env)->GetStaticMethodID(env, g_cls, "voice", "(Ljava/lang/String;F)V");
    m_http = (*env)->GetStaticMethodID(env, g_cls, "httpGet", "(Ljava/lang/String;)[B");
    jni_check(env);
    plat_log("java: audio %s, http %s", m_sfx ? "ok" : "MIST", m_http ? "ok" : "MIST");
}

static jobjectArray str_array(JNIEnv *env, const char *const *s, int n) {
    jclass sc = (*env)->FindClass(env, "java/lang/String");
    jobjectArray a = (*env)->NewObjectArray(env, n, sc, NULL);
    int i;
    for (i = 0; i < n; i++) {
        jstring js = (*env)->NewStringUTF(env, s[i]);
        (*env)->SetObjectArrayElement(env, a, i, js);
        (*env)->DeleteLocalRef(env, js);
    }
    return a;
}

void plat_audio_init(const char *const *sfx, int nsfx, const char *const *loops, int nloops) {
    JNIEnv *env = jni_env();
    jobjectArray a, b;
    if (!m_audioInit) return;
    a = str_array(env, sfx, nsfx); b = str_array(env, loops, nloops);
    (*env)->CallStaticVoidMethod(env, g_cls, m_audioInit, a, b);
    jni_check(env);
    (*env)->DeleteLocalRef(env, a); (*env)->DeleteLocalRef(env, b);
}

void plat_sfx(int id, float vol) {
    JNIEnv *env;
    if (!m_sfx || vol <= 0.01f) return;
    env = jni_env();
    (*env)->CallStaticVoidMethod(env, g_cls, m_sfx, id, vol);
    jni_check(env);
}

void plat_loop(int id, float vol, float rate) {
    static float lastV[8] = {-1, -1, -1, -1, -1, -1, -1, -1}, lastR[8];
    JNIEnv *env;
    if (!m_loop || id < 0 || id >= 8) return;
    if (fabsf(vol - lastV[id]) < 0.02f && fabsf(rate - lastR[id]) < 0.03f) return;   /* niet elke frame naar Java */
    lastV[id] = vol; lastR[id] = rate;
    env = jni_env();
    (*env)->CallStaticVoidMethod(env, g_cls, m_loop, id, vol, rate);
    jni_check(env);
}

void plat_music(int on) {
    JNIEnv *env;
    if (!m_music) return;
    env = jni_env();
    (*env)->CallStaticVoidMethod(env, g_cls, m_music, (jboolean)(on != 0));
    jni_check(env);
}

void plat_voice(const char *key, float vol) {
    JNIEnv *env;
    jstring js;
    if (!m_voice) return;
    env = jni_env();
    js = key ? (*env)->NewStringUTF(env, key) : NULL;
    (*env)->CallStaticVoidMethod(env, g_cls, m_voice, js, vol);
    jni_check(env);
    if (js) (*env)->DeleteLocalRef(env, js);
}

/* internet: elke aanvraag in een eigen thread (Java HttpURLConnection) */
#define NREQ 256
typedef struct { volatile int state; char url[1024]; char *data; long len; } Req;
static Req reqs[NREQ];
static int reqNext;

static void *http_thread(void *arg) {
    Req *r = (Req *)arg;
    JNIEnv *env = NULL;
    int ok = 0;
    (*g_vm)->AttachCurrentThread(g_vm, &env, NULL);
    if (env && m_http) {
        jstring js = (*env)->NewStringUTF(env, r->url);
        jbyteArray arr = (jbyteArray)(*env)->CallStaticObjectMethod(env, g_cls, m_http, js);
        if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); arr = NULL; }
        if (arr) {
            jsize n = (*env)->GetArrayLength(env, arr);
            if (n > 0 && (r->data = (char *)malloc((size_t)n + 1)) != NULL) {
                (*env)->GetByteArrayRegion(env, arr, 0, n, (jbyte *)r->data);
                r->data[n] = 0; r->len = n; ok = 1;
            }
            (*env)->DeleteLocalRef(env, arr);
        }
        (*env)->DeleteLocalRef(env, js);
    }
    (*g_vm)->DetachCurrentThread(g_vm);
    __atomic_store_n(&r->state, ok ? 2 : 3, __ATOMIC_SEQ_CST);
    return NULL;
}

int plat_http_start(const char *url) {
    int i;
    for (i = 0; i < NREQ; i++) {
        int k = (reqNext + i) % NREQ;
        if (reqs[k].state == 0) {
            pthread_t th;
            reqNext = k + 1;
            snprintf(reqs[k].url, sizeof(reqs[k].url), "%s", url);
            reqs[k].data = NULL; reqs[k].len = 0; reqs[k].state = 1;
            if (pthread_create(&th, NULL, http_thread, &reqs[k]) != 0) { reqs[k].state = 0; return -1; }
            pthread_detach(th);
            return k + 1;
        }
    }
    return -1;
}

int plat_http_poll(int rid, char **data, long *len) {
    Req *r;
    if (rid <= 0 || rid > NREQ) return -1;
    r = &reqs[rid - 1];
    if (r->state == 1) return 0;
    if (r->state == 2) { *data = r->data; *len = r->len; r->data = NULL; r->state = 0; return 1; }
    r->state = 0;
    return -1;
}

/* ================================================================== kleine wiskunde: XR-poses -> M4 */

static M4 quat_mat(XrQuaternionf q) {
    M4 m = m4_identity();
    float x = q.x, y = q.y, z = q.z, w = q.w;
    m.m[0] = 1 - 2 * (y * y + z * z); m.m[1] = 2 * (x * y + z * w);     m.m[2] = 2 * (x * z - y * w);
    m.m[4] = 2 * (x * y - z * w);     m.m[5] = 1 - 2 * (x * x + z * z); m.m[6] = 2 * (y * z + x * w);
    m.m[8] = 2 * (x * z + y * w);     m.m[9] = 2 * (y * z - x * w);     m.m[10] = 1 - 2 * (x * x + y * y);
    return m;
}

static M4 pose_mat(XrPosef p) {
    M4 m = quat_mat(p.orientation);
    m.m[12] = p.position.x; m.m[13] = p.position.y; m.m[14] = p.position.z;
    return m;
}

/* inverse van een starre transformatie (rotatie + verschuiving) */
static M4 rigid_inv(M4 a) {
    M4 r = m4_identity();
    int i, j;
    for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) r.m[i * 4 + j] = a.m[j * 4 + i];
    r.m[12] = -(r.m[0] * a.m[12] + r.m[4] * a.m[13] + r.m[8] * a.m[14]);
    r.m[13] = -(r.m[1] * a.m[12] + r.m[5] * a.m[13] + r.m[9] * a.m[14]);
    r.m[14] = -(r.m[2] * a.m[12] + r.m[6] * a.m[13] + r.m[10] * a.m[14]);
    return r;
}

static M4 fov_proj(XrFovf f, float n, float fa) {
    M4 m;
    float l = tanf(f.angleLeft), r = tanf(f.angleRight), u = tanf(f.angleUp), d = tanf(f.angleDown);
    float w = r - l, h = u - d;
    memset(&m, 0, sizeof(m));
    m.m[0] = 2.0f / w; m.m[5] = 2.0f / h;
    m.m[8] = (r + l) / w; m.m[9] = (u + d) / h;
    m.m[10] = -(fa + n) / (fa - n); m.m[11] = -1.0f;
    m.m[14] = -(2.0f * fa * n) / (fa - n);
    return m;
}

static V3 mat_pos(M4 m) { return v3(m.m[12], m.m[13], m.m[14]); }
static V3 mat_fwd(M4 m) { return v3(-m.m[8], -m.m[9], -m.m[10]); }   /* -Z van de pose */

/* ================================================================== OpenXR */

typedef struct {
    XrSwapchain sc;
    int w, h;
    uint32_t n, cur;
    XrSwapchainImageOpenGLESKHR img[8];
    GLuint fbo[8], depth;
} Chain;

static struct {
    EGLDisplay dpy; EGLContext ctx; EGLSurface surf; EGLConfig cfg;
    XrInstance inst; XrSystemId sys; XrSession sess;
    XrSpace local, view;
    XrSessionState state;
    int running, resumed;
    Chain eye[2], ui;
    XrViewConfigurationView vcv[2];
    XrView views[2];
    XrFrameState fs;
    /* invoer */
    XrActionSet aset;
    XrAction aMove, aLook, aTrigL, aTrigR, aGripL, aGripR, aA, aB, aX, aY, aMenu, aStickL, aStickR, aAim, aHaptic;
    XrPath hand[2];
    XrSpace aimSpace[2];
    XrPosef aimLocal[2]; int aimOk[2];
    /* rig */
    int firstPerson;
    float fpYaw;          /* first person te voet: kijkrichting van het lichaam (snap-turn) */
    float vehYawOff;      /* first person in een voertuig: extra draai */
    float snapLatch;
    int lastVeh;
    M4 rig;               /* LOCAL-ruimte -> wereld */
    XrPosef uiPose; int uiInMenu;
    int trigWas, lastShots;
    int msaa;
    PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC fbTexMS;
    PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC rbMS;
} X;

#define XRC(call) do { XrResult _r = (call); if (XR_FAILED(_r)) { LOGI("%s mislukt: %d", #call, (int)_r); } } while (0)

static void egl_init(void) {
    EGLint n = 0, maj, min;
    EGLConfig cfgs[64];
    const EGLint ctxAttr[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    const EGLint sAttr[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
    int i;
    X.dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(X.dpy, &maj, &min);
    eglGetConfigs(X.dpy, cfgs, 64, &n);
    for (i = 0; i < n; i++) {
        EGLint r, g, b, a, d, s, rt, st;
        eglGetConfigAttrib(X.dpy, cfgs[i], EGL_RED_SIZE, &r); eglGetConfigAttrib(X.dpy, cfgs[i], EGL_GREEN_SIZE, &g);
        eglGetConfigAttrib(X.dpy, cfgs[i], EGL_BLUE_SIZE, &b); eglGetConfigAttrib(X.dpy, cfgs[i], EGL_ALPHA_SIZE, &a);
        eglGetConfigAttrib(X.dpy, cfgs[i], EGL_DEPTH_SIZE, &d); eglGetConfigAttrib(X.dpy, cfgs[i], EGL_SAMPLES, &s);
        eglGetConfigAttrib(X.dpy, cfgs[i], EGL_RENDERABLE_TYPE, &rt); eglGetConfigAttrib(X.dpy, cfgs[i], EGL_SURFACE_TYPE, &st);
        if (r == 8 && g == 8 && b == 8 && a == 8 && s == 0 && (rt & 0x40 /* ES3 */) && (st & EGL_PBUFFER_BIT)) { X.cfg = cfgs[i]; break; }
    }
    X.ctx = eglCreateContext(X.dpy, X.cfg, EGL_NO_CONTEXT, ctxAttr);
    X.surf = eglCreatePbufferSurface(X.dpy, X.cfg, sAttr);
    eglMakeCurrent(X.dpy, X.surf, X.surf, X.ctx);
    {
        const char *ext = (const char *)glGetString(GL_EXTENSIONS);
        if (ext && strstr(ext, "GL_EXT_multisampled_render_to_texture")) {
            X.fbTexMS = (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)eglGetProcAddress("glFramebufferTexture2DMultisampleEXT");
            X.rbMS = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)eglGetProcAddress("glRenderbufferStorageMultisampleEXT");
            X.msaa = X.fbTexMS && X.rbMS ? 4 : 0;
        }
    }
    plat_log("EGL %d.%d, GL %s, msaa %d", maj, min, (const char *)glGetString(GL_VERSION), X.msaa);
}

static int xr_init(void) {
    PFN_xrInitializeLoaderKHR initLoader = NULL;
    const char *exts[] = {XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME, XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME};
    XrInstanceCreateInfoAndroidKHR ia = {XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
    XrInstanceCreateInfo ci = {XR_TYPE_INSTANCE_CREATE_INFO};
    XrSystemGetInfo sg = {XR_TYPE_SYSTEM_GET_INFO};
    XrGraphicsRequirementsOpenGLESKHR req = {XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
    PFN_xrGetOpenGLESGraphicsRequirementsKHR getReq = NULL;
    XrGraphicsBindingOpenGLESAndroidKHR gb = {XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
    XrSessionCreateInfo sci = {XR_TYPE_SESSION_CREATE_INFO};
    XrReferenceSpaceCreateInfo rs = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    uint32_t nv = 0;

    xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction *)&initLoader);
    if (initLoader) {
        XrLoaderInitInfoAndroidKHR li = {XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
        li.applicationVM = g_app->activity->vm;
        li.applicationContext = g_app->activity->clazz;
        initLoader((const XrLoaderInitInfoBaseHeaderKHR *)&li);
    }
    ia.applicationVM = g_app->activity->vm;
    ia.applicationActivity = g_app->activity->clazz;
    ci.next = &ia;
    ci.enabledExtensionCount = 2;
    ci.enabledExtensionNames = exts;
    snprintf(ci.applicationInfo.applicationName, sizeof(ci.applicationInfo.applicationName), "Giveaway Bike VR");
    ci.applicationInfo.applicationVersion = 5;
    snprintf(ci.applicationInfo.engineName, sizeof(ci.applicationInfo.engineName), "SGB");
    ci.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
    if (XR_FAILED(xrCreateInstance(&ci, &X.inst))) { LOGI("geen OpenXR-instance"); return 0; }
    sg.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    if (XR_FAILED(xrGetSystem(X.inst, &sg, &X.sys))) { LOGI("geen headset"); return 0; }
    xrGetInstanceProcAddr(X.inst, "xrGetOpenGLESGraphicsRequirementsKHR", (PFN_xrVoidFunction *)&getReq);
    if (getReq) getReq(X.inst, X.sys, &req);

    xrEnumerateViewConfigurationViews(X.inst, X.sys, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &nv, NULL);
    X.vcv[0].type = X.vcv[1].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
    xrEnumerateViewConfigurationViews(X.inst, X.sys, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &nv, X.vcv);

    gb.display = X.dpy; gb.config = X.cfg; gb.context = X.ctx;
    sci.next = &gb;
    sci.systemId = X.sys;
    if (XR_FAILED(xrCreateSession(X.inst, &sci, &X.sess))) { LOGI("geen sessie"); return 0; }
    rs.poseInReferenceSpace.orientation.w = 1;
    rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    XRC(xrCreateReferenceSpace(X.sess, &rs, &X.local));
    rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    XRC(xrCreateReferenceSpace(X.sess, &rs, &X.view));
    plat_log("OpenXR klaar: oog %ux%u", X.vcv[0].recommendedImageRectWidth, X.vcv[0].recommendedImageRectHeight);
    return 1;
}

static void chain_make(Chain *c, int w, int h, int depth, int msaa) {
    XrSwapchainCreateInfo sc = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
    int64_t fm[32];
    uint32_t nf = 0, i;
    int64_t want = GL_RGBA8;
    xrEnumerateSwapchainFormats(X.sess, 32, &nf, fm);
    for (i = 0; i < nf; i++) if (fm[i] == GL_SRGB8_ALPHA8) want = GL_SRGB8_ALPHA8;
    sc.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
    sc.format = want;
    sc.sampleCount = 1;
    sc.width = (uint32_t)w; sc.height = (uint32_t)h;
    sc.faceCount = 1; sc.arraySize = 1; sc.mipCount = 1;
    XRC(xrCreateSwapchain(X.sess, &sc, &c->sc));
    c->w = w; c->h = h;
    for (i = 0; i < 8; i++) c->img[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
    xrEnumerateSwapchainImages(c->sc, 8, &c->n, (XrSwapchainImageBaseHeader *)c->img);
    glGenRenderbuffers(1, &c->depth);
    glBindRenderbuffer(GL_RENDERBUFFER, c->depth);
    if (msaa && X.rbMS) X.rbMS(GL_RENDERBUFFER, msaa, GL_DEPTH_COMPONENT24, w, h);
    else glRenderbufferStorage(GL_RENDERBUFFER, depth ? GL_DEPTH_COMPONENT24 : GL_DEPTH_COMPONENT16, w, h);
    for (i = 0; i < c->n; i++) {
        glGenFramebuffers(1, &c->fbo[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, c->fbo[i]);
        if (msaa && X.fbTexMS) X.fbTexMS(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, c->img[i].image, 0, msaa);
        else glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, c->img[i].image, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, c->depth);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) LOGI("FBO onvolledig (%dx%d)", w, h);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void chain_acquire(Chain *c) {
    XrSwapchainImageAcquireInfo ai = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    XrSwapchainImageWaitInfo wi = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    xrAcquireSwapchainImage(c->sc, &ai, &c->cur);
    wi.timeout = XR_INFINITE_DURATION;
    xrWaitSwapchainImage(c->sc, &wi);
}

static void chain_release(Chain *c) {
    XrSwapchainImageReleaseInfo ri = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    xrReleaseSwapchainImage(c->sc, &ri);
}

static XrPath path(const char *s) { XrPath p = XR_NULL_PATH; xrStringToPath(X.inst, s, &p); return p; }

static XrAction mk_action(const char *name, XrActionType t, int sub) {
    XrActionCreateInfo ac = {XR_TYPE_ACTION_CREATE_INFO};
    XrAction a = XR_NULL_HANDLE;
    snprintf(ac.actionName, sizeof(ac.actionName), "%s", name);
    snprintf(ac.localizedActionName, sizeof(ac.localizedActionName), "%s", name);
    ac.actionType = t;
    if (sub) { ac.countSubactionPaths = 2; ac.subactionPaths = X.hand; }
    XRC(xrCreateAction(X.aset, &ac, &a));
    return a;
}

static void input_init(void) {
    XrActionSetCreateInfo as = {XR_TYPE_ACTION_SET_CREATE_INFO};
    XrActionSuggestedBinding b[32];
    XrInteractionProfileSuggestedBinding sb = {XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    XrSessionActionSetsAttachInfo at = {XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    int n = 0, k;
    snprintf(as.actionSetName, sizeof(as.actionSetName), "game");
    snprintf(as.localizedActionSetName, sizeof(as.localizedActionSetName), "Game");
    XRC(xrCreateActionSet(X.inst, &as, &X.aset));
    X.hand[0] = path("/user/hand/left");
    X.hand[1] = path("/user/hand/right");
    X.aMove = mk_action("move", XR_ACTION_TYPE_VECTOR2F_INPUT, 0);
    X.aLook = mk_action("look", XR_ACTION_TYPE_VECTOR2F_INPUT, 0);
    X.aTrigL = mk_action("trigger_left", XR_ACTION_TYPE_FLOAT_INPUT, 0);
    X.aTrigR = mk_action("trigger_right", XR_ACTION_TYPE_FLOAT_INPUT, 0);
    X.aGripL = mk_action("grip_left", XR_ACTION_TYPE_FLOAT_INPUT, 0);
    X.aGripR = mk_action("grip_right", XR_ACTION_TYPE_FLOAT_INPUT, 0);
    X.aA = mk_action("button_a", XR_ACTION_TYPE_BOOLEAN_INPUT, 0);
    X.aB = mk_action("button_b", XR_ACTION_TYPE_BOOLEAN_INPUT, 0);
    X.aX = mk_action("button_x", XR_ACTION_TYPE_BOOLEAN_INPUT, 0);
    X.aY = mk_action("button_y", XR_ACTION_TYPE_BOOLEAN_INPUT, 0);
    X.aMenu = mk_action("menu", XR_ACTION_TYPE_BOOLEAN_INPUT, 0);
    X.aStickL = mk_action("stick_left_click", XR_ACTION_TYPE_BOOLEAN_INPUT, 0);
    X.aStickR = mk_action("stick_right_click", XR_ACTION_TYPE_BOOLEAN_INPUT, 0);
    X.aAim = mk_action("aim", XR_ACTION_TYPE_POSE_INPUT, 1);
    X.aHaptic = mk_action("haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT, 1);
#define BIND(A, P) do { b[n].action = (A); b[n].binding = path(P); n++; } while (0)
    BIND(X.aMove, "/user/hand/left/input/thumbstick");
    BIND(X.aLook, "/user/hand/right/input/thumbstick");
    BIND(X.aTrigL, "/user/hand/left/input/trigger/value");
    BIND(X.aTrigR, "/user/hand/right/input/trigger/value");
    BIND(X.aGripL, "/user/hand/left/input/squeeze/value");
    BIND(X.aGripR, "/user/hand/right/input/squeeze/value");
    BIND(X.aA, "/user/hand/right/input/a/click");
    BIND(X.aB, "/user/hand/right/input/b/click");
    BIND(X.aX, "/user/hand/left/input/x/click");
    BIND(X.aY, "/user/hand/left/input/y/click");
    BIND(X.aMenu, "/user/hand/left/input/menu/click");
    BIND(X.aStickL, "/user/hand/left/input/thumbstick/click");
    BIND(X.aStickR, "/user/hand/right/input/thumbstick/click");
    BIND(X.aAim, "/user/hand/left/input/aim/pose");
    BIND(X.aAim, "/user/hand/right/input/aim/pose");
    BIND(X.aHaptic, "/user/hand/left/output/haptic");
    BIND(X.aHaptic, "/user/hand/right/output/haptic");
#undef BIND
    sb.interactionProfile = path("/interaction_profiles/oculus/touch_controller");
    sb.suggestedBindings = b;
    sb.countSuggestedBindings = (uint32_t)n;
    XRC(xrSuggestInteractionProfileBindings(X.inst, &sb));
    for (k = 0; k < 2; k++) {
        XrActionSpaceCreateInfo sp = {XR_TYPE_ACTION_SPACE_CREATE_INFO};
        sp.action = X.aAim;
        sp.subactionPath = X.hand[k];
        sp.poseInActionSpace.orientation.w = 1;
        XRC(xrCreateActionSpace(X.sess, &sp, &X.aimSpace[k]));
    }
    at.countActionSets = 1;
    at.actionSets = &X.aset;
    XRC(xrAttachSessionActionSets(X.sess, &at));
}

static float get_f(XrAction a) {
    XrActionStateGetInfo gi = {XR_TYPE_ACTION_STATE_GET_INFO};
    XrActionStateFloat st = {XR_TYPE_ACTION_STATE_FLOAT};
    gi.action = a;
    return XR_SUCCEEDED(xrGetActionStateFloat(X.sess, &gi, &st)) && st.isActive ? st.currentState : 0.0f;
}
static int get_b(XrAction a) {
    XrActionStateGetInfo gi = {XR_TYPE_ACTION_STATE_GET_INFO};
    XrActionStateBoolean st = {XR_TYPE_ACTION_STATE_BOOLEAN};
    gi.action = a;
    return XR_SUCCEEDED(xrGetActionStateBoolean(X.sess, &gi, &st)) && st.isActive && st.currentState;
}
static XrVector2f get_v(XrAction a) {
    XrActionStateGetInfo gi = {XR_TYPE_ACTION_STATE_GET_INFO};
    XrActionStateVector2f st = {XR_TYPE_ACTION_STATE_VECTOR2F};
    XrVector2f z = {0, 0};
    gi.action = a;
    return XR_SUCCEEDED(xrGetActionStateVector2f(X.sess, &gi, &st)) && st.isActive ? st.currentState : z;
}

static void haptic(int hand, float amp, float sec) {
    XrHapticVibration v = {XR_TYPE_HAPTIC_VIBRATION};
    XrHapticActionInfo hi = {XR_TYPE_HAPTIC_ACTION_INFO};
    v.amplitude = amp; v.duration = (XrDuration)(sec * 1e9); v.frequency = XR_FREQUENCY_UNSPECIFIED;
    hi.action = X.aHaptic; hi.subactionPath = X.hand[hand];
    xrApplyHapticFeedback(X.sess, &hi, (const XrHapticBaseHeader *)&v);
}

/* ================================================================== rig: waar staat de speler in de wereld */

static float yaw_of(V3 f) { return atan2f(f.x, f.z); }

/* LOCAL -> wereld; heading = richting waar "vooruit" (-Z in LOCAL) naartoe wijst */
static M4 rig_mat(V3 pos, float heading) { return m4_mul(m4_translate(pos.x, pos.y, pos.z), m4_rot_y(heading + PI_F)); }

static void rig_update(Game *g) {
    if (X.firstPerson) {
        int veh = g->p.veh >= 0 && !(g->p.anT > 0);
        if (veh) {
            const Vehicle *v = &g->veh[g->p.veh];
            V3 f = v3(sinf(v->h), 0, cosf(v->h)), r = v3(cosf(v->h), 0, -sinf(v->h));
            V3 seat = v->type == VEH_CAR ? v3add(v3add(v3(v->x, v->y + 1.15f, v->z), v3scale(r, -0.38f)), v3scale(f, -0.15f))
                                         : v3add(v3(v->x, v->y + 1.62f, v->z), v3scale(f, -0.2f));
            if (X.lastVeh != g->p.veh) { X.vehYawOff = 0; X.lastVeh = g->p.veh; }
            X.rig = rig_mat(seat, v->h + X.vehYawOff);
        } else {
            if (X.lastVeh >= 0) { X.fpYaw = g->p.h; X.lastVeh = -1; }
            X.rig = rig_mat(v3(g->p.x, g->p.y + 1.62f, g->p.z), X.fpYaw);
        }
    } else {
        V3 f = v3sub(g->camTarget, g->camPos);
        X.rig = rig_mat(g->camPos, yaw_of(f));
        X.lastVeh = -2;
    }
}

/* Renderer.vr.begin: na game_update, vlak voor het tekenen van de ogen */
static void vr_begin(void *rp, void *gp, void *u) {
    Renderer *r = (Renderer *)rp;
    Game *g = (Game *)gp;
    int e, k;
    (void)u;
    rig_update(g);
    for (e = 0; e < 2; e++) {
        M4 w = m4_mul(X.rig, pose_mat(X.views[e].pose));
        r->vr.view[e] = rigid_inv(w);
        r->vr.proj[e] = fov_proj(X.views[e].fov, 0.08f, g->drawDist + 60.0f);
        r->vr.pos[e] = mat_pos(w);
    }
    r->vr.hidePlayer = X.firstPerson;
    r->vr.hands = X.firstPerson && X.aimOk[0] && X.aimOk[1];
    for (k = 0; k < 2; k++) if (X.aimOk[k]) r->vr.hand[k] = m4_mul(X.rig, pose_mat(X.aimLocal[k]));
}

static void vr_bind_eye(int eye, void *u) {
    Renderer *r = (Renderer *)u;
    Chain *c = &X.eye[eye];
    glBindFramebuffer(GL_FRAMEBUFFER, c->fbo[c->cur]);
    glViewport(0, 0, c->w, c->h);
    glClearColor(r->skyHor[0] * 0.55f + r->skyTop[0] * 0.45f, r->skyHor[1] * 0.55f + r->skyTop[1] * 0.45f, r->skyHor[2] * 0.55f + r->skyTop[2] * 0.45f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

static void vr_bind_ui(void *u) {
    (void)u;
    glBindFramebuffer(GL_FRAMEBUFFER, X.ui.fbo[X.ui.cur]);
    glViewport(0, 0, X.ui.w, X.ui.h);
}

/* ================================================================== een frame */

#define UI_W 1024
#define UI_H 768
#define UI_MW 1.9f              /* breedte van het menuscherm in meters */
#define UI_DIST 1.7f

static XrPosef ident_pose(void) { XrPosef p; memset(&p, 0, sizeof(p)); p.orientation.w = 1; return p; }

/* menuscherm voor je neerzetten (in de LOCAL-ruimte), op ooghoogte, recht naar je toe */
static void ui_place(XrPosef head) {
    M4 hm = pose_mat(head);
    V3 f = mat_fwd(hm);
    float yaw = atan2f(-f.x, -f.z);   /* rotatie om Y zodat het scherm -Z naar je kijkt */
    X.uiPose.position.x = head.position.x + f.x / sqrtf(f.x * f.x + f.z * f.z + 1e-6f) * UI_DIST;
    X.uiPose.position.y = head.position.y - 0.1f;
    X.uiPose.position.z = head.position.z + f.z / sqrtf(f.x * f.x + f.z * f.z + 1e-6f) * UI_DIST;
    X.uiPose.orientation.x = 0; X.uiPose.orientation.z = 0;
    X.uiPose.orientation.y = sinf(yaw * 0.5f); X.uiPose.orientation.w = cosf(yaw * 0.5f);
}

/* laserpointer van de rechterhand op het menuscherm -> UI-coordinaten */
static int ui_hit(float *ux, float *uy) {
    M4 pm, inv, am;
    V3 o, d, lo, ld;
    float t, w = UI_MW, h = UI_MW * UI_H / UI_W;
    if (!X.aimOk[1]) return 0;
    pm = pose_mat(X.uiPose);
    inv = rigid_inv(pm);
    am = pose_mat(X.aimLocal[1]);
    o = mat_pos(am); d = mat_fwd(am);
    lo = v3(inv.m[0] * o.x + inv.m[4] * o.y + inv.m[8] * o.z + inv.m[12], inv.m[1] * o.x + inv.m[5] * o.y + inv.m[9] * o.z + inv.m[13],
            inv.m[2] * o.x + inv.m[6] * o.y + inv.m[10] * o.z + inv.m[14]);
    ld = v3(inv.m[0] * d.x + inv.m[4] * d.y + inv.m[8] * d.z, inv.m[1] * d.x + inv.m[5] * d.y + inv.m[9] * d.z, inv.m[2] * d.x + inv.m[6] * d.y + inv.m[10] * d.z);
    if (ld.z >= -1e-4f) return 0;
    t = -lo.z / ld.z;
    if (t <= 0) return 0;
    *ux = (lo.x + ld.x * t + w * 0.5f) / w * UI_W;
    *uy = (h * 0.5f - (lo.y + ld.y * t)) / h * UI_H;
    return *ux > -40 && *ux < UI_W + 40 && *uy > -40 && *uy < UI_H + 40;
}

static void do_input(Game *g, XrPosef head) {
    XrActiveActionSet aas;
    XrActionsSyncInfo si = {XR_TYPE_ACTIONS_SYNC_INFO};
    float axes[6];
    unsigned btn = 0;
    XrVector2f mv, lk;
    int k, inGame = fe_in_game(), stickR;
    static int stickRWas;
    aas.actionSet = X.aset; aas.subactionPath = XR_NULL_PATH;
    si.countActiveActionSets = 1; si.activeActionSets = &aas;
    xrSyncActions(X.sess, &si);
    for (k = 0; k < 2; k++) {
        XrSpaceLocation sl = {XR_TYPE_SPACE_LOCATION};
        X.aimOk[k] = XR_SUCCEEDED(xrLocateSpace(X.aimSpace[k], X.local, X.fs.predictedDisplayTime, &sl)) &&
                     (sl.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) && (sl.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT);
        if (X.aimOk[k]) X.aimLocal[k] = sl.pose;
    }
    mv = get_v(X.aMove); lk = get_v(X.aLook);
    /* first/third person wisselen: rechter stick indrukken */
    stickR = get_b(X.aStickR);
    if (stickR && !stickRWas) {
        X.firstPerson = !X.firstPerson;
        X.fpYaw = g->p.h; X.vehYawOff = 0;
        plat_save_int("vr_fp", X.firstPerson); plat_save_flush();
        if (inGame) game_message(g, X.firstPerson ? "FIRST PERSON" : "THIRD PERSON", 1.5f);
    }
    stickRWas = stickR;
    axes[0] = mv.x; axes[1] = -mv.y;
    axes[2] = X.firstPerson ? 0.0f : lk.x; axes[3] = X.firstPerson ? 0.0f : -lk.y * 0.5f;
    axes[4] = get_f(X.aTrigL); axes[5] = get_f(X.aTrigR);
    if (get_b(X.aA)) btn |= 1u << PB_A;
    if (get_b(X.aB)) btn |= 1u << PB_B;
    if (get_b(X.aX)) btn |= 1u << PB_X;
    if (get_b(X.aY)) btn |= 1u << PB_Y;
    if (get_b(X.aMenu)) btn |= 1u << PB_START;
    if (get_b(X.aStickL)) btn |= 1u << PB_BACK;
    if (get_f(X.aGripL) > 0.6f) btn |= 1u << PB_LB;
    if (get_f(X.aGripR) > 0.6f) btn |= 1u << PB_RB;
    /* first person: rechter stick = draaien in stappen van 30 graden */
    if (X.firstPerson) {
        if (fabsf(lk.x) > 0.7f && X.snapLatch == 0) {
            float d = lk.x > 0 ? -PI_F / 6.0f : PI_F / 6.0f;
            if (g->p.veh >= 0) X.vehYawOff += d; else X.fpYaw += d;
            X.snapLatch = 1;
        } else if (fabsf(lk.x) < 0.3f) X.snapLatch = 0;
    }
    if (!inGame) {
        /* menu: laserpointer; trekker = tikken */
        float ux, uy;
        int hit = ui_hit(&ux, &uy), trig = axes[5] > 0.55f;
        axes[5] = 0;
        if (hit) fe_vr_pointer(1, ux, uy); else fe_vr_pointer(0, 0, 0);
        if (hit && trig && !X.trigWas) fe_touch(-77, TOUCH_BEGIN, ux, uy);
        else if (hit && trig && X.trigWas) fe_touch(-77, TOUCH_MOVE, ux, uy);
        else if (!trig && X.trigWas) fe_touch(-77, TOUCH_END, ux, uy);
        X.trigWas = trig;
    } else {
        if (X.trigWas) { fe_touch(-77, TOUCH_END, 0, 0); X.trigWas = 0; }
        fe_vr_pointer(0, 0, 0);
    }
    fe_pad_input(1, axes, btn, "Meta Quest Touch");
    /* first person te voet: lopen waar je naar kijkt, richten met de rechterhand */
    g->vrAim = 0;
    if (inGame && X.firstPerson && g->p.veh < 0) {
        M4 hw;
        rig_update(g);
        hw = m4_mul(X.rig, pose_mat(head));
        g->camYaw = yaw_of(mat_fwd(hw));
        if (X.aimOk[1]) {
            M4 aw = m4_mul(X.rig, pose_mat(X.aimLocal[1]));
            g->vrAim = 1;
            g->vrAimYaw = yaw_of(mat_fwd(aw));
            g->vrMuzzle = mat_pos(aw);
        }
    }
    if (g->statShots != X.lastShots) { if (g->statShots > X.lastShots) haptic(1, 0.6f, 0.05f); X.lastShots = g->statShots; }
}

static void frame(void) {
    XrFrameWaitInfo fw = {XR_TYPE_FRAME_WAIT_INFO};
    XrFrameBeginInfo fb = {XR_TYPE_FRAME_BEGIN_INFO};
    XrFrameEndInfo fe = {XR_TYPE_FRAME_END_INFO};
    XrViewLocateInfo vl = {XR_TYPE_VIEW_LOCATE_INFO};
    XrViewState vs = {XR_TYPE_VIEW_STATE};
    XrCompositionLayerProjection lp = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    XrCompositionLayerProjectionView pv[2];
    XrCompositionLayerQuad lq = {XR_TYPE_COMPOSITION_LAYER_QUAD};
    const XrCompositionLayerBaseHeader *layers[2];
    XrSpaceLocation hl = {XR_TYPE_SPACE_LOCATION};
    Renderer *r = (Renderer *)fe_renderer();
    Game *g = (Game *)fe_game();
    uint32_t nv = 0;
    int e, nl = 0, inGame;
    X.fs.type = XR_TYPE_FRAME_STATE;
    if (XR_FAILED(xrWaitFrame(X.sess, &fw, &X.fs))) return;
    xrBeginFrame(X.sess, &fb);
    vl.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    vl.displayTime = X.fs.predictedDisplayTime;
    vl.space = X.local;
    X.views[0].type = X.views[1].type = XR_TYPE_VIEW;
    xrLocateViews(X.sess, &vl, &vs, 2, &nv, X.views);
    xrLocateSpace(X.view, X.local, X.fs.predictedDisplayTime, &hl);
    if (!(hl.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) hl.pose = ident_pose();

    inGame = fe_in_game();
    if (!inGame && !X.uiInMenu) ui_place(hl.pose);   /* menu komt net open: scherm voor je zetten */
    X.uiInMenu = !inGame;
    do_input(g, hl.pose);

    if (X.fs.shouldRender) {
        for (e = 0; e < 2; e++) chain_acquire(&X.eye[e]);
        chain_acquire(&X.ui);
        glDisable(GL_FRAMEBUFFER_SRGB_EXT);   /* de shaders geven al sRGB-kleuren */
        r->vr.active = 1;
        r->vr.drew = 0;
        r->vr.begin = vr_begin; r->vr.bind_eye = vr_bind_eye; r->vr.bind_ui = vr_bind_ui; r->vr.u = r;
        vr_bind_ui(NULL);
        fe_frame(plat_time());
        if (!r->vr.drew) {   /* geen wereld (menu, laden): donkere ruimte */
            for (e = 0; e < 2; e++) {
                glBindFramebuffer(GL_FRAMEBUFFER, X.eye[e].fbo[X.eye[e].cur]);
                glViewport(0, 0, X.eye[e].w, X.eye[e].h);
                glClearColor(0.05f, 0.06f, 0.1f, 1);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        for (e = 0; e < 2; e++) chain_release(&X.eye[e]);
        chain_release(&X.ui);

        for (e = 0; e < 2; e++) {
            memset(&pv[e], 0, sizeof(pv[e]));
            pv[e].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
            pv[e].pose = X.views[e].pose;
            pv[e].fov = X.views[e].fov;
            pv[e].subImage.swapchain = X.eye[e].sc;
            pv[e].subImage.imageRect.extent.width = X.eye[e].w;
            pv[e].subImage.imageRect.extent.height = X.eye[e].h;
        }
        lp.space = X.local;
        lp.viewCount = 2;
        lp.views = pv;
        layers[nl++] = (const XrCompositionLayerBaseHeader *)&lp;
        lq.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        lq.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
        lq.subImage.swapchain = X.ui.sc;
        lq.subImage.imageRect.extent.width = X.ui.w;
        lq.subImage.imageRect.extent.height = X.ui.h;
        if (inGame) {   /* HUD: voor je ogen */
            lq.space = X.view;
            lq.pose = ident_pose();
            lq.pose.position.y = -0.05f;
            lq.pose.position.z = -1.6f;
            lq.size.width = 1.7f; lq.size.height = 1.7f * UI_H / UI_W;
        } else {
            lq.space = X.local;
            lq.pose = X.uiPose;
            lq.size.width = UI_MW; lq.size.height = UI_MW * UI_H / UI_W;
        }
        layers[nl++] = (const XrCompositionLayerBaseHeader *)&lq;
    } else {
        fe_frame(plat_time());   /* spel loopt door (bijv. even niet zichtbaar) */
    }
    fe.displayTime = X.fs.predictedDisplayTime;
    fe.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    fe.layerCount = X.fs.shouldRender ? (uint32_t)nl : 0;
    fe.layers = layers;
    xrEndFrame(X.sess, &fe);
}

static void poll_xr(void) {
    XrEventDataBuffer ev = {XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent(X.inst, &ev) == XR_SUCCESS) {
        if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            XrEventDataSessionStateChanged *sc = (XrEventDataSessionStateChanged *)&ev;
            X.state = sc->state;
            if (X.state == XR_SESSION_STATE_READY) {
                XrSessionBeginInfo bi = {XR_TYPE_SESSION_BEGIN_INFO};
                bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                if (XR_SUCCEEDED(xrBeginSession(X.sess, &bi))) X.running = 1;
            } else if (X.state == XR_SESSION_STATE_STOPPING) {
                xrEndSession(X.sess);
                X.running = 0;
                fe_background();
            } else if (X.state == XR_SESSION_STATE_EXITING || X.state == XR_SESSION_STATE_LOSS_PENDING) {
                X.running = 0;
                ANativeActivity_finish(g_app->activity);
            }
        }
        ev.type = XR_TYPE_EVENT_DATA_BUFFER;
    }
}

static void on_cmd(struct android_app *app, int32_t cmd) {
    (void)app;
    if (cmd == APP_CMD_RESUME) X.resumed = 1;
    else if (cmd == APP_CMD_PAUSE) { X.resumed = 0; fe_background(); }
    else if (cmd == APP_CMD_DESTROY) fe_background();
}

void android_main(struct android_app *app) {
    int e;
    g_app = app;
    app->onAppCmd = on_cmd;
    snprintf(BASE, sizeof(BASE), "%s", app->activity->internalDataPath ? app->activity->internalDataPath : "/sdcard");
    mkdir(BASE, 0755);
    settings_load();
    java_init();
    egl_init();
    if (!xr_init()) { LOGI("OpenXR werkt niet - stoppen"); ANativeActivity_finish(app->activity); }
    else {
        int ew = (int)(X.vcv[0].recommendedImageRectWidth * 0.9f), eh = (int)(X.vcv[0].recommendedImageRectHeight * 0.9f);
        for (e = 0; e < 2; e++) chain_make(&X.eye[e], ew, eh, 1, X.msaa);
        chain_make(&X.ui, UI_W, UI_H, 0, 0);
        input_init();
        X.firstPerson = plat_load_int("vr_fp", 0);
        X.lastVeh = -2;
        X.uiPose = ident_pose(); X.uiPose.position.z = -UI_DIST;
        glBindFramebuffer(GL_FRAMEBUFFER, X.ui.fbo[0]);
        fe_init(UI_W, UI_H);
        fe_resize_viewport(UI_W, UI_H);
    }
    while (!app->destroyRequested) {
        int events;
        struct android_poll_source *src;
        while (ALooper_pollOnce(X.running || X.resumed ? 0 : 100, NULL, &events, (void **)&src) >= 0) {
            if (src) src->process(app, src);
            if (app->destroyRequested) break;
        }
        if (!X.inst || app->destroyRequested) continue;
        poll_xr();
        if (!X.running) continue;
        frame();
    }
    fe_background();
}
