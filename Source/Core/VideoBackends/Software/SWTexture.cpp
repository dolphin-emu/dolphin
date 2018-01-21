// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Software/SWTexture.h"

namespace SW
{
SWTexture::SWTexture(const TextureConfig& tex_config) : AbstractTexture(tex_config)
{
}

void SWTexture::Bind(unsigned int stage)
{
}

void SWTexture::CopyRectangleFromTexture(const AbstractTexture* source,
                                         const MathUtil::Rectangle<int>& srcrect,
                                         const MathUtil::Rectangle<int>& dstrect)
{
}

void SWTexture::Load(u32 level, u32 width, u32 height, u32 row_length, const u8* buffer,
                     size_t buffer_size)
{
}

}  // namespace SW
