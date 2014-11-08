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

// this should be removed in future, but as long as glsl is unstable, we should really read this messages
#if defined(_DEBUG) || defined(DEBUGFAST)
#define DEBUG_GLSL 1
#else
#define DEBUG_GLSL 0
#endif
