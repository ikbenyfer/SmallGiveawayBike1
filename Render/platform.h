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
void plat_screenshot(const char *name, int w, int h);       /* glReadPixels -> Documents/<name>.png */
long plat_memory_kb(void);

#endif
