// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cassert>
#include <cstdint>

#include "Common/MsgHandler.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/Helpers.h"

namespace Vulkan
{
CommandBufferManager::CommandBufferManager(
    VkDevice device, VkQueue graphics_queue, uint32_t graphics_queue_family_index,
    const VkQueueFamilyProperties& graphics_queue_family_properties, bool use_threaded_submission)
    : m_device(device), m_graphics_queue(graphics_queue),
      m_graphics_queue_family_index(graphics_queue_family_index),
      m_graphics_queue_family_properties(graphics_queue_family_properties),
      m_submit_semaphore(1, 1), m_use_threaded_submission(use_threaded_submission)
{
}

CommandBufferManager::~CommandBufferManager()
{
  // If the worker thread is enabled, wait for it to exit.
  if (m_use_threaded_submission)
  {
    // Wait for all command buffers to be consumed by the worker thread.
    m_submit_semaphore.Wait();
    m_submit_loop->Stop();
    m_submit_thread.join();
  }

  vkDeviceWaitIdle(m_device);

  DestroyCommandBuffers();
  DestroyCommandPool();
}

bool CommandBufferManager::Initialize()
{
  if (!CreateCommandPool())
    return false;

  if (!CreateCommandBuffers())
    return false;

  if (m_use_threaded_submission && !CreateSubmitThread())
    return false;

  return true;
}

bool CommandBufferManager::CreateCommandPool()
{
  VkCommandPoolCreateInfo info = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr,
                                  VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                                      VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                  m_graphics_queue_family_index};

  VkResult res = vkCreateCommandPool(m_device, &info, nullptr, &m_command_pool);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateCommandPool failed: ");
    return false;
  }

  return true;
}

void CommandBufferManager::DestroyCommandPool()
{
  if (m_command_pool)
  {
    vkDestroyCommandPool(m_device, m_command_pool, nullptr);
    m_command_pool = nullptr;
  }
}

bool CommandBufferManager::CreateCommandBuffers()
{
  for (size_t i = 0; i < m_frame_resources.size(); i++)
  {
    FrameResources& resources = m_frame_resources[i];
    VkCommandBufferAllocateInfo allocate_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, m_command_pool,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        static_cast<uint32_t>(ARRAYSIZE(resources.command_buffers))};

    VkResult res = vkAllocateCommandBuffers(m_device, &allocate_info, resources.command_buffers);
    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkAllocateCommandBuffers failed: ");
      return false;
    }

    VkFenceCreateInfo fence_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr,
                                    VK_FENCE_CREATE_SIGNALED_BIT};

    res = vkCreateFence(m_device, &fence_info, nullptr, &resources.fence);
    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkCreateFence failed: ");
      return false;
    }

    // TODO: Sort this mess out
    VkDescriptorPoolSize pool_sizes[] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 500000},
                                         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 500000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1024}};

    VkDescriptorPoolCreateInfo pool_create_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                   nullptr,
                                                   0,
                                                   100000,  // tweak this
                                                   ARRAYSIZE(pool_sizes),
                                                   pool_sizes};

    res = vkCreateDescriptorPool(m_device, &pool_create_info, nullptr, &resources.descriptor_pool);
    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkCreateDescriptorPool failed: ");
      return false;
    }
  }

  // Activate the first command buffer. ActivateCommandBuffer moves forward, so start with the last
  m_current_frame = m_frame_resources.size() - 1;
  ActivateCommandBuffer();
  return true;
}

void CommandBufferManager::DestroyCommandBuffers()
{
  for (size_t i = 0; i < m_frame_resources.size(); i++)
  {
    FrameResources& resources = m_frame_resources[i];
    for (const auto& it : resources.cleanup_resources)
      it.destroy_callback(m_device, it.object);
    resources.cleanup_resources.clear();

    if (resources.fence != VK_NULL_HANDLE)
    {
      vkDestroyFence(m_device, resources.fence, nullptr);
      resources.fence = VK_NULL_HANDLE;
    }
    if (resources.descriptor_pool != VK_NULL_HANDLE)
    {
      vkDestroyDescriptorPool(m_device, resources.descriptor_pool, nullptr);
      resources.descriptor_pool = VK_NULL_HANDLE;
    }
    if (resources.init_command_buffer != VK_NULL_HANDLE)
    {
      vkFreeCommandBuffers(m_device, m_command_pool, ARRAYSIZE(resources.command_buffers),
                           resources.command_buffers);

      resources.init_command_buffer = VK_NULL_HANDLE;
    }
  }
}

VkDescriptorSet CommandBufferManager::AllocateDescriptorSet(VkDescriptorSetLayout set_layout)
{
  VkDescriptorSetAllocateInfo allocate_info = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
      m_frame_resources[m_current_frame].descriptor_pool, 1, &set_layout};

  VkDescriptorSet descriptor_set;
  VkResult res = vkAllocateDescriptorSets(m_device, &allocate_info, &descriptor_set);
  if (res != VK_SUCCESS)
  {
    // Failing to allocate a descriptor set is not a fatal error, we can
    // recover by moving to the next command buffer.
    return VK_NULL_HANDLE;
  }

  return descriptor_set;
}

