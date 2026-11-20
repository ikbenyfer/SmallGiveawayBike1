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
void fe_set_viewport_origin(int x, int y);
int fe_ui_width(void);   /* breedte van het UI-coordinatenstelsel (hoogte is altijd 768): 1024 bij 4:3, 1365 bij 16:9 */
void fe_set_mptest(int mode);   /* 1 = host, 2 = meedoen (test met twee vensters) */

/* controllers (alle platformen, Xbox-indeling) en instelbare toetsen */
enum { PB_A = 0, PB_B, PB_X, PB_Y, PB_LB, PB_RB, PB_LT, PB_RT, PB_BACK, PB_START, PB_L3, PB_R3, PB_UP, PB_DOWN, PB_LEFT, PB_RIGHT, PB_COUNT };
enum { PA_JUMP = 0, PA_ACTION, PA_FIRE, PA_SWITCH, PA_RUN, PA_GAS, PA_BRAKE, PA_MAP, PA_PAUSE, PA_COUNT };
enum { KA_FWD = 0, KA_BACK, KA_LEFT, KA_RIGHT, KA_JUMP, KA_ACTION, KA_FIRE, KA_SWITCH, KA_RUN, KA_MAP, KA_COUNT };
/* axes: linker x, linker y (omlaag = +), rechter x, rechter y, LT, RT (0..1); buttons: bits 1 << PB_* */
void fe_pad_input(int connected, const float *axes, unsigned buttons, const char *name);
void fe_set_key_defaults(const int *codes, int n);   /* desktop: standaardtoets per KA_* */
int fe_key_bind(int action);                         /* desktop: huidige toets per KA_* */
void fe_raw_key(int code);                           /* desktop: elke ingedrukte toets (voor instellen) */
int fe_bind_waiting(void);                           /* CONTROLS wacht op een knop/toets */
int fe_should_quit(void);

/* v5.0 VR (Meta Quest) */
void *fe_renderer(void);   /* Renderer * */
void *fe_game(void);       /* Game * */
int fe_in_game(void);      /* 1 = rondlopen/rijden (geen menu) */
void fe_vr_pointer(int on, float x, float y);   /* laserpointer als cursor (UI-coordinaten) */

#endif
