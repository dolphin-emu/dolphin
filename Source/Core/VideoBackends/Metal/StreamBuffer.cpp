// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Metal/StreamBuffer.h"

#include <algorithm>
#include <cstdint>
#include <functional>

#include "Common/Assert.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Metal/CommandBufferManager.h"
#include "VideoBackends/Metal/MetalContext.h"
#include "VideoBackends/Metal/Util.h"

namespace Metal
{
StreamBuffer::StreamBuffer(u32 max_size) : m_maximum_size(max_size)
{
  // Add a callback that fires on fence point creation and signal
  g_command_buffer_mgr->AddFencePointCallback(
      this, std::bind(&StreamBuffer::OnCommandBufferQueued, this, std::placeholders::_1),
      std::bind(&StreamBuffer::OnCommandBufferExecuted, this, std::placeholders::_1));
}

StreamBuffer::~StreamBuffer()
{
  g_command_buffer_mgr->RemoveFencePointCallback(this);
  g_command_buffer_mgr->DeferBufferDestruction(m_buffer);
}

std::unique_ptr<StreamBuffer> StreamBuffer::Create(u32 initial_size, u32 max_size)
{
  std::unique_ptr<StreamBuffer> buffer = std::make_unique<StreamBuffer>(max_size);
  if (!buffer->ResizeBuffer(initial_size))
    return nullptr;

  return buffer;
}

bool StreamBuffer::ResizeBuffer(u32 size)
{
  const mtlpp::ResourceOptions options = static_cast<mtlpp::ResourceOptions>(
      static_cast<u32>(mtlpp::ResourceOptions::CpuCacheModeWriteCombined) |
      static_cast<u32>(mtlpp::ResourceOptions::StorageModeShared) |
      static_cast<u32>(mtlpp::ResourceOptions::HazardTrackingModeUntracked));
  auto buf = g_metal_context->GetDevice().NewBuffer(static_cast<u32>(size), options);
  if (!buf)
  {
    ERROR_LOG(VIDEO, "Failed to resize streaming buffer.");
    PanicAlert("Failed to resize streaming buffer.");
    return false;
  }

  // Replace with the new buffer.
  g_command_buffer_mgr->DeferBufferDestruction(m_buffer);
  INFO_LOG(VIDEO, "Allocated a %u byte streaming buffer", static_cast<unsigned>(size));
  m_buffer = buf;
  m_host_pointer = reinterpret_cast<u8*>(buf.GetContents());
  m_current_size = size;
  m_current_offset = 0;
  m_current_gpu_position = 0;
  m_tracked_fences.clear();
  return true;
}

bool StreamBuffer::ReserveMemory(u32 num_bytes, u32 alignment, bool allow_reuse /* = true */,
                                 bool allow_growth /* = true */,
                                 bool reallocate_if_full /* = false */)
{
  u32 required_bytes = num_bytes + alignment;

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
    u32 remaining_bytes = m_current_size - m_current_offset;
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
    u32 remaining_bytes = m_current_gpu_position - m_current_offset;
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
    u32 new_size = std::min(std::max(num_bytes, m_current_size * 2), m_maximum_size);
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

void StreamBuffer::CommitMemory(u32 final_num_bytes)
{
  _assert_((m_current_offset + final_num_bytes) <= m_current_size);
  _assert_(final_num_bytes <= m_last_allocation_size);

  // DidModify only applies when storage mode is managed.
  // m_buffer.DidModify(ns::Range(static_cast<u32>(m_current_offset),
  // static_cast<u32>(final_num_bytes)));
  m_current_offset += final_num_bytes;
}

void StreamBuffer::OnCommandBufferQueued(CommandBufferId id)
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

  m_tracked_fences.emplace_back(id, m_current_offset);
}

void StreamBuffer::OnCommandBufferExecuted(CommandBufferId id)
{
  // Locate the entry for this fence (if any, we may have been forced to wait already)
  auto iter = std::find_if(m_tracked_fences.begin(), m_tracked_fences.end(),
                           [id](const auto& it) { return it.first == id; });

  if (iter != m_tracked_fences.end())
  {
    // Update the GPU position, and remove any fences before this fence (since
    // it is implied that they have been signaled as well, though the callback
    // should have removed them already).
    m_current_gpu_position = iter->second;
    m_tracked_fences.erase(m_tracked_fences.begin(), ++iter);
  }
}

bool StreamBuffer::WaitForClearSpace(u32 num_bytes)
{
  u32 new_offset = 0;
  u32 new_gpu_position = 0;
  auto iter = m_tracked_fences.begin();
  for (; iter != m_tracked_fences.end(); iter++)
  {
    // Would this fence bring us in line with the GPU?
    // This is the "last resort" case, where a command buffer execution has been forced
    // after no additional data has been written to it, so we can assume that after the
    // fence has been signaled the entire buffer is now consumed.
    u32 gpu_position = iter->second;
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
  g_command_buffer_mgr->WaitForCommandBuffer(iter->first);

  // Update GPU position, and remove all fences up to (and including) this fence.
  m_current_offset = new_offset;
  m_current_gpu_position = new_gpu_position;
  m_tracked_fences.erase(m_tracked_fences.begin(), ++iter);
  return true;
}

}  // namespace Metal
