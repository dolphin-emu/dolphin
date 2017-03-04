// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Vulkan/StreamBuffer.h"

#include <algorithm>
#include <cstdint>
#include <functional>

#include "Common/Assert.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

namespace Vulkan
{
StreamBuffer::StreamBuffer(VkBufferUsageFlags usage, size_t max_size)
    : m_usage(usage), m_maximum_size(max_size)
{
  // Add a callback that fires on fence point creation and signal
  g_command_buffer_mgr->AddFencePointCallback(
      this, std::bind(&StreamBuffer::OnCommandBufferQueued, this, std::placeholders::_1,
                      std::placeholders::_2),
      std::bind(&StreamBuffer::OnCommandBufferExecuted, this, std::placeholders::_1));
}

StreamBuffer::~StreamBuffer()
{
  g_command_buffer_mgr->RemoveFencePointCallback(this);

  if (m_host_pointer)
    vkUnmapMemory(g_vulkan_context->GetDevice(), m_memory);

  if (m_buffer != VK_NULL_HANDLE)
    g_command_buffer_mgr->DeferBufferDestruction(m_buffer);
  if (m_memory != VK_NULL_HANDLE)
    g_command_buffer_mgr->DeferDeviceMemoryDestruction(m_memory);
}

std::unique_ptr<StreamBuffer> StreamBuffer::Create(VkBufferUsageFlags usage, size_t initial_size,
                                                   size_t max_size)
{
  std::unique_ptr<StreamBuffer> buffer = std::make_unique<StreamBuffer>(usage, max_size);
  if (!buffer->ResizeBuffer(initial_size))
    return nullptr;

  return buffer;
}

bool StreamBuffer::ResizeBuffer(size_t size)
{
  // Create the buffer descriptor
  VkBufferCreateInfo buffer_create_info = {
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,  // VkStructureType        sType
      nullptr,                               // const void*            pNext
      0,                                     // VkBufferCreateFlags    flags
      static_cast<VkDeviceSize>(size),       // VkDeviceSize           size
      m_usage,                               // VkBufferUsageFlags     usage
      VK_SHARING_MODE_EXCLUSIVE,             // VkSharingMode          sharingMode
      0,                                     // uint32_t               queueFamilyIndexCount
      nullptr                                // const uint32_t*        pQueueFamilyIndices
  };

  VkBuffer buffer = VK_NULL_HANDLE;
  VkResult res =
      vkCreateBuffer(g_vulkan_context->GetDevice(), &buffer_create_info, nullptr, &buffer);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateBuffer failed: ");
    return false;
  }

  // Get memory requirements (types etc) for this buffer
  VkMemoryRequirements memory_requirements;
  vkGetBufferMemoryRequirements(g_vulkan_context->GetDevice(), buffer, &memory_requirements);

  // Aim for a coherent mapping if possible.
  u32 memory_type_index = g_vulkan_context->GetUploadMemoryType(memory_requirements.memoryTypeBits,
                                                                &m_coherent_mapping);

  // Allocate memory for backing this buffer
  VkMemoryAllocateInfo memory_allocate_info = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,  // VkStructureType    sType
      nullptr,                                 // const void*        pNext
      memory_requirements.size,                // VkDeviceSize       allocationSize
      memory_type_index                        // uint32_t           memoryTypeIndex
  };
  VkDeviceMemory memory = VK_NULL_HANDLE;
  res = vkAllocateMemory(g_vulkan_context->GetDevice(), &memory_allocate_info, nullptr, &memory);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkAllocateMemory failed: ");
    vkDestroyBuffer(g_vulkan_context->GetDevice(), buffer, nullptr);
    return false;
  }

  // Bind memory to buffer
  res = vkBindBufferMemory(g_vulkan_context->GetDevice(), buffer, memory, 0);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkBindBufferMemory failed: ");
    vkDestroyBuffer(g_vulkan_context->GetDevice(), buffer, nullptr);
    vkFreeMemory(g_vulkan_context->GetDevice(), memory, nullptr);
    return false;
  }

  // Map this buffer into user-space
  void* mapped_ptr = nullptr;
  res = vkMapMemory(g_vulkan_context->GetDevice(), memory, 0, size, 0, &mapped_ptr);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkMapMemory failed: ");
    vkDestroyBuffer(g_vulkan_context->GetDevice(), buffer, nullptr);
    vkFreeMemory(g_vulkan_context->GetDevice(), memory, nullptr);
    return false;
  }

  // Unmap current host pointer (if there was a previous buffer)
  if (m_host_pointer)
    vkUnmapMemory(g_vulkan_context->GetDevice(), m_memory);

  // Destroy the backings for the buffer after the command buffer executes
  if (m_buffer != VK_NULL_HANDLE)
    g_command_buffer_mgr->DeferBufferDestruction(m_buffer);
  if (m_memory != VK_NULL_HANDLE)
    g_command_buffer_mgr->DeferDeviceMemoryDestruction(m_memory);

  // Replace with the new buffer
  m_buffer = buffer;
  m_memory = memory;
  m_host_pointer = reinterpret_cast<u8*>(mapped_ptr);
  m_current_size = size;
  m_current_offset = 0;
  m_current_gpu_position = 0;
  m_tracked_fences.clear();
  return true;
}

