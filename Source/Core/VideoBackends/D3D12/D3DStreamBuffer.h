// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <queue>

struct ID3D12Resource;

namespace DX12
{

class D3DStreamBuffer
{
public:
	D3DStreamBuffer(size_t initial_size, size_t max_size, bool* buffer_reallocation_notification);
	~D3DStreamBuffer();

	bool AllocateSpaceInBuffer(size_t allocation_size, size_t alignment, bool allow_execute = true);
	void OverrideSizeOfPreviousAllocation(size_t override_allocation_size);

	void*                     GetBaseCPUAddress() const;
	D3D12_GPU_VIRTUAL_ADDRESS GetBaseGPUAddress() const;
	ID3D12Resource*           GetBuffer() const;
	void*                     GetCPUAddressOfCurrentAllocation() const;
	D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddressOfCurrentAllocation() const;
	size_t                    GetOffsetOfCurrentAllocation() const;
	size_t                    GetSize() const;

	static void QueueFenceCallback(void* owning_object, UINT64 fence_value);

private:
	void AllocateBuffer(size_t size);
	bool AttemptBufferResizeOrElseStall(size_t allocation_size, bool allow_execute);

	bool AttemptToAllocateOutOfExistingUnusedSpaceInBuffer(size_t allocation_size);

	bool AttemptToFindExistingFenceToStallOn(size_t allocation_size);

	void UpdateGPUProgress();

	void ClearFences();
	bool HasBufferOffsetChangedSinceLastFence() const;
	void QueueFence(UINT64 fence_value);

	struct FenceTrackingInformation
	{
		UINT64 fence_value;
		size_t buffer_offset;
	};

	std::queue<FenceTrackingInformation> m_queued_fences;

	ID3D12Fence* m_buffer_tracking_fence = nullptr;

	ID3D12Resource* m_buffer = nullptr;

	void* m_buffer_cpu_address = nullptr;
	D3D12_GPU_VIRTUAL_ADDRESS m_buffer_gpu_address = {};

	size_t m_buffer_current_allocation_offset = 0;
	size_t m_buffer_offset = 0;
	size_t m_buffer_size = 0;

	const size_t m_buffer_max_size = 0;

	size_t m_buffer_gpu_completion_offset = 0;

	bool* m_buffer_reallocation_notification = nullptr;
};

}