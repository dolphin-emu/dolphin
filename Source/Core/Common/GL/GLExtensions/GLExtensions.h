// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "Common/CommonTypes.h"

class GLContext;

namespace GLExtensions
{
// Initializes the interface
bool Init(GLContext* context);

// Function for checking if the hardware supports an extension
// example: if (GLExtensions::Supports("GL_ARB_multi_map"))
bool Supports(const std::string& name);

// Returns OpenGL version in format 430
u32 Version();
}  // namespace GLExtensions
