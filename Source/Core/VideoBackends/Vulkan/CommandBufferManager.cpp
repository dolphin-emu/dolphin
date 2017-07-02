// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstdint>

#include "Common/Assert.h"
#include "Common/CommonFuncs.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

namespace Vulkan
{
CommandBufferManager::CommandBufferManager(bool use_threaded_submission)
    : m_submit_semaphore(1, 1), m_use_threaded_submission(use_threaded_submission)
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

  vkDeviceWaitIdle(g_vulkan_context->GetDevice());

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
  VkDevice device = g_vulkan_context->GetDevice();
  VkResult res;

  for (FrameResources& resources : m_frame_resources)
  {
    resources.init_command_buffer_used = false;
    resources.needs_fence_wait = false;

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

    // TODO: A better way to choose the number of descriptors.
    VkDescriptorPoolSize pool_sizes[] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 500000},
                                         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 500000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 16384},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 16384}};

    VkDescriptorPoolCreateInfo pool_create_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                   nullptr,
                                                   0,
                                                   100000,  // tweak this
                                                   static_cast<u32>(ArraySize(pool_sizes)),
                                                   pool_sizes};

    res = vkCreateDescriptorPool(device, &pool_create_info, nullptr, &resources.descriptor_pool);
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
  VkDevice device = g_vulkan_context->GetDevice();

  for (FrameResources& resources : m_frame_resources)
  {
    for (auto& it : resources.cleanup_resources)
      it();
    resources.cleanup_resources.clear();

    if (resources.fence != VK_NULL_HANDLE)
    {
      vkDestroyFence(device, resources.fence, nullptr);
      resources.fence = VK_NULL_HANDLE;
    }
    if (resources.descriptor_pool != VK_NULL_HANDLE)
    {
      vkDestroyDescriptorPool(device, resources.descriptor_pool, nullptr);
      resources.descriptor_pool = VK_NULL_HANDLE;
    }
    if (resources.command_buffers[0] != VK_NULL_HANDLE)
    {
      vkFreeCommandBuffers(device, resources.command_pool,
                           static_cast<u32>(resources.command_buffers.size()),
                           resources.command_buffers.data());

      resources.command_buffers.fill(VK_NULL_HANDLE);
    }
    if (resources.command_pool != VK_NULL_HANDLE)
    {
      vkDestroyCommandPool(device, resources.command_pool, nullptr);
      resources.command_pool = VK_NULL_HANDLE;
    }
  }
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
  vkDeviceWaitIdle(g_vulkan_context->GetDevice());
}

void CommandBufferManager::WaitForFence(VkFence fence)
{
  // Find the command buffer that this fence corresponds to.
  size_t command_buffer_index = 0;
  for (; command_buffer_index < m_frame_resources.size(); command_buffer_index++)
  {
    if (m_frame_resources[command_buffer_index].fence == fence)
      break;
  }
  _assert_(command_buffer_index < m_frame_resources.size());

  // Has this command buffer already been waited for?
  if (!m_frame_resources[command_buffer_index].needs_fence_wait)
    return;

  // Wait for this command buffer to be completed.
  VkResult res =
      vkWaitForFences(g_vulkan_context->GetDevice(), 1,
                      &m_frame_resources[command_buffer_index].fence, VK_TRUE, UINT64_MAX);
  if (res != VK_SUCCESS)
    LOG_VULKAN_ERROR(res, "vkWaitForFences failed: ");

  // Immediately fire callbacks and cleanups, since the commands has been completed.
  m_frame_resources[command_buffer_index].needs_fence_wait = false;
  OnCommandBufferExecuted(command_buffer_index);
}

