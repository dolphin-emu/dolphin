// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <queue>
#include <vector>

#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DDescriptorHeapManager.h"
#include "VideoBackends/D3D12/D3DQueuedCommandList.h"
#include "VideoBackends/D3D12/D3DState.h"
#include "VideoBackends/D3D12/D3DTexture.h"

#include "VideoBackends/D3D12/Render.h"
#include "VideoBackends/D3D12/ShaderConstantsManager.h"
#include "VideoBackends/D3D12/VertexManager.h"

static const unsigned int COMMAND_ALLOCATORS_PER_LIST = 2;

namespace DX12
{
extern StateCache gx_state_cache;

D3DCommandListManager::D3DCommandListManager(
	D3D12_COMMAND_LIST_TYPE command_list_type,
	ID3D12Device* device,
	ID3D12CommandQueue* command_queue
	) :
	m_device(device),
	m_command_queue(command_queue)
{
	// Create two lists, with two command allocators each. This corresponds to up to two frames in flight at once.
	m_current_command_allocator = 0;
	m_current_command_allocator_list = 0;
	for (UINT i = 0; i < COMMAND_ALLOCATORS_PER_LIST; i++)
	{
		for (UINT j = 0; j < ARRAYSIZE(m_command_allocator_lists); j++)
		{
			ID3D12CommandAllocator* command_allocator = nullptr;

			CheckHR(m_device->CreateCommandAllocator(command_list_type, IID_PPV_ARGS(&command_allocator)));
			m_command_allocator_lists[j].push_back(command_allocator);
		}
	}

	// Create backing command list.
	CheckHR(m_device->CreateCommandList(0, command_list_type, m_command_allocator_lists[m_current_command_allocator_list][0], nullptr, IID_PPV_ARGS(&m_backing_command_list)));

#ifdef USE_D3D12_QUEUED_COMMAND_LISTS
	m_queued_command_list = new ID3D12QueuedCommandList(m_backing_command_list, m_command_queue);
#endif

	// Create fence that will be used to measure GPU progress of app rendering requests (e.g. CPU readback of GPU data).
	m_queue_fence_value = 0;
	CheckHR(m_device->CreateFence(m_queue_fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_queue_fence)));

	// Create fence that will be used internally by D3DCommandListManager for frame-level resource tracking.
	m_queue_frame_fence_value = 0;
	CheckHR(m_device->CreateFence(m_queue_frame_fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_queue_frame_fence)));

	// Create event that will be used for waiting on CPU until a fence is signaled by GPU.
	m_wait_on_cpu_fence_event = CreateEvent(NULL, FALSE, FALSE, NULL);

	// Pre-size the deferred destruction lists.
	for (UINT i = 0; i < ARRAYSIZE(m_deferred_destruction_lists); i++)
	{
		m_deferred_destruction_lists[0].reserve(200);
	}

	m_current_deferred_destruction_list = 0;
}

void D3DCommandListManager::SetInitialCommandListState()
{
	ID3D12GraphicsCommandList* command_list = nullptr;
	GetCommandList(&command_list);

	command_list->SetDescriptorHeaps(ARRAYSIZE(D3D::gpu_descriptor_heaps), D3D::gpu_descriptor_heaps);
	command_list->SetGraphicsRootSignature(D3D::default_root_signature);

	if (g_renderer)
	{
		// It is possible that we change command lists in the middle of the frame. In that case, restore
		// the viewport/scissor to the current console GPU state.
		g_renderer->RestoreAPIState();
	}

	command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	if (g_vertex_manager)
		reinterpret_cast<VertexManager*>(g_vertex_manager.get())->SetIndexBuffer();

	m_dirty_pso = true;
	m_dirty_vertex_buffer = true;
	m_dirty_ps_cbv = true;
	m_dirty_vs_cbv = true;
	m_dirty_gs_cbv = true;
	m_dirty_samplers = true;
	m_current_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
}

void D3DCommandListManager::GetCommandList(ID3D12GraphicsCommandList** command_list) const
{
#ifdef USE_D3D12_QUEUED_COMMAND_LISTS
	*command_list = this->m_queued_command_list;
#else
	*command_list = this->m_backing_command_list;
#endif
}

