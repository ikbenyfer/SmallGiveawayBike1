/* Alles wat de speler ziet: logo-intro, laadscherm, menu, instellingen, HUD, touch-besturing. */
#ifndef SGB_FRONTEND_H
#define SGB_FRONTEND_H

enum { TOUCH_BEGIN = 0, TOUCH_MOVE = 1, TOUCH_END = 2 };

void fe_init(int w, int h);
void fe_frame(double now);
void fe_touch(long id, int phase, float x, float y);
void fe_background(void);   /* app gaat naar achtergrond: opslaan, pauzeren */
int fe_is_playing(void);

/* desktop (Windows): toetsenbord en muis */
enum { FE_CMD_BACK = 1, FE_CMD_MAP = 2 };
struct InputS;
void fe_set_desktop(int on);
void fe_key_input(const void *keys);        /* const Input * uit game.h */
void fe_mouse_look(float dx, float dy);
void fe_mouse_wheel(float dy);
int fe_wants_mouse(void);
int fe_text_wanted(void);
void fe_text_char(int c);
void fe_key_command(int cmd);
void fe_resize_viewport(int w, int h);

#endif
