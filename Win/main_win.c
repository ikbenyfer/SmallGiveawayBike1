/* Windows-versie: SDL2 (venster, toetsenbord/muis, geluid via SDL_mixer), desktop-OpenGL,
 * WinHTTP (Windows) of libcurl (Linux, bouwen met -DSGB_LINUX_GL) voor de echte NL-kaart. Opslag in %APPDATA%\SmallGiveawayBike1. */
#ifdef _WIN32
#include "glwin.h"
#include <winhttp.h>
#include <psapi.h>
#include <shlobj.h>
#define SEP "\\"
#define strtok_r strtok_s
#else
#define _GNU_SOURCE
#include "../Render/gl_inc.h"
#include <curl/curl.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#define MAX_PATH 4096
#define SEP "/"
#define InterlockedExchange(p, v) __atomic_store_n((p), (v), __ATOMIC_SEQ_CST)
#endif
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../Render/platform.h"
#include "../Render/frontend.h"
#include "../Game/game.h"

static char BASE[MAX_PATH];     /* %APPDATA%\SmallGiveawayBike1 */
static char DATA[MAX_PATH];     /* <exe-map>\data */
static int g_autotest, g_autotestDone;

/* ------------------------------------------------------------------ paden en bestanden */

static void mkdirs(const char *path) {
    char tmp[MAX_PATH];
    size_t i;
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (i = 3; tmp[i]; i++) {
        if (tmp[i] == '\\' || tmp[i] == '/') { char c = tmp[i]; tmp[i] = 0; 
#ifdef _WIN32
            CreateDirectoryA(tmp, NULL);
#else
            mkdir(tmp, 0755);
#endif
            tmp[i] = c; }
    }
}

static int read_all(const char *path, char **data, long *len) {
    FILE *f = fopen(path, "rb");
    long n;
    if (!f) return 0;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    *data = (char *)malloc((size_t)n + 1);
    if (!*data || fread(*data, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(*data); return 0; }
    (*data)[n] = 0;
    *len = n;
    fclose(f);
    return 1;
}

int plat_read_resource(const char *name, char **data, long *len) {
    char p[MAX_PATH];
    snprintf(p, sizeof(p), "%s" SEP "%s", DATA, name);
    return read_all(p, data, len);
}

int plat_read_file(const char *name, char **data, long *len) {
    char p[MAX_PATH];
    snprintf(p, sizeof(p), "%s" SEP "%s", BASE, name);
    return read_all(p, data, len);
}

int plat_write_file(const char *name, const char *data, long len) {
    char p[MAX_PATH];
    FILE *f;
    snprintf(p, sizeof(p), "%s" SEP "%s", BASE, name);
    mkdirs(p);
    f = fopen(p, "wb");
    if (!f) return 0;
    fwrite(data, 1, (size_t)len, f);
    fclose(f);
    return 1;
}

void plat_delete_file(const char *name) {
    char p[MAX_PATH];
    snprintf(p, sizeof(p), "%s" SEP "%s", BASE, name);
    remove(p);
}

void plat_write_text(const char *name, const char *text) { plat_write_file(name, text, (long)strlen(text)); }

int plat_load_image(const char *name, PlatImage *out) {
    char *d = NULL;
    long len = 0;
    char n2[260];
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
    fprintf(stdout, "%s\n", buf);
    fflush(stdout);
    if (BASE[0]) {   /* ook naar %APPDATA%\SmallGiveawayBike1\log.txt (handig bij problemen) */
        static FILE *lf;
        if (!lf) { char p[MAX_PATH]; snprintf(p, sizeof(p), "%s" SEP "log.txt", BASE); lf = fopen(p, "w"); }
        if (lf) { fprintf(lf, "%s\n", buf); fflush(lf); }
    }
}

double plat_time(void) { return (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency(); }

long plat_memory_kb(void) {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) return (long)(pmc.WorkingSetSize / 1024);
    return -1;
#else
    long pages = 0, rss = 0;
    FILE *f = fopen("/proc/self/statm", "r");
    if (!f) return -1;
    if (fscanf(f, "%ld %ld", &pages, &rss) != 2) rss = -1;
    fclose(f);
    return rss < 0 ? -1 : rss * (sysconf(_SC_PAGESIZE) / 1024);
#endif
}

/* ------------------------------------------------------------------ instellingen (settings.txt) */

#define MAXKEYS 64
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
    char buf[MAXKEYS * 48];
    int i, n = 0;
    for (i = 0; i < nkeys; i++) n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s=%d\n", keys[i], vals[i]);
    plat_write_file("settings.txt", buf, n);
}

