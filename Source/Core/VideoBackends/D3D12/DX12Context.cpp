// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/D3D12/DX12Context.h"

#include <algorithm>
#include <array>
#include <dxgi1_2.h>
#include <queue>
#include <vector>

#include "Common/Assert.h"
#include "Common/DynamicLibrary.h"
#include "Common/StringUtil.h"

#include "VideoBackends/D3D12/Common.h"
#include "VideoBackends/D3D12/D3D12StreamBuffer.h"
#include "VideoBackends/D3D12/DescriptorHeapManager.h"
#include "VideoCommon/VideoConfig.h"

namespace DX12
{
std::unique_ptr<DXContext> g_dx_context;

// Private D3D12 state
static Common::DynamicLibrary s_d3d12_library;
static PFN_D3D12_CREATE_DEVICE s_d3d12_create_device;
static PFN_D3D12_GET_DEBUG_INTERFACE s_d3d12_get_debug_interface;
static PFN_D3D12_SERIALIZE_ROOT_SIGNATURE s_d3d12_serialize_root_signature;

DXContext::DXContext() = default;

DXContext::~DXContext()
{
  if (m_fence_event)
    CloseHandle(m_fence_event);
}

std::vector<u32> DXContext::GetAAModes(u32 adapter_index)
{
  // Use a temporary device if we aren't booting.
  Common::DynamicLibrary temp_lib;
  ComPtr<ID3D12Device> temp_device = g_dx_context ? g_dx_context->m_device : nullptr;
  if (!temp_device)
  {
    ComPtr<IDXGIFactory> temp_dxgi_factory = D3DCommon::CreateDXGIFactory(false);
    if (!temp_dxgi_factory)
      return {};

    ComPtr<IDXGIAdapter> adapter;
    temp_dxgi_factory->EnumAdapters(adapter_index, &adapter);

    PFN_D3D12_CREATE_DEVICE d3d12_create_device;
    if (!temp_lib.Open("d3d12.dll") ||
        !temp_lib.GetSymbol("D3D12CreateDevice", &d3d12_create_device))
    {
      return {};
    }

    HRESULT hr =
        d3d12_create_device(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&temp_device));
    if (!SUCCEEDED(hr))
      return {};
  }

  std::vector<u32> aa_modes;
  for (u32 samples = 1; samples < D3D12_MAX_MULTISAMPLE_SAMPLE_COUNT; ++samples)
  {
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS multisample_quality_levels = {};
    multisample_quality_levels.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    multisample_quality_levels.SampleCount = samples;

    temp_device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
                                     &multisample_quality_levels,
                                     sizeof(multisample_quality_levels));

    if (multisample_quality_levels.NumQualityLevels > 0)
      aa_modes.push_back(samples);
  }

  return aa_modes;
}

bool DXContext::SupportsTextureFormat(DXGI_FORMAT format)
{
  constexpr u32 required = D3D12_FORMAT_SUPPORT1_TEXTURE2D | D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE;

  D3D12_FEATURE_DATA_FORMAT_SUPPORT support = {format};
  return SUCCEEDED(m_device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support,
                                                 sizeof(support))) &&
         (support.Support1 & required) == required;
}

bool DXContext::Create(u32 adapter_index, bool enable_debug_layer)
{
  ASSERT(!g_dx_context);
  if (!s_d3d12_library.Open("d3d12.dll") ||
      !s_d3d12_library.GetSymbol("D3D12CreateDevice", &s_d3d12_create_device) ||
      !s_d3d12_library.GetSymbol("D3D12GetDebugInterface", &s_d3d12_get_debug_interface) ||
      !s_d3d12_library.GetSymbol("D3D12SerializeRootSignature", &s_d3d12_serialize_root_signature))
  {
    PanicAlertFmtT("d3d12.dll could not be loaded.");
    s_d3d12_library.Close();
    return false;
  }

  if (!D3DCommon::LoadLibraries())
  {
    s_d3d12_library.Close();
    return false;
  }

  g_dx_context.reset(new DXContext());
  if (!g_dx_context->CreateDXGIFactory(enable_debug_layer) ||
      !g_dx_context->CreateDevice(adapter_index, enable_debug_layer) ||
      !g_dx_context->CreateCommandQueue() || !g_dx_context->CreateFence())
  {
    Destroy();
    return false;
  }

  return true;
}

bool DXContext::CreateGlobalResources()
{
  return g_dx_context->CreateDescriptorHeaps() && g_dx_context->CreateRootSignatures() &&
         g_dx_context->CreateTextureUploadBuffer() && g_dx_context->CreateCommandLists();
}

