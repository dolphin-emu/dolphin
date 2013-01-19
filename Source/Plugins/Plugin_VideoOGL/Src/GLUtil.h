// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

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
#define TEXFUNC "texture"
#else
#define TEX2D	GL_TEXTURE_RECTANGLE_ARB
#define PREC 
#define TEXTYPE "sampler2DRect"
#define TEXFUNC "texture"
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
#define GL_REPORT_ERROR() GL_NO_ERROR
#define GL_REPORT_ERRORD() (void)GL_NO_ERROR
#define GL_REPORT_FBO_ERROR() (void)true
#define GL_REPORT_PROGRAM_ERROR() (void)0
#endif

// Isn't defined if we aren't using GLEW 1.6
#ifndef GL_ONE_MINUS_SRC1_ALPHA 
#define GL_ONE_MINUS_SRC1_ALPHA 0x88FB
#endif

#endif  // _GLINIT_H_
