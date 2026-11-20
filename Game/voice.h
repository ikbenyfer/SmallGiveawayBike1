/* v4.5: stemmen (ondertitels + audio) en cutscenes voor de missies.
 * De teksten en de audio komen uit tools/make_voices.py (voice_data.c, Resources/vo_*.mp3).
 * Het spel zet alleen g->voReq; de frontend speelt het bestand af (plat_voice). */
#ifndef SGB_VOICE_H
#define SGB_VOICE_H

#include "game.h"
#include "voice_data.h"

enum { VP_BARK = 0, VP_MISSION = 1, VP_CUT = 2 };

void vo_say(Game *g, int line, int prio);       /* nu afspelen, tenzij er iets belangrijkers loopt */
void vo_queue(Game *g, int line, int prio);     /* na de huidige regel */
void vo_bark(Game *g, int bark, float chance);  /* losse opmerking (met pauze ertussen) */
void vo_stop(Game *g);
int vo_pick(Game *g, int bark);               /* willekeurige regel uit een groep */
void vo_tick(Game *g, float dt);
int vo_speaker_variant(int spk);                /* personage-model voor een spreker */

/* cutscene voor missie m: contact staat voor je en vertelt de opdracht; daarna start de missie */
void cut_start(Game *g, int m);
void cut_update(Game *g, const Input *in, float dt);
void cut_camera(Game *g);
void cut_end(Game *g);

#endif
