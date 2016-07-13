// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <vector>

#include "Common/Assert.h"

#include "VideoBackends/Vulkan/BoundingBox.h"
#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/Util.h"

namespace Vulkan
{
BoundingBox::BoundingBox()
{
}

BoundingBox::~BoundingBox()
{
  if (m_gpu_buffer != VK_NULL_HANDLE)
  {
    vkDestroyBuffer(g_object_cache->GetDevice(), m_gpu_buffer, nullptr);
    vkFreeMemory(g_object_cache->GetDevice(), m_gpu_memory, nullptr);
  }

  if (m_readback_buffer != VK_NULL_HANDLE)
  {
    if (m_readback_map)
      vkUnmapMemory(g_object_cache->GetDevice(), m_readback_memory);

    vkDestroyBuffer(g_object_cache->GetDevice(), m_readback_buffer, nullptr);
    vkFreeMemory(g_object_cache->GetDevice(), m_readback_memory, nullptr);
  }
}

bool BoundingBox::Initialize()
{
  if (!g_object_cache->SupportsBoundingBox())
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

void BoundingBox::Flush(StateTracker* state_tracker)
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
    std::array<int, 4> write_values;
    for (; (start + count) <= 4; count++)
    {
      if (!m_values_dirty[start + count])
        break;

      write_values[count] = static_cast<int>(m_values[start + count]);
      m_values_dirty[start + count] = false;
    }

    // We can't issue vkCmdUpdateBuffer within a render pass.
    // However, the writes must be serialized, so we can't put it in the init buffer.
    if (!updated_buffer)
    {
      state_tracker->EndRenderPass();

      // Ensure GPU buffer is in a state where it can be transferred to.
      Util::BufferMemoryBarrier(
          g_command_buffer_mgr->GetCurrentCommandBuffer(), m_gpu_buffer,
          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, 0,
          BUFFER_SIZE, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

      updated_buffer = true;
    }

    vkCmdUpdateBuffer(g_command_buffer_mgr->GetCurrentCommandBuffer(), m_gpu_buffer,
                      start * sizeof(int), count * sizeof(int),
                      reinterpret_cast<const uint32_t*>(write_values.data()));
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

void BoundingBox::Invalidate(StateTracker* state_tracker)
{
  if (m_gpu_buffer == VK_NULL_HANDLE)
    return;

  m_valid = false;
}

u16 BoundingBox::Get(StateTracker* state_tracker, int index)
{
  _assert_(static_cast<size_t>(index) < m_values.size());

  if (m_valid)
    return m_values[index];

  Readback(state_tracker);
  return m_values[index];
}

void BoundingBox::Set(StateTracker* state_tracker, int index, u16 value)
{
  _assert_(static_cast<size_t>(index) < m_values.size());

  // If we're currently valid, update the stored value in both our cache and the GPU buffer.
  if (m_valid)
  {
    // Skip when it hasn't changed.
    if (m_values[index] == value)
      return;
  }

  // Flag as dirty, and update values.
  m_values_dirty[index] = true;
  m_values[index] = value;
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
  VkResult res = vkCreateBuffer(g_object_cache->GetDevice(), &info, nullptr, &buffer);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateBuffer failed: ");
    return false;
  }

  VkMemoryRequirements memory_requirements;
  vkGetBufferMemoryRequirements(g_object_cache->GetDevice(), buffer, &memory_requirements);

  uint32_t memory_type_index = g_object_cache->GetMemoryType(memory_requirements.memoryTypeBits,
                                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VkMemoryAllocateInfo memory_allocate_info = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,  // VkStructureType    sType
      nullptr,                                 // const void*        pNext
      memory_requirements.size,                // VkDeviceSize       allocationSize
      memory_type_index                        // uint32_t           memoryTypeIndex
  };
  VkDeviceMemory memory;
  res = vkAllocateMemory(g_object_cache->GetDevice(), &memory_allocate_info, nullptr, &memory);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkAllocateMemory failed: ");
    vkDestroyBuffer(g_object_cache->GetDevice(), buffer, nullptr);
    return false;
  }

  res = vkBindBufferMemory(g_object_cache->GetDevice(), buffer, memory, 0);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkBindBufferMemory failed: ");
    vkDestroyBuffer(g_object_cache->GetDevice(), buffer, nullptr);
    vkFreeMemory(g_object_cache->GetDevice(), memory, nullptr);
    return false;
  }

