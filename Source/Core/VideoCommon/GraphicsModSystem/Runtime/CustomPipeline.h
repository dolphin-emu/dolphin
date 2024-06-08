// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"
#include "VideoCommon/Assets/MaterialAsset.h"
#include "VideoCommon/Assets/ShaderAsset.h"
#include "VideoCommon/Assets/TextureAsset.h"
#include "VideoCommon/GraphicsModSystem/Runtime/CustomTextureCache.h"
#include "VideoCommon/ShaderGenCommon.h"

namespace VideoCommon
{
class CustomAssetLoader;
class CustomTextureCache;
}  // namespace VideoCommon

struct CustomPipeline
{
  void UpdatePixelData(VideoCommon::CustomAssetLoader& loader,
                       std::shared_ptr<VideoCommon::CustomAssetLibrary> library,
                       std::shared_ptr<VideoCommon::CustomTextureCache> texture_cache,
                       std::span<const u32> texture_units,
                       const VideoCommon::CustomAssetLibrary::AssetID& material_to_load);

  VideoCommon::CachedAsset<VideoCommon::MaterialAsset> m_pixel_material;
  VideoCommon::CachedAsset<VideoCommon::PixelShaderAsset> m_pixel_shader;

  struct TextureData
  {
    AbstractTexture* m_texture;
    std::string m_sampler_code;
    std::string m_define_code;
  };
  std::vector<std::optional<TextureData>> m_game_textures;

  ShaderCode m_last_generated_shader_code;
  ShaderCode m_last_generated_material_code;

  std::vector<u8> m_material_data;
};
