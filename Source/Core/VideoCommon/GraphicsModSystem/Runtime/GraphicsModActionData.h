// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Matrix.h"
#include "Common/SmallVector.h"
#include "VideoCommon/Assets/TextureAsset.h"
#include "VideoCommon/Resources/MaterialResource.h"

namespace GraphicsModActionData
{
struct DrawStarted
{
  const Common::SmallVector<u32, 8>& texture_units;
  bool* skip;
};

struct PreEFB
{
  u32 texture_width;
  u32 texture_height;
  bool* skip;
  u32* scaled_width;
  u32* scaled_height;
};

struct PostEFB
{
  VideoCommon::MaterialResource* material = nullptr;
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
  std::vector<VideoCommon::CachedAsset<VideoCommon::TextureAsset>>* custom_textures;

  // Dependencies needed to reload the texture and trigger this create again
  std::vector<VideoCommon::CachedAsset<VideoCommon::CustomAsset>>* additional_dependencies;
};
}  // namespace GraphicsModActionData
