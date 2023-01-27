// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Vulkan/CommandBufferManager.h"

#include <array>
#include <cstdint>

#include "Common/Assert.h"
#include "Common/MsgHandler.h"
#include "Common/Thread.h"

#include "VideoBackends/Vulkan/VulkanContext.h"
#include "VideoCommon/Constants.h"

namespace Vulkan
{
CommandBufferManager::CommandBufferManager(bool use_threaded_submission)
    : m_use_threaded_submission(use_threaded_submission)
{
}

CommandBufferManager::~CommandBufferManager()
{
  // If the worker thread is enabled, stop and block until it exits.
  if (m_use_threaded_submission)
  {
    WaitForWorkerThreadIdle();
    m_submit_thread.Shutdown();
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

  for (CmdBufferResources& resources : m_command_buffers)
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
  }

  res = vkCreateSemaphore(device, &semaphore_create_info, nullptr, &m_present_semaphore);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateSemaphore failed: ");
    return false;
  }

  // Activate the first command buffer. BeginCommandBuffer moves forward, so start with the last
  m_current_cmd_buffer = static_cast<u32>(m_command_buffers.size()) - 1;
  BeginCommandBuffer();
  return true;
}

void CommandBufferManager::DestroyCommandBuffers()
{
  VkDevice device = g_vulkan_context->GetDevice();

  for (CmdBufferResources& resources : m_command_buffers)
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
  }

  for (FrameResources& resources : m_frame_resources)
  {
    for (VkDescriptorPool descriptor_pool : resources.descriptor_pools)
    {
      vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
    }
  }

  vkDestroySemaphore(device, m_present_semaphore, nullptr);
}

VkDescriptorPool CommandBufferManager::CreateDescriptorPool(u32 max_descriptor_sets)
{
  /*
   * Worst case descriptor counts according to the descriptor layout created in ObjectCache.cpp:
   * UNIFORM_BUFFER_DYNAMIC: 3
   * COMBINED_IMAGE_SAMPLER: NUM_UTILITY_PIXEL_SAMPLERS + NUM_COMPUTE_SHADER_SAMPLERS +
   * VideoCommon::MAX_PIXEL_SHADER_SAMPLERS
   * STORAGE_BUFFER: 2
   * UNIFORM_TEXEL_BUFFER: 3
   * STORAGE_IMAGE: 1
   */
  const std::array<VkDescriptorPoolSize, 5> pool_sizes{{
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, max_descriptor_sets * 3},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
       max_descriptor_sets * (VideoCommon::MAX_PIXEL_SHADER_SAMPLERS + NUM_COMPUTE_SHADER_SAMPLERS +
                              NUM_UTILITY_PIXEL_SAMPLERS)},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, max_descriptor_sets * 2},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, max_descriptor_sets * 3},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, max_descriptor_sets * 1},
  }};

  const VkDescriptorPoolCreateInfo pool_create_info = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr,           0, max_descriptor_sets,
      static_cast<u32>(pool_sizes.size()),           pool_sizes.data(),
  };

  VkDevice device = g_vulkan_context->GetDevice();
  VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
  VkResult res = vkCreateDescriptorPool(device, &pool_create_info, nullptr, &descriptor_pool);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateDescriptorPool failed: ");
    return VK_NULL_HANDLE;
  }
  return descriptor_pool;
}

VkDescriptorSet CommandBufferManager::AllocateDescriptorSet(VkDescriptorSetLayout set_layout)
{
  VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
  FrameResources& resources = GetCurrentFrameResources();

  if (!resources.descriptor_pools.empty()) [[likely]]
  {
    VkDescriptorSetAllocateInfo allocate_info = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
        resources.descriptor_pools[resources.current_descriptor_pool_index], 1, &set_layout};

    VkResult res =
        vkAllocateDescriptorSets(g_vulkan_context->GetDevice(), &allocate_info, &descriptor_set);
    if (res != VK_SUCCESS &&
        resources.descriptor_pools.size() > resources.current_descriptor_pool_index + 1)
    {
      // Mark the next descriptor set as active and try again.
      resources.current_descriptor_pool_index++;
      descriptor_set = AllocateDescriptorSet(set_layout);
    }
  }

  if (descriptor_set == VK_NULL_HANDLE) [[unlikely]]
  {
    VkDescriptorPool descriptor_pool = CreateDescriptorPool(DESCRIPTOR_SETS_PER_POOL);
    m_descriptor_set_count += DESCRIPTOR_SETS_PER_POOL;
    if (descriptor_pool == VK_NULL_HANDLE) [[unlikely]]
      return VK_NULL_HANDLE;

    resources.descriptor_pools.push_back(descriptor_pool);
    resources.current_descriptor_pool_index =
        static_cast<u32>(resources.descriptor_pools.size()) - 1;
    descriptor_set = AllocateDescriptorSet(set_layout);
  }

  return descriptor_set;
}

