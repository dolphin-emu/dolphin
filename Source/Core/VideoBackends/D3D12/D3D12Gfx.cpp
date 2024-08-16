// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/D3D12/D3D12Gfx.h"

#include "Common/Logging/Log.h"

#include "VideoBackends/D3D12/Common.h"
#include "VideoBackends/D3D12/D3D12BoundingBox.h"
#include "VideoBackends/D3D12/D3D12PerfQuery.h"
#include "VideoBackends/D3D12/D3D12SwapChain.h"
#include "VideoBackends/D3D12/DX12Context.h"
#include "VideoBackends/D3D12/DX12Pipeline.h"
#include "VideoBackends/D3D12/DX12Shader.h"
#include "VideoBackends/D3D12/DX12Texture.h"
#include "VideoBackends/D3D12/DX12VertexFormat.h"
#include "VideoBackends/D3D12/DescriptorHeapManager.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/VideoConfig.h"

namespace DX12
{
static bool UsesDynamicVertexLoader(const AbstractPipeline* pipeline)
{
  const AbstractPipelineUsage usage = static_cast<const DXPipeline*>(pipeline)->GetUsage();
  return (g_ActiveConfig.backend_info.bSupportsDynamicVertexLoader &&
          usage == AbstractPipelineUsage::GXUber) ||
         (g_ActiveConfig.UseVSForLinePointExpand() && usage != AbstractPipelineUsage::Utility);
}

Gfx::Gfx(std::unique_ptr<SwapChain> swap_chain, float backbuffer_scale)
    : m_backbuffer_scale(backbuffer_scale), m_swap_chain(std::move(swap_chain))
{
  m_state.root_signature = g_dx_context->GetGXRootSignature();

  for (u32 i = 0; i < VideoCommon::MAX_PIXEL_SHADER_SAMPLERS; i++)
  {
    m_state.textures[i].ptr = g_dx_context->GetNullSRVDescriptor().cpu_handle.ptr;
    m_state.samplers.states[i] = RenderState::GetPointSamplerState();
  }
}

Gfx::~Gfx() = default;

bool Gfx::IsHeadless() const
{
  return !m_swap_chain;
}

std::unique_ptr<AbstractTexture> Gfx::CreateTexture(const TextureConfig& config,
                                                    std::string_view name)
{
  return DXTexture::Create(config, name);
}

std::unique_ptr<AbstractStagingTexture> Gfx::CreateStagingTexture(StagingTextureType type,
                                                                  const TextureConfig& config)
{
  return DXStagingTexture::Create(type, config);
}

std::unique_ptr<AbstractFramebuffer>
Gfx::CreateFramebuffer(AbstractTexture* color_attachment, AbstractTexture* depth_attachment,
                       std::vector<AbstractTexture*> additional_color_attachments)
{
  return DXFramebuffer::Create(static_cast<DXTexture*>(color_attachment),
                               static_cast<DXTexture*>(depth_attachment),
                               std::move(additional_color_attachments));
}

std::unique_ptr<AbstractShader>
Gfx::CreateShaderFromSource(ShaderStage stage, std::string_view source, std::string_view name)
{
  return DXShader::CreateFromSource(stage, source, name);
}

std::unique_ptr<AbstractShader> Gfx::CreateShaderFromBinary(ShaderStage stage, const void* data,
                                                            size_t length, std::string_view name)
{
  return DXShader::CreateFromBytecode(stage, DXShader::CreateByteCode(data, length), name);
}

std::unique_ptr<NativeVertexFormat>
Gfx::CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl)
{
  return std::make_unique<DXVertexFormat>(vtx_decl);
}

std::unique_ptr<AbstractPipeline> Gfx::CreatePipeline(const AbstractPipelineConfig& config,
                                                      const void* cache_data,
                                                      size_t cache_data_length)
{
  return DXPipeline::Create(config, cache_data, cache_data_length);
}

void Gfx::Flush()
{
  ExecuteCommandList(false);
}

