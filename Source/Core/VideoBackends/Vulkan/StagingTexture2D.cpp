// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>

#include "Common/Assert.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/StagingTexture2D.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

namespace Vulkan
{
StagingTexture2D::StagingTexture2D(STAGING_BUFFER_TYPE type, VkBuffer buffer, VkDeviceMemory memory,
                                   VkDeviceSize size, bool coherent, u32 width, u32 height,
                                   VkFormat format, u32 stride)
    : StagingBuffer(type, buffer, memory, size, coherent), m_width(width), m_height(height),
      m_format(format), m_texel_size(Util::GetTexelSize(format)), m_row_stride(stride)
{
}

StagingTexture2D::~StagingTexture2D()
{
}

void StagingTexture2D::ReadTexel(u32 x, u32 y, void* data, size_t data_size) const
{
  _assert_(data_size >= m_texel_size);

  VkDeviceSize offset = y * m_row_stride + x * m_texel_size;
  VkDeviceSize map_offset = offset - m_map_offset;
  _assert_(offset >= m_map_offset && (map_offset + m_texel_size) <= (m_map_offset + m_map_size));

  const char* ptr = m_map_pointer + map_offset;
  memcpy(data, ptr, data_size);
}

void StagingTexture2D::WriteTexel(u32 x, u32 y, const void* data, size_t data_size)
{
  _assert_(data_size >= m_texel_size);

  VkDeviceSize offset = y * m_row_stride + x * m_texel_size;
  VkDeviceSize map_offset = offset - m_map_offset;
  _assert_(offset >= m_map_offset && (map_offset + m_texel_size) <= (m_map_offset + m_map_size));

  char* ptr = m_map_pointer + map_offset;
  memcpy(ptr, data, data_size);
}

void StagingTexture2D::ReadTexels(u32 x, u32 y, u32 width, u32 height, void* data,
                                  u32 data_stride) const
{
  const char* src_ptr = GetRowPointer(y);

  // Optimal path: same dimensions, same stride.
  _assert_((x + width) <= m_width && (y + height) <= m_height);
  if (x == 0 && width == m_width && m_row_stride == data_stride)
  {
    memcpy(data, src_ptr, m_row_stride * height);
    return;
  }

  u32 copy_size = std::min(width * m_texel_size, data_stride);
  char* dst_ptr = reinterpret_cast<char*>(data);
  for (u32 row = 0; row < height; row++)
  {
    memcpy(dst_ptr, src_ptr + (x * m_texel_size), copy_size);
    src_ptr += m_row_stride;
    dst_ptr += data_stride;
  }
}

void StagingTexture2D::WriteTexels(u32 x, u32 y, u32 width, u32 height, const void* data,
                                   u32 data_stride)
{
  char* dst_ptr = GetRowPointer(y);

  // Optimal path: same dimensions, same stride.
  _assert_((x + width) <= m_width && (y + height) <= m_height);
  if (x == 0 && width == m_width && m_row_stride == data_stride)
  {
    memcpy(dst_ptr, data, m_row_stride * height);
    return;
  }

  u32 copy_size = std::min(width * m_texel_size, data_stride);
  const char* src_ptr = reinterpret_cast<const char*>(data);
  for (u32 row = 0; row < height; row++)
  {
    memcpy(dst_ptr + (x * m_texel_size), src_ptr, copy_size);
    dst_ptr += m_row_stride;
    src_ptr += data_stride;
  }
}

void StagingTexture2D::CopyFromImage(VkCommandBuffer command_buffer, VkImage image,
                                     VkImageAspectFlags src_aspect, u32 x, u32 y, u32 width,
                                     u32 height, u32 level, u32 layer)
{
  // Issue the image->buffer copy.
  VkBufferImageCopy image_copy = {
      y * m_row_stride + x * m_texel_size,            // VkDeviceSize             bufferOffset
      m_width,                                        // uint32_t                 bufferRowLength
      0,                                              // uint32_t                 bufferImageHeight
      {src_aspect, level, layer, 1},                  // VkImageSubresourceLayers imageSubresource
      {static_cast<s32>(x), static_cast<s32>(y), 0},  // VkOffset3D               imageOffset
      {width, height, 1}                              // VkExtent3D               imageExtent
  };
  vkCmdCopyImageToBuffer(command_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_buffer, 1,
                         &image_copy);

  // Flush CPU and GPU caches if not coherent mapping.
  VkDeviceSize buffer_flush_offset = y * m_row_stride;
  VkDeviceSize buffer_flush_size = height * m_row_stride;
  FlushGPUCache(command_buffer, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                buffer_flush_offset, buffer_flush_size);
  InvalidateCPUCache(buffer_flush_offset, buffer_flush_size);
}

void StagingTexture2D::CopyToImage(VkCommandBuffer command_buffer, VkImage image,
                                   VkImageAspectFlags dst_aspect, u32 x, u32 y, u32 width,
                                   u32 height, u32 level, u32 layer)
{
  // Flush CPU and GPU caches if not coherent mapping.
  VkDeviceSize buffer_flush_offset = y * m_row_stride;
  VkDeviceSize buffer_flush_size = height * m_row_stride;
  FlushCPUCache(buffer_flush_offset, buffer_flush_size);
  InvalidateGPUCache(command_buffer, VK_ACCESS_HOST_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                     buffer_flush_offset, buffer_flush_size);

  // Issue the buffer->image copy.
  VkBufferImageCopy image_copy = {
      y * m_row_stride + x * m_texel_size,            // VkDeviceSize             bufferOffset
      m_width,                                        // uint32_t                 bufferRowLength
      0,                                              // uint32_t                 bufferImageHeight
      {dst_aspect, level, layer, 1},                  // VkImageSubresourceLayers imageSubresource
      {static_cast<s32>(x), static_cast<s32>(y), 0},  // VkOffset3D               imageOffset
      {width, height, 1}                              // VkExtent3D               imageExtent
  };
  vkCmdCopyBufferToImage(command_buffer, m_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                         &image_copy);
}

std::unique_ptr<StagingTexture2D> StagingTexture2D::Create(STAGING_BUFFER_TYPE type, u32 width,
                                                           u32 height, VkFormat format)
{
  // Assume tight packing.
  u32 stride = Util::GetTexelSize(format) * width;
  u32 size = stride * height;
  VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  VkBuffer buffer;
  VkDeviceMemory memory;
  bool coherent;
  if (!AllocateBuffer(type, size, usage, &buffer, &memory, &coherent))
    return nullptr;

  return std::make_unique<StagingTexture2D>(type, buffer, memory, size, coherent, width, height,
                                            format, stride);
}
}  // namespace Vulkan