void DXContext::Destroy()
{
  if (g_dx_context)
    g_dx_context.reset();

  s_d3d12_serialize_root_signature = nullptr;
  s_d3d12_get_debug_interface = nullptr;
  s_d3d12_create_device = nullptr;
  s_d3d12_library.Close();
  D3DCommon::UnloadLibraries();
}

bool DXContext::CreateDXGIFactory(bool enable_debug_layer)
{
  m_dxgi_factory = D3DCommon::CreateDXGIFactory(enable_debug_layer);
  return m_dxgi_factory != nullptr;
}

bool DXContext::CreateDevice(u32 adapter_index, bool enable_debug_layer)
{
  ComPtr<IDXGIAdapter> adapter;
  HRESULT hr = m_dxgi_factory->EnumAdapters(adapter_index, &adapter);
  if (FAILED(hr))
  {
    ERROR_LOG_FMT(VIDEO, "Adapter {} not found, using default: {}", adapter_index, DX12HRWrap(hr));
    adapter = nullptr;
  }

  // Enabling the debug layer will fail if the Graphics Tools feature is not installed.
  if (enable_debug_layer)
  {
    hr = s_d3d12_get_debug_interface(IID_PPV_ARGS(&m_debug_interface));
    if (SUCCEEDED(hr))
    {
      m_debug_interface->EnableDebugLayer();
    }
    else
    {
      ERROR_LOG_FMT(VIDEO, "Debug layer requested but not available: {}", DX12HRWrap(hr));
      enable_debug_layer = false;
    }
  }

  // Create the actual device.
  hr = s_d3d12_create_device(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to create D3D12 device: {}", DX12HRWrap(hr));
  if (FAILED(hr))
    return false;

  if (enable_debug_layer)
  {
    ComPtr<ID3D12InfoQueue> info_queue;
    if (SUCCEEDED(m_device.As(&info_queue)))
    {
      info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
      info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

      D3D12_INFO_QUEUE_FILTER filter = {};
      std::array<D3D12_MESSAGE_ID, 5> id_list{
          D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
          D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
          D3D12_MESSAGE_ID_CREATEGRAPHICSPIPELINESTATE_RENDERTARGETVIEW_NOT_SET,
          D3D12_MESSAGE_ID_CREATEINPUTLAYOUT_TYPE_MISMATCH,
          D3D12_MESSAGE_ID_DRAW_EMPTY_SCISSOR_RECTANGLE,
      };
      filter.DenyList.NumIDs = static_cast<UINT>(id_list.size());
      filter.DenyList.pIDList = id_list.data();
      info_queue->PushStorageFilter(&filter);
    }
  }

  return true;
}

bool DXContext::CreateCommandQueue()
{
  const D3D12_COMMAND_QUEUE_DESC queue_desc = {D3D12_COMMAND_LIST_TYPE_DIRECT,
                                               D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
                                               D3D12_COMMAND_QUEUE_FLAG_NONE};
  HRESULT hr = m_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_command_queue));
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to create command queue: {}", DX12HRWrap(hr));
  return SUCCEEDED(hr);
}