void CommandBufferManager::SubmitCommandBuffer(bool submit_on_worker_thread,
                                               VkSemaphore wait_semaphore,
                                               VkSemaphore signal_semaphore,
                                               VkSwapchainKHR present_swap_chain,
                                               uint32_t present_image_index)
{
  FrameResources& resources = m_frame_resources[m_current_frame];

  // Fire fence tracking callbacks. This can't happen on the worker thread.
  // We invoke these before submitting so that any last-minute commands can be added.
  for (const auto& iter : m_fence_point_callbacks)
    iter.second.first(resources.command_buffers[1], resources.fence);

  // End the current command buffer.
  for (VkCommandBuffer command_buffer : resources.command_buffers)
  {
    VkResult res = vkEndCommandBuffer(command_buffer);
    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkEndCommandBuffer failed: ");
      PanicAlert("Failed to end command buffer");
    }
  }

  // This command buffer now has commands, so can't be re-used without waiting.
  resources.needs_fence_wait = true;

  // Submitting off-thread?
  if (m_use_threaded_submission && submit_on_worker_thread)
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
}

void CommandBufferManager::SubmitCommandBuffer(size_t index, VkSemaphore wait_semaphore,
                                               VkSemaphore signal_semaphore,
                                               VkSwapchainKHR present_swap_chain,
                                               uint32_t present_image_index)
{
  FrameResources& resources = m_frame_resources[index];

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
  if (!m_frame_resources[index].init_command_buffer_used)
  {
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_frame_resources[index].command_buffers[1];
  }

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
    _assert_(signal_semaphore != VK_NULL_HANDLE);
    VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                     nullptr,
                                     1,
                                     &signal_semaphore,
                                     1,
                                     &present_swap_chain,
                                     &present_image_index,
                                     nullptr};

    res = vkQueuePresentKHR(g_vulkan_context->GetGraphicsQueue(), &present_info);
    if (res != VK_SUCCESS && res != VK_ERROR_OUT_OF_DATE_KHR && res != VK_SUBOPTIMAL_KHR)
      LOG_VULKAN_ERROR(res, "vkQueuePresentKHR failed: ");
  }

  // Command buffer has been queued, so permit the next one.
  m_submit_semaphore.Post();
}

void CommandBufferManager::OnCommandBufferExecuted(size_t index)
{
  FrameResources& resources = m_frame_resources[index];

  // Fire fence tracking callbacks.
  for (const auto& iter : m_fence_point_callbacks)
    iter.second.second(resources.fence);

  // Clean up all objects pending destruction on this command buffer
  for (auto& it : resources.cleanup_resources)
    it();
  resources.cleanup_resources.clear();
}

void CommandBufferManager::ActivateCommandBuffer()
{
  // Move to the next command buffer.
  m_current_frame = (m_current_frame + 1) % NUM_COMMAND_BUFFERS;
  FrameResources& resources = m_frame_resources[m_current_frame];

  // Wait for the GPU to finish with all resources for this command buffer.
  if (resources.needs_fence_wait)
  {
    VkResult res =
        vkWaitForFences(g_vulkan_context->GetDevice(), 1, &resources.fence, true, UINT64_MAX);
    if (res != VK_SUCCESS)
      LOG_VULKAN_ERROR(res, "vkWaitForFences failed: ");

    OnCommandBufferExecuted(m_current_frame);
  }

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
}

void CommandBufferManager::ExecuteCommandBuffer(bool submit_off_thread, bool wait_for_completion)
{
  VkFence pending_fence = GetCurrentCommandBufferFence();

  // If we're waiting for completion, don't bother waking the worker thread.
  PrepareToSubmitCommandBuffer();
  SubmitCommandBuffer((submit_off_thread && wait_for_completion));
  ActivateCommandBuffer();

  if (wait_for_completion)
    WaitForFence(pending_fence);
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

void CommandBufferManager::AddFencePointCallback(
    const void* key, const CommandBufferQueuedCallback& queued_callback,
    const CommandBufferExecutedCallback& executed_callback)
{
  // Shouldn't be adding twice.
  _assert_(m_fence_point_callbacks.find(key) == m_fence_point_callbacks.end());
  m_fence_point_callbacks.emplace(key, std::make_pair(queued_callback, executed_callback));
}

void CommandBufferManager::RemoveFencePointCallback(const void* key)
{
  auto iter = m_fence_point_callbacks.find(key);
  _assert_(iter != m_fence_point_callbacks.end());
  m_fence_point_callbacks.erase(iter);
}

std::unique_ptr<CommandBufferManager> g_command_buffer_mgr;
}
