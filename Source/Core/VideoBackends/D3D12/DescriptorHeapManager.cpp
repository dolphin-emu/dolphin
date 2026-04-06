// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/D3D12/DescriptorHeapManager.h"

#include "Common/Assert.h"

#include "VideoBackends/D3D12/DX12Context.h"
#include "VideoCommon/VideoConfig.h"

namespace DX12
{
DescriptorHeapManager::DescriptorHeapManager() = default;
DescriptorHeapManager::~DescriptorHeapManager() = default;

bool DescriptorHeapManager::Create(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type,
                                   u32 num_descriptors)
{
  D3D12_DESCRIPTOR_HEAP_DESC desc = {type, static_cast<UINT>(num_descriptors),
                                     D3D12_DESCRIPTOR_HEAP_FLAG_NONE};

  HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_descriptor_heap));
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to create descriptor heap: {}", DX12HRWrap(hr));
  if (FAILED(hr))
    return false;

  m_heap_base_cpu = m_descriptor_heap->GetCPUDescriptorHandleForHeapStart();
  m_num_descriptors = num_descriptors;
  m_descriptor_increment_size = device->GetDescriptorHandleIncrementSize(type);

  // Set all slots to unallocated (1)
  const u32 bitset_count =
      num_descriptors / BITSET_SIZE + (((num_descriptors % BITSET_SIZE) != 0) ? 1 : 0);
  m_free_slots.resize(bitset_count);
  for (BitSetType& bs : m_free_slots)
    bs.flip();

  return true;
}

bool DescriptorHeapManager::Allocate(DescriptorHandle* handle)
{
  // Start past the temporary slots, no point in searching those.
  for (u32 group = 0; group < m_free_slots.size(); group++)
  {
    BitSetType& bs = m_free_slots[group];
    if (bs.none())
      continue;

    u32 bit = 0;
    for (; bit < BITSET_SIZE; bit++)
    {
      if (bs[bit])
        break;
    }

    u32 index = group * BITSET_SIZE + bit;
    bs[bit] = false;

    handle->index = index;
    handle->cpu_handle.ptr = m_heap_base_cpu.ptr + index * m_descriptor_increment_size;
    handle->gpu_handle.ptr = 0;
    return true;
  }

  PanicAlertFmt("Out of fixed descriptors");
  return false;
}

void DescriptorHeapManager::Free(u32 index)
{
  ASSERT(index < m_num_descriptors);

  u32 group = index / BITSET_SIZE;
  u32 bit = index % BITSET_SIZE;
  m_free_slots[group][bit] = true;
}

void DescriptorHeapManager::Free(const DescriptorHandle& handle)
{
  Free(handle.index);
}

SamplerHeapManager::SamplerHeapManager() = default;
SamplerHeapManager::~SamplerHeapManager() = default;

static void GetD3DSamplerDesc(D3D12_SAMPLER_DESC* desc, const SamplerState& state)
{
  if (state.tm0.mipmap_filter == FilterMode::Linear)
  {
    if (state.tm0.min_filter == FilterMode::Linear)
    {
      desc->Filter = (state.tm0.mag_filter == FilterMode::Linear) ?
                         D3D12_FILTER_MIN_MAG_MIP_LINEAR :
                         D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
    }
    else
    {
      desc->Filter = (state.tm0.mag_filter == FilterMode::Linear) ?
                         D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR :
                         D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
    }
  }
  else
  {
    if (state.tm0.min_filter == FilterMode::Linear)
    {
      desc->Filter = (state.tm0.mag_filter == FilterMode::Linear) ?
                         D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT :
                         D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
    }
    else
    {
      desc->Filter = (state.tm0.mag_filter == FilterMode::Linear) ?
                         D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT :
                         D3D12_FILTER_MIN_MAG_MIP_POINT;
    }
  }

  static constexpr std::array<D3D12_TEXTURE_ADDRESS_MODE, 3> address_modes = {
      {D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_WRAP,
       D3D12_TEXTURE_ADDRESS_MODE_MIRROR}};
  desc->AddressU = address_modes[static_cast<u32>(state.tm0.wrap_u.Value())];
  desc->AddressV = address_modes[static_cast<u32>(state.tm0.wrap_v.Value())];
  desc->AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  desc->MaxLOD = state.tm1.max_lod / 16.f;
  desc->MinLOD = state.tm1.min_lod / 16.f;
  desc->MipLODBias = static_cast<s32>(state.tm0.lod_bias) / 256.f;
  desc->ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

  if (state.tm0.anisotropic_filtering != 0)
  {
    desc->Filter = D3D12_FILTER_ANISOTROPIC;
    desc->MaxAnisotropy = 1u << state.tm0.anisotropic_filtering;
  }
}

bool SamplerHeapManager::Lookup(const SamplerState& ss, D3D12_CPU_DESCRIPTOR_HANDLE* handle)
{
  const auto it = m_sampler_map.find(ss);
  if (it != m_sampler_map.end())
  {
    *handle = it->second;
    return true;
  }

  if (m_current_offset == m_num_descriptors)
  {
    // We can clear at any time because the descriptors are copied prior to execution.
    // It's still not free, since we have to recreate all our samplers again.
    WARN_LOG_FMT(VIDEO, "Out of samplers, resetting CPU heap");
    Clear();
  }

  D3D12_SAMPLER_DESC desc = {};
  GetD3DSamplerDesc(&desc, ss);

  const D3D12_CPU_DESCRIPTOR_HANDLE new_handle = {m_heap_base_cpu.ptr +
                                                  m_current_offset * m_descriptor_increment_size};
  g_dx_context->GetDevice()->CreateSampler(&desc, new_handle);

  m_sampler_map.emplace(ss, new_handle);
  m_current_offset++;
  *handle = new_handle;
  return true;
}

void SamplerHeapManager::Clear()
{
  m_sampler_map.clear();
  m_current_offset = 0;
}

bool SamplerHeapManager::Create(ID3D12Device* device, u32 num_descriptors)
{
  const D3D12_DESCRIPTOR_HEAP_DESC desc = {D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, num_descriptors};
  HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_descriptor_heap));
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to create sampler descriptor heap: {}", DX12HRWrap(hr));
  if (FAILED(hr))
    return false;

  m_num_descriptors = num_descriptors;
  m_descriptor_increment_size =
      device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
  m_heap_base_cpu = m_descriptor_heap->GetCPUDescriptorHandleForHeapStart();
  return true;
}
}  // namespace DX12
