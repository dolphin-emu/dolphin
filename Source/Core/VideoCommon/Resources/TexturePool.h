// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <optional>
#include <unordered_map>

#include "VideoCommon/TextureConfig.h"

class AbstractTexture;

namespace VideoCommon
{
class TexturePool
{
public:
  TexturePool();
  ~TexturePool();

  void Reset();

  std::optional<std::unique_ptr<AbstractTexture>> AllocateTexture(const TextureConfig& config);
  void ReleaseTexture(std::unique_ptr<AbstractTexture> texture);

private:
  using Cache = std::unordered_multimap<TextureConfig, std::unique_ptr<AbstractTexture>>;
  Cache::iterator FindMatchingTexture(const TextureConfig& config);

  Cache m_cache;
};
}  // namespace VideoCommon