void D3DCommandListManager::ExecuteQueuedWork(bool wait_for_gpu_completion)
{
	m_queue_fence_value++;



#ifdef USE_D3D12_QUEUED_COMMAND_LISTS
	CheckHR(m_queued_command_list->Close());
	m_queued_command_list->QueueExecute();

	m_queued_command_list->QueueFenceGpuSignal(m_queue_fence, m_queue_fence_value);

	ResetCommandListWithIdleCommandAllocator();

	m_queued_command_list->ProcessQueuedItems();
#else
	CheckHR(m_backing_command_list->Close());

	ID3D12CommandList* const commandListsToExecute[1] = { m_backing_command_list };
	m_command_queue->ExecuteCommandLists(1, commandListsToExecute);

	if (wait_for_gpu_completion)
	{
		CheckHR(m_command_queue->Signal(m_queue_fence, m_queue_fence_value));
	}

	if (m_current_command_allocator == 0)
	{
		PerformGpuRolloverChecks();
	}

	ResetCommandListWithIdleCommandAllocator();
#endif

	for (auto it : m_queue_fence_callbacks)
		it.second(it.first, m_queue_fence_value);

	SetInitialCommandListState();

	if (wait_for_gpu_completion)
	{
		WaitOnCPUForFence(m_queue_fence, m_queue_fence_value);
	}
}

void D3DCommandListManager::ExecuteQueuedWorkAndPresent(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags)
{
	m_queue_fence_value++;

#ifdef USE_D3D12_QUEUED_COMMAND_LISTS
	CheckHR(m_queued_command_list->Close());
	m_queued_command_list->QueueExecute();
	m_queued_command_list->QueueFenceGpuSignal(m_queue_fence, m_queue_fence_value);
	m_queued_command_list->QueuePresent(swap_chain, sync_interval, flags);
	m_queued_command_list->ProcessQueuedItems(true);

	if (m_current_command_allocator == 0)
	{
		PerformGpuRolloverChecks();
	}

	m_current_command_allocator = (m_current_command_allocator + 1) % m_command_allocator_lists[m_current_command_allocator_list].size();

	ResetCommandListWithIdleCommandAllocator();

	SetInitialCommandListState();
#else
	ExecuteQueuedWork();
	m_command_queue->Signal(m_queue_fence, m_queue_fence_value);
	CheckHR(swap_chain->Present(sync_interval, flags));
#endif

	for (auto it : m_queue_fence_callbacks)
		it.second(it.first, m_queue_fence_value);
}

void D3DCommandListManager::WaitForQueuedWorkToBeExecutedOnGPU()
{
	// Wait for GPU to finish all outstanding work.
	m_queue_fence_value++;

#ifdef USE_D3D12_QUEUED_COMMAND_LISTS
	m_queued_command_list->QueueExecute();
	m_queued_command_list->QueueFenceGpuSignal(m_queue_fence, m_queue_fence_value);

	m_queued_command_list->ProcessQueuedItems(true);
#else
	CheckHR(m_command_queue->Signal(m_queue_fence, m_queue_fence_value));
#endif

	WaitOnCPUForFence(m_queue_fence, m_queue_fence_value);
}

void D3DCommandListManager::PerformGpuRolloverChecks()
{
	// Insert fence to measure GPU progress, ensure we aren't using in-use command allocators.
	if (m_queue_frame_fence->GetCompletedValue() < m_queue_frame_fence_value)
	{
		WaitOnCPUForFence(m_queue_frame_fence, m_queue_frame_fence_value);
	}

	// We now know that the previous 'set' of command lists has completed on GPU, and it is safe to
	// release resources / start back at beginning of command allocator list.

	// Begin Deferred Resource Destruction
	UINT safe_to_delete_deferred_destruction_list = (m_current_deferred_destruction_list - 1) % ARRAYSIZE(m_deferred_destruction_lists);

	for (UINT i = 0; i < m_deferred_destruction_lists[safe_to_delete_deferred_destruction_list].size(); i++)
	{
		CHECK(m_deferred_destruction_lists[safe_to_delete_deferred_destruction_list][i]->Release() == 0, "Resource leak.");
	}

	m_deferred_destruction_lists[safe_to_delete_deferred_destruction_list].clear();

	m_current_deferred_destruction_list = (m_current_deferred_destruction_list + 1) % ARRAYSIZE(m_deferred_destruction_lists);
	// End Deferred Resource Destruction


	// Begin Command Allocator Resets
	UINT safe_to_reset_command_allocator_list = (m_current_command_allocator_list - 1) % ARRAYSIZE(m_command_allocator_lists);

	for (UINT i = 0; i < m_command_allocator_lists[safe_to_reset_command_allocator_list].size(); i++)
	{
		CheckHR(m_command_allocator_lists[safe_to_reset_command_allocator_list][i]->Reset());
	}

	m_current_command_allocator_list = (m_current_command_allocator_list + 1) % ARRAYSIZE(m_command_allocator_lists);
	// End Command Allocator Resets

	m_queue_frame_fence_value++;
#ifdef USE_D3D12_QUEUED_COMMAND_LISTS
	m_queued_command_list->QueueFenceGpuSignal(m_queue_frame_fence, m_queue_frame_fence_value);
#else
	CheckHR(m_command_queue->Signal(m_queue_frame_fence, m_queue_frame_fence_value));
#endif

	// Shader constant buffers are rolled-over independently of command allocator usage.
	ShaderConstantsManager::CheckToResetIndexPositionInUploadHeaps();
}

