/* Wat het platform (iOS of de Linux-testopstelling) moet leveren. */
#ifndef SGB_PLATFORM_H
#define SGB_PLATFORM_H

#include <stdint.h>

typedef struct { int w, h; uint8_t *rgba; } PlatImage;   /* premultiplied RGBA, rij 0 = boven */

typedef struct {
    float u0, v0, u1, v1;  /* textuur */
    float w, h;            /* celgrootte in pixels */
    float xoff, yoff;      /* tekenpositie t.o.v. pen (boven-links) */
    float adv;             /* opschuiven */
} Glyph;

typedef struct {
    PlatImage img;
    Glyph g[96];           /* ASCII 32..127 */
    float size, lineh;
} FontData;

int plat_load_image(const char *name, PlatImage *out);   /* uit de app-bundel */
void plat_free_image(PlatImage *img);
int plat_make_font(FontData *out, float size);
void plat_log(const char *fmt, ...);
double plat_time(void);
int plat_load_int(const char *key, int def);
void plat_save_int(const char *key, int val);
void plat_save_flush(void);
/* testmodus */
int plat_autotest_requested(void);
void plat_autotest_done(void);
void plat_write_text(const char *name, const char *text);   /* in Documents */
const char *plat_key_name(int code);   /* naam van een toets (pc); "" op de iPad */
void plat_screenshot(const char *name, int w, int h);       /* glReadPixels -> Documents/<name>.png */
long plat_memory_kb(void);
/* netwerk (asynchroon): start geeft id > 0; poll: 0 bezig, 1 klaar (data = malloc, aanroeper geeft vrij), -1 fout */
int plat_http_start(const char *url);
int plat_http_poll(int id, char **data, long *len);
/* bestanden in Documents (submappen worden aangemaakt) */
int plat_read_file(const char *name, char **data, long *len);
int plat_write_file(const char *name, const char *data, long len);
void plat_delete_file(const char *name);
int plat_read_resource(const char *name, char **data, long *len);   /* uit de app-bundel */
/* geluid: sfx_<naam>.wav en music.mp3 uit de bundel */
void plat_audio_init(const char *const *sfx, int nsfx, const char *const *loops, int nloops);
void plat_sfx(int id, float vol);
void plat_loop(int id, float vol, float rate);   /* vol 0 = stil */
void plat_music(int on);

#endif
