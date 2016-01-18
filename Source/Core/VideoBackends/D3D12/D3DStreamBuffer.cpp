// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>

#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DStreamBuffer.h"
#include "VideoBackends/D3D12/D3DUtil.h"

namespace DX12
{

D3DStreamBuffer::D3DStreamBuffer(unsigned int initial_size, unsigned int max_size, bool* buffer_reallocation_notification) :
	m_buffer_size(initial_size),
	m_buffer_max_size(max_size),
	m_buffer_reallocation_notification(buffer_reallocation_notification)
{
	CHECK(initial_size <= max_size, "Error: Initial size for D3DStreamBuffer is greater than max_size.");

	AllocateBuffer(initial_size);

	// Register for callback from D3DCommandListManager each time a fence is queued to be signaled.
	m_buffer_tracking_fence = D3D::command_list_mgr->RegisterQueueFenceCallback(this, &D3DStreamBuffer::QueueFenceCallback);
}

D3DStreamBuffer::~D3DStreamBuffer()
{
	D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(m_buffer);
}

// Function returns true if (worst case), needed to flush existing command list in order to
// ensure the GPU finished with current use of buffer. The calling function will need to take
// care to reset GPU state to what it was previously.

// Obviously this is non-performant, so the buffer max_size should be large enough to
// ensure this never happens.
bool D3DStreamBuffer::AllocateSpaceInBuffer(unsigned int allocation_size, unsigned int alignment)
{
	CHECK(allocation_size <= m_buffer_max_size, "Error: Requested allocation size in D3DStreamBuffer is greater than max allowed size of backing buffer.");

	if (alignment)
	{
		unsigned int padding = m_buffer_offset % alignment;
		m_buffer_offset += alignment - padding;

		if (m_buffer_offset > m_buffer_size)
		{
			m_buffer_offset = 0;
			if (m_buffer_gpu_completion_offset == 0)
				m_buffer_gpu_completion_offset = 1; // Correct for case where CPU was about to run into GPU.
		}
	}

	// First, check if there is available (not-in-use-by-GPU) space in existing buffer.
	if (AttemptToAllocateOutOfExistingUnusedSpaceInBuffer(allocation_size))
	{
		return false;
	}

	// Slow path. No room at front, or back, due to the GPU still (possibly) accessing parts of the buffer.
	// Resize if possible, else stall.
	bool command_list_executed = AttemptBufferResizeOrElseStall(allocation_size);

	return command_list_executed;
}

// In VertexManager, we don't know the 'real' size of the allocation at the time
// we call AllocateSpaceInBuffer. We have to conservatively allocate 16MB (!).
// After the vertex data is written, we can choose to specify the 'real' allocation
// size to avoid wasting space.
void D3DStreamBuffer::OverrideSizeOfPreviousAllocation(unsigned int override_allocation_size)
{
	m_buffer_offset = m_buffer_current_allocation_offset + override_allocation_size;
}

void D3DStreamBuffer::AllocateBuffer(unsigned int size)
{
	// First, put existing buffer (if it exists) in deferred destruction list.
	if (m_buffer)
	{
		D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(m_buffer);
		m_buffer = nullptr;
	}

	CheckHR(
		D3D::device12->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(size),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_buffer)
			)
		);

	CheckHR(m_buffer->Map(0, nullptr, &m_buffer_cpu_address));

	m_buffer_gpu_address = m_buffer->GetGPUVirtualAddress();

	m_buffer_size = size;
}