void D3DCommandListManager::ResetCommandListWithIdleCommandAllocator()
{
#ifdef USE_D3D12_QUEUED_COMMAND_LISTS
	ID3D12QueuedCommandList* command_list = m_queued_command_list;
#else
	ID3D12GraphicsCommandList* command_list = m_backing_command_list;
#endif

	CheckHR(command_list->Reset(m_command_allocator_lists[m_current_command_allocator_list][m_current_command_allocator], nullptr));
}

void D3DCommandListManager::DestroyResourceAfterCurrentCommandListExecuted(ID3D12Resource* resource)
{
	CHECK(resource, "Null resource being inserted!");

	m_deferred_destruction_lists[m_current_deferred_destruction_list].push_back(resource);
}

void D3DCommandListManager::ImmediatelyDestroyAllResourcesScheduledForDestruction()
{
	for (UINT i = 0; i < ARRAYSIZE(m_deferred_destruction_lists); i++)
	{
		for (UINT j = 0; j < m_deferred_destruction_lists[i].size(); j++)
		{
			m_deferred_destruction_lists[i][j]->Release();
		}

		m_deferred_destruction_lists[i].clear();
	}
}

void D3DCommandListManager::ClearQueueAndWaitForCompletionOfInflightWork()
{
	// Wait for GPU to finish all outstanding work.
	m_queue_fence_value++;
#ifdef USE_D3D12_QUEUED_COMMAND_LISTS
	m_queued_command_list->ClearQueue(); // Waits for currently-processing work to finish, then clears queue.
	m_queued_command_list->QueueFenceGpuSignal(m_queue_fence, m_queue_fence_value);
	m_queued_command_list->ProcessQueuedItems(true);
#else
	CheckHR(m_command_queue->Signal(m_queue_fence, m_queue_fence_value));
#endif
	WaitOnCPUForFence(m_queue_fence, m_queue_fence_value);
}

D3DCommandListManager::~D3DCommandListManager()
{
	ImmediatelyDestroyAllResourcesScheduledForDestruction();

#ifdef USE_D3D12_QUEUED_COMMAND_LISTS
	m_queued_command_list->Release();
#endif
	m_backing_command_list->Release();

	for (unsigned int i = 0; i < ARRAYSIZE(m_command_allocator_lists); i++)
	{
		for (unsigned int j = 0; j < m_command_allocator_lists[i].size(); j++)
		{
			m_command_allocator_lists[i][j]->Release();
		}
	}

	m_queue_fence->Release();
	m_queue_frame_fence->Release();

	CloseHandle(m_wait_on_cpu_fence_event);
}

void D3DCommandListManager::WaitOnCPUForFence(ID3D12Fence* fence, UINT64 fence_value)
{
	CheckHR(fence->SetEventOnCompletion(fence_value, m_wait_on_cpu_fence_event));

	WaitForSingleObject(m_wait_on_cpu_fence_event, INFINITE);
}

ID3D12Fence* D3DCommandListManager::RegisterQueueFenceCallback(void* owning_object, PFN_QUEUE_FENCE_CALLBACK* callback_function)
{
	m_queue_fence_callbacks[owning_object] = callback_function;

	return m_queue_fence;
}

void D3DCommandListManager::RemoveQueueFenceCallback(void* owning_object)
{
	m_queue_fence_callbacks.erase(owning_object);
}

void D3DCommandListManager::AddRef()
{
	++m_ref;
}

unsigned int D3DCommandListManager::Release()
{
	if (--m_ref == 0)
	{
		delete this;
		return 0;
	}
	return m_ref;
}

}  // namespace DX12