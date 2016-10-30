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
StagingTexture2D::StagingTexture2D(STAGING_BUFFER_TYPE type, u32 width, u32 height, VkFormat format,
                                   u32 stride)
    : m_type(type), m_width(width), m_height(height), m_format(format),
      m_texel_size(Util::GetTexelSize(format)), m_row_stride(stride)
{
}

StagingTexture2D::~StagingTexture2D()
{
  _assert_(!m_map_pointer);
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

std::unique_ptr<StagingTexture2D> StagingTexture2D::Create(STAGING_BUFFER_TYPE type, u32 width,
                                                           u32 height, VkFormat format)
{
// TODO: Using a buffer here as opposed to a linear texture is faster on AMD.
// NVIDIA also seems faster with buffers over textures.
#if 0
  // Check for support for this format as a linear texture.
  // Some drivers don't support this (e.g. adreno).
  VkImageFormatProperties properties;
  VkResult res = vkGetPhysicalDeviceImageFormatProperties(
      g_object_cache->GetPhysicalDevice(), format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_LINEAR,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, &properties);
  if (res == VK_SUCCESS && width <= properties.maxExtent.width &&
      height <= properties.maxExtent.height)
  {
    return StagingTexture2DLinear::Create(type, width, height, format);
  }
#endif

  // Fall back to a buffer copy.
  return StagingTexture2DBuffer::Create(type, width, height, format);
}

StagingTexture2DLinear::StagingTexture2DLinear(STAGING_BUFFER_TYPE type, u32 width, u32 height,
                                               VkFormat format, u32 stride, VkImage image,
                                               VkDeviceMemory memory, VkDeviceSize size,
                                               bool coherent)
    : StagingTexture2D(type, width, height, format, stride), m_image(image), m_memory(memory),
      m_size(size), m_layout(VK_IMAGE_LAYOUT_PREINITIALIZED), m_coherent(coherent)
{
}

StagingTexture2DLinear::~StagingTexture2DLinear()
{
  if (m_map_pointer)
    Unmap();

  g_command_buffer_mgr->DeferDeviceMemoryDestruction(m_memory);
  g_command_buffer_mgr->DeferImageDestruction(m_image);
}

void StagingTexture2DLinear::CopyFromImage(VkCommandBuffer command_buffer, VkImage image,
                                           VkImageAspectFlags src_aspect, u32 x, u32 y, u32 width,
                                           u32 height, u32 level, u32 layer)
{
  // Prepare the buffer for copying.
  // We don't care about the existing contents, so set to UNDEFINED.
  VkImageMemoryBarrier before_transfer_barrier = {
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,  // VkStructureType            sType
      nullptr,                                 // const void*                pNext
      0,                                       // VkAccessFlags              srcAccessMask
      VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags              dstAccessMask
      VK_IMAGE_LAYOUT_UNDEFINED,               // VkImageLayout              oldLayout
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,    // VkImageLayout              newLayout
      VK_QUEUE_FAMILY_IGNORED,                 // uint32_t                   srcQueueFamilyIndex
      VK_QUEUE_FAMILY_IGNORED,                 // uint32_t                   dstQueueFamilyIndex
      m_image,                                 // VkImage                    image
      {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}  // VkImageSubresourceRange    subresourceRange
  };
  vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                       &before_transfer_barrier);

  // Issue the image copy, gpu -> host.
  VkImageCopy copy_region = {
      {src_aspect, level, layer, 1},                  // VkImageSubresourceLayers srcSubresource
      {static_cast<s32>(x), static_cast<s32>(y), 0},  // VkOffset3D               srcOffset
      {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},           // VkImageSubresourceLayers dstSubresource
      {0, 0, 0},                                      // VkOffset3D               dstOffset
      {width, height, 1}                              // VkExtent3D               extent
  };
  vkCmdCopyImage(command_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_image,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

  // Ensure writes are visible to the host.
  VkImageMemoryBarrier visible_barrier = {
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,  // VkStructureType            sType
      nullptr,                                 // const void*                pNext
      VK_ACCESS_TRANSFER_WRITE_BIT,            // VkAccessFlags              srcAccessMask
      VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags              dstAccessMask
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,    // VkImageLayout              oldLayout
      VK_IMAGE_LAYOUT_GENERAL,                 // VkImageLayout              newLayout
      VK_QUEUE_FAMILY_IGNORED,                 // uint32_t                   srcQueueFamilyIndex
      VK_QUEUE_FAMILY_IGNORED,                 // uint32_t                   dstQueueFamilyIndex
      m_image,                                 // VkImage                    image
      {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}  // VkImageSubresourceRange    subresourceRange
  };
  vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                       0, 0, nullptr, 0, nullptr, 1, &visible_barrier);
  m_layout = VK_IMAGE_LAYOUT_GENERAL;

  // Invalidate memory range if currently mapped.
  if (m_map_pointer && !m_coherent)
  {
    VkMappedMemoryRange range = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, m_memory,
                                 m_map_offset, m_map_size};
    vkInvalidateMappedMemoryRanges(g_vulkan_context->GetDevice(), 1, &range);
  }
}

