// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Vulkan/CommandBufferManager.h"

#include <array>
#include <cstdint>

#include "Common/Assert.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Vulkan/VulkanContext.h"

namespace Vulkan
{
CommandBufferManager::CommandBufferManager(bool use_threaded_submission)
    : m_submit_semaphore(1, 1), m_use_threaded_submission(use_threaded_submission)
{
}

CommandBufferManager::~CommandBufferManager()
{
  // If the worker thread is enabled, stop and block until it exits.
  if (m_use_threaded_submission)
  {
    m_submit_loop->Stop();
    m_submit_thread.join();
  }

  DestroyCommandBuffers();
}

bool CommandBufferManager::Initialize()
{
  if (!CreateCommandBuffers())
    return false;

  if (m_use_threaded_submission && !CreateSubmitThread())
    return false;

  return true;
}

bool CommandBufferManager::CreateCommandBuffers()
{
  static constexpr VkSemaphoreCreateInfo semaphore_create_info = {
      VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0};

  VkDevice device = g_vulkan_context->GetDevice();
  VkResult res;

  for (FrameResources& resources : m_frame_resources)
  {
    resources.init_command_buffer_used = false;
    resources.semaphore_used = false;

    VkCommandPoolCreateInfo pool_info = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, 0,
                                         g_vulkan_context->GetGraphicsQueueFamilyIndex()};
    res = vkCreateCommandPool(g_vulkan_context->GetDevice(), &pool_info, nullptr,
                              &resources.command_pool);
    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkCreateCommandPool failed: ");
      return false;
    }

    VkCommandBufferAllocateInfo buffer_info = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, resources.command_pool,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY, static_cast<uint32_t>(resources.command_buffers.size())};

    res = vkAllocateCommandBuffers(device, &buffer_info, resources.command_buffers.data());
    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkAllocateCommandBuffers failed: ");
      return false;
    }

    VkFenceCreateInfo fence_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr,
                                    VK_FENCE_CREATE_SIGNALED_BIT};

    res = vkCreateFence(device, &fence_info, nullptr, &resources.fence);
    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkCreateFence failed: ");
      return false;
    }

    res = vkCreateSemaphore(device, &semaphore_create_info, nullptr, &resources.semaphore);
    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkCreateSemaphore failed: ");
      return false;
    }

    // TODO: A better way to choose the number of descriptors.
    const std::array<VkDescriptorPoolSize, 5> pool_sizes{{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 500000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 500000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 16384},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 16384},
    }};

    const VkDescriptorPoolCreateInfo pool_create_info = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        nullptr,
        0,
        100000,  // tweak this
        static_cast<u32>(pool_sizes.size()),
        pool_sizes.data(),
    };

    res = vkCreateDescriptorPool(device, &pool_create_info, nullptr, &resources.descriptor_pool);
    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkCreateDescriptorPool failed: ");
      return false;
    }
  }

  res = vkCreateSemaphore(device, &semaphore_create_info, nullptr, &m_present_semaphore);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateSemaphore failed: ");
    return false;
  }

  // Activate the first command buffer. ActivateCommandBuffer moves forward, so start with the last
  m_current_frame = static_cast<u32>(m_frame_resources.size()) - 1;
  BeginCommandBuffer();
  return true;
}

void CommandBufferManager::DestroyCommandBuffers()
{
  VkDevice device = g_vulkan_context->GetDevice();

  for (FrameResources& resources : m_frame_resources)
  {
    // The Vulkan spec section 5.2 says: "When a pool is destroyed, all command buffers allocated
    // from the pool are freed.". So we don't need to free the command buffers, just the pools.
    // We destroy the command pool first, to avoid any warnings from the validation layers about
    // objects which are pending destruction being in-use.
    if (resources.command_pool != VK_NULL_HANDLE)
      vkDestroyCommandPool(device, resources.command_pool, nullptr);

    // Destroy any pending objects.
    for (auto& it : resources.cleanup_resources)
      it();

    if (resources.semaphore != VK_NULL_HANDLE)
      vkDestroySemaphore(device, resources.semaphore, nullptr);

    if (resources.fence != VK_NULL_HANDLE)
      vkDestroyFence(device, resources.fence, nullptr);

    if (resources.descriptor_pool != VK_NULL_HANDLE)
      vkDestroyDescriptorPool(device, resources.descriptor_pool, nullptr);
  }

  vkDestroySemaphore(device, m_present_semaphore, nullptr);
}

VkDescriptorSet CommandBufferManager::AllocateDescriptorSet(VkDescriptorSetLayout set_layout)
{
  VkDescriptorSetAllocateInfo allocate_info = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
      m_frame_resources[m_current_frame].descriptor_pool, 1, &set_layout};

  VkDescriptorSet descriptor_set;
  VkResult res =
      vkAllocateDescriptorSets(g_vulkan_context->GetDevice(), &allocate_info, &descriptor_set);
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

      SubmitCommandBuffer(submit.command_buffer_index, submit.present_swap_chain,
                          submit.present_image_index);
    });
  });

  return true;
}