void Gfx::WaitForGPUIdle()
{
  ExecuteCommandList(true);
}

void Gfx::ClearRegion(const MathUtil::Rectangle<int>& target_rc, bool color_enable,
                      bool alpha_enable, bool z_enable, u32 color, u32 z)
{
  // Use a fast path without the shader if both color/alpha are enabled.
  const bool fast_color_clear = color_enable && alpha_enable;
  if (fast_color_clear || z_enable)
  {
    const D3D12_RECT d3d_clear_rc{target_rc.left, target_rc.top, target_rc.right, target_rc.bottom};
    auto* d3d_frame_buffer = static_cast<const DXFramebuffer*>(m_current_framebuffer);

    if (fast_color_clear)
    {
      d3d_frame_buffer->TransitionRenderTargets();

      const std::array<float, 4> clear_color = {
          {static_cast<float>((color >> 16) & 0xFF) / 255.0f,
           static_cast<float>((color >> 8) & 0xFF) / 255.0f,
           static_cast<float>((color >> 0) & 0xFF) / 255.0f,
           static_cast<float>((color >> 24) & 0xFF) / 255.0f}};
      d3d_frame_buffer->ClearRenderTargets(clear_color, &d3d_clear_rc);
      color_enable = false;
      alpha_enable = false;
    }

    if (z_enable)
    {
      static_cast<DXTexture*>(m_current_framebuffer->GetDepthAttachment())
          ->TransitionToState(D3D12_RESOURCE_STATE_DEPTH_WRITE);

      // D3D does not support reversed depth ranges.
      const float clear_depth = 1.0f - static_cast<float>(z & 0xFFFFFF) / 16777216.0f;
      d3d_frame_buffer->ClearDepth(clear_depth, &d3d_clear_rc);
      z_enable = false;
    }
  }

  // Anything left over, fall back to clear triangle.
  if (color_enable || alpha_enable || z_enable)
    ::AbstractGfx::ClearRegion(target_rc, color_enable, alpha_enable, z_enable, color, z);
}

void Gfx::SetPipeline(const AbstractPipeline* pipeline)
{
  const DXPipeline* dx_pipeline = static_cast<const DXPipeline*>(pipeline);
  if (m_current_pipeline == dx_pipeline)
    return;

  m_current_pipeline = dx_pipeline;
  m_dirty_bits |= DirtyState_Pipeline;

  if (dx_pipeline)
  {
    if (dx_pipeline->GetRootSignature() != m_state.root_signature)
    {
      m_state.root_signature = dx_pipeline->GetRootSignature();
      m_dirty_bits |= DirtyState_RootSignature | DirtyState_PS_CBV | DirtyState_VS_CBV |
                      DirtyState_GS_CBV | DirtyState_SRV_Descriptor |
                      DirtyState_Sampler_Descriptor | DirtyState_UAV_Descriptor |
                      DirtyState_VS_SRV_Descriptor | DirtyState_PS_CUS_CBV;
    }
    if (dx_pipeline->UseIntegerRTV() != m_state.using_integer_rtv)
    {
      m_state.using_integer_rtv = dx_pipeline->UseIntegerRTV();
      m_dirty_bits |= DirtyState_Framebuffer;
    }
    if (dx_pipeline->GetPrimitiveTopology() != m_state.primitive_topology)
    {
      m_state.primitive_topology = dx_pipeline->GetPrimitiveTopology();
      m_dirty_bits |= DirtyState_PrimitiveTopology;
    }
  }
}

void Gfx::BindFramebuffer(DXFramebuffer* fb)
{
  fb->TransitionRenderTargets();
  if (fb->HasDepthBuffer())
  {
    static_cast<DXTexture*>(fb->GetDepthAttachment())
        ->TransitionToState(D3D12_RESOURCE_STATE_DEPTH_WRITE);
  }

  g_dx_context->GetCommandList()->OMSetRenderTargets(
      fb->GetRTVDescriptorCount(),
      m_state.using_integer_rtv ? fb->GetIntRTVDescriptorArray() : fb->GetRTVDescriptorArray(),
      FALSE, fb->GetDSVDescriptorArray());
  m_current_framebuffer = fb;
  m_dirty_bits &= ~DirtyState_Framebuffer;
}