/* ------------------------------------------------------------------ testmodus en screenshots */

int plat_autotest_requested(void) { return g_autotest && !g_autotestDone; }
void plat_autotest_done(void) { g_autotestDone = 1; }

void plat_screenshot(const char *name, int w, int h) {
    char p[MAX_PATH];
    unsigned char *px = (unsigned char *)malloc((size_t)w * h * 4);
    FILE *f;
    int y, x, rowpad = (4 - (w * 3) % 4) % 4;
    unsigned int filesize = 54 + (unsigned int)((w * 3 + rowpad) * h);
    unsigned char hdr[54] = {'B', 'M'};
    if (!px) return;
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
    snprintf(p, sizeof(p), "%s" SEP "shots" SEP "%s.bmp", BASE, name);
    mkdirs(p);
    f = fopen(p, "wb");
    if (!f) { free(px); return; }
    memcpy(hdr + 2, &filesize, 4);
    hdr[10] = 54; hdr[14] = 40;
    memcpy(hdr + 18, &w, 4); memcpy(hdr + 22, &h, 4);
    hdr[26] = 1; hdr[28] = 24;
    fwrite(hdr, 1, 54, f);
    for (y = 0; y < h; y++) {           /* BMP is van onder naar boven, net als glReadPixels */
        for (x = 0; x < w; x++) {
            unsigned char *q = px + ((size_t)y * w + x) * 4;
            unsigned char bgr[3] = {q[2], q[1], q[0]};
            fwrite(bgr, 1, 3, f);
        }
        if (rowpad) { unsigned char z[3] = {0, 0, 0}; fwrite(z, 1, (size_t)rowpad, f); }
    }
    fclose(f);
    free(px);
}

/* ------------------------------------------------------------------ geluid (SDL_mixer) */

static Mix_Chunk *sfx[64], *loopc[8];
static int nsfxc, nloops;
static Mix_Music *music;
static int audioOk;

void plat_audio_init(const char *const *names, int n, const char *const *loops, int nl) {
    int i;
    char p[MAX_PATH];
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) < 0) { plat_log("geluid uit: %s", Mix_GetError()); return; }
    Mix_Init(MIX_INIT_MP3);
    Mix_AllocateChannels(32);
    Mix_ReserveChannels(nl);
    for (i = 0; i < n && i < 64; i++) { snprintf(p, sizeof(p), "%s" SEP "sfx_%s.wav", DATA, names[i]); sfx[i] = Mix_LoadWAV(p); }
    for (i = 0; i < nl && i < 8; i++) { snprintf(p, sizeof(p), "%s" SEP "sfx_%s.wav", DATA, loops[i]); loopc[i] = Mix_LoadWAV(p); }
    nsfxc = n; nloops = nl;
    snprintf(p, sizeof(p), "%s" SEP "music.mp3", DATA);
    music = Mix_LoadMUS(p);
    if (!music) plat_log("muziek niet geladen: %s", Mix_GetError());
    Mix_VolumeMusic(70);
    audioOk = 1;
}

void plat_sfx(int sid, float vol) {
    int ch;
    if (!audioOk || sid < 0 || sid >= nsfxc || !sfx[sid] || vol <= 0.01f) return;
    ch = Mix_PlayChannel(-1, sfx[sid], 0);
    if (ch >= 0) Mix_Volume(ch, (int)(vol * MIX_MAX_VOLUME));
}

void plat_loop(int sid, float vol, float rate) {
    (void)rate;   /* SDL_mixer kent geen afspeelsnelheid: alleen volume */
    if (!audioOk || sid < 0 || sid >= nloops || !loopc[sid]) return;
    if (vol <= 0.01f) { if (Mix_Playing(sid)) Mix_HaltChannel(sid); return; }
    if (!Mix_Playing(sid)) Mix_PlayChannel(sid, loopc[sid], -1);
    Mix_Volume(sid, (int)(vol * MIX_MAX_VOLUME));
}

