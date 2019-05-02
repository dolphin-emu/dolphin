// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Software/SWTexture.h"
#include "VideoBackends/Software/SWRenderer.h"

#include <cstring>
#include "Common/Assert.h"

#include "VideoBackends/Software/CopyRegion.h"

namespace SW
{
namespace
{
#pragma pack(push, 1)
struct Pixel
{
  u8 r;
  u8 g;
  u8 b;
  u8 a;
};
#pragma pack(pop)

void CopyTextureData(const TextureConfig& src_config, const u8* src_ptr, u32 src_x, u32 src_y,
                     u32 width, u32 height, const TextureConfig& dst_config, u8* dst_ptr, u32 dst_x,
                     u32 dst_y)
{
  size_t texel_size = AbstractTexture::GetTexelSizeForFormat(src_config.format);
  size_t src_stride = src_config.GetStride();
  size_t src_offset =
      static_cast<size_t>(src_y) * src_stride + static_cast<size_t>(src_x) * texel_size;
  size_t dst_stride = dst_config.GetStride();
  size_t dst_offset =
      static_cast<size_t>(dst_y) * dst_stride + static_cast<size_t>(dst_x) * texel_size;
  size_t copy_len = static_cast<size_t>(width) * texel_size;

  src_ptr += src_offset;
  dst_ptr += dst_offset;
  for (u32 i = 0; i < height; i++)
  {
    std::memcpy(dst_ptr, src_ptr, copy_len);
    src_ptr += src_stride;
    dst_ptr += dst_stride;
  }
}
}  // namespace

void SWRenderer::ScaleTexture(AbstractFramebuffer* dst_framebuffer,
                              const MathUtil::Rectangle<int>& dst_rect,
                              const AbstractTexture* src_texture,
                              const MathUtil::Rectangle<int>& src_rect)
{
  const SWTexture* software_source_texture = static_cast<const SWTexture*>(src_texture);
  SWTexture* software_dest_texture = static_cast<SWTexture*>(dst_framebuffer->GetColorAttachment());

  std::vector<Pixel> source_pixels;
  source_pixels.resize(src_rect.GetHeight() * src_rect.GetWidth() * 4);
  memcpy(source_pixels.data(), software_source_texture->GetData(), source_pixels.size());

  std::vector<Pixel> destination_pixels;
  destination_pixels.resize(dst_rect.GetHeight() * dst_rect.GetWidth() * 4);

  CopyRegion(source_pixels.data(), src_rect, destination_pixels.data(), dst_rect);
  memcpy(software_dest_texture->GetData(), destination_pixels.data(), destination_pixels.size());
}

SWTexture::SWTexture(const TextureConfig& tex_config) : AbstractTexture(tex_config)
{
  m_data.resize(tex_config.width * tex_config.height * 4);
}

void SWTexture::CopyRectangleFromTexture(const AbstractTexture* src,
                                         const MathUtil::Rectangle<int>& src_rect, u32 src_layer,
                                         u32 src_level, const MathUtil::Rectangle<int>& dst_rect,
                                         u32 dst_layer, u32 dst_level)
{
  ASSERT(src_level == 0 && src_layer == 0 && dst_layer == 0 && dst_level == 0);
  CopyTextureData(src->GetConfig(), static_cast<const SWTexture*>(src)->m_data.data(),
                  src_rect.left, src_rect.top, src_rect.GetWidth(), src_rect.GetHeight(), m_config,
                  m_data.data(), dst_rect.left, dst_rect.top);
}
void SWTexture::ResolveFromTexture(const AbstractTexture* src, const MathUtil::Rectangle<int>& rect,
                                   u32 layer, u32 level)
{
}

void SWTexture::Load(u32 level, u32 width, u32 height, u32 row_length, const u8* buffer,
                     size_t buffer_size)
{
  m_data.assign(buffer, buffer + buffer_size);
}

const u8* SWTexture::GetData() const
{
  return m_data.data();
}

u8* SWTexture::GetData()
{
  return m_data.data();
}

SWStagingTexture::SWStagingTexture(StagingTextureType type, const TextureConfig& config)
    : AbstractStagingTexture(type, config)
{
  m_data.resize(m_texel_size * config.width * config.height);
  m_map_pointer = reinterpret_cast<char*>(m_data.data());
  m_map_stride = m_texel_size * config.width;
}

SWStagingTexture::~SWStagingTexture() = default;

void SWStagingTexture::CopyFromTexture(const AbstractTexture* src,
                                       const MathUtil::Rectangle<int>& src_rect, u32 src_layer,
                                       u32 src_level, const MathUtil::Rectangle<int>& dst_rect)
{
  ASSERT(src_level == 0 && src_layer == 0);
  CopyTextureData(src->GetConfig(), static_cast<const SWTexture*>(src)->GetData(), src_rect.left,
                  src_rect.top, src_rect.GetWidth(), src_rect.GetHeight(), m_config, m_data.data(),
                  dst_rect.left, dst_rect.top);
  m_needs_flush = true;
}

void SWStagingTexture::CopyToTexture(const MathUtil::Rectangle<int>& src_rect, AbstractTexture* dst,
                                     const MathUtil::Rectangle<int>& dst_rect, u32 dst_layer,
                                     u32 dst_level)
{
  ASSERT(dst_level == 0 && dst_layer == 0);
  CopyTextureData(m_config, m_data.data(), src_rect.left, src_rect.top, src_rect.GetWidth(),
                  src_rect.GetHeight(), dst->GetConfig(), static_cast<SWTexture*>(dst)->GetData(),
                  dst_rect.left, dst_rect.top);
  m_needs_flush = true;
}

bool SWStagingTexture::Map()
{
  return true;
}

void SWStagingTexture::Unmap()
{
}

void SWStagingTexture::Flush()
{
  m_needs_flush = false;
}

SWFramebuffer::SWFramebuffer(AbstractTexture* color_attachment, AbstractTexture* depth_attachment,
                             AbstractTextureFormat color_format, AbstractTextureFormat depth_format,
                             u32 width, u32 height, u32 layers, u32 samples)
    : AbstractFramebuffer(color_attachment, depth_attachment, color_format, depth_format, width,
                          height, layers, samples)
{
}

std::unique_ptr<SWFramebuffer> SWFramebuffer::Create(SWTexture* color_attachment,
                                                     SWTexture* depth_attachment)
{
  if (!ValidateConfig(color_attachment, depth_attachment))
    return nullptr;

  const AbstractTextureFormat color_format =
      color_attachment ? color_attachment->GetFormat() : AbstractTextureFormat::Undefined;
  const AbstractTextureFormat depth_format =
      depth_attachment ? depth_attachment->GetFormat() : AbstractTextureFormat::Undefined;
  const SWTexture* either_attachment = color_attachment ? color_attachment : depth_attachment;
  const u32 width = either_attachment->GetWidth();
  const u32 height = either_attachment->GetHeight();
  const u32 layers = either_attachment->GetLayers();
  const u32 samples = either_attachment->GetSamples();

  return std::make_unique<SWFramebuffer>(color_attachment, depth_attachment, color_format,
                                         depth_format, width, height, layers, samples);
}

}  // namespace SW
