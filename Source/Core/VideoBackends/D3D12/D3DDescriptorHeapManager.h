// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d12.h>
#include <unordered_map>

#include "VideoBackends/D3D12/D3DState.h"

namespace DX12
{

// This class provides an abstraction for D3D12 descriptor heaps.
class D3DDescriptorHeapManager
{
public:

	D3DDescriptorHeapManager(D3D12_DESCRIPTOR_HEAP_DESC* desc, ID3D12Device* device, unsigned int temporarySlots = 0);
	~D3DDescriptorHeapManager();

	bool Allocate(D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handle = nullptr, D3D12_CPU_DESCRIPTOR_HANDLE* gpu_handle_cpu_shadow = nullptr, bool temporary = false);
	bool AllocateGroup(D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handles, unsigned int num_handles, D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handles = nullptr, D3D12_CPU_DESCRIPTOR_HANDLE* gpu_handle_cpu_shadows = nullptr, bool temporary = false);

	D3D12_GPU_DESCRIPTOR_HANDLE GetHandleForSamplerGroup(SamplerState* sampler_state, unsigned int num_sampler_samples);

	ID3D12DescriptorHeap* GetDescriptorHeap() const;

	struct SamplerStateSet
	{
		SamplerState desc0;
		SamplerState desc1;
		SamplerState desc2;
		SamplerState desc3;
		SamplerState desc4;
		SamplerState desc5;
		SamplerState desc6;
		SamplerState desc7;
	};

private:

	ID3D12Device* m_device = nullptr;
	ID3D12DescriptorHeap* m_descriptor_heap = nullptr;
	ID3D12DescriptorHeap* m_descriptor_heap_cpu_shadow = nullptr;

	D3D12_CPU_DESCRIPTOR_HANDLE m_heap_base_cpu;
	D3D12_GPU_DESCRIPTOR_HANDLE m_heap_base_gpu;
	D3D12_CPU_DESCRIPTOR_HANDLE m_heap_base_gpu_cpu_shadow;

	struct hash_sampler_desc
	{
		size_t operator()(const SamplerStateSet sampler_state_set) const
		{
			return sampler_state_set.desc0.hex;
		}
	};

	std::unordered_map<SamplerStateSet, D3D12_GPU_DESCRIPTOR_HANDLE, hash_sampler_desc> m_sampler_map;

	unsigned int m_current_temporary_offset_in_heap = 0;
	unsigned int m_current_permanent_offset_in_heap = 0;

	unsigned int m_descriptor_increment_size;
	unsigned int m_descriptor_heap_size;
	bool m_gpu_visible;

	unsigned int m_first_temporary_slot_in_heap;
};

}  // namespace