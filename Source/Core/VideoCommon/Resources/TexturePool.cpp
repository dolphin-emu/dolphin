// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Resources/TexturePool.h"

#include "Common/Logging/Log.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/AbstractTexture.h"

namespace VideoCommon
{
TexturePool::TexturePool() = default;
TexturePool::~TexturePool() = default;

void TexturePool::Reset()
{
  m_cache.clear();
}

std::optional<std::unique_ptr<AbstractTexture>>
TexturePool::AllocateTexture(const TextureConfig& config)
{
  Cache::iterator iter = FindMatchingTexture(config);
  if (iter != m_cache.end())
  {
    auto entry = std::move(iter->second);
    m_cache.erase(iter);
    return std::move(entry);
  }

  std::unique_ptr<AbstractTexture> texture = g_gfx->CreateTexture(config);
  if (!texture)
  {
    WARN_LOG_FMT(VIDEO, "Failed to allocate a {}x{}x{} texture", config.width, config.height,
                 config.layers);
    return {};
  }
  return texture;
}

TexturePool::Cache::iterator TexturePool::FindMatchingTexture(const TextureConfig& config)
{
  // Find a texture from the pool that does not have a frameCount of FRAMECOUNT_INVALID.
  // This prevents a texture from being used twice in a single frame with different data,
  // which potentially means that a driver has to maintain two copies of the texture anyway.
  // Render-target textures are fine through, as they have to be generated in a seperated pass.
  // As non-render-target textures are usually static, this should not matter much.
  auto range = m_cache.equal_range(config);
  auto matching_iter = std::find_if(range.first, range.second,
                                    [](const auto& iter) { return iter.first.IsRenderTarget(); });
  return matching_iter != range.second ? matching_iter : m_cache.end();
}

void TexturePool::ReleaseTexture(std::unique_ptr<AbstractTexture> texture)
{
  auto config = texture->GetConfig();
  m_cache.emplace(config, std::move(texture));
}
}  // namespace VideoCommon