// Function returns true if current command list executed as a result of current command list
// referencing all of buffer's contents, AND we are already at max_size. No alternative but to
// flush. See comments above AllocateSpaceInBuffer for more details.
bool D3DStreamBuffer::AttemptBufferResizeOrElseStall(unsigned int allocation_size)
{
	// This function will attempt to increase the size of the buffer, in response
	// to running out of room. If the buffer is already at its maximum size specified
	// at creation time, then stall waiting for the GPU to finish with the currently
	// requested memory.

	// Four possibilities, in order of desirability.
	// 1) Best - Update GPU tracking progress - maybe the GPU has made enough
	//    progress such that there is now room.
	// 2) Enlarge GPU buffer, up to our max allowed size.
	// 3) Stall until GPU finishes existing queued work/advances offset
	//    in buffer enough to free room.
	// 4) Worst - flush current GPU commands and wait, which will free all room
	//    in buffer.

	// 1) First, let's check if GPU has already continued farther along buffer. If it has freed up
	// enough of the buffer, we won't have to stall/allocate new memory.

	UpdateGPUProgress();

	// Now that GPU progress is updated, do we have room in the queue?
	if (AttemptToAllocateOutOfExistingUnusedSpaceInBuffer(allocation_size))
	{
		return false;
	}

	// 2) Next, prefer increasing buffer size instead of stalling.
	unsigned int new_size = std::min(static_cast<unsigned int>(m_buffer_size * 1.5f), m_buffer_max_size);
	new_size = std::max(new_size, allocation_size);

	// Can we grow buffer further?
	if (new_size > m_buffer_size)
	{
		AllocateBuffer(new_size);
		m_buffer_current_allocation_offset = 0;
		m_buffer_offset = allocation_size;

		if (m_buffer_reallocation_notification != nullptr)
		{
			*m_buffer_reallocation_notification = true;
		}

		return false;
	}

	// 3) Bad case - we need to stall.
	// This might be ok if we have > 2 frames queued up or something, but
	// we don't want to be stalling as we generate the front-of-queue frame.

	// First, let's find the first fence that will free up enough space in our buffer.

	UINT64 fence_value_required = 0;
	unsigned int new_buffer_offset = 0;

	while (m_queued_fences.size() > 0)
	{
		FenceTrackingInformation tracking_information = m_queued_fences.front();
		m_queued_fences.pop();

		if (m_buffer_offset >= m_buffer_gpu_completion_offset)
		{
			// At this point, we need to wrap around, so req'd gpu offset is allocation_size.
			if (tracking_information.buffer_offset >= allocation_size)
			{
				fence_value_required = tracking_information.fence_value;
				m_buffer_current_allocation_offset = 0;
				m_buffer_offset = allocation_size;
				break;
			}
		}
		else
		{
			if (m_buffer_offset + allocation_size <= m_buffer_size)
			{
				if (tracking_information.buffer_offset >= m_buffer_offset + allocation_size)
				{
					fence_value_required = tracking_information.fence_value;
					m_buffer_current_allocation_offset = m_buffer_offset;
					m_buffer_offset = m_buffer_offset + allocation_size;
					break;
				}
			}
			else
			{
				if (tracking_information.buffer_offset >= allocation_size)
				{
					fence_value_required = tracking_information.fence_value;
					m_buffer_current_allocation_offset = 0;
					m_buffer_offset = allocation_size;
					break;
				}
			}
		}
	}

	// Check if we found a fence we can wait on, for GPU to make sufficient progress.
	// If so, wait on it.
	if (fence_value_required > 0)
	{
		D3D::command_list_mgr->WaitOnCPUForFence(m_buffer_tracking_fence, fence_value_required);
		return false;
	}

	// 4) If we get to this point, that means there is no outstanding queued GPU work, and we're still out of room.
	// This is bad - and performance will suffer due to the CPU/GPU serialization, but the show must go on.

	// This is guaranteed to succeed, since we've already CHECK'd that the allocation_size <= max_buffer_size, and flushing now and waiting will
	// free all space in buffer.

	D3D::command_list_mgr->ExecuteQueuedWork(true);

	m_buffer_offset = allocation_size;
	m_buffer_current_allocation_offset = 0;
	m_buffer_gpu_completion_offset = 0;

	return true;
}

// Return true if space is found.
bool D3DStreamBuffer::AttemptToAllocateOutOfExistingUnusedSpaceInBuffer(unsigned int allocation_size)
{
	// First, check if there is room at end of buffer. Fast path.
	if (m_buffer_offset >= m_buffer_gpu_completion_offset)
	{
		if (m_buffer_offset + allocation_size <= m_buffer_size)
		{
			m_buffer_current_allocation_offset = m_buffer_offset;
			m_buffer_offset += allocation_size;
			return true;
		}

		if (0 + allocation_size < m_buffer_gpu_completion_offset)
		{
			m_buffer_current_allocation_offset = 0;
			m_buffer_offset = allocation_size;
			return true;
		}
	}

	// Next, check if there is room at front of buffer. Fast path.
	if (m_buffer_offset < m_buffer_gpu_completion_offset && m_buffer_offset + allocation_size < m_buffer_gpu_completion_offset)
	{
		m_buffer_current_allocation_offset = m_buffer_offset;
		m_buffer_offset += allocation_size;
		return true;
	}

	return false;
}

void D3DStreamBuffer::UpdateGPUProgress()
{
	const UINT64 fence_value = m_buffer_tracking_fence->GetCompletedValue();

	while (m_queued_fences.size() > 0)
	{
		FenceTrackingInformation tracking_information = m_queued_fences.front();
		m_queued_fences.pop();

		// Has fence gone past this point?
		if (fence_value > tracking_information.fence_value)
		{
			m_buffer_gpu_completion_offset = tracking_information.buffer_offset;
		}
		else
		{
			// Fences are stored in assending order, so once we hit a fence we haven't yet crossed on GPU, abort search.
			break;
		}
	}
}

void D3DStreamBuffer::QueueFenceCallback(void* owning_object, UINT64 fence_value)
{
	reinterpret_cast<D3DStreamBuffer*>(owning_object)->QueueFence(fence_value);
}

void D3DStreamBuffer::QueueFence(UINT64 fence_value)
{
	FenceTrackingInformation tracking_information = {};
	tracking_information.fence_value = fence_value;
	tracking_information.buffer_offset = m_buffer_offset;

	m_queued_fences.push(tracking_information);
}

ID3D12Resource* D3DStreamBuffer::GetBuffer() const
{
	return m_buffer;
}

D3D12_GPU_VIRTUAL_ADDRESS D3DStreamBuffer::GetGPUAddressOfCurrentAllocation() const
{
	return m_buffer_gpu_address + m_buffer_current_allocation_offset;
}

void* D3DStreamBuffer::GetCPUAddressOfCurrentAllocation() const
{
	return static_cast<u8*>(m_buffer_cpu_address) + m_buffer_current_allocation_offset;
}

unsigned int D3DStreamBuffer::GetOffsetOfCurrentAllocation() const
{
	return m_buffer_current_allocation_offset;
}

unsigned int D3DStreamBuffer::GetSize() const
{
	return m_buffer_size;
}

void* D3DStreamBuffer::GetBaseCPUAddress() const
{
	return m_buffer_cpu_address;
}

D3D12_GPU_VIRTUAL_ADDRESS D3DStreamBuffer::GetBaseGPUAddress() const
{
	return m_buffer_gpu_address;
}

}