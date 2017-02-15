// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <vector>

#include "Common/Assert.h"
#include "Common/Logging/Log.h"

#include "VideoBackends/Vulkan/BoundingBox.h"
#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/Renderer.h"
#include "VideoBackends/Vulkan/StagingBuffer.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

namespace Vulkan
{
BoundingBox::BoundingBox()
{
}

BoundingBox::~BoundingBox()
{
  if (m_gpu_buffer != VK_NULL_HANDLE)
  {
    vkDestroyBuffer(g_vulkan_context->GetDevice(), m_gpu_buffer, nullptr);
    vkFreeMemory(g_vulkan_context->GetDevice(), m_gpu_memory, nullptr);
  }
}

bool BoundingBox::Initialize()
{
  if (!g_vulkan_context->SupportsBoundingBox())
  {
    WARN_LOG(VIDEO, "Vulkan: Bounding box is unsupported by your device.");
    return true;
  }

  if (!CreateGPUBuffer())
    return false;

  if (!CreateReadbackBuffer())
    return false;

  return true;
}

void BoundingBox::Flush()
{
  if (m_gpu_buffer == VK_NULL_HANDLE)
    return;

  // Combine updates together, chances are the game would have written all 4.
  bool updated_buffer = false;
  for (size_t start = 0; start < 4; start++)
  {
    if (!m_values_dirty[start])
      continue;

    size_t count = 0;
    std::array<s32, 4> write_values;
    for (; (start + count) < 4; count++)
    {
      if (!m_values_dirty[start + count])
        break;

      m_readback_buffer->Read((start + count) * sizeof(s32), &write_values[count], sizeof(s32),
                              false);
      m_values_dirty[start + count] = false;
    }

    // We can't issue vkCmdUpdateBuffer within a render pass.
    // However, the writes must be serialized, so we can't put it in the init buffer.
    if (!updated_buffer)
    {
      StateTracker::GetInstance()->EndRenderPass();

      // Ensure GPU buffer is in a state where it can be transferred to.
      Util::BufferMemoryBarrier(
          g_command_buffer_mgr->GetCurrentCommandBuffer(), m_gpu_buffer,
          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, 0,
          BUFFER_SIZE, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

      updated_buffer = true;
    }

    vkCmdUpdateBuffer(g_command_buffer_mgr->GetCurrentCommandBuffer(), m_gpu_buffer,
                      start * sizeof(s32), count * sizeof(s32),
                      reinterpret_cast<const u32*>(write_values.data()));
  }

  // Restore fragment shader access to the buffer.
  if (updated_buffer)
  {
    Util::BufferMemoryBarrier(
        g_command_buffer_mgr->GetCurrentCommandBuffer(), m_gpu_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, 0, BUFFER_SIZE,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
  }

  // We're now up-to-date.
  m_valid = true;
}

void BoundingBox::Invalidate()
{
  if (m_gpu_buffer == VK_NULL_HANDLE)
    return;

  m_valid = false;
}

s32 BoundingBox::Get(size_t index)
{
  _assert_(index < NUM_VALUES);

  if (!m_valid)
    Readback();

  s32 value;
  m_readback_buffer->Read(index * sizeof(s32), &value, sizeof(value), false);
  return value;
}

void BoundingBox::Set(size_t index, s32 value)
{
  _assert_(index < NUM_VALUES);

  // If we're currently valid, update the stored value in both our cache and the GPU buffer.
  if (m_valid)
  {
    // Skip when it hasn't changed.
    s32 current_value;
    m_readback_buffer->Read(index * sizeof(s32), &current_value, sizeof(current_value), false);
    if (current_value == value)
      return;
  }

  // Flag as dirty, and update values.
  m_readback_buffer->Write(index * sizeof(s32), &value, sizeof(value), true);
  m_values_dirty[index] = true;
}

bool BoundingBox::CreateGPUBuffer()
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

  VkBuffer buffer;
  VkResult res = vkCreateBuffer(g_vulkan_context->GetDevice(), &info, nullptr, &buffer);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateBuffer failed: ");
    return false;
  }

  VkMemoryRequirements memory_requirements;
  vkGetBufferMemoryRequirements(g_vulkan_context->GetDevice(), buffer, &memory_requirements);

  uint32_t memory_type_index = g_vulkan_context->GetMemoryType(memory_requirements.memoryTypeBits,
                                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VkMemoryAllocateInfo memory_allocate_info = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,  // VkStructureType    sType
      nullptr,                                 // const void*        pNext
      memory_requirements.size,                // VkDeviceSize       allocationSize
      memory_type_index                        // uint32_t           memoryTypeIndex
  };
  VkDeviceMemory memory;
  res = vkAllocateMemory(g_vulkan_context->GetDevice(), &memory_allocate_info, nullptr, &memory);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkAllocateMemory failed: ");
    vkDestroyBuffer(g_vulkan_context->GetDevice(), buffer, nullptr);
    return false;
  }

  res = vkBindBufferMemory(g_vulkan_context->GetDevice(), buffer, memory, 0);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkBindBufferMemory failed: ");
    vkDestroyBuffer(g_vulkan_context->GetDevice(), buffer, nullptr);
    vkFreeMemory(g_vulkan_context->GetDevice(), memory, nullptr);
    return false;
  }

  m_gpu_buffer = buffer;
  m_gpu_memory = memory;
  return true;
}

bool BoundingBox::CreateReadbackBuffer()
{
  m_readback_buffer = StagingBuffer::Create(STAGING_BUFFER_TYPE_READBACK, BUFFER_SIZE,
                                            VK_BUFFER_USAGE_TRANSFER_DST_BIT);

  if (!m_readback_buffer || !m_readback_buffer->Map())
    return false;

  return true;
}

void BoundingBox::Readback()
{
  // Can't be done within a render pass.
  StateTracker::GetInstance()->EndRenderPass();

  // Ensure all writes are completed to the GPU buffer prior to the transfer.
  Util::BufferMemoryBarrier(
      g_command_buffer_mgr->GetCurrentCommandBuffer(), m_gpu_buffer,
      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, 0,
      BUFFER_SIZE, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
  m_readback_buffer->PrepareForGPUWrite(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                        VK_ACCESS_TRANSFER_WRITE_BIT,
                                        VK_PIPELINE_STAGE_TRANSFER_BIT);

  // Copy from GPU -> readback buffer.
  VkBufferCopy region = {0, 0, BUFFER_SIZE};
  vkCmdCopyBuffer(g_command_buffer_mgr->GetCurrentCommandBuffer(), m_gpu_buffer,
                  m_readback_buffer->GetBuffer(), 1, &region);

  // Restore GPU buffer access.
  Util::BufferMemoryBarrier(g_command_buffer_mgr->GetCurrentCommandBuffer(), m_gpu_buffer,
                            VK_ACCESS_TRANSFER_READ_BIT,
                            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, 0, BUFFER_SIZE,
                            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
  m_readback_buffer->FlushGPUCache(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                   VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

  // Wait until these commands complete.
  Util::ExecuteCurrentCommandsAndRestoreState(false, true);

  // Cache is now valid.
  m_readback_buffer->InvalidateCPUCache();
  m_valid = true;
}

}  // namespace Vulkan
