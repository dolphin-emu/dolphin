// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Vulkan/PresentWait.h"

#include <deque>
#include <thread>
#include <tuple>

#include "Common/WorkQueueThread.h"

#include "VideoBackends/Vulkan/VulkanContext.h"
#include "VideoBackends/Vulkan/VulkanLoader.h"

#include "VideoCommon/PerformanceMetrics.h"

namespace Vulkan
{
static VkDevice s_device;

struct Wait
{
  u64 present_id;
  VkSwapchainKHR swapchain;
};

static Common::WorkQueueThread<Wait> s_present_wait_thread;

void WaitFunction(Wait wait)
{
  do
  {
    // We choose a timeout of 20ms so can poll for IsFlushing
    VkResult res = vkWaitForPresentKHR(s_device, wait.swapchain, wait.present_id, 20'000'000);

    if (res == VK_TIMEOUT)
    {
      WARN_LOG_FMT(VIDEO, "vkWaitForPresentKHR timed out, retrying {}", wait.present_id);
      continue;
    }

    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkWaitForPresentKHR failed: ");
    }

    if (res == VK_SUCCESS)
      g_perf_metrics.CountPresent();

    return;
  } while (!s_present_wait_thread.IsCancelling());
}

void StartPresentWaitThread()
{
  s_device = g_vulkan_context->GetDevice();
  s_present_wait_thread.Reset("PresentWait", WaitFunction);
}

void StopPresentWaitThread()
{
  s_present_wait_thread.Shutdown();
}

void PresentQueued(u64 present_id, VkSwapchainKHR swapchain)
{
  s_present_wait_thread.EmplaceItem(Wait{present_id, swapchain});
}

void FlushPresentWaitQueue()
{
  s_present_wait_thread.Cancel();
  s_present_wait_thread.WaitForCompletion();
}

}  // namespace Vulkan
