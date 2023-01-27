// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <optional>
#include <string_view>

#include "Common/CommonTypes.h"
#include "Common/Matrix.h"
#include "VideoCommon/GraphicsModSystem/Runtime/CustomTextureData.h"
#include "VideoCommon/PixelShaderGen.h"

namespace GraphicsModActionData
{
struct DrawStarted
{
  u32 texture_unit;
  bool* skip;
  std::optional<CustomPixelShader>* custom_pixel_shader;
  u32* sampler_index;
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
  u32 texture_width;
  u32 texture_height;
  std::vector<VideoCommon::CustomTextureData*>* custom_textures;
};
}  // namespace GraphicsModActionData