bool CommandBufferManager::CreateSubmitThread()
{
  m_submit_thread.Reset("VK submission thread", [this](PendingCommandBufferSubmit submit) {
    SubmitCommandBuffer(submit.command_buffer_index, submit.present_swap_chain,
                        submit.present_image_index);
    CmdBufferResources& resources = m_command_buffers[submit.command_buffer_index];
    resources.waiting_for_submit.store(false, std::memory_order_release);
  });

  return true;
}

void CommandBufferManager::WaitForWorkerThreadIdle()
{
  if (!m_use_threaded_submission)
    return;

  m_submit_thread.WaitForCompletion();
}

void CommandBufferManager::WaitForFenceCounter(u64 fence_counter)
{
  if (m_completed_fence_counter >= fence_counter)
    return;

  // Find the first command buffer which covers this counter value.
  u32 index = (m_current_cmd_buffer + 1) % NUM_COMMAND_BUFFERS;
  while (index != m_current_cmd_buffer)
  {
    if (m_command_buffers[index].fence_counter >= fence_counter)
      break;

    index = (index + 1) % NUM_COMMAND_BUFFERS;
  }

  ASSERT(index != m_current_cmd_buffer);
  WaitForCommandBufferCompletion(index);
}

void CommandBufferManager::WaitForCommandBufferCompletion(u32 index)
{
  CmdBufferResources& resources = m_command_buffers[index];

  // Ensure this command buffer has been submitted.
  if (resources.waiting_for_submit.load(std::memory_order_acquire))
  {
    WaitForWorkerThreadIdle();
    ASSERT_MSG(VIDEO, !resources.waiting_for_submit.load(std::memory_order_relaxed),
               "Submit thread is idle but command buffer is still waiting for submission!");
  }

  // Wait for this command buffer to be completed.
  VkResult res =
      vkWaitForFences(g_vulkan_context->GetDevice(), 1, &resources.fence, VK_TRUE, UINT64_MAX);
  if (res != VK_SUCCESS)
    LOG_VULKAN_ERROR(res, "vkWaitForFences failed: ");

  // Clean up any resources for command buffers between the last known completed buffer and this
  // now-completed command buffer. If we use >2 buffers, this may be more than one buffer.
  const u64 now_completed_counter = resources.fence_counter;
  u32 cleanup_index = (m_current_cmd_buffer + 1) % NUM_COMMAND_BUFFERS;
  while (cleanup_index != m_current_cmd_buffer)
  {
    CmdBufferResources& cleanup_resources = m_command_buffers[cleanup_index];
    if (cleanup_resources.fence_counter > now_completed_counter)
      break;

    if (cleanup_resources.fence_counter > m_completed_fence_counter)
    {
      for (auto& it : cleanup_resources.cleanup_resources)
        it();
      cleanup_resources.cleanup_resources.clear();
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
  CmdBufferResources& resources = GetCurrentCmdBufferResources();
  for (VkCommandBuffer command_buffer : resources.command_buffers)
  {
    VkResult res = vkEndCommandBuffer(command_buffer);
    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkEndCommandBuffer failed: ");
      PanicAlertFmt("Failed to end command buffer: {} ({})", VkResultToString(res),
                    static_cast<int>(res));
    }
  }

  // Submitting off-thread?
  if (m_use_threaded_submission && submit_on_worker_thread && !wait_for_completion)
  {
    resources.waiting_for_submit.store(true, std::memory_order_relaxed);
    // Push to the pending submit queue.
    m_submit_thread.Push({present_swap_chain, present_image_index, m_current_cmd_buffer});
  }
  else
  {
    WaitForWorkerThreadIdle();

    // Pass through to normal submission path.
    SubmitCommandBuffer(m_current_cmd_buffer, present_swap_chain, present_image_index);
    if (wait_for_completion)
      WaitForCommandBufferCompletion(m_current_cmd_buffer);
  }

  if (present_swap_chain != VK_NULL_HANDLE)
  {
    m_current_frame = (m_current_frame + 1) % NUM_FRAMES_IN_FLIGHT;

    // Wait for all command buffers that used the descriptor pool to finish
    u32 cmd_buffer_index = (m_current_cmd_buffer + 1) % NUM_COMMAND_BUFFERS;
    while (cmd_buffer_index != m_current_cmd_buffer)
    {
      CmdBufferResources& cmd_buffer = m_command_buffers[cmd_buffer_index];
      if (cmd_buffer.frame_index == m_current_frame && cmd_buffer.fence_counter != 0 &&
          cmd_buffer.fence_counter > m_completed_fence_counter)
      {
        WaitForCommandBufferCompletion(cmd_buffer_index);
      }
      cmd_buffer_index = (cmd_buffer_index + 1) % NUM_COMMAND_BUFFERS;
    }

    // Reset the descriptor pools
    FrameResources& frame_resources = GetCurrentFrameResources();

    if (frame_resources.descriptor_pools.size() == 1) [[likely]]
    {
      VkResult res = vkResetDescriptorPool(g_vulkan_context->GetDevice(),
                                           frame_resources.descriptor_pools[0], 0);
      if (res != VK_SUCCESS)
        LOG_VULKAN_ERROR(res, "vkResetDescriptorPool failed: ");
    }
    else [[unlikely]]
    {
      for (VkDescriptorPool descriptor_pool : frame_resources.descriptor_pools)
      {
        vkDestroyDescriptorPool(g_vulkan_context->GetDevice(), descriptor_pool, nullptr);
      }
      frame_resources.descriptor_pools.clear();
      VkDescriptorPool descriptor_pool = CreateDescriptorPool(m_descriptor_set_count);
      if (descriptor_pool != VK_NULL_HANDLE) [[likely]]
        frame_resources.descriptor_pools.push_back(descriptor_pool);
    }

    frame_resources.current_descriptor_pool_index = 0;
  }

  // Switch to next cmdbuffer.
  BeginCommandBuffer();
}

void CommandBufferManager::SubmitCommandBuffer(u32 command_buffer_index,
                                               VkSwapchainKHR present_swap_chain,
                                               u32 present_image_index)
{
  CmdBufferResources& resources = m_command_buffers[command_buffer_index];

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
    PanicAlertFmt("Failed to submit command buffer: {} ({})", VkResultToString(res),
                  static_cast<int>(res));
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

    m_last_present_result = vkQueuePresentKHR(g_vulkan_context->GetPresentQueue(), &present_info);
    m_last_present_done.Set();
    if (m_last_present_result != VK_SUCCESS)
    {
      // VK_ERROR_OUT_OF_DATE_KHR is not fatal, just means we need to recreate our swap chain.
      if (m_last_present_result != VK_ERROR_OUT_OF_DATE_KHR &&
          m_last_present_result != VK_SUBOPTIMAL_KHR &&
          m_last_present_result != VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT)
      {
        LOG_VULKAN_ERROR(m_last_present_result, "vkQueuePresentKHR failed: ");
      }

      // Don't treat VK_SUBOPTIMAL_KHR as fatal on Android. Android 10+ requires prerotation.
      // See https://twitter.com/Themaister/status/1207062674011574273
#ifdef VK_USE_PLATFORM_ANDROID_KHR
      if (m_last_present_result != VK_SUBOPTIMAL_KHR)
        m_last_present_failed.Set();
#else
      m_last_present_failed.Set();
#endif
    }
  }
}