  m_gpu_buffer = buffer;
  m_gpu_memory = memory;
  return true;
}

bool BoundingBox::CreateReadbackBuffer()
{
  VkBufferUsageFlags buffer_usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
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
  VkResult res = vkCreateBuffer(g_object_cache->GetDevice(), &info, nullptr, &buffer);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateBuffer failed: ");
    return false;
  }

  VkMemoryRequirements memory_requirements;
  vkGetBufferMemoryRequirements(g_object_cache->GetDevice(), buffer, &memory_requirements);

  uint32_t memory_type_index = g_object_cache->GetMemoryType(memory_requirements.memoryTypeBits,
                                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
  VkMemoryAllocateInfo memory_allocate_info = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,  // VkStructureType    sType
      nullptr,                                 // const void*        pNext
      memory_requirements.size,                // VkDeviceSize       allocationSize
      memory_type_index                        // uint32_t           memoryTypeIndex
  };
  VkDeviceMemory memory;
  res = vkAllocateMemory(g_object_cache->GetDevice(), &memory_allocate_info, nullptr, &memory);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkAllocateMemory failed: ");
    vkDestroyBuffer(g_object_cache->GetDevice(), buffer, nullptr);
    return false;
  }

  res = vkBindBufferMemory(g_object_cache->GetDevice(), buffer, memory, 0);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkBindBufferMemory failed: ");
    vkDestroyBuffer(g_object_cache->GetDevice(), buffer, nullptr);
    vkFreeMemory(g_object_cache->GetDevice(), memory, nullptr);
    return false;
  }

  m_readback_buffer = buffer;
  m_readback_memory = memory;

  // Map the buffer and keep it mapped. We use invalidate calls in case access is not coherent.
  res = vkMapMemory(g_object_cache->GetDevice(), m_readback_memory, 0, BUFFER_SIZE, 0,
                    &m_readback_map);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkMapMemory failed: ");
    return false;
  }

  return true;
}

void BoundingBox::Readback(StateTracker* state_tracker)
{
  // Can't be done within a render pass.
  state_tracker->EndRenderPass();

  // Ensure all writes are completed to the GPU buffer prior to the transfer.
  Util::BufferMemoryBarrier(
      g_command_buffer_mgr->GetCurrentCommandBuffer(), m_gpu_buffer,
      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, 0,
      BUFFER_SIZE, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
  Util::BufferMemoryBarrier(g_command_buffer_mgr->GetCurrentCommandBuffer(), m_readback_buffer,
                            VK_ACCESS_HOST_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, 0, BUFFER_SIZE,
                            VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

  // Copy from GPU -> readback buffer.
  VkBufferCopy region = {0, 0, BUFFER_SIZE};
  vkCmdCopyBuffer(g_command_buffer_mgr->GetCurrentCommandBuffer(), m_gpu_buffer, m_readback_buffer,
                  1, &region);

  // Restore GPU buffer access.
  Util::BufferMemoryBarrier(g_command_buffer_mgr->GetCurrentCommandBuffer(), m_gpu_buffer,
                            VK_ACCESS_TRANSFER_READ_BIT,
                            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, 0, BUFFER_SIZE,
                            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

  // Ensure writes are visible to the host.
  Util::BufferMemoryBarrier(g_command_buffer_mgr->GetCurrentCommandBuffer(), m_readback_buffer,
                            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, 0, BUFFER_SIZE,
                            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);

  // Wait until these commands complete.
  g_command_buffer_mgr->ExecuteCommandBuffer(false, true);
  state_tracker->InvalidateDescriptorSets();
  state_tracker->SetPendingRebind();

  // Invalidate host caches.
  VkMappedMemoryRange range = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, m_readback_memory, 0,
                               BUFFER_SIZE};
  vkInvalidateMappedMemoryRanges(g_object_cache->GetDevice(), 1, &range);

  // Copy to our cache.
  std::array<int32_t, NUM_VALUES> values;
  memcpy(values.data(), m_readback_map, sizeof(int32_t) * values.size());
  for (size_t i = 0; i < NUM_VALUES; i++)
    m_values[i] = static_cast<u16>(values[i]);

  // Cache is now valid.
  m_valid = true;
}

}  // namespace Vulkan
