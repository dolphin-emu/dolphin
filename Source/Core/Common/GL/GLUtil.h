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

// Creates and deletes a VAO and VBO suitable for attributeless rendering.
// Called by the Renderer.
void OpenGL_CreateAttributelessVAO();
void OpenGL_DeleteAttributelessVAO();

// Binds the VAO suitable for attributeless rendering.
void OpenGL_BindAttributelessVAO();

// this should be removed in future, but as long as glsl is unstable, we should really read this messages
#if defined(_DEBUG) || defined(DEBUGFAST)
#define DEBUG_GLSL 1
#else
#define DEBUG_GLSL 0
#endif
