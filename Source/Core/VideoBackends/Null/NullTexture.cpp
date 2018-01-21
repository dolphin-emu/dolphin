// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Null/NullTexture.h"

namespace Null
{
NullTexture::NullTexture(const TextureConfig& tex_config) : AbstractTexture(tex_config)
{
}

void NullTexture::CopyRectangleFromTexture(const AbstractTexture* src,
                                           const MathUtil::Rectangle<int>& src_rect, u32 src_layer,
                                           u32 src_level, const MathUtil::Rectangle<int>& dst_rect,
                                           u32 dst_layer, u32 dst_level)
{
}
void NullTexture::ScaleRectangleFromTexture(const AbstractTexture* source,
                                            const MathUtil::Rectangle<int>& srcrect,
                                            const MathUtil::Rectangle<int>& dstrect)
{
}
void NullTexture::ResolveFromTexture(const AbstractTexture* src,
                                     const MathUtil::Rectangle<int>& rect, u32 layer, u32 level)
{
}

void NullTexture::Load(u32 level, u32 width, u32 height, u32 row_length, const u8* buffer,
                       size_t buffer_size)
{
}

NullStagingTexture::NullStagingTexture(StagingTextureType type, const TextureConfig& config)
    : AbstractStagingTexture(type, config)
{
  m_texture_buf.resize(m_texel_size * config.width * config.height);
  m_map_pointer = reinterpret_cast<char*>(m_texture_buf.data());
  m_map_stride = m_texel_size * config.width;
}

NullStagingTexture::~NullStagingTexture() = default;

void NullStagingTexture::CopyFromTexture(const AbstractTexture* src,
                                         const MathUtil::Rectangle<int>& src_rect, u32 src_layer,
                                         u32 src_level, const MathUtil::Rectangle<int>& dst_rect)
{
  m_needs_flush = true;
}

void NullStagingTexture::CopyToTexture(const MathUtil::Rectangle<int>& src_rect,
                                       AbstractTexture* dst,
                                       const MathUtil::Rectangle<int>& dst_rect, u32 dst_layer,
                                       u32 dst_level)
{
  m_needs_flush = true;
}

bool NullStagingTexture::Map()
{
  return true;
}

void NullStagingTexture::Unmap()
{
}

void NullStagingTexture::Flush()
{
  m_needs_flush = false;
}

}  // namespace Null
