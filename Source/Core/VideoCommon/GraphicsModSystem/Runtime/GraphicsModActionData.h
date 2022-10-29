// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string_view>

#include "Common/CommonTypes.h"
#include "Common/Matrix.h"

namespace GraphicsModActionData
{
struct DrawStarted
{
  bool* skip;
};

struct EFB
{
  u32 texture_width;
  u32 texture_height;
  bool* skip;
  u32* scaled_width;
  u32* scaled_height;
};

struct Projection
{
  Common::Matrix44* matrix;
};
struct TextureLoad
{
  std::string_view texture_name;
};
}  // namespace GraphicsModActionData