bool DXContext::CreateFence()
{
  HRESULT hr =
      m_device->CreateFence(m_completed_fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to create fence: {}", DX12HRWrap(hr));
  if (FAILED(hr))
    return false;

  m_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  ASSERT_MSG(VIDEO, m_fence_event != nullptr, "Failed to create fence event");
  if (!m_fence_event)
    return false;

  return true;
}

bool DXContext::CreateDescriptorHeaps()
{
  static constexpr size_t MAX_SRVS = 16384;
  static constexpr size_t MAX_RTVS = 8192;
  static constexpr size_t MAX_DSVS = 128;
  static constexpr size_t MAX_SAMPLERS = 16384;

  if (!m_descriptor_heap_manager.Create(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                                        MAX_SRVS) ||
      !m_rtv_heap_manager.Create(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, MAX_RTVS) ||
      !m_dsv_heap_manager.Create(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, MAX_DSVS) ||
      !m_sampler_heap_manager.Create(m_device.Get(), MAX_SAMPLERS))
  {
    return false;
  }

  m_gpu_descriptor_heaps[1] = m_sampler_heap_manager.GetDescriptorHeap();

  // Allocate null SRV descriptor for unbound textures.
  constexpr D3D12_SHADER_RESOURCE_VIEW_DESC null_srv_desc = {
      DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_SRV_DIMENSION_TEXTURE2D,
      D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING};

  if (!m_descriptor_heap_manager.Allocate(&m_null_srv_descriptor))
  {
    PanicAlertFmt("Failed to allocate null descriptor");
    return false;
  }

  m_device->CreateShaderResourceView(nullptr, &null_srv_desc, m_null_srv_descriptor.cpu_handle);
  return true;
}

static void SetRootParamConstant(D3D12_ROOT_PARAMETER* rp, u32 shader_reg, u32 num_values,
                                 D3D12_SHADER_VISIBILITY visibility)
{
  rp->ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  rp->Constants.Num32BitValues = num_values;
  rp->Constants.ShaderRegister = shader_reg;
  rp->Constants.RegisterSpace = 0;
  rp->ShaderVisibility = visibility;
}

static void SetRootParamCBV(D3D12_ROOT_PARAMETER* rp, u32 shader_reg,
                            D3D12_SHADER_VISIBILITY visibility)
{
  rp->ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  rp->Descriptor.ShaderRegister = shader_reg;
  rp->Descriptor.RegisterSpace = 0;
  rp->ShaderVisibility = visibility;
}

static void SetRootParamTable(D3D12_ROOT_PARAMETER* rp, D3D12_DESCRIPTOR_RANGE* dr,
                              D3D12_DESCRIPTOR_RANGE_TYPE rt, u32 start_shader_reg,
                              u32 num_shader_regs, D3D12_SHADER_VISIBILITY visibility)
{
  dr->RangeType = rt;
  dr->NumDescriptors = num_shader_regs;
  dr->BaseShaderRegister = start_shader_reg;
  dr->RegisterSpace = 0;
  dr->OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  rp->ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  rp->DescriptorTable.pDescriptorRanges = dr;
  rp->DescriptorTable.NumDescriptorRanges = 1;
  rp->ShaderVisibility = visibility;
}

static bool BuildRootSignature(ID3D12Device* device, ID3D12RootSignature** sig_ptr,
                               const D3D12_ROOT_PARAMETER* params, u32 num_params)
{
  D3D12_ROOT_SIGNATURE_DESC desc = {};
  desc.pParameters = params;
  desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
               D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
               D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
  desc.NumParameters = num_params;

  ComPtr<ID3DBlob> root_signature_blob;
  ComPtr<ID3DBlob> root_signature_error_blob;

  HRESULT hr = s_d3d12_serialize_root_signature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                                &root_signature_blob, &root_signature_error_blob);
  if (FAILED(hr))
  {
    PanicAlertFmt("Failed to serialize root signature: {}\n{}",
                  static_cast<const char*>(root_signature_error_blob->GetBufferPointer()),
                  DX12HRWrap(hr));
    return false;
  }

  hr = device->CreateRootSignature(0, root_signature_blob->GetBufferPointer(),
                                   root_signature_blob->GetBufferSize(), IID_PPV_ARGS(sig_ptr));
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to create root signature: {}", DX12HRWrap(hr));
  return true;
}

bool DXContext::CreateRootSignatures()
{
  return CreateGXRootSignature() && CreateUtilityRootSignature() && CreateComputeRootSignature();
}

bool DXContext::CreateGXRootSignature()
{
  // GX:
  //  - 3 constant buffers (bindings 0-2), 0/1 visible in PS, 2 visible in VS, 1 visible in GS.
  //  - VideoCommon::MAX_PIXEL_SHADER_SAMPLERS textures (visible in PS).
  //  - VideoCommon::MAX_PIXEL_SHADER_SAMPLERS samplers (visible in PS).
  //  - 1 UAV (visible in PS).

  std::array<D3D12_ROOT_PARAMETER, NUM_ROOT_PARAMETERS> params;
  std::array<D3D12_DESCRIPTOR_RANGE, NUM_ROOT_PARAMETERS> ranges;
  u32 param_count = 0;
  SetRootParamCBV(&params[param_count], 0, D3D12_SHADER_VISIBILITY_PIXEL);
  param_count++;
  SetRootParamTable(&params[param_count], &ranges[param_count], D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0,
                    VideoCommon::MAX_PIXEL_SHADER_SAMPLERS, D3D12_SHADER_VISIBILITY_PIXEL);
  param_count++;
  SetRootParamTable(&params[param_count], &ranges[param_count], D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
                    0, VideoCommon::MAX_PIXEL_SHADER_SAMPLERS, D3D12_SHADER_VISIBILITY_PIXEL);
  param_count++;
  SetRootParamCBV(&params[param_count], 0, D3D12_SHADER_VISIBILITY_VERTEX);
  param_count++;
  SetRootParamCBV(&params[param_count], 1, D3D12_SHADER_VISIBILITY_VERTEX);
  param_count++;
  if (g_ActiveConfig.UseVSForLinePointExpand())
    SetRootParamCBV(&params[param_count], 2, D3D12_SHADER_VISIBILITY_VERTEX);
  else
    SetRootParamCBV(&params[param_count], 0, D3D12_SHADER_VISIBILITY_GEOMETRY);
  param_count++;
  SetRootParamTable(&params[param_count], &ranges[param_count], D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3,
                    1, D3D12_SHADER_VISIBILITY_VERTEX);
  param_count++;
  SetRootParamConstant(&params[param_count], 3, 1, D3D12_SHADER_VISIBILITY_VERTEX);
  param_count++;

  // Since these must be contiguous, pixel lighting goes to bbox if not enabled.
  if (g_ActiveConfig.bBBoxEnable)
  {
    SetRootParamTable(&params[param_count], &ranges[param_count], D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
                      2, 1, D3D12_SHADER_VISIBILITY_PIXEL);
    param_count++;
  }
  if (g_ActiveConfig.bEnablePixelLighting)
  {
    SetRootParamCBV(&params[param_count], 1, D3D12_SHADER_VISIBILITY_PIXEL);
    param_count++;
  }

  return BuildRootSignature(m_device.Get(), &m_gx_root_signature, params.data(), param_count);
}

