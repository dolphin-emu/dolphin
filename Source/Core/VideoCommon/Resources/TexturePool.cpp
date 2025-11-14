// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Resources/TexturePool.h"

#include "Common/Logging/Log.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/AbstractTexture.h"

namespace VideoCommon
{
void TexturePool::Reset()
{
  m_cache.clear();
}

std::unique_ptr<AbstractTexture> TexturePool::AllocateTexture(const TextureConfig& config)
{
  const auto iter = m_cache.find(config);
  if (iter != m_cache.end())
  {
    auto entry = std::move(iter->second);
    m_cache.erase(iter);
    return entry;
  }

  std::unique_ptr<AbstractTexture> texture = g_gfx->CreateTexture(config);
  if (!texture)
  {
    ERROR_LOG_FMT(VIDEO, "Failed to allocate a {}x{}x{} texture", config.width, config.height,
                  config.layers);
    return {};
  }
  return texture;
}

void TexturePool::ReleaseTexture(std::unique_ptr<AbstractTexture> texture)
{
  auto config = texture->GetConfig();
  (void)m_cache.emplace(config, std::move(texture));
}
}  // namespace VideoCommon
