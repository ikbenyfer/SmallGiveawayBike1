/* Linux-testopstelling: draait de echte game-code (incl. de automatische test) offscreen
 * met Mesa (EGL surfaceless + GLES2) en schrijft screenshots als PPM.
 * Dit is GEEN iPad-test: het controleert alleen dat alles compileert, niet crasht
 * (AddressSanitizer) en er ongeveer goed uitziet. */
#define _POSIX_C_SOURCE 200809L
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>
#include "../Render/platform.h"
#include "../Render/frontend.h"

static const char *ASSETS = "preview_assets";
static const char *OUT = "preview_out";
static int autotest_on = 1;

int plat_load_image(const char *name, PlatImage *out) {
    char p[512];
    FILE *f;
    int wh[2];
    snprintf(p, sizeof(p), "%s/%s.raw", ASSETS, name);
    f = fopen(p, "rb");
    if (!f) return 0;
    if (fread(wh, 4, 2, f) != 2) { fclose(f); return 0; }
    out->w = wh[0]; out->h = wh[1];
    out->rgba = (uint8_t *)malloc((size_t)wh[0] * wh[1] * 4);
    if (fread(out->rgba, 4, (size_t)wh[0] * wh[1], f) != (size_t)wh[0] * wh[1]) { fclose(f); return 0; }
    fclose(f);
    return 1;
}

void plat_free_image(PlatImage *img) { free(img->rgba); img->rgba = NULL; }

int plat_make_font(FontData *out, float size) {
    char p[512];
    FILE *f;
    int i;
    (void)size;
    if (!plat_load_image("font", &out->img)) return 0;
    snprintf(p, sizeof(p), "%s/font.txt", ASSETS);
    f = fopen(p, "r");
    if (!f) return 0;
    if (fscanf(f, "%f %f", &out->size, &out->lineh) != 2) { fclose(f); return 0; }
    for (i = 0; i < 96; i++) {
        Glyph *g = &out->g[i];
        if (fscanf(f, "%f %f %f %f %f %f %f %f %f", &g->u0, &g->v0, &g->u1, &g->v1, &g->w, &g->h, &g->xoff, &g->yoff, &g->adv) != 9) break;
    }
    fclose(f);
    return 1;
}

void plat_log(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    fflush(stdout);
}

double plat_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int plat_load_int(const char *key, int def) { (void)key; return def; }
void plat_save_int(const char *key, int val) { (void)key; (void)val; }
void plat_save_flush(void) {}
int plat_autotest_requested(void) { return autotest_on; }
void plat_autotest_done(void) { autotest_on = 0; }

void plat_write_text(const char *name, const char *text) {
    char p[512];
    FILE *f;
    snprintf(p, sizeof(p), "%s/%s", OUT, name);
    f = fopen(p, "w");
    if (f) { fputs(text, f); fclose(f); }
}

void plat_screenshot(const char *name, int w, int h) {
    char p[512];
    uint8_t *px = (uint8_t *)malloc((size_t)w * h * 4);
    FILE *f;
    int y, x;
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
    snprintf(p, sizeof(p), "%s/%s.ppm", OUT, name);
    f = fopen(p, "wb");
    if (f) {
        fprintf(f, "P6\n%d %d\n255\n", w, h);
        for (y = h - 1; y >= 0; y--)
            for (x = 0; x < w; x++) fwrite(px + ((size_t)y * w + x) * 4, 1, 3, f);
        fclose(f);
    }
    free(px);
}

long plat_memory_kb(void) {
    FILE *f = fopen("/proc/self/statm", "r");
    long a = 0, b = 0;
    if (f) { if (fscanf(f, "%ld %ld", &a, &b) != 2) b = 0; fclose(f); }
    return b * 4;
}

/* netwerk: synchroon via curl (alleen voor de CI-test) */
static struct { char *data; long len; int ok; } NET[64];
static int netn;

