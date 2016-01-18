// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>

#include "D3DQueuedCommandList.h"

namespace DX12
{

// This class provides an abstraction for D3D12 descriptor heaps.
class D3DCommandListManager
{
public:

	D3DCommandListManager(D3D12_COMMAND_LIST_TYPE command_list_type, ID3D12Device* device, ID3D12CommandQueue* command_queue);

	void SetInitialCommandListState();

	void GetCommandList(ID3D12GraphicsCommandList** command_list) const;

	void ExecuteQueuedWork(bool wait_for_gpu_completion = false);
	void ExecuteQueuedWorkAndPresent(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags);

	void WaitForQueuedWorkToBeExecutedOnGPU();

	void ClearQueueAndWaitForCompletionOfInflightWork();
	void DestroyResourceAfterCurrentCommandListExecuted(ID3D12Resource* resource);
	void ImmediatelyDestroyAllResourcesScheduledForDestruction();

	void AddRef();
	unsigned int Release();

	bool m_dirty_vertex_buffer;
	bool m_dirty_pso;
	bool m_dirty_ps_cbv;
	bool m_dirty_vs_cbv;
	bool m_dirty_gs_cbv;
	bool m_dirty_samplers;
	unsigned int m_current_topology;

	unsigned int m_draws_since_last_execution = 0;
	bool m_cpu_access_last_frame = false;
	bool m_cpu_access_this_frame = false;

	void CPUAccessNotify() {
		m_cpu_access_last_frame = true;
		m_cpu_access_this_frame = true;
		m_draws_since_last_execution = 0;
	};

	// Allow other components to register for a callback each time a fence is queued.
	typedef void (PFN_QUEUE_FENCE_CALLBACK)(void* owning_object, UINT64 fence_value);

	ID3D12Fence* RegisterQueueFenceCallback(void* owning_object, PFN_QUEUE_FENCE_CALLBACK* callback_function);
	void RemoveQueueFenceCallback(void* owning_object);

	void WaitOnCPUForFence(ID3D12Fence* fence, UINT64 fence_value);

private:
	~D3DCommandListManager();

	void PerformGpuRolloverChecks();
	void ResetCommandListWithIdleCommandAllocator();

	HANDLE m_wait_on_cpu_fence_event;

	ID3D12Device* m_device;
	ID3D12CommandQueue* m_command_queue;
	UINT64 m_queue_fence_value;
	ID3D12Fence* m_queue_fence;
	UINT64 m_queue_frame_fence_value;
	ID3D12Fence* m_queue_frame_fence;

	std::map<void*,PFN_QUEUE_FENCE_CALLBACK*> m_queue_fence_callbacks;

	UINT m_current_command_allocator;
	UINT m_current_command_allocator_list;
	std::vector<ID3D12CommandAllocator*> m_command_allocator_lists[2];

	ID3D12GraphicsCommandList* m_backing_command_list;
	ID3D12QueuedCommandList* m_queued_command_list;

	ID3D12RootSignature* m_default_root_signature;

	UINT m_current_deferred_destruction_list;
	std::vector<ID3D12Resource*> m_deferred_destruction_lists[2];

	unsigned int m_ref = 1;
};

}  // namespace