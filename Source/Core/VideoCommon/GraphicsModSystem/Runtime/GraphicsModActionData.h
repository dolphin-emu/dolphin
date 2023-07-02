// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Matrix.h"
#include "VideoCommon/Assets/TextureAsset.h"

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
struct TextureCreate
{
  std::string_view texture_name;
  u32 texture_width;
  u32 texture_height;
  std::vector<VideoCommon::CachedAsset<VideoCommon::GameTextureAsset>>* custom_textures;

  // Dependencies needed to reload the texture and trigger this create again
  std::vector<VideoCommon::CachedAsset<VideoCommon::CustomAsset>>* additional_dependencies;
};
}  // namespace GraphicsModActionData
