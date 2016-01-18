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
	D3DStreamBuffer(unsigned int initial_size, unsigned int max_size, bool* buffer_reallocation_notification);
	~D3DStreamBuffer();

	bool AllocateSpaceInBuffer(unsigned int allocation_size, unsigned int alignment);
	void OverrideSizeOfPreviousAllocation(unsigned int override_allocation_size);

	void*                     GetBaseCPUAddress() const;
	D3D12_GPU_VIRTUAL_ADDRESS GetBaseGPUAddress() const;
	ID3D12Resource*           GetBuffer() const;
	void*                     GetCPUAddressOfCurrentAllocation() const;
	D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddressOfCurrentAllocation() const;
	unsigned int              GetOffsetOfCurrentAllocation() const;
	unsigned int              GetSize() const;

	static void QueueFenceCallback(void* owning_object, UINT64 fence_value);

private:
	void AllocateBuffer(unsigned int size);
	bool AttemptBufferResizeOrElseStall(unsigned int new_size);

	bool AttemptToAllocateOutOfExistingUnusedSpaceInBuffer(unsigned int allocation_size);

	void UpdateGPUProgress();
	void QueueFence(UINT64 fence_value);

	struct FenceTrackingInformation
	{
		UINT64 fence_value;
		unsigned int buffer_offset;
	};

	std::queue<FenceTrackingInformation> m_queued_fences;

	ID3D12Fence* m_buffer_tracking_fence = nullptr;

	ID3D12Resource* m_buffer = nullptr;

	void* m_buffer_cpu_address = nullptr;
	D3D12_GPU_VIRTUAL_ADDRESS m_buffer_gpu_address = {};

	unsigned int m_buffer_current_allocation_offset = 0;
	unsigned int m_buffer_offset = 0;
	unsigned int m_buffer_size = 0;

	const unsigned int m_buffer_max_size = 0;

	unsigned int m_buffer_gpu_completion_offset = 0;

	bool* m_buffer_reallocation_notification = nullptr;
};

}