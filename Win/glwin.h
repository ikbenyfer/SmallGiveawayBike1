/* Windows: desktop-OpenGL (2.1) met dezelfde namen als OpenGL ES 2.0.
 * opengl32.dll levert alleen GL 1.1; de rest wordt bij het opstarten opgehaald (glload.c). */
#ifndef SGB_GLWIN_H
#define SGB_GLWIN_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h>
/* windows.h definieert near/far als lege macro's: weg ermee */
#undef near
#undef far

#define SGB_DESKTOP_GL 1

#define SGB_GL_EXT(X) \
    X(PFNGLACTIVETEXTUREPROC, glActiveTexture) \
    X(PFNGLATTACHSHADERPROC, glAttachShader) \
    X(PFNGLBINDATTRIBLOCATIONPROC, glBindAttribLocation) \
    X(PFNGLBINDBUFFERPROC, glBindBuffer) \
    X(PFNGLBUFFERDATAPROC, glBufferData) \
    X(PFNGLCOMPILESHADERPROC, glCompileShader) \
    X(PFNGLCREATEPROGRAMPROC, glCreateProgram) \
    X(PFNGLCREATESHADERPROC, glCreateShader) \
    X(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray) \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) \
    X(PFNGLGENBUFFERSPROC, glGenBuffers) \
    X(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog) \
    X(PFNGLGETPROGRAMIVPROC, glGetProgramiv) \
    X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) \
    X(PFNGLGETSHADERIVPROC, glGetShaderiv) \
    X(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) \
    X(PFNGLLINKPROGRAMPROC, glLinkProgram) \
    X(PFNGLSHADERSOURCEPROC, glShaderSource) \
    X(PFNGLUNIFORM1FPROC, glUniform1f) \
    X(PFNGLUNIFORM1IPROC, glUniform1i) \
    X(PFNGLUNIFORM2FPROC, glUniform2f) \
    X(PFNGLUNIFORM3FPROC, glUniform3f) \
    X(PFNGLUNIFORM4FPROC, glUniform4f) \
    X(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv) \
    X(PFNGLUSEPROGRAMPROC, glUseProgram) \
    X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer)

#define SGB_GL_DECL(t, n) extern t sgb_##n;
SGB_GL_EXT(SGB_GL_DECL)
#undef SGB_GL_DECL

#define glActiveTexture sgb_glActiveTexture
#define glAttachShader sgb_glAttachShader
#define glBindAttribLocation sgb_glBindAttribLocation
#define glBindBuffer sgb_glBindBuffer
#define glBufferData sgb_glBufferData
#define glCompileShader sgb_glCompileShader
#define glCreateProgram sgb_glCreateProgram
#define glCreateShader sgb_glCreateShader
#define glDisableVertexAttribArray sgb_glDisableVertexAttribArray
#define glEnableVertexAttribArray sgb_glEnableVertexAttribArray
#define glGenBuffers sgb_glGenBuffers
#define glGetProgramInfoLog sgb_glGetProgramInfoLog
#define glGetProgramiv sgb_glGetProgramiv
#define glGetShaderInfoLog sgb_glGetShaderInfoLog
#define glGetShaderiv sgb_glGetShaderiv
#define glGetUniformLocation sgb_glGetUniformLocation
#define glLinkProgram sgb_glLinkProgram
#define glShaderSource sgb_glShaderSource
#define glUniform1f sgb_glUniform1f
#define glUniform1i sgb_glUniform1i
#define glUniform2f sgb_glUniform2f
#define glUniform3f sgb_glUniform3f
#define glUniform4f sgb_glUniform4f
#define glUniformMatrix4fv sgb_glUniformMatrix4fv
#define glUseProgram sgb_glUseProgram
#define glVertexAttribPointer sgb_glVertexAttribPointer

int sgb_gl_load(void *(*getproc)(const char *));
#endif