void Gfx::SetFramebuffer(AbstractFramebuffer* framebuffer)
{
  if (m_current_framebuffer == framebuffer)
    return;

  m_current_framebuffer = framebuffer;
  m_dirty_bits |= DirtyState_Framebuffer;
}

void Gfx::SetAndDiscardFramebuffer(AbstractFramebuffer* framebuffer)
{
  SetFramebuffer(framebuffer);

  DXFramebuffer* d3d_frame_buffer = static_cast<DXFramebuffer*>(framebuffer);
  d3d_frame_buffer->TransitionRenderTargets();
  if (d3d_frame_buffer->HasDepthBuffer())
  {
    static_cast<DXTexture*>(d3d_frame_buffer->GetDepthAttachment())
        ->TransitionToState(D3D12_RESOURCE_STATE_DEPTH_WRITE);
  }
  d3d_frame_buffer->Unbind();
}

void Gfx::SetAndClearFramebuffer(AbstractFramebuffer* framebuffer, const ClearColor& color_value,
                                 float depth_value)
{
  DXFramebuffer* dxfb = static_cast<DXFramebuffer*>(framebuffer);
  BindFramebuffer(dxfb);

  dxfb->ClearRenderTargets(color_value, nullptr);
  dxfb->ClearDepth(depth_value, nullptr);
}

void Gfx::SetScissorRect(const MathUtil::Rectangle<int>& rc)
{
  if (m_state.scissor.left == rc.left && m_state.scissor.right == rc.right &&
      m_state.scissor.top == rc.top && m_state.scissor.bottom == rc.bottom)
  {
    return;
  }

  m_state.scissor.left = rc.left;
  m_state.scissor.right = rc.right;
  m_state.scissor.top = rc.top;
  m_state.scissor.bottom = rc.bottom;
  m_dirty_bits |= DirtyState_ScissorRect;
}

void Gfx::SetTexture(u32 index, const AbstractTexture* texture)
{
  const DXTexture* dxtex = static_cast<const DXTexture*>(texture);
  if (m_state.textures[index].ptr == dxtex->GetSRVDescriptor().cpu_handle.ptr)
    return;

  m_state.textures[index].ptr = dxtex->GetSRVDescriptor().cpu_handle.ptr;
  if (dxtex)
    dxtex->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

  m_dirty_bits |= DirtyState_Textures;
}

void Gfx::SetSamplerState(u32 index, const SamplerState& state)
{
  if (m_state.samplers.states[index] == state)
    return;

  m_state.samplers.states[index] = state;
  m_dirty_bits |= DirtyState_Samplers;
}

