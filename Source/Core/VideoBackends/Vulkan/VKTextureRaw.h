// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "VideoCommon/AbstractRawTexture.h"

namespace Vulkan
{
class StagingTexture2D;

class VKTextureRaw final : public AbstractRawTexture
{
public:
  VKTextureRaw(const u8* data, u32 stride, u32 width, u32 height,
               std::unique_ptr<StagingTexture2D> texture);
  ~VKTextureRaw();

private:
  std::unique_ptr<StagingTexture2D> m_stagingTexture;
};

}  // namespace Vulkan