bool CommandBufferManager::CreateSubmitThread()
{
  m_submit_loop = std::make_unique<Common::BlockingLoop>();
  m_submit_thread = std::thread([this]() {
    m_submit_loop->Run([this]() {
      PendingCommandBufferSubmit submit;
      {
        std::lock_guard<std::mutex> guard(m_pending_submit_lock);
        if (m_pending_submits.empty())
        {
          m_submit_loop->AllowSleep();
          return;
        }

        submit = m_pending_submits.front();
        m_pending_submits.pop_front();
      }

      SubmitCommandBuffer(submit.index, submit.wait_semaphore, submit.signal_semaphore,
                          submit.present_swap_chain, submit.present_image_index);
    });
  });

  return true;
}

void CommandBufferManager::PrepareToSubmitCommandBuffer()
{
  // Grab the semaphore before submitting command buffer either on-thread or off-thread.
  // This prevents a race from occurring where a second command buffer is executed
  // before the worker thread has woken and executed the first one yet.
  m_submit_semaphore.Wait();
}

void CommandBufferManager::WaitForWorkerThreadIdle()
{
  // Drain the semaphore, then allow another request in the future.
  m_submit_semaphore.Wait();
  m_submit_semaphore.Post();
}

void CommandBufferManager::WaitForGPUIdle()
{
  WaitForWorkerThreadIdle();
  vkDeviceWaitIdle(m_device);
}

void CommandBufferManager::SubmitCommandBuffer(bool submit_off_thread)
{
  // End the current command buffer.
  // TODO: Can we move this off-thread?
  VkResult res = vkEndCommandBuffer(m_frame_resources[m_current_frame].init_command_buffer);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkEndCommandBuffer failed: ");
    PanicAlert("Failed to end command buffer");
  }
  res = vkEndCommandBuffer(m_frame_resources[m_current_frame].draw_command_buffer);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkEndCommandBuffer failed: ");
    PanicAlert("Failed to end command buffer");
  }

  // Submitting off-thread?
  if (m_use_threaded_submission && submit_off_thread)
  {
    // Push to the pending submit queue.
    {
      std::lock_guard<std::mutex> guard(m_pending_submit_lock);
      m_pending_submits.push_back(
          {m_current_frame, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0});
    }

    // Wake up the worker thread for a single iteration.
    m_submit_loop->Wakeup();
  }
  else
  {
    // Pass through to normal submission path.
    SubmitCommandBuffer(m_current_frame, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0);
  }

  // Fire fence tracking callbacks. This can't happen on the worker thread.
  for (const auto& iter : m_fence_point_callbacks)
    iter.second.first(m_frame_resources[m_current_frame].fence);
}

void CommandBufferManager::SubmitCommandBufferAndPresent(VkSemaphore wait_semaphore,
                                                         VkSemaphore signal_semaphore,
                                                         VkSwapchainKHR present_swap_chain,
                                                         uint32_t present_image_index,
                                                         bool submit_off_thread)
{
  // End the current command buffer.
  // TODO: Can we move this off-thread?
  VkResult res = vkEndCommandBuffer(m_frame_resources[m_current_frame].init_command_buffer);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkEndCommandBuffer failed: ");
    PanicAlert("Failed to end command buffer");
  }
  res = vkEndCommandBuffer(m_frame_resources[m_current_frame].draw_command_buffer);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkEndCommandBuffer failed: ");
    PanicAlert("Failed to end command buffer");
  }

  // Submitting off-thread?
  if (m_use_threaded_submission && submit_off_thread)
  {
    // Push to the pending submit queue.
    {
      std::lock_guard<std::mutex> guard(m_pending_submit_lock);
      m_pending_submits.push_back({m_current_frame, wait_semaphore, signal_semaphore,
                                   present_swap_chain, present_image_index});
    }

    // Wake up the worker thread for a single iteration.
    m_submit_loop->Wakeup();
  }
  else
  {
    // Pass through to normal submission path.
    SubmitCommandBuffer(m_current_frame, wait_semaphore, signal_semaphore, present_swap_chain,
                        present_image_index);
  }

  // Fire fence tracking callbacks. This can't happen on the worker thread.
  for (const auto& iter : m_fence_point_callbacks)
    iter.second.first(m_frame_resources[m_current_frame].fence);
}

