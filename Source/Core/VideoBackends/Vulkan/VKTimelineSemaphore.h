// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <deque>
#include <mutex>

#include "Common/BlockingLoop.h"
#include "Common/CommonTypes.h"

#include "VideoBackends/Vulkan/VulkanLoader.h"

namespace Vulkan
{
// Handles GPU synchronization in a manner similar to Vulkan Timeline semaphores or D3D12 fences.
// It does not use actual Vulkan timeline semaphores though to not break compatibility with older
// Android drivers.
class VKTimelineSemaphore
{
public:
  VKTimelineSemaphore();

  ~VKTimelineSemaphore();

  u64 GetCurrentFenceCounter() const
  {
    return m_current_fence_counter.load(std::memory_order_acquire);
  }

  u64 GetCompletedFenceCounter() const
  {
    return m_completed_fence_counter.load(std::memory_order_acquire);
  }

  void WaitForFenceCounter(u64 fence_counter);

  void PushPendingFenceValue(VkFence fence, u64 counter);

  u64 BumpNextFenceCounter() { return m_current_fence_counter++; }

private:
  void FenceThreadFunc();

  std::atomic<u64> m_current_fence_counter = 1;
  std::atomic<u64> m_completed_fence_counter = 0;

  std::thread m_fence_thread;
  std::unique_ptr<Common::BlockingLoop> m_fence_loop;
  struct PendingFenceCounter
  {
    VkFence fence;
    u64 counter;
  };
  std::deque<PendingFenceCounter> m_pending_fences;
  std::mutex m_pending_fences_lock;
  std::condition_variable m_fence_condvar;
};

}  // namespace Vulkan
