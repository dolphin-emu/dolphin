// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "VideoCommon/RenderState.h"
#include "Common/GL/GLExtensions/GLExtensions.h"

class GLContext;

// Texture which we use to not disturb the other bindings.
constexpr GLenum GL_MUTABLE_TEXTURE_INDEX = GL_TEXTURE10;

namespace GLUtil
{
GLuint CompileProgram(const std::string& vertexShader, const std::string& fragmentShader);

GLenum MapToGLPrimitive(PrimitiveType primitive_type);
}  // namespace GLUtil