void StagingTexture2DLinear::CopyToImage(VkCommandBuffer command_buffer, VkImage image,
                                         VkImageAspectFlags dst_aspect, u32 x, u32 y, u32 width,
                                         u32 height, u32 level, u32 layer)
{
  // Flush memory range if currently mapped.
  if (m_map_pointer && !m_coherent)
  {
    VkMappedMemoryRange range = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, m_memory,
                                 m_map_offset, m_map_size};
    vkFlushMappedMemoryRanges(g_vulkan_context->GetDevice(), 1, &range);
  }

  // Ensure any writes to the image are visible to the GPU.
  VkImageMemoryBarrier barrier = {
      VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,  // VkStructureType            sType
      nullptr,                                 // const void*                pNext
      VK_ACCESS_HOST_WRITE_BIT,                // VkAccessFlags              srcAccessMask
      VK_ACCESS_TRANSFER_READ_BIT,             // VkAccessFlags              dstAccessMask
      m_layout,                                // VkImageLayout              oldLayout
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,    // VkImageLayout              newLayout
      VK_QUEUE_FAMILY_IGNORED,                 // uint32_t                   srcQueueFamilyIndex
      VK_QUEUE_FAMILY_IGNORED,                 // uint32_t                   dstQueueFamilyIndex
      m_image,                                 // VkImage                    image
      {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}  // VkImageSubresourceRange    subresourceRange
  };
  vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       0, 0, nullptr, 0, nullptr, 1, &barrier);

  m_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

  // Issue the image copy, host -> gpu.
  VkImageCopy copy_region = {
      {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},           // VkImageSubresourceLayers srcSubresource
      {0, 0, 0},                                      // VkOffset3D               srcOffset
      {dst_aspect, level, layer, 1},                  // VkImageSubresourceLayers dstSubresource
      {static_cast<s32>(x), static_cast<s32>(y), 0},  // VkOffset3D               dstOffset
      {width, height, 1}                              // VkExtent3D               extent
  };
  vkCmdCopyImage(command_buffer, m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);
}

bool StagingTexture2DLinear::Map(VkDeviceSize offset /* = 0 */,
                                 VkDeviceSize size /* = VK_WHOLE_SIZE */)
{
  m_map_offset = offset;
  if (size == VK_WHOLE_SIZE)
    m_map_size = m_size - offset;
  else
    m_map_size = size;

  _assert_(!m_map_pointer);
  _assert_(m_map_offset + m_map_size <= m_size);

  void* map_pointer;
  VkResult res = vkMapMemory(g_vulkan_context->GetDevice(), m_memory, m_map_offset, m_map_size, 0,
                             &map_pointer);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkMapMemory failed: ");
    return false;
  }

  m_map_pointer = reinterpret_cast<char*>(map_pointer);
  return true;
}

void StagingTexture2DLinear::Unmap()
{
  _assert_(m_map_pointer);

  vkUnmapMemory(g_vulkan_context->GetDevice(), m_memory);
  m_map_pointer = nullptr;
  m_map_offset = 0;
  m_map_size = 0;
}

