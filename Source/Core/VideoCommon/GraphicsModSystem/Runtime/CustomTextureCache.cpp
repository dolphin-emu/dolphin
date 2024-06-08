// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/CustomTextureCache.h"

#include <fmt/format.h>

#include "Common/Logging/Log.h"

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/Assets/CustomAssetLoader.h"
#include "VideoCommon/VideoEvents.h"

namespace VideoCommon
{
CustomTextureCache::CustomTextureCache()
{
  m_frame_event =
      AfterFrameEvent::Register([this](Core::System&) { OnFrameEnd(); }, "CustomTextureCache");
}

CustomTextureCache::~CustomTextureCache() = default;

std::optional<CustomTextureCache::TextureResult> CustomTextureCache::GetTextureAsset(
    CustomAssetLoader& loader, std::shared_ptr<CustomAssetLibrary> library,
    const CustomAssetLibrary::AssetID& asset_id, AbstractTextureType texture_type)
{
  auto asset = loader.LoadGameTexture(asset_id, library);
  if (!asset)
  {
    return std::nullopt;
  }

  const auto [iter, inserted] = m_cached_texture_assets.try_emplace(asset_id, CachedTextureAsset{});
  if (inserted ||
      iter->second.cached_asset.m_asset->GetLastLoadedTime() >
          iter->second.cached_asset.m_cached_write_time ||
      asset_id != iter->second.cached_asset.m_asset->GetAssetId() ||
      iter->second.texture->GetConfig().type != texture_type)
  {
    const auto loaded_time = asset->GetLastLoadedTime();
    iter->second.cached_asset =
        VideoCommon::CachedAsset<VideoCommon::GameTextureAsset>{std::move(asset), loaded_time};
    ReleaseToPool(&iter->second);
  }

  const auto texture_data = iter->second.cached_asset.m_asset->GetData();
  if (!texture_data)
  {
    return std::nullopt;
  }

  if (iter->second.texture)
  {
    iter->second.time = std::chrono::steady_clock::now();
    return TextureResult{iter->second.texture.get(), iter->second.framebuffer.get(), texture_data};
  }

  auto& first_slice = texture_data->m_texture.m_slices[0];
  const TextureConfig texture_config(first_slice.m_levels[0].width, first_slice.m_levels[0].height,
                                     static_cast<u32>(first_slice.m_levels.size()),
                                     static_cast<u32>(texture_data->m_texture.m_slices.size()), 1,
                                     first_slice.m_levels[0].format, 0, texture_type);

  auto new_texture = AllocateTexture(texture_config);
  if (!new_texture)
  {
    ERROR_LOG_FMT(VIDEO, "Custom texture creation failed due to texture allocation failure");
    return std::nullopt;
  }

  iter->second.texture.swap(new_texture->texture);
  iter->second.framebuffer.swap(new_texture->framebuffer);
  for (std::size_t slice_index = 0; slice_index < texture_data->m_texture.m_slices.size();
       slice_index++)
  {
    auto& slice = texture_data->m_texture.m_slices[slice_index];
    for (u32 level_index = 0; level_index < static_cast<u32>(slice.m_levels.size()); ++level_index)
    {
      auto& level = slice.m_levels[level_index];
      iter->second.texture->Load(level_index, level.width, level.height, level.row_length,
                                 level.data.data(), level.data.size(),
                                 static_cast<u32>(slice_index));
    }
  }

  iter->second.time = std::chrono::steady_clock::now();
  return TextureResult{iter->second.texture.get(), iter->second.framebuffer.get(), texture_data};
}

CustomTextureCache::TexPoolEntry::TexPoolEntry(std::unique_ptr<AbstractTexture> tex,
                                               std::unique_ptr<AbstractFramebuffer> fb)
    : texture(std::move(tex)), framebuffer(std::move(fb)), time(std::chrono::steady_clock::now())
{
}

std::optional<CustomTextureCache::TexPoolEntry>
CustomTextureCache::AllocateTexture(const TextureConfig& config)
{
  TexPool::iterator iter = FindMatchingTextureFromPool(config);
  if (iter != m_texture_pool.end())
  {
    auto entry = std::move(iter->second);
    m_texture_pool.erase(iter);
    return std::move(entry);
  }

  std::unique_ptr<AbstractTexture> texture = g_gfx->CreateTexture(config);
  if (!texture)
  {
    WARN_LOG_FMT(VIDEO, "Failed to allocate a {}x{}x{} texture", config.width, config.height,
                 config.layers);
    return {};
  }

  std::unique_ptr<AbstractFramebuffer> framebuffer;
  if (config.IsRenderTarget())
  {
    framebuffer = g_gfx->CreateFramebuffer(texture.get(), nullptr);
    if (!framebuffer)
    {
      WARN_LOG_FMT(VIDEO, "Failed to allocate a {}x{}x{} framebuffer", config.width, config.height,
                   config.layers);
      return {};
    }
  }

  return TexPoolEntry(std::move(texture), std::move(framebuffer));
}

CustomTextureCache::TexPool::iterator
CustomTextureCache::FindMatchingTextureFromPool(const TextureConfig& config)
{
  // Find a texture from the pool that does not have a frameCount of FRAMECOUNT_INVALID.
  // This prevents a texture from being used twice in a single frame with different data,
  // which potentially means that a driver has to maintain two copies of the texture anyway.
  // Render-target textures are fine through, as they have to be generated in a seperated pass.
  // As non-render-target textures are usually static, this should not matter much.
  auto range = m_texture_pool.equal_range(config);
  auto matching_iter = std::find_if(range.first, range.second,
                                    [](const auto& iter) { return iter.first.IsRenderTarget(); });
  return matching_iter != range.second ? matching_iter : m_texture_pool.end();
}

void CustomTextureCache::ReleaseToPool(CachedTextureAsset* entry)
{
  if (!entry->texture)
    return;
  auto config = entry->texture->GetConfig();
  m_texture_pool.emplace(config,
                         TexPoolEntry(std::move(entry->texture), std::move(entry->framebuffer)));
  entry->texture = nullptr;
  entry->framebuffer = nullptr;
}

void CustomTextureCache::OnFrameEnd()
{
  // Iterate over outstanding textures
  {
    auto iter = m_cached_texture_assets.begin();
    const auto end = m_cached_texture_assets.end();
    while (iter != end)
    {
      if ((std::chrono::steady_clock::now() - iter->second.time) > std::chrono::milliseconds{500})
      {
        ReleaseToPool(&iter->second);
        iter = m_cached_texture_assets.erase(iter);
      }
      else
      {
        ++iter;
      }
    }
  }

  // Iterate over pool
  {
    auto iter = m_texture_pool.begin();
    const auto end = m_texture_pool.end();
    while (iter != end)
    {
      if ((std::chrono::steady_clock::now() - iter->second.time) > std::chrono::milliseconds{250})
      {
        iter = m_texture_pool.erase(iter);
      }
      else
      {
        ++iter;
      }
    }
  }
}
}  // namespace VideoCommon
