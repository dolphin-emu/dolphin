// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>

#include "Common/CommonTypes.h"
#include "Common/HRWrap.h"

#include "VideoBackends/D3D12/Common.h"
#include "VideoBackends/D3D12/D3D12StreamBuffer.h"
#include "VideoBackends/D3D12/DescriptorAllocator.h"
#include "VideoBackends/D3D12/DescriptorHeapManager.h"

struct IDXGIFactory;

namespace DX12
{
// Vertex/Pixel shader root parameters
enum ROOT_PARAMETER
{
  ROOT_PARAMETER_PS_CBV,
  ROOT_PARAMETER_PS_SRV,
  ROOT_PARAMETER_PS_SAMPLERS,
  ROOT_PARAMETER_VS_CBV,
  ROOT_PARAMETER_VS_CBV2,
  ROOT_PARAMETER_GS_CBV,
  ROOT_PARAMETER_VS_SRV,
  ROOT_PARAMETER_BASE_VERTEX_CONSTANT,
  ROOT_PARAMETER_PS_UAV_OR_CBV2,
  ROOT_PARAMETER_PS_CBV2,  // ROOT_PARAMETER_PS_UAV_OR_CBV2 if bbox is not enabled
  NUM_ROOT_PARAMETERS
};
// Compute shader root parameters
enum CS_ROOT_PARAMETERS
{
  CS_ROOT_PARAMETER_CBV,
  CS_ROOT_PARAMETER_SRV,
  CS_ROOT_PARAMETER_SAMPLERS,
  CS_ROOT_PARAMETER_UAV,
  NUM_CS_ROOT_PARAMETERS,
};

class DXContext
{
public:
  ~DXContext();

  // Returns a list of AA modes.
  static std::vector<u32> GetAAModes(u32 adapter_index);

  // Creates new device and context.
  static bool Create(u32 adapter_index, bool enable_debug_layer);

  // Destroys active context.
  static void Destroy();

  IDXGIFactory* GetDXGIFactory() const { return m_dxgi_factory.Get(); }
  ID3D12Device* GetDevice() const { return m_device.Get(); }
  ID3D12CommandQueue* GetCommandQueue() const { return m_command_queue.Get(); }

  // Returns the current command list, commands can be recorded directly.
  ID3D12GraphicsCommandList* GetCommandList() const
  {
    return m_command_lists[m_current_command_list].command_list.Get();
  }
  DescriptorAllocator* GetDescriptorAllocator()
  {
    return &m_command_lists[m_current_command_list].descriptor_allocator;
  }
  SamplerAllocator* GetSamplerAllocator()
  {
    return &m_command_lists[m_current_command_list].sampler_allocator;
  }

  // Descriptor manager access.
  DescriptorHeapManager& GetDescriptorHeapManager() { return m_descriptor_heap_manager; }
  DescriptorHeapManager& GetRTVHeapManager() { return m_rtv_heap_manager; }
  DescriptorHeapManager& GetDSVHeapManager() { return m_dsv_heap_manager; }
  SamplerHeapManager& GetSamplerHeapManager() { return m_sampler_heap_manager; }
  ID3D12DescriptorHeap* const* GetGPUDescriptorHeaps() const
  {
    return m_gpu_descriptor_heaps.data();
  }
  u32 GetGPUDescriptorHeapCount() const { return static_cast<u32>(m_gpu_descriptor_heaps.size()); }
  const DescriptorHandle& GetNullSRVDescriptor() const { return m_null_srv_descriptor; }

  // Root signature access.
  ID3D12RootSignature* GetGXRootSignature() const { return m_gx_root_signature.Get(); }
  ID3D12RootSignature* GetUtilityRootSignature() const { return m_utility_root_signature.Get(); }
  ID3D12RootSignature* GetComputeRootSignature() const { return m_compute_root_signature.Get(); }

  // Fence value for current command list.
  u64 GetCurrentFenceValue() const { return m_current_fence_value; }

  // Last "completed" fence.
  u64 GetCompletedFenceValue() const { return m_completed_fence_value; }

  // Texture streaming buffer for uploads.
  StreamBuffer& GetTextureUploadBuffer() { return m_texture_upload_buffer; }

  // Feature level to use when compiling shaders.
  D3D_FEATURE_LEVEL GetFeatureLevel() const { return m_feature_level; }

  // Test for support for the specified texture format.
  bool SupportsTextureFormat(DXGI_FORMAT format);

  // Creates command lists, global buffers and descriptor heaps.
  bool CreateGlobalResources();

  // Executes the current command list.
  void ExecuteCommandList(bool wait_for_completion);

