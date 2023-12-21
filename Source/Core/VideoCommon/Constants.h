// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

namespace VideoCommon
{
#ifdef ANDROID
// Some devices seem to have graphical errors when providing 16 pixel samplers
// given the logic is for a performance heavy feature (custom shaders), will just disable for now
// TODO: handle this more elegantly
constexpr u32 MAX_PIXEL_SHADER_SAMPLERS = 8;
#else
constexpr u32 MAX_PIXEL_SHADER_SAMPLERS = 16;
#endif
constexpr u32 MAX_COMPUTE_SHADER_SAMPLERS = 8;
}  // namespace VideoCommon
