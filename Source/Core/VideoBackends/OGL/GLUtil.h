// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/MathUtil.h"
#include "VideoBackends/OGL/GLExtensions/GLExtensions.h"
#include "VideoCommon/VideoConfig.h"

#ifndef _WIN32

#include <sys/types.h>

#endif
void InitInterface();

// Helpers
GLuint OpenGL_CompileProgram(const char *vertexShader, const char *fragmentShader);

// Error reporting - use the convenient macros.
GLenum OpenGL_ReportGLError(const char *function, const char *file, int line);
bool OpenGL_ReportFBOError(const char *function, const char *file, int line);

#if defined(_DEBUG) || defined(DEBUGFAST)
#define GL_REPORT_ERROR()         OpenGL_ReportGLError(__FUNCTION__, __FILE__, __LINE__)
#define GL_REPORT_ERRORD()        OpenGL_ReportGLError(__FUNCTION__, __FILE__, __LINE__)
#define GL_REPORT_FBO_ERROR()     OpenGL_ReportFBOError(__FUNCTION__, __FILE__, __LINE__)
#else
__forceinline GLenum GL_REPORT_ERROR() { return GL_NO_ERROR; }
#define GL_REPORT_ERRORD() (void)GL_NO_ERROR
#define GL_REPORT_FBO_ERROR() (void)true
#endif

// this should be removed in future, but as long as glsl is unstable, we should really read this messages
#if defined(_DEBUG) || defined(DEBUGFAST)
#define DEBUG_GLSL 1
#else
#define DEBUG_GLSL 0
#endif
