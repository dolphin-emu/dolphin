// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/AbstractStagingTexture.h"

#include <algorithm>
#include <cstring>

#include "Common/Assert.h"
#include "Common/MsgHandler.h"
#include "VideoCommon/AbstractTexture.h"

AbstractStagingTexture::AbstractStagingTexture(const StagingTextureType type, const TextureConfig& c)
    : m_type(type), m_config(c), m_texel_size(AbstractTexture::GetTexelSizeForFormat(c.format))
{
}

AbstractStagingTexture::~AbstractStagingTexture() = default;

void AbstractStagingTexture::CopyFromTexture(const AbstractTexture* src, const u32 src_layer,
                                             const u32 src_level)
{
  const MathUtil::Rectangle<int> src_rect = src->GetConfig().GetMipRect(src_level);
  const MathUtil::Rectangle<int> dst_rect = m_config.GetRect();
  CopyFromTexture(src, src_rect, src_layer, src_level, dst_rect);
}

void AbstractStagingTexture::CopyToTexture(AbstractTexture* dst, const u32 dst_layer, const u32 dst_level)
{
  const MathUtil::Rectangle<int> src_rect = m_config.GetRect();
  const MathUtil::Rectangle<int> dst_rect = dst->GetConfig().GetMipRect(dst_level);
  CopyToTexture(src_rect, dst, dst_rect, dst_layer, dst_level);
}

void AbstractStagingTexture::ReadTexels(const MathUtil::Rectangle<int>& rect, void* out_ptr,
                                        const u32 out_stride)
{
  ASSERT(m_type != StagingTextureType::Upload);
  if (!PrepareForAccess())
    return;

  ASSERT(rect.left >= 0 && static_cast<u32>(rect.right) <= m_config.width && rect.top >= 0 &&
         static_cast<u32>(rect.bottom) <= m_config.height);

  // Offset pointer to point to start of region being copied out.
  const char* current_ptr = m_map_pointer;
  current_ptr += rect.top * m_map_stride;
  current_ptr += rect.left * m_texel_size;

  // Optimal path: same dimensions, same stride.
  if (rect.left == 0 && static_cast<u32>(rect.right) == m_config.width &&
      m_map_stride == out_stride)
  {
    std::memcpy(out_ptr, current_ptr, m_map_stride * rect.GetHeight());
    return;
  }

  const size_t copy_size = std::min(rect.GetWidth() * m_texel_size, m_map_stride);
  const int copy_height = rect.GetHeight();
  auto dst_ptr = static_cast<char*>(out_ptr);
  for (int row = 0; row < copy_height; row++)
  {
    std::memcpy(dst_ptr, current_ptr, copy_size);
    current_ptr += m_map_stride;
    dst_ptr += out_stride;
  }
}

void AbstractStagingTexture::ReadTexel(const u32 x, const u32 y, void* out_ptr)
{
  ASSERT(m_type != StagingTextureType::Upload);
  if (!PrepareForAccess())
    return;

  ASSERT(x < m_config.width && y < m_config.height);
  const char* src_ptr = m_map_pointer + y * m_map_stride + x * m_texel_size;
  std::memcpy(out_ptr, src_ptr, m_texel_size);
}

void AbstractStagingTexture::WriteTexels(const MathUtil::Rectangle<int>& rect, const void* in_ptr,
                                         const u32 in_stride)
{
  ASSERT(m_type != StagingTextureType::Readback);
  if (!PrepareForAccess())
    return;

  ASSERT(rect.left >= 0 && static_cast<u32>(rect.right) <= m_config.width && rect.top >= 0 &&
         static_cast<u32>(rect.bottom) <= m_config.height);

  // Offset pointer to point to start of region being copied to.
  char* current_ptr = m_map_pointer;
  current_ptr += rect.top * m_map_stride;
  current_ptr += rect.left * m_texel_size;

  // Optimal path: same dimensions, same stride.
  if (rect.left == 0 && static_cast<u32>(rect.right) == m_config.width && m_map_stride == in_stride)
  {
    std::memcpy(current_ptr, in_ptr, m_map_stride * rect.GetHeight());
    return;
  }

  const size_t copy_size = std::min(rect.GetWidth() * m_texel_size, m_map_stride);
  const int copy_height = rect.GetHeight();
  auto src_ptr = static_cast<const char*>(in_ptr);
  for (int row = 0; row < copy_height; row++)
  {
    std::memcpy(current_ptr, src_ptr, copy_size);
    current_ptr += m_map_stride;
    src_ptr += in_stride;
  }
}

void AbstractStagingTexture::WriteTexel(const u32 x, const u32 y, const void* in_ptr)
{
  ASSERT(m_type != StagingTextureType::Readback);
  if (!PrepareForAccess())
    return;

  ASSERT(x < m_config.width && y < m_config.height);
  char* dest_ptr = m_map_pointer + y * m_map_stride + x * m_texel_size;
  std::memcpy(dest_ptr, in_ptr, m_texel_size);
}

bool AbstractStagingTexture::PrepareForAccess()
{
  if (m_needs_flush)
  {
    if (IsMapped())
      Unmap();
    Flush();
  }
  return IsMapped() || Map();
}
