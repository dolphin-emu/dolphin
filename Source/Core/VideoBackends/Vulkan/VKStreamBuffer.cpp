// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Vulkan/VKStreamBuffer.h"

#include <algorithm>
#include <cstdint>
#include <functional>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

namespace Vulkan
{
StreamBuffer::StreamBuffer(VkBufferUsageFlags usage, u32 size) : m_usage(usage), m_size(size)
{
}

StreamBuffer::~StreamBuffer()
{
  // VMA_ALLOCATION_CREATE_MAPPED_BIT automatically handles unmapping for us
  if (m_buffer != VK_NULL_HANDLE)
    g_command_buffer_mgr->DeferBufferDestruction(m_buffer, m_alloc);
}

std::unique_ptr<StreamBuffer> StreamBuffer::Create(VkBufferUsageFlags usage, u32 size)
{
  std::unique_ptr<StreamBuffer> buffer = std::make_unique<StreamBuffer>(usage, size);
  if (!buffer->AllocateBuffer())
    return nullptr;

  return buffer;
}

bool StreamBuffer::AllocateBuffer()
{
  // Create the buffer descriptor
  VkBufferCreateInfo buffer_create_info = {
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,  // VkStructureType        sType
      nullptr,                               // const void*            pNext
      0,                                     // VkBufferCreateFlags    flags
      static_cast<VkDeviceSize>(m_size),     // VkDeviceSize           size
      m_usage,                               // VkBufferUsageFlags     usage
      VK_SHARING_MODE_EXCLUSIVE,             // VkSharingMode          sharingMode
      0,                                     // uint32_t               queueFamilyIndexCount
      nullptr                                // const uint32_t*        pQueueFamilyIndices
  };

  VmaAllocationCreateInfo alloc_create_info = {};
  alloc_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                            VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT |
                            VMA_ALLOCATION_CREATE_MAPPED_BIT;
  alloc_create_info.usage =
      VMA_MEMORY_USAGE_AUTO_PREFER_HOST;  // Host visible VRAM is slower in practice
  alloc_create_info.pool = VK_NULL_HANDLE;
  alloc_create_info.pUserData = nullptr;
  alloc_create_info.priority = 0.0;
  alloc_create_info.requiredFlags = 0;
  alloc_create_info.preferredFlags = 0;

  VkBuffer buffer = VK_NULL_HANDLE;
  VmaAllocation alloc = VK_NULL_HANDLE;
  VmaAllocationInfo alloc_info;
  VkResult res = vmaCreateBuffer(g_vulkan_context->GetMemoryAllocator(), &buffer_create_info,
                                 &alloc_create_info, &buffer, &alloc, &alloc_info);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vmaCreateBuffer failed: ");
    return false;
  }

  // Destroy the backings for the buffer after the command buffer executes
  // VMA_ALLOCATION_CREATE_MAPPED_BIT automatically handles unmapping for us
  if (m_buffer != VK_NULL_HANDLE)
    g_command_buffer_mgr->DeferBufferDestruction(m_buffer, m_alloc);

  // Replace with the new buffer
  m_buffer = buffer;
  m_alloc = alloc;
  m_host_pointer = reinterpret_cast<u8*>(alloc_info.pMappedData);
  m_current_offset = 0;
  m_current_gpu_position = 0;
  m_tracked_fences.clear();
  return true;
}

bool StreamBuffer::ReserveMemory(u32 num_bytes, u32 alignment)
{
  const u32 required_bytes = num_bytes + alignment;

  // Check for sane allocations
  if (required_bytes > m_size)
  {
    PanicAlertFmt("Attempting to allocate {} bytes from a {} byte stream buffer", num_bytes,
                  m_size);

    return false;
  }

  // Is the GPU behind or up to date with our current offset?
  UpdateCurrentFencePosition();
  if (m_current_offset >= m_current_gpu_position)
  {
    const u32 remaining_bytes = m_size - m_current_offset;
    if (required_bytes <= remaining_bytes)
    {
      // Place at the current position, after the GPU position.
      m_current_offset = Common::AlignUp(m_current_offset, alignment);
      m_last_allocation_size = num_bytes;
      return true;
    }

    // Check for space at the start of the buffer
    // We use < here because we don't want to have the case of m_current_offset ==
    // m_current_gpu_position. That would mean the code above would assume the
    // GPU has caught up to us, which it hasn't.
    if (required_bytes < m_current_gpu_position)
    {
      // Reset offset to zero, since we're allocating behind the gpu now
      m_current_offset = 0;
      m_last_allocation_size = num_bytes;
      return true;
    }
  }

  // Is the GPU ahead of our current offset?
  if (m_current_offset < m_current_gpu_position)
  {
    // We have from m_current_offset..m_current_gpu_position space to use.
    const u32 remaining_bytes = m_current_gpu_position - m_current_offset;
    if (required_bytes < remaining_bytes)
    {
      // Place at the current position, since this is still behind the GPU.
      m_current_offset = Common::AlignUp(m_current_offset, alignment);
      m_last_allocation_size = num_bytes;
      return true;
    }
  }

  // Can we find a fence to wait on that will give us enough memory?
  if (WaitForClearSpace(required_bytes))
  {
    m_current_offset = Common::AlignUp(m_current_offset, alignment);
    m_last_allocation_size = num_bytes;
    return true;
  }

  // We tried everything we could, and still couldn't get anything. This means that too much space
  // in the buffer is being used by the command buffer currently being recorded. Therefore, the
  // only option is to execute it, and wait until it's done.
  return false;
}

