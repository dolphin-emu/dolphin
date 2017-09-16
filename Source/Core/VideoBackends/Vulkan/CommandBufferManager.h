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

#include "VideoCommon/VideoCommon.h"

#include "VideoBackends/Vulkan/Constants.h"
#include "VideoBackends/Vulkan/Util.h"

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

  // Gets the fence that will be signaled when the currently executing command buffer is
  // queued and executed. Do not wait for this fence before the buffer is executed.
  VkFence GetCurrentCommandBufferFence() const { return m_frame_resources[m_current_frame].fence; }
  // Ensure the worker thread has submitted the previous frame's command buffer.
  void PrepareToSubmitCommandBuffer();

  // Ensure that the worker thread has submitted any previous command buffers and is idle.
  void WaitForWorkerThreadIdle();

  // Ensure that the worker thread has both submitted all commands, and the GPU has caught up.
  // Use with caution, huge performance penalty.
  void WaitForGPUIdle();

  // Wait for a fence to be completed.
  // Also invokes callbacks for completion.
  void WaitForFence(VkFence fence);

  void SubmitCommandBuffer(bool submit_on_worker_thread,
                           VkSemaphore wait_semaphore = VK_NULL_HANDLE,
                           VkSemaphore signal_semaphore = VK_NULL_HANDLE,
                           VkSwapchainKHR present_swap_chain = VK_NULL_HANDLE,
                           uint32_t present_image_index = 0xFFFFFFFF);

  void ActivateCommandBuffer();

  void ExecuteCommandBuffer(bool submit_off_thread, bool wait_for_completion);

  // Was the last present submitted to the queue a failure? If so, we must recreate our swapchain.
  bool CheckLastPresentFail() { return m_present_failed_flag.TestAndClear(); }
  // Schedule a vulkan resource for destruction later on. This will occur when the command buffer
  // is next re-used, and the GPU has finished working with the specified resource.
  void DeferBufferDestruction(VkBuffer object);
  void DeferBufferViewDestruction(VkBufferView object);
  void DeferDeviceMemoryDestruction(VkDeviceMemory object);
  void DeferFramebufferDestruction(VkFramebuffer object);
  void DeferImageDestruction(VkImage object);
  void DeferImageViewDestruction(VkImageView object);

  // Instruct the manager to fire the specified callback when a fence is flagged to be signaled.
  // This happens when command buffers are executed, and can be tested if signaled, which means
  // that all commands up to the point when the callback was fired have completed.
  using CommandBufferQueuedCallback = std::function<void(VkCommandBuffer, VkFence)>;
  using CommandBufferExecutedCallback = std::function<void(VkFence)>;

  void AddFencePointCallback(const void* key, const CommandBufferQueuedCallback& queued_callback,
                             const CommandBufferExecutedCallback& executed_callback);

  void RemoveFencePointCallback(const void* key);

private:
  bool CreateCommandBuffers();
  void DestroyCommandBuffers();

  bool CreateSubmitThread();

  void SubmitCommandBuffer(size_t index, VkSemaphore wait_semaphore, VkSemaphore signal_semaphore,
                           VkSwapchainKHR present_swap_chain, uint32_t present_image_index);

  void OnCommandBufferExecuted(size_t index);

  struct FrameResources
  {
    // [0] - Init (upload) command buffer, [1] - draw command buffer
    VkCommandPool command_pool;
    std::array<VkCommandBuffer, 2> command_buffers;
    VkDescriptorPool descriptor_pool;
    VkFence fence;
    bool init_command_buffer_used;
    bool needs_fence_wait;

    std::vector<std::function<void()>> cleanup_resources;
  };

  std::array<FrameResources, NUM_COMMAND_BUFFERS> m_frame_resources = {};
  size_t m_current_frame;

  // callbacks when a fence point is set
  std::map<const void*, std::pair<CommandBufferQueuedCallback, CommandBufferExecutedCallback>>
      m_fence_point_callbacks;

  // Threaded command buffer execution
  // Semaphore determines when a command buffer can be queued
  Common::Semaphore m_submit_semaphore;
  std::thread m_submit_thread;
  std::unique_ptr<Common::BlockingLoop> m_submit_loop;
  struct PendingCommandBufferSubmit
  {
    size_t index;
    VkSemaphore wait_semaphore;
    VkSemaphore signal_semaphore;
    VkSwapchainKHR present_swap_chain;
    uint32_t present_image_index;
  };
  std::deque<PendingCommandBufferSubmit> m_pending_submits;
  std::mutex m_pending_submit_lock;
  Common::Flag m_present_failed_flag;
  bool m_use_threaded_submission = false;
};

extern std::unique_ptr<CommandBufferManager> g_command_buffer_mgr;

}  // namespace Vulkan
