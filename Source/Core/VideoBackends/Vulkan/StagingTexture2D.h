// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan
{
class StagingTexture2D
{
public:
  StagingTexture2D(u32 width, u32 height, VkFormat format, u32 stride);
  virtual ~StagingTexture2D();

  u32 GetWidth() const { return m_width; }
  u32 GetHeight() const { return m_height; }
  VkFormat GetFormat() const { return m_format; }
  u32 GetRowStride() const { return m_row_stride; }
  u32 GetTexelSize() const { return m_texel_size; }
  bool IsMapped() const { return (m_map_pointer != nullptr); }
  const char* GetMapPointer() const { return m_map_pointer; }
  char* GetMapPointer() { return m_map_pointer; }
  VkDeviceSize GetMapOffset() const { return m_map_offset; }
  VkDeviceSize GetMapSize() const { return m_map_size; }
  const char* GetRowPointer(u32 row) const { return m_map_pointer + row * m_row_stride; }
  char* GetRowPointer(u32 row) { return m_map_pointer + row * m_row_stride; }
  // Requires Map() to be called first.
  void ReadTexel(u32 x, u32 y, void* data, size_t data_size) const;
  void WriteTexel(u32 x, u32 y, const void* data, size_t data_size);
  void ReadTexels(u32 x, u32 y, u32 width, u32 height, void* data, u32 data_stride) const;
  void WriteTexels(u32 x, u32 y, u32 width, u32 height, const void* data, u32 data_stride);

  // Assumes that image is in TRANSFER_SRC layout.
  // Results are not ready until command_buffer has been executed.
  virtual void CopyFromImage(VkCommandBuffer command_buffer, VkImage image,
                             VkImageAspectFlags src_aspect, u32 x, u32 y, u32 width, u32 height,
                             u32 level, u32 layer) = 0;

  // Assumes that image is in TRANSFER_DST layout.
  // Buffer is not safe for re-use until after command_buffer has been executed.
  virtual void CopyToImage(VkCommandBuffer command_buffer, VkImage image,
                           VkImageAspectFlags dst_aspect, u32 x, u32 y, u32 width, u32 height,
                           u32 level, u32 layer) = 0;
  virtual bool Map(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE) = 0;
  virtual void Unmap() = 0;

  // Creates the optimal format of image copy.
  static std::unique_ptr<StagingTexture2D> Create(u32 width, u32 height, VkFormat format);

protected:
  u32 m_width;
  u32 m_height;
  VkFormat m_format;
  u32 m_texel_size;
  u32 m_row_stride;

  char* m_map_pointer;
  VkDeviceSize m_map_offset;
  VkDeviceSize m_map_size;
};

class StagingTexture2DLinear : public StagingTexture2D
{
public:
  StagingTexture2DLinear(u32 width, u32 height, VkFormat format, u32 stride, VkImage image,
                         VkDeviceMemory memory, VkDeviceSize size, bool coherent);

  ~StagingTexture2DLinear();

  void CopyFromImage(VkCommandBuffer command_buffer, VkImage image, VkImageAspectFlags src_aspect,
                     u32 x, u32 y, u32 width, u32 height, u32 level, u32 layer) override;

  void CopyToImage(VkCommandBuffer command_buffer, VkImage image, VkImageAspectFlags dst_aspect,
                   u32 x, u32 y, u32 width, u32 height, u32 level, u32 layer) override;

  bool Map(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE) override;
  void Unmap() override;

  static std::unique_ptr<StagingTexture2D> Create(u32 width, u32 height, VkFormat format);

private:
  VkImage m_image;
  VkDeviceMemory m_memory;
  VkDeviceSize m_size;
  VkImageLayout m_layout;
  bool m_coherent;
};

class StagingTexture2DBuffer : public StagingTexture2D
{
public:
  StagingTexture2DBuffer(u32 width, u32 height, VkFormat format, u32 stride, VkBuffer buffer,
                         VkDeviceMemory memory, VkDeviceSize size, bool coherent);

  ~StagingTexture2DBuffer();

  void CopyFromImage(VkCommandBuffer command_buffer, VkImage image, VkImageAspectFlags src_aspect,
                     u32 x, u32 y, u32 width, u32 height, u32 level, u32 layer) override;

  void CopyToImage(VkCommandBuffer command_buffer, VkImage image, VkImageAspectFlags dst_aspect,
                   u32 x, u32 y, u32 width, u32 height, u32 level, u32 layer) override;

  bool Map(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE) override;
  void Unmap() override;

  static std::unique_ptr<StagingTexture2D> Create(u32 width, u32 height, VkFormat format);

private:
  VkBuffer m_buffer;
  VkDeviceMemory m_memory;
  VkDeviceSize m_size;
  bool m_coherent;
};
}
