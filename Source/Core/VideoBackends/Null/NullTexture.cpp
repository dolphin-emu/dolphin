// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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
void NullTexture::ResolveFromTexture(const AbstractTexture* src,
                                     const MathUtil::Rectangle<int>& rect, u32 layer, u32 level)
{
}

void NullTexture::Load(u32 level, u32 width, u32 height, u32 row_length, const u8* buffer,
                       size_t buffer_size, u32 layer)
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

NullFramebuffer::NullFramebuffer(AbstractTexture* color_attachment,
                                 AbstractTexture* depth_attachment,
                                 std::vector<AbstractTexture*> additional_color_attachments,
                                 AbstractTextureFormat color_format,
                                 AbstractTextureFormat depth_format, u32 width, u32 height,
                                 u32 layers, u32 samples)
    : AbstractFramebuffer(color_attachment, depth_attachment,
                          std::move(additional_color_attachments), color_format, depth_format,
                          width, height, layers, samples)
{
}

std::unique_ptr<NullFramebuffer>
NullFramebuffer::Create(NullTexture* color_attachment, NullTexture* depth_attachment,
                        std::vector<AbstractTexture*> additional_color_attachments)
{
  if (!ValidateConfig(color_attachment, depth_attachment, additional_color_attachments))
    return nullptr;

  const AbstractTextureFormat color_format =
      color_attachment ? color_attachment->GetFormat() : AbstractTextureFormat::Undefined;
  const AbstractTextureFormat depth_format =
      depth_attachment ? depth_attachment->GetFormat() : AbstractTextureFormat::Undefined;
  const NullTexture* either_attachment = color_attachment ? color_attachment : depth_attachment;
  const u32 width = either_attachment->GetWidth();
  const u32 height = either_attachment->GetHeight();
  const u32 layers = either_attachment->GetLayers();
  const u32 samples = either_attachment->GetSamples();

  return std::make_unique<NullFramebuffer>(color_attachment, depth_attachment,
                                           std::move(additional_color_attachments), color_format,
                                           depth_format, width, height, layers, samples);
}

}  // namespace Null
