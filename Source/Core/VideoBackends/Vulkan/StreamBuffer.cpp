// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cassert>
#include <cstdint>

#include "Common/MsgHandler.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan
{
StreamBuffer::StreamBuffer(ObjectCache* object_cache, CommandBufferManager* command_buffer_mgr,
                           VkBufferUsageFlags usage, size_t max_size)
    : m_object_cache(object_cache), m_command_buffer_mgr(command_buffer_mgr), m_usage(usage),
      m_current_size(0), m_maximum_size(max_size), m_current_offset(0), m_current_gpu_position(0),
      m_last_allocation_size(0), m_buffer(VK_NULL_HANDLE), m_memory(VK_NULL_HANDLE),
      m_host_pointer(nullptr)
{
  // Add a callback that fires on fence point creation and signal
  command_buffer_mgr->AddFencePointCallback(
      this,
      [this](VkFence fence) { this->OnFencePointCreated(fence); },
      [this](VkFence fence) { this->OnFencePointReached(fence); }
  );
}

StreamBuffer::~StreamBuffer()
{
  m_command_buffer_mgr->RemoveFencePointCallback(this);

  if (m_host_pointer)
    vkUnmapMemory(m_object_cache->GetDevice(), m_memory);

  if (m_buffer != VK_NULL_HANDLE)
    m_command_buffer_mgr->DeferResourceDestruction(m_buffer);
  if (m_memory != VK_NULL_HANDLE)
    m_command_buffer_mgr->DeferResourceDestruction(m_memory);
}

std::unique_ptr<StreamBuffer> StreamBuffer::Create(ObjectCache* object_cache,
                                                   CommandBufferManager* command_buffer_mgr,
                                                   VkBufferUsageFlags usage, size_t initial_size,
                                                   size_t max_size)
{
  std::unique_ptr<StreamBuffer> buffer =
      std::make_unique<StreamBuffer>(object_cache, command_buffer_mgr, usage, max_size);
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
  VkResult res = vkCreateBuffer(m_object_cache->GetDevice(), &buffer_create_info, nullptr, &buffer);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateBuffer failed: ");
    return false;
  }

  // Get memory requirements (types etc) for this buffer
  VkMemoryRequirements memory_requirements;
  vkGetBufferMemoryRequirements(m_object_cache->GetDevice(), buffer, &memory_requirements);

  // TODO: Support non-coherent mapping by use of vkFlushMappedMemoryRanges
  uint32_t memory_type_index = m_object_cache->GetMemoryType(
      memory_requirements.memoryTypeBits,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  // Allocate memory for backing this buffer
  VkMemoryAllocateInfo memory_allocate_info = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,  // VkStructureType    sType
      nullptr,                                 // const void*        pNext
      memory_requirements.size,                // VkDeviceSize       allocationSize
      memory_type_index                        // uint32_t           memoryTypeIndex
  };
  VkDeviceMemory memory = VK_NULL_HANDLE;
  res = vkAllocateMemory(m_object_cache->GetDevice(), &memory_allocate_info, nullptr, &memory);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkAllocateMemory failed: ");
    vkDestroyBuffer(m_object_cache->GetDevice(), buffer, nullptr);
    return false;
  }

  // Bind memory to buffer
  res = vkBindBufferMemory(m_object_cache->GetDevice(), buffer, memory, 0);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkBindBufferMemory failed: ");
    vkDestroyBuffer(m_object_cache->GetDevice(), buffer, nullptr);
    vkFreeMemory(m_object_cache->GetDevice(), memory, nullptr);
    return false;
  }

  // Map this buffer into user-space
  void* mapped_ptr = nullptr;
  res = vkMapMemory(m_object_cache->GetDevice(), memory, 0, size, 0, &mapped_ptr);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkMapMemory failed: ");
    vkDestroyBuffer(m_object_cache->GetDevice(), buffer, nullptr);
    vkFreeMemory(m_object_cache->GetDevice(), memory, nullptr);
    return false;
  }

  // Unmap current host pointer (if there was a previous buffer)
  if (m_host_pointer)
    vkUnmapMemory(m_object_cache->GetDevice(), m_memory);

  // Destroy the backings for the buffer after the command buffer executes
  if (m_buffer != VK_NULL_HANDLE)
    m_command_buffer_mgr->DeferResourceDestruction(m_buffer);
  if (m_memory != VK_NULL_HANDLE)
    m_command_buffer_mgr->DeferResourceDestruction(m_memory);

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

  // Check for space past the current gpu position
  if (m_current_offset >= m_current_gpu_position)
  {
    size_t remaining_bytes = m_current_size - m_current_offset;
    if (required_bytes <= remaining_bytes)
    {
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

  // Are we behind the gpu?
  if (m_current_offset < m_current_gpu_position)
  {
    size_t remaining_bytes = m_current_gpu_position - m_current_offset;
    if (required_bytes < remaining_bytes)
    {
      // Put after the current allocation but before the gpu
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
    // Allocate from the start of the buffer.
    if (m_current_offset > m_current_gpu_position)
      m_current_offset = 0;

    assert((m_current_offset + required_bytes) < m_current_gpu_position);
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
  assert((m_current_offset + final_num_bytes) <= m_current_size);
  assert(final_num_bytes <= m_last_allocation_size);
  m_current_offset += final_num_bytes;

  // TODO: For non-coherent mappings, flush the memory range
}

void StreamBuffer::OnFencePointCreated(VkFence fence)
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

  m_tracked_fences.push_back(std::make_pair(fence, m_current_offset));
}

void StreamBuffer::OnFencePointReached(VkFence fence)
{
  // Locate the entry for this fence (if any, we may have been forced to wait already)
  auto iter = std::find_if(m_tracked_fences.begin(), m_tracked_fences.end(),
                           [fence](const auto& it) { return (it.first == fence); });

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
  // Currently behind the GPU? If so, we have to take that into consideration.
  size_t required_position = num_bytes;
  if (m_current_offset < m_current_gpu_position)
    required_position += m_current_offset;

  // Find a fence to wait on that would give us enough space.
  auto iter =
      std::find_if(m_tracked_fences.begin(), m_tracked_fences.end(),
                   [required_position](const auto& it) { return (it.second > required_position); });

  // Did any fences satisfy this condition?
  if (iter == m_tracked_fences.end())
    return false;

  // Wait until this fence is signaled.
  VkResult res = vkWaitForFences(m_object_cache->GetDevice(), 1, &iter->first, VK_TRUE, UINT64_MAX);
  if (res != VK_SUCCESS)
    LOG_VULKAN_ERROR(res, "vkWaitForFences failed: ");

  // Update GPU position, and remove all fences up to (and including) this fence.
  m_current_gpu_position = iter->second;
  m_tracked_fences.erase(m_tracked_fences.begin(), ++iter);
  return true;
}

}  // namespace Vulkan
