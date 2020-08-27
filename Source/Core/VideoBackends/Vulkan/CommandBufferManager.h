// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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

#include "VideoBackends/Vulkan/Constants.h"

namespace Vulkan
{
class CommandBufferManager
{
public:
  explicit CommandBufferManager(bool use_threaded_submission);
  ~CommandBufferManager();

  bool Initialize();

  // These command buffers are allocated per-frame. They are valid until the command buffer
  // is submitted, after that you should call these functions again.
  VkCommandBuffer GetCurrentInitCommandBuffer()
  {
    m_frame_resources[m_current_frame].init_command_buffer_used = true;
    return m_frame_resources[m_current_frame].command_buffers[0];
  }
  VkCommandBuffer GetCurrentCommandBuffer() const
  {
    return m_frame_resources[m_current_frame].command_buffers[1];
  }
  VkDescriptorPool GetCurrentDescriptorPool() const
  {
    return m_frame_resources[m_current_frame].descriptor_pool;
  }
  // Allocates a descriptors set from the pool reserved for the current frame.
  VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout set_layout);

  // Fence "counters" are used to track which commands have been completed by the GPU.
  // If the last completed fence counter is greater or equal to N, it means that the work
  // associated counter N has been completed by the GPU. The value of N to associate with
  // commands can be retreived by calling GetCurrentFenceCounter().
  u64 GetCompletedFenceCounter() const { return m_completed_fence_counter; }

  // Gets the fence that will be signaled when the currently executing command buffer is
  // queued and executed. Do not wait for this fence before the buffer is executed.
  u64 GetCurrentFenceCounter() const { return m_frame_resources[m_current_frame].fence_counter; }

  // Returns the semaphore for the current command buffer, which can be used to ensure the
  // swap chain image is ready before the command buffer executes.
  VkSemaphore GetCurrentCommandBufferSemaphore()
  {
    m_frame_resources[m_current_frame].semaphore_used = true;
    return m_frame_resources[m_current_frame].semaphore;
  }

  // Ensure that the worker thread has submitted any previous command buffers and is idle.
  void WaitForWorkerThreadIdle();

  // Wait for a fence to be completed.
  // Also invokes callbacks for completion.
  void WaitForFenceCounter(u64 fence_counter);

  void SubmitCommandBuffer(bool submit_on_worker_thread, bool wait_for_completion,
                           VkSwapchainKHR present_swap_chain = VK_NULL_HANDLE,
                           uint32_t present_image_index = 0xFFFFFFFF);

  // Was the last present submitted to the queue a failure? If so, we must recreate our swapchain.
  bool CheckLastPresentFail() { return m_last_present_failed.TestAndClear(); }
  VkResult GetLastPresentResult() const { return m_last_present_result; }

  // Schedule a vulkan resource for destruction later on. This will occur when the command buffer
  // is next re-used, and the GPU has finished working with the specified resource.
  void DeferBufferDestruction(VkBuffer object);
  void DeferBufferViewDestruction(VkBufferView object);
  void DeferDeviceMemoryDestruction(VkDeviceMemory object);
  void DeferFramebufferDestruction(VkFramebuffer object);
  void DeferImageDestruction(VkImage object);
  void DeferImageViewDestruction(VkImageView object);

private:
  bool CreateCommandBuffers();
  void DestroyCommandBuffers();

  bool CreateSubmitThread();

  void WaitForCommandBufferCompletion(u32 command_buffer_index);
  void SubmitCommandBuffer(u32 command_buffer_index, VkSwapchainKHR present_swap_chain,
                           u32 present_image_index);
  void BeginCommandBuffer();

  struct FrameResources
  {
    // [0] - Init (upload) command buffer, [1] - draw command buffer
    VkCommandPool command_pool = VK_NULL_HANDLE;
    std::array<VkCommandBuffer, 2> command_buffers = {};
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkSemaphore semaphore = VK_NULL_HANDLE;
    u64 fence_counter = 0;
    bool init_command_buffer_used = false;
    bool semaphore_used = false;

    std::vector<std::function<void()>> cleanup_resources;
  };

  u64 m_next_fence_counter = 1;
  u64 m_completed_fence_counter = 0;

  std::array<FrameResources, NUM_COMMAND_BUFFERS> m_frame_resources;
  u32 m_current_frame;

  // Threaded command buffer execution
  // Semaphore determines when a command buffer can be queued
  Common::Semaphore m_submit_semaphore;
  std::thread m_submit_thread;
  std::unique_ptr<Common::BlockingLoop> m_submit_loop;
  struct PendingCommandBufferSubmit
  {
    VkSwapchainKHR present_swap_chain;
    u32 present_image_index;
    u32 command_buffer_index;
  };
  VkSemaphore m_present_semaphore = VK_NULL_HANDLE;
  std::deque<PendingCommandBufferSubmit> m_pending_submits;
  std::mutex m_pending_submit_lock;
  Common::Flag m_last_present_failed;
  VkResult m_last_present_result = VK_SUCCESS;
  bool m_use_threaded_submission = false;
};

extern std::unique_ptr<CommandBufferManager> g_command_buffer_mgr;

}  // namespace Vulkan
