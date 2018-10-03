// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/GL/GLExtensions/GLExtensions.h"

class GLContext;

namespace GLUtil
{
GLuint CompileProgram(const std::string& vertexShader, const std::string& fragmentShader);
void EnablePrimitiveRestart(const GLContext* context);
}  // namespace GLUtil