void StreamBuffer::CommitMemory(u32 final_num_bytes)
{
  ASSERT((m_current_offset + final_num_bytes) <= m_size);
  ASSERT(final_num_bytes <= m_last_allocation_size);

  // For non-coherent mappings, flush the memory range
  // vmaFlushAllocation checks whether the allocation uses a coherent memory type internally
  vmaFlushAllocation(g_vulkan_context->GetMemoryAllocator(), m_alloc, m_current_offset,
                     final_num_bytes);

  m_current_offset += final_num_bytes;
}

void StreamBuffer::UpdateCurrentFencePosition()
{
  // Don't create a tracking entry if the GPU is caught up with the buffer.
  if (m_current_offset == m_current_gpu_position)
    return;

  // Has the offset changed since the last fence?
  const u64 counter = g_command_buffer_mgr->GetCurrentFenceCounter();
  if (!m_tracked_fences.empty() && m_tracked_fences.back().first == counter)
  {
    // Still haven't executed a command buffer, so just update the offset.
    m_tracked_fences.back().second = m_current_offset;
    return;
  }

  // New buffer, so update the GPU position while we're at it.
  UpdateGPUPosition();
  m_tracked_fences.emplace_back(counter, m_current_offset);
}

void StreamBuffer::UpdateGPUPosition()
{
  auto start = m_tracked_fences.begin();
  auto end = start;

  const u64 completed_counter = g_command_buffer_mgr->GetCompletedFenceCounter();
  while (end != m_tracked_fences.end() && completed_counter >= end->first)
  {
    m_current_gpu_position = end->second;
    ++end;
  }

  if (start != end)
    m_tracked_fences.erase(start, end);
}

bool StreamBuffer::WaitForClearSpace(u32 num_bytes)
{
  u32 new_offset = 0;
  u32 new_gpu_position = 0;

  auto iter = m_tracked_fences.begin();
  for (; iter != m_tracked_fences.end(); ++iter)
  {
    // Would this fence bring us in line with the GPU?
    // This is the "last resort" case, where a command buffer execution has been forced
    // after no additional data has been written to it, so we can assume that after the
    // fence has been signaled the entire buffer is now consumed.
    u32 gpu_position = iter->second;
    if (m_current_offset == gpu_position)
    {
      new_offset = 0;
      new_gpu_position = 0;
      break;
    }

    // Assuming that we wait for this fence, are we allocating in front of the GPU?
    if (m_current_offset > gpu_position)
    {
      // This would suggest the GPU has now followed us and wrapped around, so we have from
      // m_current_position..m_size free, as well as and 0..gpu_position.
      const u32 remaining_space_after_offset = m_size - m_current_offset;
      if (remaining_space_after_offset >= num_bytes)
      {
        // Switch to allocating in front of the GPU, using the remainder of the buffer.
        new_offset = m_current_offset;
        new_gpu_position = gpu_position;
        break;
      }

      // We can wrap around to the start, behind the GPU, if there is enough space.
      // We use > here because otherwise we'd end up lining up with the GPU, and then the
      // allocator would assume that the GPU has consumed what we just wrote.
      if (gpu_position > num_bytes)
      {
        new_offset = 0;
        new_gpu_position = gpu_position;
        break;
      }
    }
    else
    {
      // We're currently allocating behind the GPU. This would give us between the current
      // offset and the GPU position worth of space to work with. Again, > because we can't
      // align the GPU position with the buffer offset.
      u32 available_space_inbetween = gpu_position - m_current_offset;
      if (available_space_inbetween > num_bytes)
      {
        // Leave the offset as-is, but update the GPU position.
        new_offset = m_current_offset;
        new_gpu_position = gpu_position;
        break;
      }
    }
  }

  // Did any fences satisfy this condition?
  // Has the command buffer been executed yet? If not, the caller should execute it.
  if (iter == m_tracked_fences.end() ||
      iter->first == g_command_buffer_mgr->GetCurrentFenceCounter())
  {
    return false;
  }

  // Wait until this fence is signaled. This will fire the callback, updating the GPU position.
  g_command_buffer_mgr->WaitForFenceCounter(iter->first);
  m_tracked_fences.erase(m_tracked_fences.begin(),
                         m_current_offset == iter->second ? m_tracked_fences.end() : ++iter);
  m_current_offset = new_offset;
  m_current_gpu_position = new_gpu_position;
  return true;
}

}  // namespace Vulkan
