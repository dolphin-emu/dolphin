// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include "Common/CommonTypes.h"

#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan
{
class StateTracker;

class BoundingBox
{
public:
  BoundingBox();
  ~BoundingBox();

  bool Initialize();

  VkBuffer GetGPUBuffer() const { return m_gpu_buffer; }
  VkDeviceSize GetGPUBufferOffset() const { return 0; }
  VkDeviceSize GetGPUBufferSize() const { return BUFFER_SIZE; }
  s32 Get(StateTracker* state_tracker, size_t index);
  void Set(StateTracker* state_tracker, size_t index, s32 value);

  void Invalidate(StateTracker* state_tracker);
  void Flush(StateTracker* state_tracker);

private:
  bool CreateGPUBuffer();
  bool CreateReadbackBuffer();
  void Readback(StateTracker* state_tracker);

  VkBuffer m_gpu_buffer = VK_NULL_HANDLE;
  VkDeviceMemory m_gpu_memory = nullptr;

  VkBuffer m_readback_buffer = VK_NULL_HANDLE;
  VkDeviceMemory m_readback_memory = nullptr;
  void* m_readback_map = nullptr;

  static const size_t NUM_VALUES = 4;
  static const size_t BUFFER_SIZE = sizeof(uint32_t) * NUM_VALUES;

  std::array<s32, NUM_VALUES> m_values = {};
  std::array<bool, NUM_VALUES> m_values_dirty = {};
  bool m_valid = true;
};

}  // namespace Vulkan