void plat_music(int on) {
    if (!audioOk || !music) return;
    if (on) {
        if (!Mix_PlayingMusic()) Mix_PlayMusic(music, -1);
        else if (Mix_PausedMusic()) Mix_ResumeMusic();
    } else if (Mix_PlayingMusic() && !Mix_PausedMusic()) Mix_PauseMusic();
}

/* ------------------------------------------------------------------ netwerk (WinHTTP in een thread per verzoek) */

#define NREQ 256
#ifdef _WIN32
typedef struct { volatile LONG state; char url[1024]; char *data; long len; } Req;   /* state: 1 bezig, 2 klaar, 3 fout */
#else
typedef struct { volatile int state; char url[1024]; char *data; long len; } Req;
#endif
static Req reqs[NREQ];
static int reqNext;

#ifndef _WIN32
typedef struct { char *buf; long len, cap; } CurlBuf;
static size_t curl_write(char *p, size_t sz, size_t n, void *ud) {
    CurlBuf *b = (CurlBuf *)ud;
    long add = (long)(sz * n);
    if (b->len + add + 1 > b->cap) {
        char *nb;
        long ncap = (b->len + add + 1) * 2;
        nb = (char *)realloc(b->buf, (size_t)ncap);
        if (!nb) return 0;
        b->buf = nb; b->cap = ncap;
    }
    memcpy(b->buf + b->len, p, (size_t)add);
    b->len += add;
    return (size_t)add;
}