std::unique_ptr<StagingTexture2D>
StagingTexture2DLinear::Create(STAGING_BUFFER_TYPE type, u32 width, u32 height, VkFormat format)
{
  VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  VkImageCreateInfo create_info = {
      VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,  // VkStructureType          sType
      nullptr,                              // const void*              pNext
      0,                                    // VkImageCreateFlags       flags
      VK_IMAGE_TYPE_2D,                     // VkImageType              imageType
      format,                               // VkFormat                 format
      {width, height, 1},                   // VkExtent3D               extent
      1,                                    // uint32_t                 mipLevels
      1,                                    // uint32_t                 arrayLayers
      VK_SAMPLE_COUNT_1_BIT,                // VkSampleCountFlagBits    samples
      VK_IMAGE_TILING_LINEAR,               // VkImageTiling            tiling
      usage,                                // VkImageUsageFlags        usage
      VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode            sharingMode
      0,                                    // uint32_t                 queueFamilyIndexCount
      nullptr,                              // const uint32_t*          pQueueFamilyIndices
      VK_IMAGE_LAYOUT_PREINITIALIZED        // VkImageLayout            initialLayout
  };

  VkImage image;
  VkResult res = vkCreateImage(g_vulkan_context->GetDevice(), &create_info, nullptr, &image);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateImage failed: ");
    return nullptr;
  }

  VkMemoryRequirements memory_requirements;
  vkGetImageMemoryRequirements(g_vulkan_context->GetDevice(), image, &memory_requirements);

  bool is_coherent;
  u32 memory_type_index;
  if (type == STAGING_BUFFER_TYPE_READBACK)
  {
    memory_type_index =
        g_vulkan_context->GetReadbackMemoryType(memory_requirements.memoryTypeBits, &is_coherent);
  }
  else
  {
    memory_type_index =
        g_vulkan_context->GetUploadMemoryType(memory_requirements.memoryTypeBits, &is_coherent);
  }
  VkMemoryAllocateInfo memory_allocate_info = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,  // VkStructureType    sType
      nullptr,                                 // const void*        pNext
      memory_requirements.size,                // VkDeviceSize       allocationSize
      memory_type_index                        // uint32_t           memoryTypeIndex
  };
  VkDeviceMemory memory;
  res = vkAllocateMemory(g_vulkan_context->GetDevice(), &memory_allocate_info, nullptr, &memory);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkAllocateMemory failed: ");
    vkDestroyImage(g_vulkan_context->GetDevice(), image, nullptr);
    return nullptr;
  }

  res = vkBindImageMemory(g_vulkan_context->GetDevice(), image, memory, 0);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkBindImageMemory failed: ");
    vkDestroyImage(g_vulkan_context->GetDevice(), image, nullptr);
    vkFreeMemory(g_vulkan_context->GetDevice(), memory, nullptr);
    return nullptr;
  }

  // Assume tight packing. Is this correct?
  u32 stride = width * Util::GetTexelSize(format);
  return std::make_unique<StagingTexture2DLinear>(type, width, height, format, stride, image,
                                                  memory, memory_requirements.size, is_coherent);
}

StagingTexture2DBuffer::StagingTexture2DBuffer(STAGING_BUFFER_TYPE type, u32 width, u32 height,
                                               VkFormat format, u32 stride, VkBuffer buffer,
                                               VkDeviceMemory memory, VkDeviceSize size,
                                               bool coherent)
    : StagingTexture2D(type, width, height, format, stride), m_buffer(buffer), m_memory(memory),
      m_size(size), m_coherent(coherent)
{
}

StagingTexture2DBuffer::~StagingTexture2DBuffer()
{
  if (m_map_pointer)
    Unmap();

  g_command_buffer_mgr->DeferDeviceMemoryDestruction(m_memory);
  g_command_buffer_mgr->DeferBufferDestruction(m_buffer);
}

void StagingTexture2DBuffer::CopyFromImage(VkCommandBuffer command_buffer, VkImage image,
                                           VkImageAspectFlags src_aspect, u32 x, u32 y, u32 width,
                                           u32 height, u32 level, u32 layer)
{
  // Issue the image->buffer copy.
  VkBufferImageCopy image_copy = {
      0,                                              // VkDeviceSize             bufferOffset
      m_width,                                        // uint32_t                 bufferRowLength
      0,                                              // uint32_t                 bufferImageHeight
      {src_aspect, level, layer, 1},                  // VkImageSubresourceLayers imageSubresource
      {static_cast<s32>(x), static_cast<s32>(y), 0},  // VkOffset3D               imageOffset
      {width, height, 1}                              // VkExtent3D               imageExtent
  };
  vkCmdCopyImageToBuffer(command_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_buffer, 1,
                         &image_copy);

  // Ensure the write has completed.
  VkDeviceSize copy_size = m_row_stride * height;
  Util::BufferMemoryBarrier(command_buffer, m_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
                            VK_ACCESS_HOST_READ_BIT, 0, copy_size, VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_HOST_BIT);

  // If we're still mapped, invalidate the mapped range
  if (m_map_pointer && !m_coherent)
  {
    VkMappedMemoryRange range = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, m_memory,
                                 m_map_offset, m_map_size};
    vkInvalidateMappedMemoryRanges(g_vulkan_context->GetDevice(), 1, &range);
  }
}