void CommandBufferManager::WaitForWorkerThreadIdle()
{
  // Drain the semaphore, then allow another request in the future.
  m_submit_semaphore.Wait();
  m_submit_semaphore.Post();
}

void CommandBufferManager::WaitForFenceCounter(u64 fence_counter)
{
  if (m_completed_fence_counter >= fence_counter)
    return;

  // Find the first command buffer which covers this counter value.
  u32 index = (m_current_frame + 1) % NUM_COMMAND_BUFFERS;
  while (index != m_current_frame)
  {
    if (m_frame_resources[index].fence_counter >= fence_counter)
      break;

    index = (index + 1) % NUM_COMMAND_BUFFERS;
  }

  ASSERT(index != m_current_frame);
  WaitForCommandBufferCompletion(index);
}

void CommandBufferManager::WaitForCommandBufferCompletion(u32 index)
{
  // Ensure this command buffer has been submitted.
  WaitForWorkerThreadIdle();

  // Wait for this command buffer to be completed.
  VkResult res = vkWaitForFences(g_vulkan_context->GetDevice(), 1, &m_frame_resources[index].fence,
                                 VK_TRUE, UINT64_MAX);
  if (res != VK_SUCCESS)
    LOG_VULKAN_ERROR(res, "vkWaitForFences failed: ");

  // Clean up any resources for command buffers between the last known completed buffer and this
  // now-completed command buffer. If we use >2 buffers, this may be more than one buffer.
  const u64 now_completed_counter = m_frame_resources[index].fence_counter;
  u32 cleanup_index = (m_current_frame + 1) % NUM_COMMAND_BUFFERS;
  while (cleanup_index != m_current_frame)
  {
    FrameResources& resources = m_frame_resources[cleanup_index];
    if (resources.fence_counter > now_completed_counter)
      break;

    if (resources.fence_counter > m_completed_fence_counter)
    {
      for (auto& it : resources.cleanup_resources)
        it();
      resources.cleanup_resources.clear();
    }

    cleanup_index = (cleanup_index + 1) % NUM_COMMAND_BUFFERS;
  }

  m_completed_fence_counter = now_completed_counter;
}

void CommandBufferManager::SubmitCommandBuffer(bool submit_on_worker_thread,
                                               bool wait_for_completion,
                                               VkSwapchainKHR present_swap_chain,
                                               uint32_t present_image_index)
{
  // End the current command buffer.
  FrameResources& resources = m_frame_resources[m_current_frame];
  for (VkCommandBuffer command_buffer : resources.command_buffers)
  {
    VkResult res = vkEndCommandBuffer(command_buffer);
    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkEndCommandBuffer failed: ");
      PanicAlert("Failed to end command buffer");
    }
  }

  // Grab the semaphore before submitting command buffer either on-thread or off-thread.
  // This prevents a race from occurring where a second command buffer is executed
  // before the worker thread has woken and executed the first one yet.
  m_submit_semaphore.Wait();

  // Submitting off-thread?
  if (m_use_threaded_submission && submit_on_worker_thread && !wait_for_completion)
  {
    // Push to the pending submit queue.
    {
      std::lock_guard<std::mutex> guard(m_pending_submit_lock);
      m_pending_submits.push_back({present_swap_chain, present_image_index, m_current_frame});
    }

    // Wake up the worker thread for a single iteration.
    m_submit_loop->Wakeup();
  }
  else
  {
    // Pass through to normal submission path.
    SubmitCommandBuffer(m_current_frame, present_swap_chain, present_image_index);
    if (wait_for_completion)
      WaitForCommandBufferCompletion(m_current_frame);
  }

  // Switch to next cmdbuffer.
  BeginCommandBuffer();
}

void CommandBufferManager::SubmitCommandBuffer(u32 command_buffer_index,
                                               VkSwapchainKHR present_swap_chain,
                                               u32 present_image_index)
{
  FrameResources& resources = m_frame_resources[command_buffer_index];

  // This may be executed on the worker thread, so don't modify any state of the manager class.
  uint32_t wait_bits = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO,
                              nullptr,
                              0,
                              nullptr,
                              &wait_bits,
                              static_cast<u32>(resources.command_buffers.size()),
                              resources.command_buffers.data(),
                              0,
                              nullptr};

  // If the init command buffer did not have any commands recorded, don't submit it.
  if (!resources.init_command_buffer_used)
  {
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &resources.command_buffers[1];
  }

  if (resources.semaphore_used)
  {
    submit_info.pWaitSemaphores = &resources.semaphore;
    submit_info.waitSemaphoreCount = 1;
  }

  if (present_swap_chain != VK_NULL_HANDLE)
  {
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &m_present_semaphore;
  }

  VkResult res =
      vkQueueSubmit(g_vulkan_context->GetGraphicsQueue(), 1, &submit_info, resources.fence);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkQueueSubmit failed: ");
    PanicAlert("Failed to submit command buffer.");
  }

  // Do we have a swap chain to present?
  if (present_swap_chain != VK_NULL_HANDLE)
  {
    // Should have a signal semaphore.
    VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                     nullptr,
                                     1,
                                     &m_present_semaphore,
                                     1,
                                     &present_swap_chain,
                                     &present_image_index,
                                     nullptr};

    res = vkQueuePresentKHR(g_vulkan_context->GetPresentQueue(), &present_info);
    if (res != VK_SUCCESS)
    {
      // VK_ERROR_OUT_OF_DATE_KHR is not fatal, just means we need to recreate our swap chain.
      if (res != VK_ERROR_OUT_OF_DATE_KHR && res != VK_SUBOPTIMAL_KHR)
        LOG_VULKAN_ERROR(res, "vkQueuePresentKHR failed: ");
      m_present_failed_flag.Set();
    }
  }

  // Command buffer has been queued, so permit the next one.
  m_submit_semaphore.Post();
}

