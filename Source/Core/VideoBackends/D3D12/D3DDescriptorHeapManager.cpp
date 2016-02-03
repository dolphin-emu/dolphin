// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DDescriptorHeapManager.h"
#include "VideoBackends/D3D12/D3DState.h"

namespace DX12
{

bool operator==(const D3DDescriptorHeapManager::SamplerStateSet& lhs, const D3DDescriptorHeapManager::SamplerStateSet& rhs)
{
	// D3D12TODO: Do something more efficient than this.
	return (!memcmp(&lhs, &rhs, sizeof(D3DDescriptorHeapManager::SamplerStateSet)));
}

D3DDescriptorHeapManager::D3DDescriptorHeapManager(D3D12_DESCRIPTOR_HEAP_DESC* desc, ID3D12Device* device, unsigned int temporarySlots) :
	m_device(device)
{
	CheckHR(device->CreateDescriptorHeap(desc, IID_PPV_ARGS(&m_descriptor_heap)));

	m_descriptor_heap_size = desc->NumDescriptors;
	m_descriptor_increment_size = device->GetDescriptorHandleIncrementSize(desc->Type);
	m_gpu_visible = (desc->Flags == D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

	if (m_gpu_visible)
	{
		D3D12_DESCRIPTOR_HEAP_DESC cpu_shadow_heap_desc = *desc;
		cpu_shadow_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		CheckHR(device->CreateDescriptorHeap(&cpu_shadow_heap_desc, IID_PPV_ARGS(&m_descriptor_heap_cpu_shadow)));

		m_heap_base_gpu = m_descriptor_heap->GetGPUDescriptorHandleForHeapStart();
		m_heap_base_gpu_cpu_shadow = m_descriptor_heap_cpu_shadow->GetCPUDescriptorHandleForHeapStart();
	}

	m_heap_base_cpu = m_descriptor_heap->GetCPUDescriptorHandleForHeapStart();

	m_first_temporary_slot_in_heap = m_descriptor_heap_size - temporarySlots;
	m_current_temporary_offset_in_heap = m_first_temporary_slot_in_heap;
}

bool D3DDescriptorHeapManager::Allocate(D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handle, D3D12_CPU_DESCRIPTOR_HANDLE* gpu_handle_cpu_shadow, bool temporary)
{
	bool allocated_from_current_heap = true;

	if (m_current_permanent_offset_in_heap + 1 >= m_first_temporary_slot_in_heap)
	{
		// If out of room in the heap, start back at beginning.
		allocated_from_current_heap = false;
		m_current_permanent_offset_in_heap = 0;
	}

	CHECK(!gpu_handle || (gpu_handle && m_gpu_visible), "D3D12_GPU_DESCRIPTOR_HANDLE used on non-GPU-visible heap.");

	if (temporary && m_current_temporary_offset_in_heap + 1 >= m_descriptor_heap_size)
	{
		m_current_temporary_offset_in_heap = m_first_temporary_slot_in_heap;
	}

	unsigned int heapOffsetToUse = temporary ? m_current_temporary_offset_in_heap : m_current_permanent_offset_in_heap;

	if (m_gpu_visible)
	{
		gpu_handle->ptr = m_heap_base_gpu.ptr + heapOffsetToUse * m_descriptor_increment_size;

		if (gpu_handle_cpu_shadow)
			gpu_handle_cpu_shadow->ptr = m_heap_base_gpu_cpu_shadow.ptr + heapOffsetToUse * m_descriptor_increment_size;
	}

	cpu_handle->ptr = m_heap_base_cpu.ptr + heapOffsetToUse * m_descriptor_increment_size;

	if (!temporary)
	{
		m_current_permanent_offset_in_heap++;
	}

	return allocated_from_current_heap;
}

bool D3DDescriptorHeapManager::AllocateGroup(D3D12_CPU_DESCRIPTOR_HANDLE* base_cpu_handle, unsigned int num_handles, D3D12_GPU_DESCRIPTOR_HANDLE* base_gpu_handle, D3D12_CPU_DESCRIPTOR_HANDLE* base_gpu_handle_cpu_shadow, bool temporary)
{
	bool allocated_from_current_heap = true;

	if (m_current_permanent_offset_in_heap + num_handles >= m_first_temporary_slot_in_heap)
	{
		// If out of room in the heap, start back at beginning.
		allocated_from_current_heap = false;
		m_current_permanent_offset_in_heap = 0;
	}

	CHECK(!base_gpu_handle || (base_gpu_handle && m_gpu_visible), "D3D12_GPU_DESCRIPTOR_HANDLE used on non-GPU-visible heap.");

	if (temporary && m_current_temporary_offset_in_heap + num_handles >= m_descriptor_heap_size)
	{
		m_current_temporary_offset_in_heap = m_first_temporary_slot_in_heap;
	}

	unsigned int heapOffsetToUse = temporary ? m_current_temporary_offset_in_heap : m_current_permanent_offset_in_heap;

	if (m_gpu_visible)
	{
		base_gpu_handle->ptr = m_heap_base_gpu.ptr + heapOffsetToUse * m_descriptor_increment_size;

		if (base_gpu_handle_cpu_shadow)
			base_gpu_handle_cpu_shadow->ptr = m_heap_base_gpu_cpu_shadow.ptr + heapOffsetToUse * m_descriptor_increment_size;
	}

	base_cpu_handle->ptr = m_heap_base_cpu.ptr + heapOffsetToUse * m_descriptor_increment_size;

	if (temporary)
	{
		m_current_temporary_offset_in_heap += num_handles;
	}
	else
	{
		m_current_permanent_offset_in_heap += num_handles;
	}

	return allocated_from_current_heap;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3DDescriptorHeapManager::GetHandleForSamplerGroup(SamplerState* sampler_state, unsigned int num_sampler_samples)
{
	auto it = m_sampler_map.find(*reinterpret_cast<SamplerStateSet*>(sampler_state));

	if (it == m_sampler_map.end())
	{
		D3D12_CPU_DESCRIPTOR_HANDLE base_sampler_cpu_handle;
		D3D12_GPU_DESCRIPTOR_HANDLE base_sampler_gpu_handle;

		bool allocatedFromExistingHeap = AllocateGroup(&base_sampler_cpu_handle, num_sampler_samples, &base_sampler_gpu_handle);

		if (!allocatedFromExistingHeap)
		{
			m_sampler_map.clear();
		}

		for (unsigned int i = 0; i < num_sampler_samples; i++)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE destinationDescriptor;
			destinationDescriptor.ptr = base_sampler_cpu_handle.ptr + i * D3D::sampler_descriptor_size;

			D3D::device12->CreateSampler(&StateCache::GetDesc12(sampler_state[i]), destinationDescriptor);
		}

		m_sampler_map[*reinterpret_cast<SamplerStateSet*>(sampler_state)] = base_sampler_gpu_handle;

		return base_sampler_gpu_handle;
	}
	else
	{
		return it->second;
	}
}

ID3D12DescriptorHeap* D3DDescriptorHeapManager::GetDescriptorHeap() const
{
	return m_descriptor_heap;
}

D3DDescriptorHeapManager::~D3DDescriptorHeapManager()
{
	SAFE_RELEASE(m_descriptor_heap);
	SAFE_RELEASE(m_descriptor_heap_cpu_shadow);
}

}  // namespace DX12