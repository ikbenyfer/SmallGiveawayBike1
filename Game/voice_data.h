/* GEMAAKT DOOR tools/make_voices.py - niet met de hand aanpassen */
#ifndef SGB_VOICE_DATA_H
#define SGB_VOICE_DATA_H

typedef struct { const char *key; int spk; const char *text; float dur; } VoLine;
typedef struct { short intro[6]; short nintro; short step[11]; short alarm, open, pass, fail; short who; } VoMission;
#define VO_SPEAKERS 15
#define VO_LINES 216
#define VO_MISSIONS 22
enum { VB_WANTED, VB_LOSTCOPS, VB_BUSTED, VB_WASTED, VB_HURT, VB_CARIN, VB_BIKEIN, VB_CASH, VB_CRASH, VB_SHOOT, VB_PASS, VB_FAIL, VB_STORE_GUN, VB_STORE_BURGER, VB_STORE_GYM, VB_STORE_DRIP, VB_STORE_BLING, VB_STORE_MART, VB_ROB_YOU, VB_ROB_CLERK, VB_ROB_EMPTY, VB_BUY, VB_HOME, VB_SKIP_NONE, VB_COUNT };
#define VO_BARK_MAX 4
extern const VoLine VO[VO_LINES];
extern const char *const VO_SPK_NAME[VO_SPEAKERS];
extern const VoMission VO_MIS[VO_MISSIONS];
extern const short VO_BARK[VB_COUNT][VO_BARK_MAX];
enum { VS_YOU, VS_GG, VS_ROSA, VS_COACH, VS_RICO, VS_BIGT, VS_NINA, VS_COP, VS_CLERK, VS_GUNNY, VS_BURG, VS_DRIP, VS_BLING, VS_GYM, VS_PED };

#endif
