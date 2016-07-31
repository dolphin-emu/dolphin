// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DDescriptorHeapManager.h"
#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DState.h"

namespace DX12
{
D3DDescriptorHeapManager::D3DDescriptorHeapManager(ID3D12DescriptorHeap* descriptor_heap,
                                                   ID3D12DescriptorHeap* shadow_descriptor_heap,
                                                   size_t num_descriptors,
                                                   size_t descriptor_increment_size,
                                                   size_t temporary_slots)
  : m_descriptor_heap(descriptor_heap), m_shadow_descriptor_heap(shadow_descriptor_heap),
  m_num_descriptors(num_descriptors), m_descriptor_increment_size(descriptor_increment_size),
  m_temporary_slots(temporary_slots),
  m_heap_base_cpu(descriptor_heap->GetCPUDescriptorHandleForHeapStart()),
  m_heap_base_gpu(descriptor_heap->GetGPUDescriptorHandleForHeapStart())
{
  if (shadow_descriptor_heap)
    m_shadow_heap_base = shadow_descriptor_heap->GetCPUDescriptorHandleForHeapStart();

  // Set all slots to unallocated (1)
  size_t bitset_count = num_descriptors / BITSET_SIZE + (((num_descriptors % BITSET_SIZE) != 0) ? 1 : 0);
  m_free_slots.resize(bitset_count);
  for (BitSetType& bs : m_free_slots)
    bs.flip();

  // Uses temporary slots?
  if (temporary_slots > 0)
  {
    _assert_(temporary_slots <= num_descriptors);

    // Set all temporary slots to allocated so we don't allocate them as a fixed handle.
    for (size_t i = 0; i < temporary_slots; i++)
      m_free_slots[i / BITSET_SIZE][i % BITSET_SIZE] = false;

    // Set up fence tracking callback.
    m_fence = D3D::command_list_mgr->RegisterQueueFenceCallback(this, &D3DDescriptorHeapManager::QueueFenceCallback);
  }
}

D3DDescriptorHeapManager::~D3DDescriptorHeapManager()
{
  D3D::command_list_mgr->RemoveQueueFenceCallback(this);
  if (m_shadow_descriptor_heap)
    m_shadow_descriptor_heap->Release();
  m_descriptor_heap->Release();
}

bool D3DDescriptorHeapManager::Allocate(size_t* out_index,
                                        D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle,
                                        D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle_shadow,
                                        D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle)
{
  // Start past the temporary slots, no point in searching those.
  for (size_t group = m_temporary_slots / BITSET_SIZE; group < m_free_slots.size(); group++)
  {
    BitSetType& bs = m_free_slots[group];
    if (bs.none())
      continue;

    size_t bit = 0;
    for (; bit < BITSET_SIZE; bit++)
    {
      if (bs[bit])
        break;
    }

    size_t index = group * BITSET_SIZE + bit;
    bs[bit] = false;

    if (out_index)
      *out_index = index;
    if (out_cpu_handle)
      out_cpu_handle->ptr = m_heap_base_cpu.ptr + index * m_descriptor_increment_size;
    if (out_cpu_handle_shadow && m_shadow_descriptor_heap)
      out_cpu_handle_shadow->ptr = m_shadow_heap_base.ptr + index * m_descriptor_increment_size;
    if (out_gpu_handle)
      out_gpu_handle->ptr = m_heap_base_gpu.ptr + index * m_descriptor_increment_size;

    return true;
  }

  PanicAlert("Out of fixed descriptors");
  return false;
}

void D3DDescriptorHeapManager::Free(size_t index)
{
  _assert_(index < m_num_descriptors);

  size_t group = index / BITSET_SIZE;
  size_t bit = index % BITSET_SIZE;
  m_free_slots[group][bit] = true;
}

bool D3DDescriptorHeapManager::AllocateTemporary(size_t num_handles,
                                                 D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_base_handle,
                                                 D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_base_handle)
{
  // Try allocating without waiting.
  if (TryAllocateTemporaryHandles(num_handles, out_cpu_base_handle, out_gpu_base_handle))
    return true;

  // Consume fences to create more space.
  while (!m_fences.empty())
  {
    auto& iter = m_fences.begin();
    D3D::command_list_mgr->WaitOnCPUForFence(m_fence, iter->first);
    m_gpu_temporary_descriptor_index = iter->second;
    m_fences.pop_front();

    // Retry allocation.
    if (TryAllocateTemporaryHandles(num_handles, out_cpu_base_handle, out_gpu_base_handle))
      return true;
  }

  // No space. Need to execute command list.
  return false;
}

bool D3DDescriptorHeapManager::TryAllocateTemporaryHandles(size_t num_handles,
                                                           D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_base_handle,
                                                           D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_base_handle)
{
  size_t allocation_start_index;

  // In front of the GPU?
  if (m_current_temporary_descriptor_index >= m_gpu_temporary_descriptor_index)
  {
    size_t remaining_slots = m_temporary_slots - m_current_temporary_descriptor_index;
    if (num_handles <= remaining_slots)
    {
      // Allocate at current offset.
      allocation_start_index = m_current_temporary_descriptor_index;
    }
    else
    {
      // Move behind the GPU?
      if (num_handles < m_gpu_temporary_descriptor_index)
      {
        // Allocate at start of list.
        allocation_start_index = 0;
      }
      else
      {
        // Can't allocate.
        return false;
      }
    }
  }
  else
  {
    // Behind the GPU. We use < here because if we used <= gpu could equal current.
    size_t remaining_slots = m_gpu_temporary_descriptor_index - m_current_temporary_descriptor_index;
    if (num_handles < remaining_slots)
    {
      // Allocate at current offset.
      allocation_start_index = m_current_temporary_descriptor_index;
    }
    else
    {
      // Can't allocate.
      return false;
    }
  }

  if (out_cpu_base_handle)
    out_cpu_base_handle->ptr = m_heap_base_cpu.ptr + allocation_start_index * m_descriptor_increment_size;
  if (out_gpu_base_handle)
    out_gpu_base_handle->ptr = m_heap_base_gpu.ptr + allocation_start_index * m_descriptor_increment_size;

  m_current_temporary_descriptor_index = allocation_start_index + num_handles;
  return true;
}

void D3DDescriptorHeapManager::QueueFenceCallback(void* owner, u64 fence_value)
{
  D3DDescriptorHeapManager* this_ptr = reinterpret_cast<D3DDescriptorHeapManager*>(owner);

  // Don't add a new entry when the offset doesn't change.
  size_t current_index = this_ptr->m_current_temporary_descriptor_index;
  if (!this_ptr->m_fences.empty() && this_ptr->m_fences.back().second == current_index)
    return;
  
  this_ptr->m_fences.emplace_back(fence_value, current_index);
}

std::unique_ptr<D3DDescriptorHeapManager> D3DDescriptorHeapManager::Create(ID3D12Device* device,
                                                                           D3D12_DESCRIPTOR_HEAP_TYPE type,
                                                                           D3D12_DESCRIPTOR_HEAP_FLAGS flags,
                                                                           size_t num_descriptors,
                                                                           size_t temporary_slots)
{
  D3D12_DESCRIPTOR_HEAP_DESC desc = {
    type,                               // D3D12_DESCRIPTOR_HEAP_TYPE Type
    static_cast<UINT>(num_descriptors), // UINT NumDescriptors
    flags,                              // D3D12_DESCRIPTOR_HEAP_FLAGS Flags
    0                                   // UINT NodeMask
  };

  // So that we can bind a single texture as a descriptor table (and not run off the edge),
  // we add 8 slots to the SRV heap.
  if (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
    desc.NumDescriptors += 8;

  ID3D12DescriptorHeap* descriptor_heap;
  HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptor_heap));
  if (FAILED(hr))
  {
    PanicAlert("Failed to create D3D12 descriptor heap: %x", hr);
    return nullptr;
  }