static void *http_thread(void *arg) {
    Req *r = (Req *)arg;
    CurlBuf b = {NULL, 0, 0};
    long status = 0;
    int ok = 0;
    CURL *c = curl_easy_init();
    if (c) {
        curl_easy_setopt(c, CURLOPT_URL, r->url);
        curl_easy_setopt(c, CURLOPT_USERAGENT, "SmallGiveawayBike1/1.0 (Linux)");
        curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 15L);
        curl_easy_setopt(c, CURLOPT_TIMEOUT, 90L);
        curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_write);
        curl_easy_setopt(c, CURLOPT_WRITEDATA, &b);
        if (curl_easy_perform(c) == CURLE_OK) {
            curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
            ok = status == 200 && b.len > 0;
        }
        curl_easy_cleanup(c);
    }
    if (ok) { b.buf[b.len] = 0; r->data = b.buf; r->len = b.len; InterlockedExchange(&r->state, 2); }
    else { free(b.buf); InterlockedExchange(&r->state, 3); }
    plat_log("NET %s: http %ld, %ld bytes", ok ? "ok" : "fout", status, b.len);
    return NULL;
}
#else
static DWORD WINAPI http_thread(LPVOID arg) {
    Req *r = (Req *)arg;
    wchar_t wurl[1024], host[256], path[1024];
    URL_COMPONENTS uc;
    HINTERNET s = NULL, c = NULL, q = NULL;
    DWORD status = 0, sz = sizeof(status);
    char *buf = NULL;
    long len = 0, cap = 0;
    int ok = 0;
    MultiByteToWideChar(CP_UTF8, 0, r->url, -1, wurl, 1024);
    memset(&uc, 0, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host; uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path; uc.dwUrlPathLength = 1024;
    if (WinHttpCrackUrl(wurl, 0, 0, &uc)) {
        s = WinHttpOpen(L"SmallGiveawayBike1/1.0 (Windows)", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
        if (s) c = WinHttpConnect(s, host, uc.nPort, 0);
        if (c) q = WinHttpOpenRequest(c, L"GET", path, NULL, NULL, NULL, uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
        if (q && WinHttpSendRequest(q, NULL, 0, NULL, 0, 0, 0) && WinHttpReceiveResponse(q, NULL)) {
            WinHttpQueryHeaders(q, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &sz, NULL);
            for (;;) {
                DWORD avail = 0, got = 0;
                if (!WinHttpQueryDataAvailable(q, &avail) || avail == 0) break;
                if (len + (long)avail + 1 > cap) { cap = (len + (long)avail + 1) * 2; buf = (char *)realloc(buf, (size_t)cap); }
                if (!WinHttpReadData(q, buf + len, avail, &got)) break;
                len += (long)got;
            }
            ok = status == 200 && len > 0;
        }
    }
    if (q) WinHttpCloseHandle(q);
    if (c) WinHttpCloseHandle(c);
    if (s) WinHttpCloseHandle(s);
    if (ok) { buf[len] = 0; r->data = buf; r->len = len; InterlockedExchange(&r->state, 2); }
    else { free(buf); InterlockedExchange(&r->state, 3); }
    plat_log("NET %s: http %lu, %ld bytes", ok ? "ok" : "fout", (unsigned long)status, len);
    return 0;
}
#endif

int plat_http_start(const char *url) {
    int i;
    for (i = 0; i < NREQ; i++) {
        int k = (reqNext + i) % NREQ;
        if (reqs[k].state == 0) {
            reqNext = k + 1;
            snprintf(reqs[k].url, sizeof(reqs[k].url), "%s", url);
            reqs[k].data = NULL; reqs[k].len = 0;
            reqs[k].state = 1;
#ifdef _WIN32
            {
                HANDLE h = CreateThread(NULL, 0, http_thread, &reqs[k], 0, NULL);
                if (!h) { reqs[k].state = 0; return -1; }
                CloseHandle(h);
            }
#else
            {
                pthread_t th;
                if (pthread_create(&th, NULL, http_thread, &reqs[k]) != 0) { reqs[k].state = 0; return -1; }
                pthread_detach(th);
            }
#endif
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

/* ------------------------------------------------------------------ hoofdprogramma */

static void *getproc(const char *n) { return SDL_GL_GetProcAddress(n); }

int main(int argc, char **argv) {
    SDL_Window *win;
    SDL_GLContext ctx;
    int i, running = 1, W = 1024, H = 768, fullscreen = 0;
    char exe[MAX_PATH], *slash;
    Input keys;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--autotest") == 0) g_autotest = 1;
        if (strcmp(argv[i], "--mptest-host") == 0) fe_set_mptest(1);
        if (strcmp(argv[i], "--mptest-join") == 0) fe_set_mptest(2);
        if (strcmp(argv[i], "--appdata") == 0 && i + 1 < argc) snprintf(BASE, sizeof(BASE), "%s", argv[++i]);
    }
#ifdef _WIN32
    GetModuleFileNameA(NULL, exe, MAX_PATH);
    slash = strrchr(exe, '\\');
#else
    {
        ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
        if (n > 0) exe[n] = 0; else snprintf(exe, sizeof(exe), "./x");
    }
    slash = strrchr(exe, '/');
    curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
    if (slash) *slash = 0;
    snprintf(DATA, sizeof(DATA), "%s" SEP "data", exe);
    if (!BASE[0]) {
#ifdef _WIN32
        if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, BASE) != S_OK) snprintf(BASE, sizeof(BASE), "%s", exe);
        strncat(BASE, "\\SmallGiveawayBike1", sizeof(BASE) - strlen(BASE) - 1);
#else
        const char *xdg = getenv("XDG_DATA_HOME"), *home = getenv("HOME");
        if (xdg && xdg[0]) snprintf(BASE, sizeof(BASE), "%s/SmallGiveawayBike1", xdg);
        else snprintf(BASE, sizeof(BASE), "%s/.local/share/SmallGiveawayBike1", home ? home : ".");
#endif
    }
    {
        char p[MAX_PATH + 4];
        snprintf(p, sizeof(p), "%s" SEP "x", BASE);
        mkdirs(p);
    }
    settings_load();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Small Giveaway Bike 1", SDL_GetError(), NULL);
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    win = SDL_CreateWindow("Small Giveaway Bike 1", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_OPENGL);
    if (!win) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Small Giveaway Bike 1", SDL_GetError(), NULL); return 1; }
    {
        PlatImage ic;
        memset(&ic, 0, sizeof(ic));
        if (plat_load_image("Icon-152.png", &ic)) {
            SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(ic.rgba, ic.w, ic.h, 32, ic.w * 4, SDL_PIXELFORMAT_ABGR8888);
            if (s) { SDL_SetWindowIcon(win, s); SDL_FreeSurface(s); }
            plat_free_image(&ic);
        }
    }
    ctx = SDL_GL_CreateContext(win);
#ifdef _WIN32
    if (!ctx || !sgb_gl_load(getproc)) {
#else
    (void)getproc;
    if (!ctx) {
#endif
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Small Giveaway Bike 1", "OpenGL 2.1 is nodig. Werk je grafische driver bij.", win);
        return 1;
    }
    SDL_GL_SetSwapInterval(1);
    plat_log("GL: %s / %s", glGetString(GL_RENDERER), glGetString(GL_VERSION));
    fe_set_desktop(1);
    fe_init(W, H);

    while (running) {
        SDL_Event e;
        int mdx = 0, mdy = 0;
        static int edgeJump, edgeAction, edgeSwitch;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT: running = 0; break;
            case SDL_KEYDOWN:
                if (e.key.repeat) break;
                switch (e.key.keysym.sym) {
                case SDLK_SPACE: edgeJump = 1; break;
                case SDLK_e: case SDLK_f: case SDLK_RETURN: edgeAction = 1; break;
                case SDLK_q: edgeSwitch = 1; break;
                case SDLK_ESCAPE: fe_key_command(FE_CMD_BACK); break;
                case SDLK_m: case SDLK_TAB: fe_key_command(FE_CMD_MAP); break;
                case SDLK_BACKSPACE: fe_text_char(8); break;
                case SDLK_F11:
                    fullscreen = !fullscreen;
                    SDL_SetWindowFullscreen(win, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                    break;
                default: break;
                }
                if (e.key.keysym.sym == SDLK_RETURN) fe_text_char(13);
                break;
            case SDL_TEXTINPUT: fe_text_char((unsigned char)e.text.text[0]); break;
            case SDL_MOUSEMOTION:
                if (SDL_GetRelativeMouseMode()) { mdx += e.motion.xrel; mdy += e.motion.yrel; }
                else if (e.motion.state & SDL_BUTTON_LMASK) {
                    int ww, wh; SDL_GetWindowSize(win, &ww, &wh);
                    fe_touch(1, TOUCH_MOVE, e.motion.x * 1024.0f / ww, e.motion.y * 768.0f / wh);
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                if (e.button.button == SDL_BUTTON_LEFT && !SDL_GetRelativeMouseMode()) {
                    int ww, wh; SDL_GetWindowSize(win, &ww, &wh);
                    fe_touch(1, e.type == SDL_MOUSEBUTTONDOWN ? TOUCH_BEGIN : TOUCH_END, e.button.x * 1024.0f / ww, e.button.y * 768.0f / wh);
                }
                break;
            case SDL_MOUSEWHEEL: fe_mouse_wheel((float)e.wheel.y); break;
            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) fe_background();
                break;
            default: break;
            }
        }
        {
            const Uint8 *k = SDL_GetKeyboardState(NULL);
            Uint32 mb = SDL_GetMouseState(NULL, NULL);
            int rel = fe_wants_mouse();
            memset(&keys, 0, sizeof(keys));
            keys.mx = (float)((k[SDL_SCANCODE_D] || k[SDL_SCANCODE_RIGHT]) - (k[SDL_SCANCODE_A] || k[SDL_SCANCODE_LEFT]));
            keys.my = (float)((k[SDL_SCANCODE_W] || k[SDL_SCANCODE_UP]) - (k[SDL_SCANCODE_S] || k[SDL_SCANCODE_DOWN]));
            keys.gas = k[SDL_SCANCODE_W] || k[SDL_SCANCODE_UP];
            keys.brake = k[SDL_SCANCODE_S] || k[SDL_SCANCODE_DOWN];
            keys.run = k[SDL_SCANCODE_LSHIFT] || k[SDL_SCANCODE_RSHIFT];   /* rennen */
            keys.fire = rel && (mb & SDL_BUTTON_LMASK);
            keys.jumpPressed = edgeJump; keys.actionPressed = edgeAction; keys.switchPressed = edgeSwitch;
            edgeJump = edgeAction = edgeSwitch = 0;
            if ((SDL_bool)rel != SDL_GetRelativeMouseMode()) SDL_SetRelativeMouseMode(rel ? SDL_TRUE : SDL_FALSE);
            if (fe_text_wanted()) SDL_StartTextInput(); else SDL_StopTextInput();
            fe_key_input(&keys);
            if (mdx || mdy) fe_mouse_look((float)mdx, (float)mdy);
        }
        {
            int dw, dh;
            SDL_GL_GetDrawableSize(win, &dw, &dh);
            fe_resize_viewport(dw, dh);
        }
        fe_frame(plat_time());
        SDL_GL_SwapWindow(win);
        if (g_autotestDone && g_autotest) { static int quitT; if (++quitT > 90) running = 0; }
        if (fe_should_quit()) running = 0;
    }
    fe_background();
    Mix_CloseAudio();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
