// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <unordered_map>

#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/TextureConfig.h"

namespace VideoCommon
{
class TexturePool
{
public:
  void Reset();

  std::unique_ptr<AbstractTexture> AllocateTexture(const TextureConfig& config);
  void ReleaseTexture(std::unique_ptr<AbstractTexture> texture);

private:
  std::unordered_multimap<TextureConfig, std::unique_ptr<AbstractTexture>> m_cache;
};
}  // namespace VideoCommon