  // Waits for a specific fence.
  void WaitForFence(u64 fence);

  // Defers destruction of a D3D resource (associates it with the current list).
  void DeferResourceDestruction(ID3D12Resource* resource);

  // Defers destruction of a descriptor handle (associates it with the current list).
  void DeferDescriptorDestruction(DescriptorHeapManager& manager, u32 index);

  // Clears all samplers from the per-frame allocators.
  void ResetSamplerAllocators();

  // Re-creates the root signature. Call when the host config changes (e.g. bbox/per-pixel shading).
  void RecreateGXRootSignature();

private:
  // Number of command lists. One is being built while the other(s) are executed.
  static const u32 NUM_COMMAND_LISTS = 3;

  // Textures that don't fit into this buffer will be uploaded with a staging buffer.
  static const u32 TEXTURE_UPLOAD_BUFFER_SIZE = 32 * 1024 * 1024;

  struct CommandListResources
  {
    ComPtr<ID3D12CommandAllocator> command_allocator;
    ComPtr<ID3D12GraphicsCommandList> command_list;
    DescriptorAllocator descriptor_allocator;
    SamplerAllocator sampler_allocator;
    std::vector<ID3D12Resource*> pending_resources;
    std::vector<std::pair<DescriptorHeapManager&, u32>> pending_descriptors;
    u64 ready_fence_value = 0;
  };

  DXContext();

  bool CreateDXGIFactory(bool enable_debug_layer);
  bool CreateDevice(u32 adapter_index, bool enable_debug_layer);
  bool CreateCommandQueue();
  bool CreateFence();
  bool CreateDescriptorHeaps();
  bool CreateRootSignatures();
  bool CreateGXRootSignature();
  bool CreateUtilityRootSignature();
  bool CreateComputeRootSignature();
  bool CreateTextureUploadBuffer();
  bool CreateCommandLists();
  void MoveToNextCommandList();
  void DestroyPendingResources(CommandListResources& cmdlist);

  ComPtr<IDXGIFactory> m_dxgi_factory;
  ComPtr<ID3D12Debug> m_debug_interface;
  ComPtr<ID3D12Device> m_device;
  ComPtr<ID3D12CommandQueue> m_command_queue;

  ComPtr<ID3D12Fence> m_fence = nullptr;
  HANDLE m_fence_event = {};
  u32 m_current_fence_value = 0;
  u64 m_completed_fence_value = 0;

  std::array<CommandListResources, NUM_COMMAND_LISTS> m_command_lists;
  u32 m_current_command_list = NUM_COMMAND_LISTS - 1;

  DescriptorHeapManager m_descriptor_heap_manager;
  DescriptorHeapManager m_rtv_heap_manager;
  DescriptorHeapManager m_dsv_heap_manager;
  SamplerHeapManager m_sampler_heap_manager;
  std::array<ID3D12DescriptorHeap*, 2> m_gpu_descriptor_heaps = {};
  DescriptorHandle m_null_srv_descriptor;
  D3D_FEATURE_LEVEL m_feature_level = D3D_FEATURE_LEVEL_11_0;

  ComPtr<ID3D12RootSignature> m_gx_root_signature;
  ComPtr<ID3D12RootSignature> m_utility_root_signature;
  ComPtr<ID3D12RootSignature> m_compute_root_signature;

  StreamBuffer m_texture_upload_buffer;
};

extern std::unique_ptr<DXContext> g_dx_context;

// Wrapper for HRESULT to be used with fmt.  Note that we can't create a fmt::formatter directly
// for HRESULT as HRESULT is simply a typedef on long and not a distinct type.
// Unlike the version in Common, this variant also knows to call GetDeviceRemovedReason if needed.
struct DX12HRWrap
{
  constexpr explicit DX12HRWrap(HRESULT hr) : m_hr(hr) {}
  const HRESULT m_hr;
};

}  // namespace DX12

template <>
struct fmt::formatter<DX12::DX12HRWrap>
{
  constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const DX12::DX12HRWrap& hr, FormatContext& ctx) const
  {
    if (hr.m_hr == DXGI_ERROR_DEVICE_REMOVED && DX12::g_dx_context != nullptr &&
        DX12::g_dx_context->GetDevice() != nullptr)
    {
      return fmt::format_to(
          ctx.out(), "{}\nDevice removal reason: {}", Common::HRWrap(hr.m_hr),
          Common::HRWrap(DX12::g_dx_context->GetDevice()->GetDeviceRemovedReason()));
    }
    else
    {
      return fmt::format_to(ctx.out(), "{}", Common::HRWrap(hr.m_hr));
    }
  }
};