void CommandBufferManager::BeginCommandBuffer()
{
  // Move to the next command buffer.
  const u32 next_buffer_index = (m_current_cmd_buffer + 1) % NUM_COMMAND_BUFFERS;
  CmdBufferResources& resources = m_command_buffers[next_buffer_index];

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

  // Reset upload command buffer state
  resources.init_command_buffer_used = false;
  resources.semaphore_used = false;
  resources.fence_counter = m_next_fence_counter++;
  resources.frame_index = m_current_frame;
  m_current_cmd_buffer = next_buffer_index;
}

void CommandBufferManager::DeferBufferViewDestruction(VkBufferView object)
{
  CmdBufferResources& cmd_buffer_resources = GetCurrentCmdBufferResources();
  cmd_buffer_resources.cleanup_resources.push_back(
      [object]() { vkDestroyBufferView(g_vulkan_context->GetDevice(), object, nullptr); });
}

void CommandBufferManager::DeferBufferDestruction(VkBuffer buffer, VmaAllocation alloc)
{
  CmdBufferResources& cmd_buffer_resources = GetCurrentCmdBufferResources();
  cmd_buffer_resources.cleanup_resources.push_back([buffer, alloc]() {
    vmaDestroyBuffer(g_vulkan_context->GetMemoryAllocator(), buffer, alloc);
  });
}

void CommandBufferManager::DeferFramebufferDestruction(VkFramebuffer object)
{
  CmdBufferResources& cmd_buffer_resources = GetCurrentCmdBufferResources();
  cmd_buffer_resources.cleanup_resources.push_back(
      [object]() { vkDestroyFramebuffer(g_vulkan_context->GetDevice(), object, nullptr); });
}

void CommandBufferManager::DeferImageDestruction(VkImage image, VmaAllocation alloc)
{
  CmdBufferResources& cmd_buffer_resources = GetCurrentCmdBufferResources();
  cmd_buffer_resources.cleanup_resources.push_back(
      [image, alloc]() { vmaDestroyImage(g_vulkan_context->GetMemoryAllocator(), image, alloc); });
}

void CommandBufferManager::DeferImageViewDestruction(VkImageView object)
{
  CmdBufferResources& cmd_buffer_resources = GetCurrentCmdBufferResources();
  cmd_buffer_resources.cleanup_resources.push_back(
      [object]() { vkDestroyImageView(g_vulkan_context->GetDevice(), object, nullptr); });
}

std::unique_ptr<CommandBufferManager> g_command_buffer_mgr;
}  // namespace Vulkan
