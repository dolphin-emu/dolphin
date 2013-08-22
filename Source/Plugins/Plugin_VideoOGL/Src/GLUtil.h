// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _GLINIT_H_
#define _GLINIT_H_

#include "VideoConfig.h"
#include "MathUtil.h"
#include "GLInterface.h"

#ifndef GL_DEPTH24_STENCIL8_EXT // allows FBOs to support stencils
#define GL_DEPTH_STENCIL_EXT 0x84F9
#define GL_UNSIGNED_INT_24_8_EXT 0x84FA
#define GL_DEPTH24_STENCIL8_EXT 0x88F0
#define GL_TEXTURE_STENCIL_SIZE_EXT 0x88F1
#endif

#ifdef USE_GLES
#define TEX2D	GL_TEXTURE_2D
#define PREC	"highp"
#define TEXTYPE "sampler2D"
#define TEXFUNC "texture2D"
#ifdef USE_GLES3
#include "GLFunctions.h"
#define GLAPIENTRY GL_APIENTRY
#define GL_SAMPLES_PASSED GL_ANY_SAMPLES_PASSED
#define GL_READ_ONLY                      0x88B8
#define GL_WRITE_ONLY                     0x88B9
#define GL_READ_WRITE                     0x88BA
#define GL_SRC1_ALPHA 0
#define GL_BGRA GL_RGBA
#define glDrawElementsBaseVertex
#define glDrawRangeElementsBaseVertex
#endif
#else
#define TEX2D	GL_TEXTURE_RECTANGLE_ARB
#define PREC 
#define TEXTYPE "sampler2DRect"
#define TEXFUNC "texture2DRect"
#endif


#ifndef _WIN32

#include <sys/types.h>

#endif
void InitInterface();

// Helpers
GLuint OpenGL_CompileProgram(const char *vertexShader, const char *fragmentShader);

// Error reporting - use the convenient macros.
void OpenGL_ReportARBProgramError();
GLuint OpenGL_ReportGLError(const char *function, const char *file, int line);
bool OpenGL_ReportFBOError(const char *function, const char *file, int line);

#if defined(_DEBUG) || defined(DEBUGFAST)
#define GL_REPORT_ERROR()         OpenGL_ReportGLError(__FUNCTION__, __FILE__, __LINE__)
#define GL_REPORT_ERRORD()        OpenGL_ReportGLError(__FUNCTION__, __FILE__, __LINE__)
#define GL_REPORT_FBO_ERROR()     OpenGL_ReportFBOError(__FUNCTION__, __FILE__, __LINE__)
#define GL_REPORT_PROGRAM_ERROR() OpenGL_ReportARBProgramError()
#else
__forceinline GLenum GL_REPORT_ERROR() { return GL_NO_ERROR; }
#define GL_REPORT_ERRORD() (void)GL_NO_ERROR
#define GL_REPORT_FBO_ERROR() (void)true
#define GL_REPORT_PROGRAM_ERROR() (void)0
#endif

// this should be removed in future, but as long as glsl is unstable, we should really read this messages
#if defined(_DEBUG) || defined(DEBUGFAST) 
#define DEBUG_GLSL 1
#else
#define DEBUG_GLSL 0
#endif

// Isn't defined if we aren't using GLEW 1.6
#ifndef GL_ONE_MINUS_SRC1_ALPHA 
#define GL_ONE_MINUS_SRC1_ALPHA 0x88FB
#endif

#endif  // _GLINIT_H_
