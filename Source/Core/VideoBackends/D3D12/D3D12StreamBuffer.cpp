// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/D3D12/D3D12StreamBuffer.h"

#include <algorithm>
#include <functional>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/D3D12/DX12Context.h"

namespace DX12
{
StreamBuffer::StreamBuffer() = default;

StreamBuffer::~StreamBuffer()
{
  if (m_host_pointer)
  {
    const D3D12_RANGE written_range = {0, m_size};
    m_buffer->Unmap(0, &written_range);
  }

  // These get destroyed at shutdown anyway, so no need to defer destruction.
  if (m_buffer)
    m_buffer->Release();
}

bool StreamBuffer::AllocateBuffer(u32 size)
{
  static const D3D12_HEAP_PROPERTIES heap_properties = {D3D12_HEAP_TYPE_UPLOAD};
  const D3D12_RESOURCE_DESC resource_desc = {D3D12_RESOURCE_DIMENSION_BUFFER,
                                             0,
                                             size,
                                             1,
                                             1,
                                             1,
                                             DXGI_FORMAT_UNKNOWN,
                                             {1, 0},
                                             D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                                             D3D12_RESOURCE_FLAG_NONE};

  HRESULT hr = g_dx_context->GetDevice()->CreateCommittedResource(
      &heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr, IID_PPV_ARGS(&m_buffer));
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to allocate buffer of size {}: {}", size,
             DX12HRWrap(hr));
  if (FAILED(hr))
    return false;

  static const D3D12_RANGE read_range = {};
  hr = m_buffer->Map(0, &read_range, reinterpret_cast<void**>(&m_host_pointer));
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to map buffer of size {}: {}", size, DX12HRWrap(hr));
  if (FAILED(hr))
    return false;

  m_size = size;
  m_gpu_pointer = m_buffer->GetGPUVirtualAddress();
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
  else
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
  m_current_offset += final_num_bytes;
}

void StreamBuffer::UpdateCurrentFencePosition()
{
  // Don't create a tracking entry if the GPU is caught up with the buffer.
  if (m_current_offset == m_current_gpu_position)
    return;

  // Has the offset changed since the last fence?
  const u64 fence = g_dx_context->GetCurrentFenceValue();
  if (!m_tracked_fences.empty() && m_tracked_fences.back().first == fence)
  {
    // Still haven't executed a command buffer, so just update the offset.
    m_tracked_fences.back().second = m_current_offset;
    return;
  }

  UpdateGPUPosition();
  m_tracked_fences.emplace_back(fence, m_current_offset);
}

void StreamBuffer::UpdateGPUPosition()
{
  auto start = m_tracked_fences.begin();
  auto end = start;

  const u64 completed_counter = g_dx_context->GetCompletedFenceValue();
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
  if (iter == m_tracked_fences.end() || iter->first == g_dx_context->GetCurrentFenceValue())
    return false;

  // Wait until this fence is signaled. This will fire the callback, updating the GPU position.
  g_dx_context->WaitForFence(iter->first);
  m_tracked_fences.erase(m_tracked_fences.begin(),
                         m_current_offset == iter->second ? m_tracked_fences.end() : ++iter);
  m_current_offset = new_offset;
  m_current_gpu_position = new_gpu_position;
  return true;
}

}  // namespace DX12