void CommandBufferManager::SubmitCommandBuffer(size_t index, VkSemaphore wait_semaphore,
                                               VkSemaphore signal_semaphore,
                                               VkSwapchainKHR present_swap_chain,
                                               uint32_t present_image_index)
{
  // This may be executed on the worker thread, so don't modify any state of the manager class.
  uint32_t wait_bits = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO,
                              nullptr,
                              0,
                              nullptr,
                              &wait_bits,
                              ARRAYSIZE(m_frame_resources[index].command_buffers),
                              m_frame_resources[index].command_buffers,
                              0,
                              nullptr};

  if (wait_semaphore != VK_NULL_HANDLE)
  {
    submit_info.pWaitSemaphores = &wait_semaphore;
    submit_info.waitSemaphoreCount = 1;
  }

  if (signal_semaphore != VK_NULL_HANDLE)
  {
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &signal_semaphore;
  }

  VkResult res = vkQueueSubmit(m_graphics_queue, 1, &submit_info, m_frame_resources[index].fence);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkQueueSubmit failed: ");
    PanicAlert("Failed to submit command buffer.");
  }

  // Do we have a swap chain to present?
  if (present_swap_chain != VK_NULL_HANDLE)
  {
    // Should have a signal semaphore.
    assert(signal_semaphore != VK_NULL_HANDLE);
    VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                     nullptr,
                                     1,
                                     &signal_semaphore,
                                     1,
                                     &present_swap_chain,
                                     &present_image_index,
                                     nullptr};

    res = vkQueuePresentKHR(m_graphics_queue, &present_info);
    if (res != VK_SUCCESS && res != VK_ERROR_OUT_OF_DATE_KHR && res != VK_SUBOPTIMAL_KHR)
      LOG_VULKAN_ERROR(res, "vkQueuePresentKHR failed: ");
  }

  // Command buffer has been queued, so permit the next one.
  m_submit_semaphore.Post();
}

void CommandBufferManager::ActivateCommandBuffer()
{
  // Move to the next command buffer.
  m_current_frame = (m_current_frame + 1) % NUM_COMMAND_BUFFERS;
  FrameResources& resources = m_frame_resources[m_current_frame];

  VkResult res = vkWaitForFences(m_device, 1, &resources.fence, true, UINT64_MAX);
  if (res != VK_SUCCESS)
    LOG_VULKAN_ERROR(res, "vkWaitForFences failed: ");

  res = vkResetFences(m_device, 1, &resources.fence);
  if (res != VK_SUCCESS)
    LOG_VULKAN_ERROR(res, "vkResetFences failed: ");

  // Fire fence tracking callbacks.
  for (const auto& iter : m_fence_point_callbacks)
    iter.second.second(resources.fence);

  // Clean up all objects pending destruction on this command buffer
  for (const auto& it : resources.cleanup_resources)
    it.destroy_callback(m_device, it.object);
  resources.cleanup_resources.clear();

  // Reset command buffer to beginning since we can re-use the memory now
  res = vkResetCommandBuffer(resources.init_command_buffer, 0);
  if (res != VK_SUCCESS)
    LOG_VULKAN_ERROR(res, "vkResetCommandBuffer failed: ");
  res = vkResetCommandBuffer(resources.draw_command_buffer, 0);
  if (res != VK_SUCCESS)
    LOG_VULKAN_ERROR(res, "vkResetCommandBuffer failed: ");

  // Also can do the same for the descriptor pools
  res = vkResetDescriptorPool(m_device, resources.descriptor_pool, 0);
  if (res != VK_SUCCESS)
    LOG_VULKAN_ERROR(res, "vkResetDescriptorPool failed: ");

  VkCommandBufferBeginInfo begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
                                         VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr};

  res = vkBeginCommandBuffer(resources.init_command_buffer, &begin_info);
  if (res != VK_SUCCESS)
    LOG_VULKAN_ERROR(res, "vkBeginCommandBuffer failed: ");
  res = vkBeginCommandBuffer(resources.draw_command_buffer, &begin_info);
  if (res != VK_SUCCESS)
    LOG_VULKAN_ERROR(res, "vkBeginCommandBuffer failed: ");
}

void CommandBufferManager::ExecuteCommandBuffer(bool submit_off_thread, bool wait_for_completion)
{
  PrepareToSubmitCommandBuffer();

  // If we're waiting for completion, don't bother waking the worker thread.
  SubmitCommandBuffer((submit_off_thread && wait_for_completion));

  if (wait_for_completion)
  {
    VkResult res = vkWaitForFences(m_device, 1, &m_frame_resources[m_current_frame].fence, VK_TRUE,
                                   UINT64_MAX);
    if (res != VK_SUCCESS)
      LOG_VULKAN_ERROR(res, "vkWaitForFences failed: ");
  }

  ActivateCommandBuffer();
}

void CommandBufferManager::AddFencePointCallback(const void* key,
                                                 const FencePointCallback& created_callback,
                                                 const FencePointCallback& reached_callback)
{
  // Shouldn't be adding twice.
  assert(m_fence_point_callbacks.find(key) == m_fence_point_callbacks.end());
  m_fence_point_callbacks.emplace(key, std::make_pair(created_callback, reached_callback));
}

void CommandBufferManager::RemoveFencePointCallback(const void* key)
{
  auto iter = m_fence_point_callbacks.find(key);
  assert(iter != m_fence_point_callbacks.end());
  m_fence_point_callbacks.erase(iter);
}

std::unique_ptr<CommandBufferManager> g_command_buffer_mgr;
}
