// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Vulkan/VKBoundingBox.h"

#include <vector>

#include "Common/Logging/Log.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/StagingBuffer.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/VKGfx.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

namespace Vulkan
{
VKBoundingBox::~VKBoundingBox()
{
  if (m_gpu_buffer != VK_NULL_HANDLE)
  {
    vmaDestroyBuffer(g_vulkan_context->GetMemoryAllocator(), m_gpu_buffer, m_gpu_allocation);
  }
}

bool VKBoundingBox::Initialize()
{
  if (!CreateGPUBuffer())
    return false;

  if (!CreateReadbackBuffer())
    return false;

  // Bind bounding box to state tracker
  StateTracker::GetInstance()->SetSSBO(m_gpu_buffer, 0, BUFFER_SIZE);
  return true;
}

std::vector<BBoxType> VKBoundingBox::Read(u32 index, u32 length)
{
  // Can't be done within a render pass.
  StateTracker::GetInstance()->EndRenderPass();

  // Ensure all writes are completed to the GPU buffer prior to the transfer.
  StagingBuffer::BufferMemoryBarrier(g_command_buffer_mgr->GetCurrentCommandBuffer(), m_gpu_buffer,
                                     VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, 0,
                                     BUFFER_SIZE, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT);
  m_readback_buffer->PrepareForGPUWrite(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                        VK_ACCESS_TRANSFER_WRITE_BIT,
                                        VK_PIPELINE_STAGE_TRANSFER_BIT);

  // Copy from GPU -> readback buffer.
  VkBufferCopy region = {0, 0, BUFFER_SIZE};
  vkCmdCopyBuffer(g_command_buffer_mgr->GetCurrentCommandBuffer(), m_gpu_buffer,
                  m_readback_buffer->GetBuffer(), 1, &region);

  // Restore GPU buffer access.
  StagingBuffer::BufferMemoryBarrier(g_command_buffer_mgr->GetCurrentCommandBuffer(), m_gpu_buffer,
                                     0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, 0,
                                     BUFFER_SIZE, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
  m_readback_buffer->FlushGPUCache(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                   VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

  // Wait until these commands complete.
  VKGfx::GetInstance()->ExecuteCommandBuffer(false, true);

  // Cache is now valid.
  m_readback_buffer->InvalidateCPUCache();

  // Read out the values and return
  std::vector<BBoxType> values(length);
  m_readback_buffer->Read(index * sizeof(BBoxType), values.data(), length * sizeof(BBoxType),
                          false);
  return values;
}

void VKBoundingBox::Write(u32 index, std::span<const BBoxType> values)
{
  // We can't issue vkCmdUpdateBuffer within a render pass.
  // However, the writes must be serialized, so we can't put it in the init buffer.
  StateTracker::GetInstance()->EndRenderPass();

  // Ensure GPU buffer is in a state where it can be transferred to.
  StagingBuffer::BufferMemoryBarrier(g_command_buffer_mgr->GetCurrentCommandBuffer(), m_gpu_buffer,
                                     VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, 0,
                                     BUFFER_SIZE, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT);

  // Write the values to the GPU buffer
  vkCmdUpdateBuffer(g_command_buffer_mgr->GetCurrentCommandBuffer(), m_gpu_buffer,
                    index * sizeof(BBoxType), values.size() * sizeof(BBoxType), values.data());

  // Restore fragment shader access to the buffer.
  StagingBuffer::BufferMemoryBarrier(
      g_command_buffer_mgr->GetCurrentCommandBuffer(), m_gpu_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, 0, BUFFER_SIZE,
      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

bool VKBoundingBox::CreateGPUBuffer()
{
  VkBufferUsageFlags buffer_usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  VkBufferCreateInfo info = {
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,  // VkStructureType        sType
      nullptr,                               // const void*            pNext
      0,                                     // VkBufferCreateFlags    flags
      BUFFER_SIZE,                           // VkDeviceSize           size
      buffer_usage,                          // VkBufferUsageFlags     usage
      VK_SHARING_MODE_EXCLUSIVE,             // VkSharingMode          sharingMode
      0,                                     // uint32_t               queueFamilyIndexCount
      nullptr                                // const uint32_t*        pQueueFamilyIndices
  };

  VmaAllocationCreateInfo alloc_create_info = {};
  alloc_create_info.flags = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT;
  alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
  alloc_create_info.pool = VK_NULL_HANDLE;
  alloc_create_info.pUserData = nullptr;
  alloc_create_info.priority = 0.0;
  alloc_create_info.requiredFlags = 0;
  alloc_create_info.preferredFlags = 0;

  VkBuffer buffer;
  VmaAllocation alloc;
  VkResult res = vmaCreateBuffer(g_vulkan_context->GetMemoryAllocator(), &info, &alloc_create_info,
                                 &buffer, &alloc, nullptr);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vmaCreateBuffer failed: ");
    return false;
  }

  m_gpu_buffer = buffer;
  m_gpu_allocation = alloc;
  return true;
}

bool VKBoundingBox::CreateReadbackBuffer()
{
  m_readback_buffer = StagingBuffer::Create(STAGING_BUFFER_TYPE_READBACK, BUFFER_SIZE,
                                            VK_BUFFER_USAGE_TRANSFER_DST_BIT);

  if (!m_readback_buffer || !m_readback_buffer->Map())
    return false;

  return true;
}

}  // namespace Vulkan
