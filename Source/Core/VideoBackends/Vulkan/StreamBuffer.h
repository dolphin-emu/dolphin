// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <deque>
#include <memory>
#include <utility>

#include "Common/CommonTypes.h"
#include "VideoBackends/Vulkan/Constants.h"

namespace Vulkan
{
class StreamBuffer
{
public:
  StreamBuffer(VkBufferUsageFlags usage, u32 size);
  ~StreamBuffer();

  VkBuffer GetBuffer() const { return m_buffer; }
  VkDeviceMemory GetDeviceMemory() const { return m_memory; }
  u8* GetHostPointer() const { return m_host_pointer; }
  u8* GetCurrentHostPointer() const { return m_host_pointer + m_current_offset; }
  u32 GetCurrentSize() const { return m_size; }
  u32 GetCurrentOffset() const { return m_current_offset; }
  bool ReserveMemory(u32 num_bytes, u32 alignment);
  void CommitMemory(u32 final_num_bytes);

  static std::unique_ptr<StreamBuffer> Create(VkBufferUsageFlags usage, u32 size);

private:
  bool AllocateBuffer();
  void UpdateCurrentFencePosition();
  void UpdateGPUPosition();

  // Waits for as many fences as needed to allocate num_bytes bytes from the buffer.
  bool WaitForClearSpace(u32 num_bytes);

  VkBufferUsageFlags m_usage;
  u32 m_size;
  u32 m_current_offset = 0;
  u32 m_current_gpu_position = 0;
  u32 m_last_allocation_size = 0;

  VkBuffer m_buffer = VK_NULL_HANDLE;
  VkDeviceMemory m_memory = VK_NULL_HANDLE;
  u8* m_host_pointer = nullptr;

  // List of fences and the corresponding positions in the buffer
  std::deque<std::pair<u64, u32>> m_tracked_fences;

  bool m_coherent_mapping = false;
};

}  // namespace Vulkan