int plat_http_start(const char *url) {
    char cmd[2048];
    FILE *p;
    char *buf = NULL;
    long len = 0, cap = 0;
    int id;
    if (netn >= 63) return -1;
    id = ++netn;
    snprintf(cmd, sizeof(cmd), "curl -s -f -m 90 -A SGB-CI '%s'", url);
    p = popen(cmd, "r");
    if (p) {
        for (;;) {
            size_t r;
            if (len + 65536 + 1 > cap) { cap = (cap + 65536) * 2; buf = (char *)realloc(buf, (size_t)cap); }
            r = fread(buf + len, 1, 65536, p);
            if (r == 0) break;
            len += (long)r;
        }
        NET[id].ok = pclose(p) == 0 && len > 0;
    }
    NET[id].data = buf; NET[id].len = len;
    printf("NET %d: %ld bytes, ok=%d\n", id, len, NET[id].ok);
    return id;
}

int plat_http_poll(int id, char **data, long *len) {
    if (id <= 0 || id > netn) return -1;
    if (!NET[id].ok) { free(NET[id].data); NET[id].data = NULL; return -1; }
    *data = NET[id].data; *len = NET[id].len;
    (*data)[*len] = 0;
    NET[id].data = NULL; NET[id].ok = 0;
    return 1;
}

int plat_read_file(const char *name, char **data, long *len) {
    char p[512];
    FILE *f;
    long n;
    snprintf(p, sizeof(p), "%s/docs/%s", OUT, name);
    f = fopen(p, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    *data = (char *)malloc((size_t)n + 1);
    if (fread(*data, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(*data); return 0; }
    (*data)[n] = 0; *len = n;
    fclose(f);
    return 1;
}

int plat_write_file(const char *name, const char *data, long len) {
    char p[512], d[512], *s;
    FILE *f;
    snprintf(p, sizeof(p), "%s/docs/%s", OUT, name);
    snprintf(d, sizeof(d), "%s", p);
    s = strrchr(d, '/');
    if (s) { *s = 0; { char cmd[600]; snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", d); if (system(cmd) != 0) return 0; } }
    f = fopen(p, "wb");
    if (!f) return 0;
    fwrite(data, 1, (size_t)len, f);
    fclose(f);
    return 1;
}

int main(void) {
    const int W = 1024, H = 768;
    EGLDisplay dpy;
    EGLConfig cfg;
    EGLint n = 0, maj, min;
    EGLContext ctx;
    GLuint fbo, color, depth;
    int frame;
    double t = 0;
    mkdir(OUT, 0755);
    {
        PFNEGLGETPLATFORMDISPLAYEXTPROC gpd = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
        EGLint attr[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE};
        int pass;
        n = 0;
        for (pass = 0; pass < 2 && n < 1; pass++) {
            dpy = EGL_NO_DISPLAY;
            if (pass == 0 && gpd) dpy = gpd(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
            if (pass == 1) dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
            if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, &maj, &min)) { printf("EGL poging %d: geen display\n", pass); continue; }
            printf("EGL poging %d: %s\n", pass, eglQueryString(dpy, EGL_VENDOR));
            if (!eglChooseConfig(dpy, attr, &cfg, 1, &n)) n = 0;
            if (n < 1) eglGetConfigs(dpy, &cfg, 1, &n);
        }
        if (n < 1) { printf("geen EGL-config\n"); return 2; }
    }
    eglBindAPI(EGL_OPENGL_ES_API);
    {
        EGLint ca[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
        ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ca);
    }
    if (ctx == EGL_NO_CONTEXT || !eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) { printf("context mislukt\n"); return 2; }
    printf("GL: %s / %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenTextures(1, &color);
    glBindTexture(GL_TEXTURE_2D, color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, W, H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { printf("FBO niet compleet\n"); return 2; }

    fe_init(W, H);
    for (frame = 0; frame < 30 * 200; frame++) {
        t += 1.0 / 30.0;
        fe_frame(t);
        if (frame == 25) plat_screenshot("00a_logo_fadein", W, H);
        if (frame == 80) plat_screenshot("00b_loading", W, H);
        if (!autotest_on) {
            /* nog even het menu tonen na de test */
            int k;
            for (k = 0; k < 20; k++) { t += 1.0 / 30.0; fe_frame(t); }
            plat_screenshot("99_menu_after", W, H);
            break;
        }
        if (glGetError() != GL_NO_ERROR) printf("GL-fout in frame %d\n", frame);
    }
    if (autotest_on) { printf("FOUT: autotest niet afgerond\n"); return 1; }
    printf("klaar na %d frames\n", frame);
    return 0;
}
