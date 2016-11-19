// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>

#include "Common/CommonTypes.h"
#include "VideoBackends/Vulkan/Constants.h"
#include "VideoBackends/Vulkan/StagingBuffer.h"

namespace Vulkan
{
class StagingTexture2D final : public StagingBuffer
{
public:
  StagingTexture2D(STAGING_BUFFER_TYPE type, VkBuffer buffer, VkDeviceMemory memory,
                   VkDeviceSize size, bool coherent, u32 width, u32 height, VkFormat format,
                   u32 stride);
  ~StagingTexture2D();

  u32 GetWidth() const { return m_width; }
  u32 GetHeight() const { return m_height; }
  VkFormat GetFormat() const { return m_format; }
  u32 GetRowStride() const { return m_row_stride; }
  u32 GetTexelSize() const { return m_texel_size; }
  // Requires Map() to be called first.
  const char* GetRowPointer(u32 row) const { return m_map_pointer + row * m_row_stride; }
  char* GetRowPointer(u32 row) { return m_map_pointer + row * m_row_stride; }
  void ReadTexel(u32 x, u32 y, void* data, size_t data_size) const;
  void WriteTexel(u32 x, u32 y, const void* data, size_t data_size);
  void ReadTexels(u32 x, u32 y, u32 width, u32 height, void* data, u32 data_stride) const;
  void WriteTexels(u32 x, u32 y, u32 width, u32 height, const void* data, u32 data_stride);

  // Assumes that image is in TRANSFER_SRC layout.
  // Results are not ready until command_buffer has been executed.
  void CopyFromImage(VkCommandBuffer command_buffer, VkImage image, VkImageAspectFlags src_aspect,
                     u32 x, u32 y, u32 width, u32 height, u32 level, u32 layer);

  // Assumes that image is in TRANSFER_DST layout.
  // Buffer is not safe for re-use until after command_buffer has been executed.
  void CopyToImage(VkCommandBuffer command_buffer, VkImage image, VkImageAspectFlags dst_aspect,
                   u32 x, u32 y, u32 width, u32 height, u32 level, u32 layer);

  // Creates the optimal format of image copy.
  static std::unique_ptr<StagingTexture2D> Create(STAGING_BUFFER_TYPE type, u32 width, u32 height,
                                                  VkFormat format);

protected:
  u32 m_width;
  u32 m_height;
  VkFormat m_format;
  u32 m_texel_size;
  u32 m_row_stride;
};
}
