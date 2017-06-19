// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Vulkan/VKTextureRaw.h"
#include "VideoBackends/Vulkan/StagingTexture2D.h"

namespace Vulkan
{
VKTextureRaw::VKTextureRaw(const u8* data, u32 stride, u32 width, u32 height,
                           std::unique_ptr<StagingTexture2D> texture)
    : AbstractRawTexture(data, stride, width, height), m_stagingTexture(std::move(texture))
{
}

VKTextureRaw::~VKTextureRaw()
{
  m_stagingTexture->Unmap();
}

}  // namespace Vulkan