void CommandBufferManager::BeginCommandBuffer()
{
  // Move to the next command buffer.
  const u32 next_buffer_index = (m_current_frame + 1) % NUM_COMMAND_BUFFERS;
  FrameResources& resources = m_frame_resources[next_buffer_index];

  // Wait for the GPU to finish with all resources for this command buffer.
  if (resources.fence_counter > m_completed_fence_counter)
    WaitForCommandBufferCompletion(next_buffer_index);

  // Reset fence to unsignaled before starting.
  VkResult res = vkResetFences(g_vulkan_context->GetDevice(), 1, &resources.fence);
  if (res != VK_SUCCESS)
    LOG_VULKAN_ERROR(res, "vkResetFences failed: ");

  // Reset command pools to beginning since we can re-use the memory now
  res = vkResetCommandPool(g_vulkan_context->GetDevice(), resources.command_pool, 0);
  if (res != VK_SUCCESS)
    LOG_VULKAN_ERROR(res, "vkResetCommandPool failed: ");

  // Enable commands to be recorded to the two buffers again.
  VkCommandBufferBeginInfo begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
                                         VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr};
  for (VkCommandBuffer command_buffer : resources.command_buffers)
  {
    res = vkBeginCommandBuffer(command_buffer, &begin_info);
    if (res != VK_SUCCESS)
      LOG_VULKAN_ERROR(res, "vkBeginCommandBuffer failed: ");
  }

  // Also can do the same for the descriptor pools
  res = vkResetDescriptorPool(g_vulkan_context->GetDevice(), resources.descriptor_pool, 0);
  if (res != VK_SUCCESS)
    LOG_VULKAN_ERROR(res, "vkResetDescriptorPool failed: ");

  // Reset upload command buffer state
  resources.init_command_buffer_used = false;
  resources.semaphore_used = false;
  resources.fence_counter = m_next_fence_counter++;
  m_current_frame = next_buffer_index;
}

void CommandBufferManager::DeferBufferDestruction(VkBuffer object)
{
  FrameResources& resources = m_frame_resources[m_current_frame];
  resources.cleanup_resources.push_back(
      [object]() { vkDestroyBuffer(g_vulkan_context->GetDevice(), object, nullptr); });
}

void CommandBufferManager::DeferBufferViewDestruction(VkBufferView object)
{
  FrameResources& resources = m_frame_resources[m_current_frame];
  resources.cleanup_resources.push_back(
      [object]() { vkDestroyBufferView(g_vulkan_context->GetDevice(), object, nullptr); });
}

void CommandBufferManager::DeferDeviceMemoryDestruction(VkDeviceMemory object)
{
  FrameResources& resources = m_frame_resources[m_current_frame];
  resources.cleanup_resources.push_back(
      [object]() { vkFreeMemory(g_vulkan_context->GetDevice(), object, nullptr); });
}

void CommandBufferManager::DeferFramebufferDestruction(VkFramebuffer object)
{
  FrameResources& resources = m_frame_resources[m_current_frame];
  resources.cleanup_resources.push_back(
      [object]() { vkDestroyFramebuffer(g_vulkan_context->GetDevice(), object, nullptr); });
}

void CommandBufferManager::DeferImageDestruction(VkImage object)
{
  FrameResources& resources = m_frame_resources[m_current_frame];
  resources.cleanup_resources.push_back(
      [object]() { vkDestroyImage(g_vulkan_context->GetDevice(), object, nullptr); });
}

void CommandBufferManager::DeferImageViewDestruction(VkImageView object)
{
  FrameResources& resources = m_frame_resources[m_current_frame];
  resources.cleanup_resources.push_back(
      [object]() { vkDestroyImageView(g_vulkan_context->GetDevice(), object, nullptr); });
}

std::unique_ptr<CommandBufferManager> g_command_buffer_mgr;
}  // namespace Vulkan
