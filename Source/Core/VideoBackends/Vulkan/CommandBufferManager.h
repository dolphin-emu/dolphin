// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "Common/BlockingLoop.h"
#include "Common/Semaphore.h"

#include "VideoCommon/VideoCommon.h"

#include "VideoBackends/Vulkan/Constants.h"
#include "VideoBackends/Vulkan/Helpers.h"
#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan
{
class CommandBufferManager
{
public:
  CommandBufferManager(VkDevice device, uint32_t graphics_queue_family_index,
                       VkQueue graphics_queue, bool use_threaded_submission);
  ~CommandBufferManager();

  bool Initialize();

  VkDevice GetDevice() const { return m_device; }
  VkCommandPool GetCommandPool() const { return m_command_pool; }
  VkCommandBuffer GetCurrentCommandBuffer() const
  {
    return m_command_buffers[m_current_command_buffer_index];
  }
  VkDescriptorPool GetCurrentDescriptorPool() const
  {
    return m_descriptor_pools[m_current_command_buffer_index];
  }

  VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout set_layout);

  void PrepareToSubmitCommandBuffer();

  void SubmitCommandBuffer(bool submit_off_thread);
  void SubmitCommandBufferAndPresent(VkSemaphore wait_semaphore, VkSemaphore signal_semaphore,
                                     VkSwapchainKHR present_swap_chain,
                                     uint32_t present_image_index, bool submit_off_thread);

  void ActivateCommandBuffer();

  void ExecuteCommandBuffer(bool submit_off_thread, bool wait_for_completion);

  // Schedule a vulkan resource for destruction later on. This will occur when the command buffer
  // is next re-used, and the GPU has finished working with the specified resource.
  template <typename T>
  void DeferResourceDestruction(T object)
  {
    DeferredResourceDestruction wrapper = DeferredResourceDestruction::Wrapper<T>(object);
    m_pending_destructions[m_current_command_buffer_index].push_back(wrapper);
  }

  // Instruct the manager to fire the specified callback when a fence is flagged to be signaled.
  // This happens when command buffers are executed, and can be tested if signaled, which means
  // that all commands up to the point when the callback was fired have completed.
  using FencePointCallback = std::function<void(VkFence)>;

  void AddFencePointCallback(const void* key, const FencePointCallback& created_callback,
                             const FencePointCallback& reached_callback);

  void RemoveFencePointCallback(const void* key);

private:
  bool CreateCommandPool();
  void DestroyCommandPool();

  bool CreateCommandBuffers();
  void DestroyCommandBuffers();

  bool CreateSubmitThread();

  void SubmitCommandBuffer(size_t index, VkSemaphore wait_semaphore, VkSemaphore signal_semaphore,
                           VkSwapchainKHR present_swap_chain, uint32_t present_image_index);

  VkDevice m_device = nullptr;
  uint32_t m_graphics_queue_family_index = 0;
  VkQueue m_graphics_queue = nullptr;
  VkCommandPool m_command_pool = nullptr;

  std::array<VkCommandBuffer, NUM_COMMAND_BUFFERS> m_command_buffers = {};
  std::array<VkFence, NUM_COMMAND_BUFFERS> m_fences = {};
  std::array<VkDescriptorPool, NUM_COMMAND_BUFFERS> m_descriptor_pools = {};
  std::array<std::vector<DeferredResourceDestruction>, NUM_COMMAND_BUFFERS> m_pending_destructions;
  size_t m_current_command_buffer_index;

  // callbacks when a fence point is set
  std::map<const void*, std::pair<FencePointCallback, FencePointCallback>> m_fence_point_callbacks;

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
  bool m_use_threaded_submission = false;
};

}  // namespace Vulkan
