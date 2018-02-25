// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/GL/GLExtensions/GLExtensions.h"

#ifndef _WIN32

#include <sys/types.h>

#endif
void InitInterface();

// Helpers
GLuint OpenGL_CompileProgram(const std::string& vertexShader, const std::string& fragmentShader);
