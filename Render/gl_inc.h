#ifndef SGB_GL_INC_H
#define SGB_GL_INC_H
#ifdef __APPLE__
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
#elif defined(_WIN32)
#include "../Win/glwin.h"
#elif defined(SGB_LINUX_GL)
/* Linux-desktopversie: gewone OpenGL 2.1 (libGL), zelfde functienamen als ES 2.0 */
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#define SGB_DESKTOP_GL 1
#else
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif
#endif