void Gfx::SetComputeImageTexture(u32 index, AbstractTexture* texture, bool read, bool write)
{
  const DXTexture* dxtex = static_cast<const DXTexture*>(texture);
  if (m_state.compute_image_texture == dxtex)
    return;

  m_state.compute_image_texture = dxtex;
  if (dxtex)
    dxtex->TransitionToState(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

  m_dirty_bits |= DirtyState_ComputeImageTexture;
}

void Gfx::UnbindTexture(const AbstractTexture* texture)
{
  const auto srv_shadow_descriptor =
      static_cast<const DXTexture*>(texture)->GetSRVDescriptor().cpu_handle;
  for (u32 i = 0; i < VideoCommon::MAX_PIXEL_SHADER_SAMPLERS; i++)
  {
    if (m_state.textures[i].ptr == srv_shadow_descriptor.ptr)
    {
      m_state.textures[i].ptr = g_dx_context->GetNullSRVDescriptor().cpu_handle.ptr;
      m_dirty_bits |= DirtyState_Textures;
    }
  }
  if (m_state.compute_image_texture == texture)
  {
    m_state.compute_image_texture = nullptr;
    m_dirty_bits |= DirtyState_ComputeImageTexture;
  }
}

void Gfx::SetViewport(float x, float y, float width, float height, float near_depth,
                      float far_depth)
{
  if (m_state.viewport.TopLeftX == x && m_state.viewport.TopLeftY == y &&
      m_state.viewport.Width == width && m_state.viewport.Height == height &&
      near_depth == m_state.viewport.MinDepth && far_depth == m_state.viewport.MaxDepth)
  {
    return;
  }

  m_state.viewport.TopLeftX = x;
  m_state.viewport.TopLeftY = y;
  m_state.viewport.Width = width;
  m_state.viewport.Height = height;
  m_state.viewport.MinDepth = near_depth;
  m_state.viewport.MaxDepth = far_depth;
  m_dirty_bits |= DirtyState_Viewport;
}

void Gfx::Draw(u32 base_vertex, u32 num_vertices)
{
  if (!ApplyState())
    return;

  g_dx_context->GetCommandList()->DrawInstanced(num_vertices, 1, base_vertex, 0);
}

void Gfx::DrawIndexed(u32 base_index, u32 num_indices, u32 base_vertex)
{
  if (!ApplyState())
    return;

  // DX12 is great and doesn't include the base vertex in SV_VertexID
  if (UsesDynamicVertexLoader(m_current_pipeline))
    g_dx_context->GetCommandList()->SetGraphicsRoot32BitConstant(
        ROOT_PARAMETER_BASE_VERTEX_CONSTANT, base_vertex, 0);
  g_dx_context->GetCommandList()->DrawIndexedInstanced(num_indices, 1, base_index, base_vertex, 0);
}

void Gfx::DispatchComputeShader(const AbstractShader* shader, u32 groupsize_x, u32 groupsize_y,
                                u32 groupsize_z, u32 groups_x, u32 groups_y, u32 groups_z)
{
  SetRootSignatures();
  SetDescriptorHeaps();
  UpdateDescriptorTables();

  if (m_dirty_bits & DirtyState_ComputeImageTexture && !UpdateComputeUAVDescriptorTable())
  {
    ExecuteCommandList(false);
    SetRootSignatures();
    SetDescriptorHeaps();
    UpdateDescriptorTables();
    UpdateComputeUAVDescriptorTable();
  }

  // Share graphics and compute state. No need to track now since dispatches are infrequent.
  auto* const cmdlist = g_dx_context->GetCommandList();
  cmdlist->SetPipelineState(static_cast<const DXShader*>(shader)->GetComputePipeline());
  cmdlist->SetComputeRootConstantBufferView(CS_ROOT_PARAMETER_CBV, m_state.constant_buffers[0]);
  cmdlist->SetComputeRootDescriptorTable(CS_ROOT_PARAMETER_SRV, m_state.srv_descriptor_base);
  cmdlist->SetComputeRootDescriptorTable(CS_ROOT_PARAMETER_SAMPLERS,
                                         m_state.sampler_descriptor_base);
  cmdlist->SetComputeRootDescriptorTable(CS_ROOT_PARAMETER_UAV,
                                         m_state.compute_uav_descriptor_base);
  cmdlist->Dispatch(groups_x, groups_y, groups_z);

  // Compute and graphics state share the same pipeline object? :(
  m_dirty_bits |= DirtyState_Pipeline;
}

void Gfx::BindBackbuffer(const ClearColor& clear_color)
{
  CheckForSwapChainChanges();
  SetAndClearFramebuffer(m_swap_chain->GetCurrentFramebuffer(), clear_color);
}

void Gfx::CheckForSwapChainChanges()
{
  const bool surface_changed = g_presenter->SurfaceChangedTestAndClear();
  const bool surface_resized =
      g_presenter->SurfaceResizedTestAndClear() || m_swap_chain->CheckForFullscreenChange();
  if (!surface_changed && !surface_resized)
    return;

  // The swap chain could be in use from a previous frame.
  WaitForGPUIdle();
  if (surface_changed)
  {
    m_swap_chain->ChangeSurface(g_presenter->GetNewSurfaceHandle());
  }
  else
  {
    m_swap_chain->ResizeSwapChain();
  }

  g_presenter->SetBackbuffer(m_swap_chain->GetWidth(), m_swap_chain->GetHeight());
}

void Gfx::PresentBackbuffer()
{
  m_current_framebuffer = nullptr;

  m_swap_chain->GetCurrentTexture()->TransitionToState(D3D12_RESOURCE_STATE_PRESENT);
  ExecuteCommandList(false);

  m_swap_chain->Present();
}

SurfaceInfo Gfx::GetSurfaceInfo() const
{
  return {m_swap_chain ? static_cast<u32>(m_swap_chain->GetWidth()) : 0,
          m_swap_chain ? static_cast<u32>(m_swap_chain->GetHeight()) : 0, m_backbuffer_scale,
          m_swap_chain ? m_swap_chain->GetFormat() : AbstractTextureFormat::Undefined};
}

void Gfx::OnConfigChanged(u32 bits)
{
  AbstractGfx::OnConfigChanged(bits);

  // For quad-buffered stereo we need to change the layer count, so recreate the swap chain.
  if (m_swap_chain && bits & CONFIG_CHANGE_BIT_STEREO_MODE)
  {
    ExecuteCommandList(true);
    m_swap_chain->SetStereo(SwapChain::WantsStereo());
  }

  if (m_swap_chain && bits & CONFIG_CHANGE_BIT_HDR)
  {
    ExecuteCommandList(true);
    m_swap_chain->SetHDR(SwapChain::WantsHDR());
  }

  // Wipe sampler cache if force texture filtering or anisotropy changes.
  if (bits & (CONFIG_CHANGE_BIT_ANISOTROPY | CONFIG_CHANGE_BIT_FORCE_TEXTURE_FILTERING))
  {
    ExecuteCommandList(true);
    g_dx_context->GetSamplerHeapManager().Clear();
    g_dx_context->ResetSamplerAllocators();
  }

  // If the host config changed (e.g. bbox/per-pixel-shading), recreate the root signature.
  if (bits & CONFIG_CHANGE_BIT_HOST_CONFIG)
    g_dx_context->RecreateGXRootSignature();
}

void Gfx::ExecuteCommandList(bool wait_for_completion)
{
  PerfQuery::GetInstance()->ResolveQueries();
  g_dx_context->ExecuteCommandList(wait_for_completion);
  m_dirty_bits = DirtyState_All;
}

void Gfx::SetConstantBuffer(u32 index, D3D12_GPU_VIRTUAL_ADDRESS address)
{
  if (m_state.constant_buffers[index] == address)
    return;

  m_state.constant_buffers[index] = address;
  m_dirty_bits |= DirtyState_PS_CBV << index;
}

void Gfx::SetTextureDescriptor(u32 index, D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
  if (m_state.textures[index].ptr == handle.ptr)
    return;

  m_state.textures[index].ptr = handle.ptr;
  m_dirty_bits |= DirtyState_Textures;
}

void Gfx::SetPixelShaderUAV(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
  if (m_state.ps_uav.ptr == handle.ptr)
    return;

  m_state.ps_uav = handle;
  m_dirty_bits |= DirtyState_PS_UAV;
}

void Gfx::SetVertexBuffer(D3D12_GPU_VIRTUAL_ADDRESS address, D3D12_CPU_DESCRIPTOR_HANDLE srv,
                          u32 stride, u32 size)
{
  if (m_state.vertex_buffer.BufferLocation != address ||
      m_state.vertex_buffer.StrideInBytes != stride || m_state.vertex_buffer.SizeInBytes != size)
  {
    m_state.vertex_buffer.BufferLocation = address;
    m_state.vertex_buffer.StrideInBytes = stride;
    m_state.vertex_buffer.SizeInBytes = size;
    m_dirty_bits |= DirtyState_VertexBuffer;
  }
  if (m_state.vs_srv.ptr != srv.ptr)
  {
    m_state.vs_srv = srv;
    m_dirty_bits |= DirtyState_VS_SRV;
  }
}

void Gfx::SetIndexBuffer(D3D12_GPU_VIRTUAL_ADDRESS address, u32 size, DXGI_FORMAT format)
{
  if (m_state.index_buffer.BufferLocation == address && m_state.index_buffer.SizeInBytes == size &&
      m_state.index_buffer.Format == format)
  {
    return;
  }

  m_state.index_buffer.BufferLocation = address;
  m_state.index_buffer.SizeInBytes = size;
  m_state.index_buffer.Format = format;
  m_dirty_bits |= DirtyState_IndexBuffer;
}

bool Gfx::ApplyState()
{
  if (!m_current_framebuffer || !m_current_pipeline)
    return false;

  // Updating the descriptor tables can cause command list execution if no descriptors remain.
  SetRootSignatures();
  SetDescriptorHeaps();
  UpdateDescriptorTables();

  // Clear bits before actually changing state. Some state (e.g. cbuffers) can't be set
  // if utility pipelines are bound.
  const u32 dirty_bits = m_dirty_bits;
  m_dirty_bits &=
      ~(DirtyState_Framebuffer | DirtyState_Pipeline | DirtyState_Viewport |
        DirtyState_ScissorRect | DirtyState_PS_UAV | DirtyState_PS_CBV | DirtyState_VS_CBV |
        DirtyState_GS_CBV | DirtyState_SRV_Descriptor | DirtyState_Sampler_Descriptor |
        DirtyState_UAV_Descriptor | DirtyState_VertexBuffer | DirtyState_IndexBuffer |
        DirtyState_PrimitiveTopology | DirtyState_VS_SRV_Descriptor | DirtyState_PS_CUS_CBV);

  auto* const cmdlist = g_dx_context->GetCommandList();
  auto* const pipeline = static_cast<const DXPipeline*>(m_current_pipeline);
  if (dirty_bits & DirtyState_Pipeline)
    cmdlist->SetPipelineState(pipeline->GetPipeline());

  if (dirty_bits & DirtyState_Framebuffer)
    BindFramebuffer(static_cast<DXFramebuffer*>(m_current_framebuffer));

  if (dirty_bits & DirtyState_Viewport)
    cmdlist->RSSetViewports(1, &m_state.viewport);

  if (dirty_bits & DirtyState_ScissorRect)
    cmdlist->RSSetScissorRects(1, &m_state.scissor);

  if (dirty_bits & DirtyState_VertexBuffer)
    cmdlist->IASetVertexBuffers(0, 1, &m_state.vertex_buffer);

  if (dirty_bits & DirtyState_IndexBuffer)
    cmdlist->IASetIndexBuffer(&m_state.index_buffer);

  if (dirty_bits & DirtyState_PrimitiveTopology)
    cmdlist->IASetPrimitiveTopology(m_state.primitive_topology);

  if (dirty_bits & DirtyState_SRV_Descriptor)
    cmdlist->SetGraphicsRootDescriptorTable(ROOT_PARAMETER_PS_SRV, m_state.srv_descriptor_base);

  if (dirty_bits & DirtyState_Sampler_Descriptor)
  {
    cmdlist->SetGraphicsRootDescriptorTable(ROOT_PARAMETER_PS_SAMPLERS,
                                            m_state.sampler_descriptor_base);
  }

  if (pipeline->GetUsage() != AbstractPipelineUsage::Utility)
  {
    if (dirty_bits & DirtyState_VS_CBV)
    {
      cmdlist->SetGraphicsRootConstantBufferView(ROOT_PARAMETER_VS_CBV,
                                                 m_state.constant_buffers[1]);
      cmdlist->SetGraphicsRootConstantBufferView(ROOT_PARAMETER_VS_CBV2,
                                                 m_state.constant_buffers[1]);

      if (g_ActiveConfig.bEnablePixelLighting)
      {
        cmdlist->SetGraphicsRootConstantBufferView(
            g_ActiveConfig.bBBoxEnable ? ROOT_PARAMETER_PS_CBV2 : ROOT_PARAMETER_PS_UAV_OR_CBV2,
            m_state.constant_buffers[1]);
      }
    }

    if (dirty_bits & DirtyState_PS_CUS_CBV)
    {
      cmdlist->SetGraphicsRootConstantBufferView(
          g_ActiveConfig.bBBoxEnable ? ROOT_PARAMETER_PS_CUS_CBV : ROOT_PARAMETER_PS_CBV2,
          m_state.constant_buffers[2]);
    }

    if (dirty_bits & DirtyState_VS_SRV_Descriptor && UsesDynamicVertexLoader(pipeline))
    {
      cmdlist->SetGraphicsRootDescriptorTable(ROOT_PARAMETER_VS_SRV,
                                              m_state.vertex_srv_descriptor_base);
    }

    if (dirty_bits & DirtyState_GS_CBV)
    {
      cmdlist->SetGraphicsRootConstantBufferView(ROOT_PARAMETER_GS_CBV,
                                                 m_state.constant_buffers[3]);
    }

    if (dirty_bits & DirtyState_UAV_Descriptor && g_ActiveConfig.bBBoxEnable)
    {
      cmdlist->SetGraphicsRootDescriptorTable(ROOT_PARAMETER_PS_UAV_OR_CBV2,
                                              m_state.uav_descriptor_base);
    }
  }

  if (dirty_bits & DirtyState_PS_CBV)
  {
    cmdlist->SetGraphicsRootConstantBufferView(ROOT_PARAMETER_PS_CBV, m_state.constant_buffers[0]);
  }

  return true;
}

void Gfx::SetRootSignatures()
{
  const u32 dirty_bits = m_dirty_bits;
  if (dirty_bits & DirtyState_RootSignature)
    g_dx_context->GetCommandList()->SetGraphicsRootSignature(m_state.root_signature);
  if (dirty_bits & DirtyState_ComputeRootSignature)
  {
    g_dx_context->GetCommandList()->SetComputeRootSignature(
        g_dx_context->GetComputeRootSignature());
  }
  m_dirty_bits &= ~(DirtyState_RootSignature | DirtyState_ComputeRootSignature);
}

void Gfx::SetDescriptorHeaps()
{
  if (m_dirty_bits & DirtyState_DescriptorHeaps)
  {
    g_dx_context->GetCommandList()->SetDescriptorHeaps(g_dx_context->GetGPUDescriptorHeapCount(),
                                                       g_dx_context->GetGPUDescriptorHeaps());
    m_dirty_bits &= ~DirtyState_DescriptorHeaps;
  }
}

void Gfx::UpdateDescriptorTables()
{
  // Samplers force a full sync because any of the samplers could be in use.
  const bool texture_update_failed =
      (m_dirty_bits & DirtyState_Textures) && !UpdateSRVDescriptorTable();
  const bool sampler_update_failed =
      (m_dirty_bits & DirtyState_Samplers) && !UpdateSamplerDescriptorTable();
  const bool uav_update_failed = (m_dirty_bits & DirtyState_PS_UAV) && !UpdateUAVDescriptorTable();
  const bool srv_update_failed =
      (m_dirty_bits & DirtyState_VS_SRV) && !UpdateVSSRVDescriptorTable();
  if (texture_update_failed || sampler_update_failed || uav_update_failed || srv_update_failed)
  {
    WARN_LOG_FMT(VIDEO, "Executing command list while waiting for temporary {}",
                 texture_update_failed ? "descriptors" : "samplers");
    ExecuteCommandList(false);
    SetRootSignatures();
    SetDescriptorHeaps();
    UpdateSRVDescriptorTable();
    UpdateSamplerDescriptorTable();
    UpdateUAVDescriptorTable();
    UpdateVSSRVDescriptorTable();
  }
}

bool Gfx::UpdateSRVDescriptorTable()
{
  static constexpr std::array<UINT, VideoCommon::MAX_PIXEL_SHADER_SAMPLERS> src_sizes = {
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  DescriptorHandle dst_base_handle;
  constexpr UINT dst_handle_sizes = VideoCommon::MAX_PIXEL_SHADER_SAMPLERS;
  if (!g_dx_context->GetDescriptorAllocator()->Allocate(VideoCommon::MAX_PIXEL_SHADER_SAMPLERS,
                                                        &dst_base_handle))
    return false;

  g_dx_context->GetDevice()->CopyDescriptors(
      1, &dst_base_handle.cpu_handle, &dst_handle_sizes, VideoCommon::MAX_PIXEL_SHADER_SAMPLERS,
      m_state.textures.data(), src_sizes.data(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  m_state.srv_descriptor_base = dst_base_handle.gpu_handle;
  m_dirty_bits = (m_dirty_bits & ~DirtyState_Textures) | DirtyState_SRV_Descriptor;
  return true;
}

bool Gfx::UpdateSamplerDescriptorTable()
{
  if (!g_dx_context->GetSamplerAllocator()->GetGroupHandle(m_state.samplers,
                                                           &m_state.sampler_descriptor_base))
  {
    g_dx_context->ResetSamplerAllocators();
    return false;
  }

  m_dirty_bits = (m_dirty_bits & ~DirtyState_Samplers) | DirtyState_Sampler_Descriptor;
  return true;
}

bool Gfx::UpdateUAVDescriptorTable()
{
  // We can skip writing the UAV descriptor if bbox isn't enabled, since it's not used otherwise.
  if (!g_ActiveConfig.bBBoxEnable)
    return true;

  DescriptorHandle handle;
  if (!g_dx_context->GetDescriptorAllocator()->Allocate(1, &handle))
    return false;

  g_dx_context->GetDevice()->CopyDescriptorsSimple(1, handle.cpu_handle, m_state.ps_uav,
                                                   D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  m_state.uav_descriptor_base = handle.gpu_handle;
  m_dirty_bits = (m_dirty_bits & ~DirtyState_PS_UAV) | DirtyState_UAV_Descriptor;
  return true;
}

bool Gfx::UpdateVSSRVDescriptorTable()
{
  if (!UsesDynamicVertexLoader(m_current_pipeline))
  {
    return true;
  }

  DescriptorHandle handle;
  if (!g_dx_context->GetDescriptorAllocator()->Allocate(1, &handle))
    return false;

  g_dx_context->GetDevice()->CopyDescriptorsSimple(1, handle.cpu_handle, m_state.vs_srv,
                                                   D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  m_state.vertex_srv_descriptor_base = handle.gpu_handle;
  m_dirty_bits = (m_dirty_bits & ~DirtyState_VS_SRV) | DirtyState_VS_SRV_Descriptor;
  return true;
}

bool Gfx::UpdateComputeUAVDescriptorTable()
{
  DescriptorHandle handle;
  if (!g_dx_context->GetDescriptorAllocator()->Allocate(1, &handle))
    return false;

  if (m_state.compute_image_texture)
  {
    g_dx_context->GetDevice()->CopyDescriptorsSimple(
        1, handle.cpu_handle, m_state.compute_image_texture->GetUAVDescriptor().cpu_handle,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  }
  else
  {
    constexpr D3D12_UNORDERED_ACCESS_VIEW_DESC null_uav_desc = {};
    g_dx_context->GetDevice()->CreateUnorderedAccessView(nullptr, nullptr, &null_uav_desc,
                                                         handle.cpu_handle);
  }

  m_dirty_bits &= ~DirtyState_ComputeImageTexture;
  m_state.compute_uav_descriptor_base = handle.gpu_handle;
  return true;
}

}  // namespace DX12
