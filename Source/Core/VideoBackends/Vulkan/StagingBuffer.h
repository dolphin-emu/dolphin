// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>

#include "VideoBackends/Vulkan/Constants.h"

namespace Vulkan
{
class StagingBuffer
{
public:
  StagingBuffer(STAGING_BUFFER_TYPE type, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize size,
                bool coherent);
  virtual ~StagingBuffer();

  STAGING_BUFFER_TYPE GetType() const { return m_type; }
  VkDeviceSize GetSize() const { return m_size; }
  VkBuffer GetBuffer() const { return m_buffer; }
  bool IsMapped() const { return m_map_pointer != nullptr; }
  const char* GetMapPointer() const { return m_map_pointer; }
  char* GetMapPointer() { return m_map_pointer; }
  VkDeviceSize GetMapOffset() const { return m_map_offset; }
  VkDeviceSize GetMapSize() const { return m_map_size; }
  bool Map(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
  void Unmap();

  // Upload part 1: Prepare from device read from the CPU side
  void FlushCPUCache(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);

  // Upload part 2: Prepare for device read from the GPU side
  // Implicit when submitting the command buffer, so rarely needed.
  void InvalidateGPUCache(VkCommandBuffer command_buffer, VkAccessFlagBits dst_access_flags,
                          VkPipelineStageFlagBits dst_pipeline_stage, VkDeviceSize offset = 0,
                          VkDeviceSize size = VK_WHOLE_SIZE);

  // Readback part 0: Prepare for GPU usage (if necessary)
  void PrepareForGPUWrite(VkCommandBuffer command_buffer, VkAccessFlagBits dst_access_flags,
                          VkPipelineStageFlagBits dst_pipeline_stage, VkDeviceSize offset = 0,
                          VkDeviceSize size = VK_WHOLE_SIZE);

  // Readback part 1: Prepare for host readback from the GPU side
  void FlushGPUCache(VkCommandBuffer command_buffer, VkAccessFlagBits src_access_flags,
                     VkPipelineStageFlagBits src_pipeline_stage, VkDeviceSize offset = 0,
                     VkDeviceSize size = VK_WHOLE_SIZE);

  // Readback part 2: Prepare for host readback from the CPU side
  void InvalidateCPUCache(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);

  // offset is from the start of the buffer, not from the map offset
  void Read(VkDeviceSize offset, void* data, size_t size, bool invalidate_caches = true);
  void Write(VkDeviceSize offset, const void* data, size_t size, bool invalidate_caches = true);

  // Creates the optimal format of image copy.
  static std::unique_ptr<StagingBuffer> Create(STAGING_BUFFER_TYPE type, VkDeviceSize size,
                                               VkBufferUsageFlags usage);

  // Allocates the resources needed to create a staging buffer.
  static bool AllocateBuffer(STAGING_BUFFER_TYPE type, VkDeviceSize size, VkBufferUsageFlags usage,
                             VkBuffer* out_buffer, VkDeviceMemory* out_memory, bool* out_coherent);

  // Wrapper for creating an barrier on a buffer
  static void BufferMemoryBarrier(VkCommandBuffer command_buffer, VkBuffer buffer,
                                  VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask,
                                  VkDeviceSize offset, VkDeviceSize size,
                                  VkPipelineStageFlags src_stage_mask,
                                  VkPipelineStageFlags dst_stage_mask);

protected:
  STAGING_BUFFER_TYPE m_type;
  VkBuffer m_buffer;
  VkDeviceMemory m_memory;
  VkDeviceSize m_size;
  bool m_coherent;

  char* m_map_pointer = nullptr;
  VkDeviceSize m_map_offset = 0;
  VkDeviceSize m_map_size = 0;
};
}  // namespace Vulkan
