// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Vulkan/VKTimelineSemaphore.h"

#include "Common/Thread.h"

#include "VideoBackends/Vulkan/VulkanContext.h"

namespace Vulkan
{
VKTimelineSemaphore::VKTimelineSemaphore()
{
  m_fence_loop = std::make_unique<Common::BlockingLoop>();
  m_fence_thread = std::thread([this]() {
    Common::SetCurrentThreadName("Vulkan FenceThread");
    m_fence_loop->Run([this]() { FenceThreadFunc(); });
  });
}

VKTimelineSemaphore::~VKTimelineSemaphore()
{
  m_fence_loop->Stop();
  m_fence_thread.join();
}

void VKTimelineSemaphore::FenceThreadFunc()
{
  while (true)
  {
    PendingFenceCounter fence;
    {
      std::lock_guard<std::mutex> guard(m_pending_fences_lock);
      if (m_pending_fences.empty())
      {
        m_fence_condvar.notify_all();
        m_fence_loop->AllowSleep();
        return;
      }

      fence = m_pending_fences.front();
      m_pending_fences.pop_front();
    }

    vkWaitForFences(g_vulkan_context->GetDevice(), 1, &fence.fence, true, ~0ul);

    std::lock_guard<std::mutex> guard(m_pending_fences_lock);
    m_completed_fence_counter.store(fence.counter, std::memory_order_release);
    m_fence_condvar.notify_all();
  }
}

void VKTimelineSemaphore::WaitForFenceCounter(u64 fence_counter)
{
  if (m_completed_fence_counter.load(std::memory_order_relaxed) >= fence_counter) [[likely]]
    return;

  std::unique_lock lock{m_pending_fences_lock};
  m_fence_condvar.wait(lock, [&] {
    return m_completed_fence_counter.load(std::memory_order_relaxed) >= fence_counter;
  });
}

void VKTimelineSemaphore::PushPendingFenceValue(VkFence fence, u64 fence_counter)
{
  std::lock_guard<std::mutex> guard(m_pending_fences_lock);
  m_pending_fences.push_back({fence, fence_counter});
  m_fence_loop->Wakeup();
}

}  // namespace Vulkan