bool DXContext::CreateUtilityRootSignature()
{
  // Utility:
  //  - 1 constant buffer (binding 0, visible in VS/PS).
  //  - 8 textures (visible in PS).
  //  - 8 samplers (visible in PS).

  std::array<D3D12_ROOT_PARAMETER, NUM_ROOT_PARAMETERS> params;
  std::array<D3D12_DESCRIPTOR_RANGE, NUM_ROOT_PARAMETERS> ranges;
  SetRootParamCBV(&params[ROOT_PARAMETER_PS_CBV], 0, D3D12_SHADER_VISIBILITY_ALL);
  SetRootParamTable(&params[ROOT_PARAMETER_PS_SRV], &ranges[ROOT_PARAMETER_PS_SRV],
                    D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 8, D3D12_SHADER_VISIBILITY_PIXEL);
  SetRootParamTable(&params[ROOT_PARAMETER_PS_SAMPLERS], &ranges[ROOT_PARAMETER_PS_SAMPLERS],
                    D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 0, 8, D3D12_SHADER_VISIBILITY_PIXEL);
  return BuildRootSignature(m_device.Get(), &m_utility_root_signature, params.data(), 3);
}

bool DXContext::CreateComputeRootSignature()
{
  // Compute:
  //  - 1 constant buffer (binding 0).
  //  - 8 textures.
  //  - 8 samplers.
  //  - 1 UAV.

  std::array<D3D12_ROOT_PARAMETER, NUM_ROOT_PARAMETERS> params;
  std::array<D3D12_DESCRIPTOR_RANGE, NUM_ROOT_PARAMETERS> ranges;
  SetRootParamCBV(&params[CS_ROOT_PARAMETER_CBV], 0, D3D12_SHADER_VISIBILITY_ALL);
  SetRootParamTable(&params[CS_ROOT_PARAMETER_SRV], &ranges[CS_ROOT_PARAMETER_CBV],
                    D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 8, D3D12_SHADER_VISIBILITY_ALL);
  SetRootParamTable(&params[CS_ROOT_PARAMETER_SAMPLERS], &ranges[CS_ROOT_PARAMETER_SAMPLERS],
                    D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 0, 8, D3D12_SHADER_VISIBILITY_ALL);
  SetRootParamTable(&params[CS_ROOT_PARAMETER_UAV], &ranges[CS_ROOT_PARAMETER_UAV],
                    D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
  return BuildRootSignature(m_device.Get(), &m_compute_root_signature, params.data(), 4);
}

bool DXContext::CreateTextureUploadBuffer()
{
  if (!m_texture_upload_buffer.AllocateBuffer(TEXTURE_UPLOAD_BUFFER_SIZE))
  {
    PanicAlertFmt("Failed to create texture upload buffer");
    return false;
  }

  return true;
}

bool DXContext::CreateCommandLists()
{
  static constexpr size_t MAX_DRAWS_PER_FRAME = 8192;
  static constexpr size_t TEMPORARY_SLOTS = MAX_DRAWS_PER_FRAME * 8;

  for (u32 i = 0; i < NUM_COMMAND_LISTS; i++)
  {
    CommandListResources& res = m_command_lists[i];
    HRESULT hr = m_device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(res.command_allocator.GetAddressOf()));
    ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to create command allocator: {}", DX12HRWrap(hr));
    if (FAILED(hr))
      return false;

    hr = m_device->CreateCommandList(1, D3D12_COMMAND_LIST_TYPE_DIRECT, res.command_allocator.Get(),
                                     nullptr, IID_PPV_ARGS(res.command_list.GetAddressOf()));
    ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to create command list: {}", DX12HRWrap(hr));
    if (FAILED(hr))
    {
      return false;
    }

    // Close the command list, since the first thing we do is reset them.
    hr = res.command_list->Close();
    ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Closing new command list failed: {}", DX12HRWrap(hr));
    if (FAILED(hr))
      return false;

    if (!res.descriptor_allocator.Create(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                                         TEMPORARY_SLOTS) ||
        !res.sampler_allocator.Create(m_device.Get()))
    {
      return false;
    }
  }

  MoveToNextCommandList();
  return true;
}

