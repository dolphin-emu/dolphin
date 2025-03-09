// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <optional>
#include <unordered_map>

#include "Common/CommonTypes.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"
#include "VideoCommon/Assets/CustomTextureData.h"
#include "VideoCommon/TextureConfig.h"

class AbstractTexture;

namespace VideoCommon
{
class CustomTextureCache2
{
public:
  CustomTextureCache2();
  ~CustomTextureCache2();

  void Reset();

  std::optional<AbstractTexture*>
  GetTextureFromData(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                     const CustomTextureData& data, AbstractTextureType texture_type);
  std::optional<AbstractTexture*>
  GetTextureFromConfig(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                       const TextureConfig& config);
  void ReleaseToPool(const VideoCommon::CustomAssetLibrary::AssetID& asset_id);

private:
  using TexPool = std::unordered_multimap<TextureConfig, std::unique_ptr<AbstractTexture>>;

  std::optional<std::unique_ptr<AbstractTexture>> AllocateTexture(const TextureConfig& config);
  TexPool::iterator FindMatchingTextureFromPool(const TextureConfig& config);

  TexPool m_texture_pool;
  std::unordered_map<VideoCommon::CustomAssetLibrary::AssetID, std::unique_ptr<AbstractTexture>>
      m_cached_textures;
};
}  // namespace VideoCommon