  // Create shadow descriptor heap for shader-visible heaps to use as copy source.
  ID3D12DescriptorHeap* shadow_descriptor_heap = nullptr;
  if (desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
  {
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&shadow_descriptor_heap));
    if (FAILED(hr))
    {
      PanicAlert("Failed to create D3D12 descriptor heap: %x", hr);
      descriptor_heap->Release();
      return nullptr;
    }
  }

  // Pave descriptors, needed for shader resource views.
  size_t increment_size = device->GetDescriptorHandleIncrementSize(type);
  if (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
  {
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
      DXGI_FORMAT_R8G8B8A8_UNORM,               // DXGI_FORMAT Format
      D3D12_SRV_DIMENSION_TEXTURE2D,            // D3D12_SRV_DIMENSION ViewDimension
      D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING  // UINT Shader4ComponentMapping
    };

    D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptor_heap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < desc.NumDescriptors; i++)
    {
      device->CreateShaderResourceView(nullptr, &srv_desc, handle);
      handle.ptr += increment_size;
    }

    if (shadow_descriptor_heap)
    {
      handle = shadow_descriptor_heap->GetCPUDescriptorHandleForHeapStart();
      for (UINT i = 0; i < desc.NumDescriptors; i++)
      {
        device->CreateShaderResourceView(nullptr, &srv_desc, handle);
        handle.ptr += increment_size;
      }
    }
  }

  return std::make_unique<D3DDescriptorHeapManager>(descriptor_heap,
                                                    shadow_descriptor_heap,
                                                    num_descriptors,
                                                    increment_size,
                                                    temporary_slots);
}

