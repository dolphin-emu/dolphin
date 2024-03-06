// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "Common/GL/GLExtensions/GLExtensions.h"

class GLContext;

// Texture which we use to not disturb the other bindings.
constexpr GLenum GL_MUTABLE_TEXTURE_INDEX = GL_TEXTURE10;

namespace GLUtil
{
GLuint CompileProgram(const std::string& vertexShader, const std::string& fragmentShader);
void EnablePrimitiveRestart(const GLContext* context);

void BindFrameBuffer(GLuint buffer);
void ClearViewportRect(int x, int y, int width, int height);
bool CreateFrameBuffer(GLuint& buffer, GLuint color, GLuint depth, GLuint stencil);
void DestroyFrameBuffers(GLuint* buffers, int count);
}  // namespace GLUtil
