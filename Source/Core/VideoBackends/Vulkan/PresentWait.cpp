// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Vulkan/PresentWait.h"

#include <deque>
#include <thread>
#include <tuple>

#include "Common/BlockingLoop.h"

#include "VideoBackends/Vulkan/VulkanContext.h"
#include "VideoBackends/Vulkan/VulkanLoader.h"

#include "VideoCommon/PerformanceMetrics.h"

#include <chrono>

namespace Vulkan
{

static std::thread s_present_wait_thread;
static Common::BlockingLoop s_present_wait_loop;

static std::deque<std::tuple<u64, VkSwapchainKHR>> s_present_wait_queue;

static void PresentWaitThreadFunc()
{
  VkDevice device = g_vulkan_context->GetDevice();

  s_present_wait_loop.Run([device]() {
    if (s_present_wait_queue.empty())
    {
      s_present_wait_loop.AllowSleep();
      return;
    }
    u64 present_id;
    VkSwapchainKHR swapchain;
    std::tie(present_id, swapchain) = s_present_wait_queue.back();

    auto start = std::chrono::high_resolution_clock::now();

    VkResult res = vkWaitForPresentKHR(device, swapchain, present_id, 100'000'000);  // 100ms

    if (res == VK_TIMEOUT)
    {
      WARN_LOG_FMT(VIDEO, "vkWaitForPresentKHR timed out, retrying {}", present_id);
      return;
    }

    s_present_wait_queue.pop_back();

    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkWaitForPresentKHR failed: ");
    }

    if (res == VK_SUCCESS)
    {
      auto end = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
      fmt::print("vkWaitForPresentKHR took {}us\n", duration.count());

      g_perf_metrics.CountPresent();
    }
  });
}

void StartPresentWaitThread()
{
  fmt::print("Starting PresentWaitThread");
  s_present_wait_thread = std::thread(PresentWaitThreadFunc);
}

void PresentQueued(u64 present_id, VkSwapchainKHR swapchain)
{
  s_present_wait_queue.emplace_front(std::make_tuple(present_id, swapchain));
  s_present_wait_loop.Wakeup();
}

}  // namespace Vulkan