bool StreamBuffer::ReserveMemory(size_t num_bytes, size_t alignment, bool allow_reuse /* = true */,
                                 bool allow_growth /* = true */,
                                 bool reallocate_if_full /* = false */)
{
  size_t required_bytes = num_bytes + alignment;

  // Check for sane allocations
  if (required_bytes > m_maximum_size)
  {
    PanicAlert("Attempting to allocate %u bytes from a %u byte stream buffer",
               static_cast<uint32_t>(num_bytes), static_cast<uint32_t>(m_maximum_size));

    return false;
  }

  // Is the GPU behind or up to date with our current offset?
  if (m_current_offset >= m_current_gpu_position)
  {
    size_t remaining_bytes = m_current_size - m_current_offset;
    if (required_bytes <= remaining_bytes)
    {
      // Place at the current position, after the GPU position.
      m_current_offset = Util::AlignBufferOffset(m_current_offset, alignment);
      m_last_allocation_size = num_bytes;
      return true;
    }

    // Check for space at the start of the buffer
    // We use < here because we don't want to have the case of m_current_offset ==
    // m_current_gpu_position. That would mean the code above would assume the
    // GPU has caught up to us, which it hasn't.
    if (allow_reuse && required_bytes < m_current_gpu_position)
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
    size_t remaining_bytes = m_current_gpu_position - m_current_offset;
    if (required_bytes < remaining_bytes)
    {
      // Place at the current position, since this is still behind the GPU.
      m_current_offset = Util::AlignBufferOffset(m_current_offset, alignment);
      m_last_allocation_size = num_bytes;
      return true;
    }
  }

  // Try to grow the buffer up to the maximum size before waiting.
  // Double each time until the maximum size is reached.
  if (allow_growth && m_current_size < m_maximum_size)
  {
    size_t new_size = std::min(std::max(num_bytes, m_current_size * 2), m_maximum_size);
    if (ResizeBuffer(new_size))
    {
      // Allocating from the start of the buffer.
      m_last_allocation_size = new_size;
      return true;
    }
  }

  // Can we find a fence to wait on that will give us enough memory?
  if (allow_reuse && WaitForClearSpace(required_bytes))
  {
    _assert_(m_current_offset == m_current_gpu_position ||
             (m_current_offset + required_bytes) < m_current_gpu_position);
    m_current_offset = Util::AlignBufferOffset(m_current_offset, alignment);
    m_last_allocation_size = num_bytes;
    return true;
  }

  // If we are not allowed to execute in our current state (e.g. in the middle of a render pass),
  // as a last resort, reallocate the buffer. This will incur a performance hit and is not
  // encouraged.
  if (reallocate_if_full && ResizeBuffer(m_current_size))
  {
    m_last_allocation_size = num_bytes;
    return true;
  }

  // We tried everything we could, and still couldn't get anything. If we're not at a point
  // where the state is known and can be resumed, this is probably a fatal error.
  return false;
}

void StreamBuffer::CommitMemory(size_t final_num_bytes)
{
  _assert_((m_current_offset + final_num_bytes) <= m_current_size);
  _assert_(final_num_bytes <= m_last_allocation_size);

  // For non-coherent mappings, flush the memory range
  if (!m_coherent_mapping)
  {
    VkMappedMemoryRange range = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, m_memory,
                                 m_current_offset, final_num_bytes};
    vkFlushMappedMemoryRanges(g_vulkan_context->GetDevice(), 1, &range);
  }

  m_current_offset += final_num_bytes;
}

void StreamBuffer::OnCommandBufferQueued(VkCommandBuffer command_buffer, VkFence fence)
{
  // Don't create a tracking entry if the GPU is caught up with the buffer.
  if (m_current_offset == m_current_gpu_position)
    return;

  // Has the offset changed since the last fence?
  if (!m_tracked_fences.empty() && m_tracked_fences.back().second == m_current_offset)
  {
    // No need to track the new fence, the old one is sufficient.
    return;
  }

  m_tracked_fences.emplace_back(fence, m_current_offset);
}

void StreamBuffer::OnCommandBufferExecuted(VkFence fence)
{
  // Locate the entry for this fence (if any, we may have been forced to wait already)
  auto iter = std::find_if(m_tracked_fences.begin(), m_tracked_fences.end(),
                           [fence](const auto& it) { return it.first == fence; });

  if (iter != m_tracked_fences.end())
  {
    // Update the GPU position, and remove any fences before this fence (since
    // it is implied that they have been signaled as well, though the callback
    // should have removed them already).
    m_current_gpu_position = iter->second;
    m_tracked_fences.erase(m_tracked_fences.begin(), ++iter);
  }
}

bool StreamBuffer::WaitForClearSpace(size_t num_bytes)
{
  size_t new_offset = 0;
  size_t new_gpu_position = 0;
  auto iter = m_tracked_fences.begin();
  for (; iter != m_tracked_fences.end(); iter++)
  {
    // Would this fence bring us in line with the GPU?
    // This is the "last resort" case, where a command buffer execution has been forced
    // after no additional data has been written to it, so we can assume that after the
    // fence has been signaled the entire buffer is now consumed.
    size_t gpu_position = iter->second;
    if (m_current_offset == gpu_position)
    {
      // Start at the start of the buffer again.
      new_offset = 0;
      new_gpu_position = 0;
      break;
    }

    // Assuming that we wait for this fence, are we allocating in front of the GPU?
    if (m_current_offset > gpu_position)
    {
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
      size_t available_space_inbetween = gpu_position - m_current_offset;
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
  if (iter == m_tracked_fences.end())
    return false;

  // Wait until this fence is signaled.
  VkResult res =
      vkWaitForFences(g_vulkan_context->GetDevice(), 1, &iter->first, VK_TRUE, UINT64_MAX);
  if (res != VK_SUCCESS)
    LOG_VULKAN_ERROR(res, "vkWaitForFences failed: ");

  // Update GPU position, and remove all fences up to (and including) this fence.
  m_current_offset = new_offset;
  m_current_gpu_position = new_gpu_position;
  m_tracked_fences.erase(m_tracked_fences.begin(), ++iter);
  return true;
}

}  // namespace Vulkan