void StagingTexture2DBuffer::CopyToImage(VkCommandBuffer command_buffer, VkImage image,
                                         VkImageAspectFlags dst_aspect, u32 x, u32 y, u32 width,
                                         u32 height, u32 level, u32 layer)
{
  // If we're still mapped, flush the mapped range
  if (m_map_pointer && !m_coherent)
  {
    VkMappedMemoryRange range = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, m_memory,
                                 m_map_offset, m_map_size};
    vkFlushMappedMemoryRanges(g_vulkan_context->GetDevice(), 1, &range);
  }

  // Ensure writes are visible to GPU.
  VkDeviceSize copy_size = m_row_stride * height;
  Util::BufferMemoryBarrier(command_buffer, m_buffer, VK_ACCESS_HOST_WRITE_BIT,
                            VK_ACCESS_TRANSFER_READ_BIT, 0, copy_size, VK_PIPELINE_STAGE_HOST_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT);

  // Issue the buffer->image copy
  VkBufferImageCopy image_copy = {
      0,                                              // VkDeviceSize             bufferOffset
      m_width,                                        // uint32_t                 bufferRowLength
      0,                                              // uint32_t                 bufferImageHeight
      {dst_aspect, level, layer, 1},                  // VkImageSubresourceLayers imageSubresource
      {static_cast<s32>(x), static_cast<s32>(y), 0},  // VkOffset3D               imageOffset
      {width, height, 1}                              // VkExtent3D               imageExtent
  };
  vkCmdCopyBufferToImage(command_buffer, m_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                         &image_copy);
}

bool StagingTexture2DBuffer::Map(VkDeviceSize offset /* = 0 */,
                                 VkDeviceSize size /* = VK_WHOLE_SIZE */)
{
  m_map_offset = offset;
  if (size == VK_WHOLE_SIZE)
    m_map_size = m_size - offset;
  else
    m_map_size = size;

  _assert_(!m_map_pointer);
  _assert_(m_map_offset + m_map_size <= m_size);

  void* map_pointer;
  VkResult res = vkMapMemory(g_vulkan_context->GetDevice(), m_memory, m_map_offset, m_map_size, 0,
                             &map_pointer);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkMapMemory failed: ");
    return false;
  }

  m_map_pointer = reinterpret_cast<char*>(map_pointer);
  return true;
}

void StagingTexture2DBuffer::Unmap()
{
  _assert_(m_map_pointer);

  vkUnmapMemory(g_vulkan_context->GetDevice(), m_memory);
  m_map_pointer = nullptr;
  m_map_offset = 0;
  m_map_size = 0;
}

std::unique_ptr<StagingTexture2D>
StagingTexture2DBuffer::Create(STAGING_BUFFER_TYPE type, u32 width, u32 height, VkFormat format)
{
  // Assume tight packing.
  u32 row_stride = Util::GetTexelSize(format) * width;
  u32 buffer_size = row_stride * height;
  VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  VkBufferCreateInfo buffer_create_info = {
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,  // VkStructureType        sType
      nullptr,                               // const void*            pNext
      0,                                     // VkBufferCreateFlags    flags
      buffer_size,                           // VkDeviceSize           size
      usage,                                 // VkBufferUsageFlags     usage
      VK_SHARING_MODE_EXCLUSIVE,             // VkSharingMode          sharingMode
      0,                                     // uint32_t               queueFamilyIndexCount
      nullptr                                // const uint32_t*        pQueueFamilyIndices
  };
  VkBuffer buffer;
  VkResult res =
      vkCreateBuffer(g_vulkan_context->GetDevice(), &buffer_create_info, nullptr, &buffer);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateBuffer failed: ");
    return nullptr;
  }

  VkMemoryRequirements memory_requirements;
  vkGetBufferMemoryRequirements(g_vulkan_context->GetDevice(), buffer, &memory_requirements);

  bool is_coherent;
  u32 memory_type_index;
  if (type == STAGING_BUFFER_TYPE_READBACK)
  {
    memory_type_index =
        g_vulkan_context->GetReadbackMemoryType(memory_requirements.memoryTypeBits, &is_coherent);
  }
  else
  {
    memory_type_index =
        g_vulkan_context->GetUploadMemoryType(memory_requirements.memoryTypeBits, &is_coherent);
  }

  VkMemoryAllocateInfo memory_allocate_info = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,  // VkStructureType    sType
      nullptr,                                 // const void*        pNext
      memory_requirements.size,                // VkDeviceSize       allocationSize
      memory_type_index                        // uint32_t           memoryTypeIndex
  };
  VkDeviceMemory memory;
  res = vkAllocateMemory(g_vulkan_context->GetDevice(), &memory_allocate_info, nullptr, &memory);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkAllocateMemory failed: ");
    vkDestroyBuffer(g_vulkan_context->GetDevice(), buffer, nullptr);
    return nullptr;
  }

  res = vkBindBufferMemory(g_vulkan_context->GetDevice(), buffer, memory, 0);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkBindBufferMemory failed: ");
    vkDestroyBuffer(g_vulkan_context->GetDevice(), buffer, nullptr);
    vkFreeMemory(g_vulkan_context->GetDevice(), memory, nullptr);
    return nullptr;
  }

  return std::make_unique<StagingTexture2DBuffer>(type, width, height, format, row_stride, buffer,
                                                  memory, buffer_size, is_coherent);
}

}  // namespace Vulkan
