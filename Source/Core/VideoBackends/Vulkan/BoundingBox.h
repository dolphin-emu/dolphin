// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <string>

#include "Common/CommonTypes.h"

#include "VideoBackends/Vulkan/VulkanLoader.h"

namespace Vulkan
{
class StagingBuffer;
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
  VkDeviceMemory m_gpu_memory = VK_NULL_HANDLE;

  static const size_t NUM_VALUES = 4;
  static const size_t BUFFER_SIZE = sizeof(u32) * NUM_VALUES;

  std::unique_ptr<StagingBuffer> m_readback_buffer;
  std::array<bool, NUM_VALUES> m_values_dirty = {};
  bool m_valid = true;
};

}  // namespace Vulkan
