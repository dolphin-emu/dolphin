// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>
#include <memory>
#include <unordered_map>

#include "Common/CommonTypes.h"
#include "Common/HookableEvent.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"
#include "VideoCommon/Assets/TextureAsset.h"
#include "VideoCommon/TextureConfig.h"

class AbstractFramebuffer;
class AbstractTexture;

namespace VideoCommon
{
class CustomAssetLoader;
class CustomTextureCache
{
public:
  CustomTextureCache();
  ~CustomTextureCache();

  struct TextureResult
  {
    AbstractTexture* texture;
    AbstractFramebuffer* framebuffer;
    std::shared_ptr<VideoCommon::TextureData> data;
  };
  std::optional<TextureResult> GetTextureAsset(CustomAssetLoader& loader,
                                               std::shared_ptr<CustomAssetLibrary> library,
                                               const CustomAssetLibrary::AssetID& asset_id,
                                               AbstractTextureType texture_type);

private:
  struct TexPoolEntry
  {
    std::unique_ptr<AbstractTexture> texture;
    std::unique_ptr<AbstractFramebuffer> framebuffer;
    std::chrono::steady_clock::time_point time;

    TexPoolEntry(std::unique_ptr<AbstractTexture> tex, std::unique_ptr<AbstractFramebuffer> fb);
  };

  using TexPool = std::unordered_multimap<TextureConfig, TexPoolEntry>;

  std::optional<TexPoolEntry> AllocateTexture(const TextureConfig& config);
  TexPool::iterator FindMatchingTextureFromPool(const TextureConfig& config);

  TexPool m_texture_pool;

  struct CachedTextureAsset
  {
    VideoCommon::CachedAsset<VideoCommon::GameTextureAsset> cached_asset;
    std::unique_ptr<AbstractTexture> texture;
    std::unique_ptr<AbstractFramebuffer> framebuffer;
    std::chrono::steady_clock::time_point time;
  };

  void ReleaseToPool(CachedTextureAsset* entry);

  std::unordered_map<VideoCommon::CustomAssetLibrary::AssetID, CachedTextureAsset>
      m_cached_texture_assets;

  void OnFrameEnd();
  Common::EventHook m_frame_event;
};
}  // namespace VideoCommon
