/* Alles wat de speler ziet: logo-intro, laadscherm, menu, instellingen, HUD, touch-besturing. */
#ifndef SGB_FRONTEND_H
#define SGB_FRONTEND_H

enum { TOUCH_BEGIN = 0, TOUCH_MOVE = 1, TOUCH_END = 2 };

void fe_init(int w, int h);
void fe_frame(double now);
void fe_touch(long id, int phase, float x, float y);
void fe_background(void);   /* app gaat naar achtergrond: opslaan, pauzeren */
int fe_is_playing(void);

#endif
