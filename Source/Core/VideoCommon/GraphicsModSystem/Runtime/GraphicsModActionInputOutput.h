// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Common/Matrix.h"

namespace GraphicsModActionData
{
namespace Input
{
struct DrawStarted
{
};

struct EFB
{
  u32 texture_width;
  u32 texture_height;
};

struct Projection
{
  Common::Matrix44 matrix;
};
}  // namespace Input
namespace Output
{
struct DrawStarted
{
  bool skip = false;
};

struct EFB
{
  bool skip = false;
  u32 scaled_width = 0;
  u32 scaled_height = 0;
};

struct Projection
{
  Common::Matrix44 matrix = {};
};
}  // namespace Output
}  // namespace GraphicsModActionData
