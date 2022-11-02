// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "Common/BlockingLoop.h"
#include "Common/Flag.h"
#include "Common/Semaphore.h"
#include "Common/WorkQueueThread.h"

#include "VideoBackends/Vulkan/Constants.h"

namespace Vulkan
{
class StateTracker;
class VKTimelineSemaphore;

class CommandBufferManager
{
public:
  explicit CommandBufferManager(VKTimelineSemaphore* semaphore, bool use_threaded_submission);
  ~CommandBufferManager();

  bool Initialize();

  void Shutdown();

  // These command buffers are allocated per-frame. They are valid until the command buffer
  // is submitted, after that you should call these functions again.
  VkCommandBuffer GetCurrentInitCommandBuffer()
  {
    CmdBufferResources& cmd_buffer_resources = GetCurrentCmdBufferResources();
    cmd_buffer_resources.init_command_buffer_used = true;
    return cmd_buffer_resources.command_buffers[0];
  }
  VkCommandBuffer GetCurrentCommandBuffer() const
  {
    const CmdBufferResources& cmd_buffer_resources = m_command_buffers[m_current_cmd_buffer];
    return cmd_buffer_resources.command_buffers[1];
  }
  // Allocates a descriptors set from the pool reserved for the current frame.
  VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout set_layout);

  // Gets the fence that will be signaled when the currently executing command buffer is
  // queued and executed. Do not wait for this fence before the buffer is executed.
  u64 GetCurrentFenceCounter() const
  {
    auto& resources = m_command_buffers[m_current_cmd_buffer];
    return resources.fence_counter;
  }

  // Returns the semaphore for the current command buffer, which can be used to ensure the
  // swap chain image is ready before the command buffer executes.
  void SetWaitSemaphoreForCurrentCommandBuffer(VkSemaphore semaphore)
  {
    auto& resources = m_command_buffers[m_current_cmd_buffer];
    resources.semaphore_used = true;
    resources.semaphore = semaphore;
  }

  // Ensure that the worker thread has submitted any previous command buffers and is idle.
  void WaitForSubmitWorkerThreadIdle();

  void SubmitCommandBuffer(u64 fence_counter, bool submit_on_worker_thread,
                           bool wait_for_completion,
                           VkSwapchainKHR present_swap_chain = VK_NULL_HANDLE,
                           uint32_t present_image_index = 0xFFFFFFFF);

  // Was the last present submitted to the queue a failure? If so, we must recreate our swapchain.
  bool CheckLastPresentFail() { return m_last_present_failed.TestAndClear(); }
  VkResult GetLastPresentResult() const { return m_last_present_result.load(); }
  bool CheckLastPresentDone() { return m_last_present_done.TestAndClear(); }

  // Schedule a vulkan resource for destruction later on. This will occur when the command buffer
  // is next re-used, and the GPU has finished working with the specified resource.
  void DeferBufferViewDestruction(VkBufferView object);
  void DeferBufferDestruction(VkBuffer buffer, VmaAllocation alloc);
  void DeferFramebufferDestruction(VkFramebuffer object);
  void DeferImageDestruction(VkImage object, VmaAllocation alloc);
  void DeferImageViewDestruction(VkImageView object);

  StateTracker* GetStateTracker() { return m_state_tracker.get(); }

private:
  bool CreateCommandBuffers();
  void DestroyCommandBuffers();

  bool CreateSubmitThread();

  void SubmitCommandBuffer(u32 command_buffer_index, VkSwapchainKHR present_swap_chain,
                           u32 present_image_index);
  void BeginCommandBuffer();

  void CleanupCompletedCommandBuffers();

  VkDescriptorPool CreateDescriptorPool(u32 descriptor_sizes);

  const u32 DESCRIPTOR_SETS_PER_POOL = 1024;

  struct CmdBufferResources
  {
    // [0] - Init (upload) command buffer, [1] - draw command buffer
    VkCommandPool command_pool = VK_NULL_HANDLE;
    std::array<VkCommandBuffer, 2> command_buffers = {};
    VkFence fence = VK_NULL_HANDLE;
    VkSemaphore semaphore = VK_NULL_HANDLE;
    u64 fence_counter = 0;
    bool init_command_buffer_used = false;
    bool semaphore_used = false;
    u32 frame_index = 0;

    std::vector<std::function<void()>> cleanup_resources;
  };

  struct FrameResources
  {
    std::vector<VkDescriptorPool> descriptor_pools;
    u32 current_descriptor_pool_index = 0;
  };

  FrameResources& GetCurrentFrameResources() { return m_frame_resources[m_current_frame]; }

  CmdBufferResources& GetCurrentCmdBufferResources()
  {
    return m_command_buffers[m_current_cmd_buffer];
  }

  std::array<FrameResources, NUM_FRAMES_IN_FLIGHT> m_frame_resources;
  std::array<CmdBufferResources, NUM_COMMAND_BUFFERS> m_command_buffers;
  u32 m_current_frame = 0;
  u32 m_current_cmd_buffer = 0;

  bool m_use_threaded_submission = false;

  std::unique_ptr<StateTracker> m_state_tracker;

  // Threaded command buffer execution
  struct PendingCommandBufferSubmit
  {
    VkSwapchainKHR present_swap_chain;
    u32 present_image_index;
    u32 command_buffer_index;
  };
  Common::WorkQueueThread<PendingCommandBufferSubmit> m_submit_thread;
  VkSemaphore m_present_semaphore = VK_NULL_HANDLE;
  Common::Flag m_last_present_failed;
  Common::Flag m_last_present_done;
  std::atomic<VkResult> m_last_present_result = VK_SUCCESS;
  u32 m_descriptor_set_count = DESCRIPTOR_SETS_PER_POOL;

  VKTimelineSemaphore* m_semaphore;
};

}  // namespace Vulkan