bool operator==(const D3DSamplerHeapManager::SamplerStateSet& lhs,
                const D3DSamplerHeapManager::SamplerStateSet& rhs)
{
  // D3D12TODO: Do something more efficient than this.
  return (!memcmp(&lhs, &rhs, sizeof(D3DSamplerHeapManager::SamplerStateSet)));
}

D3DSamplerHeapManager::D3DSamplerHeapManager(ID3D12DescriptorHeap* descriptor_heap,
                                             size_t num_descriptors,
                                             size_t descriptor_increment_size)
    : m_descriptor_heap(descriptor_heap), m_descriptor_heap_size(num_descriptors),
  m_descriptor_increment_size(descriptor_increment_size),
  m_heap_base_cpu(descriptor_heap->GetCPUDescriptorHandleForHeapStart()),
  m_heap_base_gpu(descriptor_heap->GetGPUDescriptorHandleForHeapStart())
{

}

bool D3DSamplerHeapManager::Allocate(D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handle,
                                     D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handle)
{
  bool allocated_from_current_heap = true;

  if (m_current_offset + 1 >= m_descriptor_heap_size)
  {
    // If out of room in the heap, start back at beginning.
    allocated_from_current_heap = false;
    m_current_offset = 0;
  }

  if (cpu_handle)
    cpu_handle->ptr = m_heap_base_cpu.ptr + m_current_offset * m_descriptor_increment_size;

  if (gpu_handle)
    gpu_handle->ptr = m_heap_base_gpu.ptr + m_current_offset * m_descriptor_increment_size;

  m_current_offset++;

  return allocated_from_current_heap;
}

bool D3DSamplerHeapManager::AllocateGroup(unsigned int num_handles,
                                          D3D12_CPU_DESCRIPTOR_HANDLE* base_cpu_handle,
                                          D3D12_GPU_DESCRIPTOR_HANDLE* base_gpu_handle)
{
  bool allocated_from_current_heap = true;

  if (m_current_offset + num_handles >= m_descriptor_heap_size)
  {
    // If out of room in the heap, start back at beginning.
    allocated_from_current_heap = false;
    m_current_offset = 0;
  }

  if (base_cpu_handle)
    base_cpu_handle->ptr = m_heap_base_cpu.ptr + m_current_offset * m_descriptor_increment_size;

  if (base_gpu_handle)
    base_gpu_handle->ptr = m_heap_base_gpu.ptr + m_current_offset * m_descriptor_increment_size;

  m_current_offset += num_handles;

  return allocated_from_current_heap;
}

D3D12_GPU_DESCRIPTOR_HANDLE
D3DSamplerHeapManager::GetHandleForSamplerGroup(SamplerState* sampler_state,
                                                   unsigned int num_sampler_samples)
{
  auto it = m_sampler_map.find(*reinterpret_cast<SamplerStateSet*>(sampler_state));

  if (it == m_sampler_map.end())
  {
    D3D12_CPU_DESCRIPTOR_HANDLE base_sampler_cpu_handle;
    D3D12_GPU_DESCRIPTOR_HANDLE base_sampler_gpu_handle;

    // This is going to be an issue when the heap wraps around.
    // It'll end up rendering incorrectly for one frame.
    bool allocatedFromExistingHeap =
        AllocateGroup(num_sampler_samples, &base_sampler_cpu_handle, &base_sampler_gpu_handle);

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

ID3D12DescriptorHeap* D3DSamplerHeapManager::GetDescriptorHeap() const
{
  return m_descriptor_heap;
}

std::unique_ptr<D3DSamplerHeapManager> D3DSamplerHeapManager::Create(ID3D12Device* device,
                                                                     size_t num_descriptors)
{
  D3D12_DESCRIPTOR_HEAP_DESC desc = {
    D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,         // D3D12_DESCRIPTOR_HEAP_TYPE Type
    static_cast<UINT>(num_descriptors),         // UINT NumDescriptors
    D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,  // D3D12_DESCRIPTOR_HEAP_FLAGS Flags
    0                                           // UINT NodeMask
  };

  ID3D12DescriptorHeap* descriptor_heap;
  HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptor_heap));
  if (FAILED(hr))
  {
    PanicAlert("Failed to create D3D12 descriptor heap: %x", hr);
    return nullptr;
  }

  size_t increment_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

  return std::make_unique<D3DSamplerHeapManager>(descriptor_heap,
                                                 num_descriptors,
                                                 increment_size);
}

D3DSamplerHeapManager::~D3DSamplerHeapManager()
{
  m_descriptor_heap->Release();
}

}  // namespace DX12