void DXContext::MoveToNextCommandList()
{
  m_current_command_list = (m_current_command_list + 1) % NUM_COMMAND_LISTS;
  m_current_fence_value++;

  // We may have to wait if this command list hasn't finished on the GPU.
  CommandListResources& res = m_command_lists[m_current_command_list];
  WaitForFence(res.ready_fence_value);

  // Begin command list.
  res.command_allocator->Reset();
  res.command_list->Reset(res.command_allocator.Get(), nullptr);
  res.descriptor_allocator.Reset();
  if (res.sampler_allocator.ShouldReset())
    res.sampler_allocator.Reset();
  m_gpu_descriptor_heaps[0] = res.descriptor_allocator.GetDescriptorHeap();
  m_gpu_descriptor_heaps[1] = res.sampler_allocator.GetDescriptorHeap();
  res.ready_fence_value = m_current_fence_value;
}

void DXContext::ExecuteCommandList(bool wait_for_completion)
{
  CommandListResources& res = m_command_lists[m_current_command_list];

  // Close and queue command list.
  HRESULT hr = res.command_list->Close();
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to close command list: {}", DX12HRWrap(hr));
  const std::array<ID3D12CommandList*, 1> execute_lists{res.command_list.Get()};
  m_command_queue->ExecuteCommandLists(static_cast<UINT>(execute_lists.size()),
                                       execute_lists.data());

  // Update fence when GPU has completed.
  hr = m_command_queue->Signal(m_fence.Get(), m_current_fence_value);

  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to signal fence: {}", DX12HRWrap(hr));

  MoveToNextCommandList();
  if (wait_for_completion)
    WaitForFence(res.ready_fence_value);
}

void DXContext::DeferResourceDestruction(ID3D12Resource* resource)
{
  resource->AddRef();
  m_command_lists[m_current_command_list].pending_resources.push_back(resource);
}

void DXContext::DeferDescriptorDestruction(DescriptorHeapManager& manager, u32 index)
{
  m_command_lists[m_current_command_list].pending_descriptors.emplace_back(manager, index);
}

void DXContext::ResetSamplerAllocators()
{
  for (CommandListResources& res : m_command_lists)
    res.sampler_allocator.Reset();
}

void DXContext::RecreateGXRootSignature()
{
  m_gx_root_signature.Reset();
  if (!CreateGXRootSignature())
    PanicAlertFmt("Failed to re-create GX root signature.");
}

void DXContext::DestroyPendingResources(CommandListResources& cmdlist)
{
  for (const auto& dd : cmdlist.pending_descriptors)
    dd.first.Free(dd.second);
  cmdlist.pending_descriptors.clear();

  for (ID3D12Resource* res : cmdlist.pending_resources)
    res->Release();
  cmdlist.pending_resources.clear();
}

void DXContext::WaitForFence(u64 fence)
{
  if (m_completed_fence_value >= fence)
    return;

  // Try non-blocking check.
  m_completed_fence_value = m_fence->GetCompletedValue();
  if (m_completed_fence_value < fence)
  {
    // Fall back to event.
    HRESULT hr = m_fence->SetEventOnCompletion(fence, m_fence_event);
    ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to set fence event on completion: {}", DX12HRWrap(hr));
    WaitForSingleObject(m_fence_event, INFINITE);
    m_completed_fence_value = m_fence->GetCompletedValue();
  }

  // Release resources for as many command lists which have completed.
  u32 index = (m_current_command_list + 1) % NUM_COMMAND_LISTS;
  for (u32 i = 0; i < NUM_COMMAND_LISTS; i++)
  {
    CommandListResources& res = m_command_lists[index];
    if (m_completed_fence_value < res.ready_fence_value)
      break;

    DestroyPendingResources(res);
    index = (index + 1) % NUM_COMMAND_LISTS;
  }
}
}  // namespace DX12
