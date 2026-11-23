#include "glwin.h"
#include <stdio.h>

#define SGB_GL_DEF(t, n) t sgb_##n;
SGB_GL_EXT(SGB_GL_DEF)
SGB_GL_OPT(SGB_GL_DEF)
#undef SGB_GL_DEF

int sgb_gl_load(void *(*getproc)(const char *)) {
    int ok = 1;
#define SGB_GL_LOAD(t, n) sgb_##n = (t)getproc(#n); if (!sgb_##n) { fprintf(stderr, "GL-functie ontbreekt: %s\n", #n); ok = 0; }
    SGB_GL_EXT(SGB_GL_LOAD)
#undef SGB_GL_LOAD
#define SGB_GL_OPTLOAD(t, n) sgb_##n = (t)getproc(#n); if (!sgb_##n) sgb_##n = (t)getproc(#n "EXT");
    SGB_GL_OPT(SGB_GL_OPTLOAD)
#undef SGB_GL_OPTLOAD
    return ok;
}
