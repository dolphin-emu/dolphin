// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/D3D/D3DState.h"

#include <algorithm>
#include <array>
#include <bit>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/DXTexture.h"
#include "VideoBackends/D3DCommon/D3DCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{
namespace D3D
{
std::unique_ptr<StateManager> stateman;

StateManager::StateManager() = default;
StateManager::~StateManager() = default;

void StateManager::Apply()
{
  if (m_dirtyFlags.none())
    return;

  // Framebuffer changes must occur before texture changes, otherwise the D3D runtime messes with
  // our bindings and sets them to null to prevent hazards.
  if (m_dirtyFlags.test(DirtyFlag_Framebuffer))
  {
    if (g_ActiveConfig.backend_info.bSupportsBBox)
    {
      D3D::context->OMSetRenderTargetsAndUnorderedAccessViews(
          m_pending.framebuffer->GetNumRTVs(),
          m_pending.use_integer_rtv ? m_pending.framebuffer->GetIntegerRTVArray() :
                                      m_pending.framebuffer->GetRTVArray(),
          m_pending.framebuffer->GetDSV(), m_pending.framebuffer->GetNumRTVs() + 1, 1,
          &m_pending.uav, nullptr);
    }
    else
    {
      D3D::context->OMSetRenderTargets(m_pending.framebuffer->GetNumRTVs(),
                                       m_pending.use_integer_rtv ?
                                           m_pending.framebuffer->GetIntegerRTVArray() :
                                           m_pending.framebuffer->GetRTVArray(),
                                       m_pending.framebuffer->GetDSV());
    }
    m_current.framebuffer = m_pending.framebuffer;
    m_current.uav = m_pending.uav;
    m_current.use_integer_rtv = m_pending.use_integer_rtv;
  }

  const bool dirtyConstants = m_dirtyFlags.test(DirtyFlag_PixelConstants) ||
                              m_dirtyFlags.test(DirtyFlag_VertexConstants) ||
                              m_dirtyFlags.test(DirtyFlag_GeometryConstants);
  const bool dirtyShaders = m_dirtyFlags.test(DirtyFlag_PixelShader) ||
                            m_dirtyFlags.test(DirtyFlag_VertexShader) ||
                            m_dirtyFlags.test(DirtyFlag_GeometryShader);
  const bool dirtyBuffers =
      m_dirtyFlags.test(DirtyFlag_VertexBuffer) || m_dirtyFlags.test(DirtyFlag_IndexBuffer);

  if (dirtyConstants)
  {
    if (m_current.pixelConstants[0] != m_pending.pixelConstants[0] ||
        m_current.pixelConstants[1] != m_pending.pixelConstants[1] ||
        m_current.pixelConstants[2] != m_pending.pixelConstants[2])
    {
      u32 count = 1;
      if (m_pending.pixelConstants[1])
        count++;
      if (m_pending.pixelConstants[2])
        count++;
      D3D::context->PSSetConstantBuffers(0, count, m_pending.pixelConstants.data());
      m_current.pixelConstants[0] = m_pending.pixelConstants[0];
      m_current.pixelConstants[1] = m_pending.pixelConstants[1];
      m_current.pixelConstants[2] = m_pending.pixelConstants[2];
    }

    if (m_current.vertexConstants != m_pending.vertexConstants)
    {
      D3D::context->VSSetConstantBuffers(0, 1, &m_pending.vertexConstants);
      D3D::context->VSSetConstantBuffers(1, 1, &m_pending.vertexConstants);
      m_current.vertexConstants = m_pending.vertexConstants;
    }

    if (m_current.geometryConstants != m_pending.geometryConstants)
    {
      D3D::context->GSSetConstantBuffers(0, 1, &m_pending.geometryConstants);
      m_current.geometryConstants = m_pending.geometryConstants;
    }
  }

  if (dirtyBuffers || (m_dirtyFlags.test(DirtyFlag_InputAssembler)))
  {
    if (m_current.vertexBuffer != m_pending.vertexBuffer ||
        m_current.vertexBufferStride != m_pending.vertexBufferStride ||
        m_current.vertexBufferOffset != m_pending.vertexBufferOffset)
    {
      D3D::context->IASetVertexBuffers(0, 1, &m_pending.vertexBuffer, &m_pending.vertexBufferStride,
                                       &m_pending.vertexBufferOffset);
      m_current.vertexBuffer = m_pending.vertexBuffer;
      m_current.vertexBufferStride = m_pending.vertexBufferStride;
      m_current.vertexBufferOffset = m_pending.vertexBufferOffset;
    }

    if (m_current.indexBuffer != m_pending.indexBuffer)
    {
      D3D::context->IASetIndexBuffer(m_pending.indexBuffer, DXGI_FORMAT_R16_UINT, 0);
      m_current.indexBuffer = m_pending.indexBuffer;
    }

    if (m_current.topology != m_pending.topology)
    {
      D3D::context->IASetPrimitiveTopology(m_pending.topology);
      m_current.topology = m_pending.topology;
    }

    if (m_current.inputLayout != m_pending.inputLayout)
    {
      D3D::context->IASetInputLayout(m_pending.inputLayout);
      m_current.inputLayout = m_pending.inputLayout;
    }
  }

  if (dirtyShaders)
  {
    if (m_current.pixelShader != m_pending.pixelShader)
    {
      D3D::context->PSSetShader(m_pending.pixelShader, nullptr, 0);
      m_current.pixelShader = m_pending.pixelShader;
    }

    if (m_current.vertexShader != m_pending.vertexShader)
    {
      D3D::context->VSSetShader(m_pending.vertexShader, nullptr, 0);
      m_current.vertexShader = m_pending.vertexShader;
    }

    if (m_current.geometryShader != m_pending.geometryShader)
    {
      D3D::context->GSSetShader(m_pending.geometryShader, nullptr, 0);
      m_current.geometryShader = m_pending.geometryShader;
    }
  }

  if (m_dirtyFlags.test(DirtyFlag_BlendState))
  {
    D3D::context->OMSetBlendState(m_pending.blendState, nullptr, 0xFFFFFFFF);
    m_current.blendState = m_pending.blendState;
  }
  if (m_dirtyFlags.test(DirtyFlag_DepthState))
  {
    D3D::context->OMSetDepthStencilState(m_pending.depthState, 0);
    m_current.depthState = m_pending.depthState;
  }
  if (m_dirtyFlags.test(DirtyFlag_RasterizerState))
  {
    D3D::context->RSSetState(m_pending.rasterizerState);
    m_current.rasterizerState = m_pending.rasterizerState;
  }

  ApplyTextures();

  m_dirtyFlags = 0;
}

void StateManager::ApplyTextures()
{
  for (u32 i = 0; i < VideoCommon::MAX_PIXEL_SHADER_SAMPLERS; i++)
  {
    const u32 flag = i;
    if (m_dirtyFlags.test(flag))
    {
      if (m_current.textures[i] != m_pending.textures[i])
      {
        D3D::context->PSSetShaderResources(i, 1, &m_pending.textures[i]);
        m_current.textures[i] = m_pending.textures[i];
      }
      m_dirtyFlags.reset(flag);
    }
  }

  for (u32 i = 0; i < VideoCommon::MAX_PIXEL_SHADER_SAMPLERS; i++)
  {
    const u32 flag = i + VideoCommon::MAX_PIXEL_SHADER_SAMPLERS;
    if (m_dirtyFlags.test(flag))
    {
      if (m_current.samplers[i] != m_pending.samplers[i])
      {
        D3D::context->PSSetSamplers(i, 1, &m_pending.samplers[i]);
        m_current.samplers[i] = m_pending.samplers[i];
      }
      m_dirtyFlags.reset(flag);
    }
  }
}

u32 StateManager::UnsetTexture(ID3D11ShaderResourceView* srv)
{
  u32 mask = 0;

  for (u32 index = 0; index < VideoCommon::MAX_PIXEL_SHADER_SAMPLERS; ++index)
  {
    if (m_current.textures[index] == srv)
    {
      SetTexture(index, nullptr);
      mask |= 1 << index;
    }
  }

  return mask;
}

void StateManager::SetTextureByMask(u32 textureSlotMask, ID3D11ShaderResourceView* srv)
{
  while (textureSlotMask)
  {
    const int index = std::countr_zero(textureSlotMask);
    SetTexture(index, srv);
    textureSlotMask &= ~(1 << index);
  }
}

void StateManager::SetComputeUAV(u32 index, ID3D11UnorderedAccessView* uav)
{
  if (m_compute_images[index] == uav)
    return;

  m_compute_images[index] = uav;
  D3D::context->CSSetUnorderedAccessViews(0, static_cast<u32>(m_compute_images.size()),
                                          m_compute_images.data(), nullptr);
}

void StateManager::SetComputeShader(ID3D11ComputeShader* shader)
{
  if (m_compute_shader == shader)
    return;

  m_compute_shader = shader;
  D3D::context->CSSetShader(shader, nullptr, 0);
}

void StateManager::SyncComputeBindings()
{
  if (m_compute_constants != m_pending.pixelConstants[0])
  {
    m_compute_constants = m_pending.pixelConstants[0];
    D3D::context->CSSetConstantBuffers(0, 1, &m_compute_constants);
  }

  for (u32 start = 0; start < static_cast<u32>(m_compute_textures.size());)
  {
    if (m_compute_textures[start] == m_pending.textures[start])
    {
      start++;
      continue;
    }

    m_compute_textures[start] = m_pending.textures[start];

    u32 end = start + 1;
    for (; end < static_cast<u32>(m_compute_textures.size()); end++)
    {
      if (m_compute_textures[end] == m_pending.textures[end])
        break;

      m_compute_textures[end] = m_pending.textures[end];
    }

    D3D::context->CSSetShaderResources(start, end - start, &m_compute_textures[start]);
    start = end;
  }

  for (u32 start = 0; start < static_cast<u32>(m_compute_samplers.size());)
  {
    if (m_compute_samplers[start] == m_pending.samplers[start])
    {
      start++;
      continue;
    }

    m_compute_samplers[start] = m_pending.samplers[start];

    u32 end = start + 1;
    for (; end < static_cast<u32>(m_compute_samplers.size()); end++)
    {
      if (m_compute_samplers[end] == m_pending.samplers[end])
        break;

      m_compute_samplers[end] = m_pending.samplers[end];
    }

    D3D::context->CSSetSamplers(start, end - start, &m_compute_samplers[start]);
    start = end;
  }
}
}  // namespace D3D

StateCache::~StateCache() = default;

ID3D11SamplerState* StateCache::Get(SamplerState state)
{
  std::lock_guard<std::mutex> guard(m_lock);
  auto it = m_sampler.find(state);
  if (it != m_sampler.end())
    return it->second.Get();

  D3D11_SAMPLER_DESC sampdc = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
  if (state.tm0.mipmap_filter == FilterMode::Linear)
  {
    if (state.tm0.min_filter == FilterMode::Linear)
      sampdc.Filter = (state.tm0.mag_filter == FilterMode::Linear) ?
                          D3D11_FILTER_MIN_MAG_MIP_LINEAR :
                          D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
    else
      sampdc.Filter = (state.tm0.mag_filter == FilterMode::Linear) ?
                          D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR :
                          D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
  }
  else
  {
    if (state.tm0.min_filter == FilterMode::Linear)
      sampdc.Filter = (state.tm0.mag_filter == FilterMode::Linear) ?
                          D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT :
                          D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
    else
      sampdc.Filter = (state.tm0.mag_filter == FilterMode::Linear) ?
                          D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT :
                          D3D11_FILTER_MIN_MAG_MIP_POINT;
  }

  static constexpr std::array<D3D11_TEXTURE_ADDRESS_MODE, 3> address_modes = {
      {D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_MIRROR}};
  sampdc.AddressU = address_modes[static_cast<u32>(state.tm0.wrap_u.Value())];
  sampdc.AddressV = address_modes[static_cast<u32>(state.tm0.wrap_v.Value())];
  sampdc.MaxLOD = state.tm1.max_lod / 16.f;
  sampdc.MinLOD = state.tm1.min_lod / 16.f;
  sampdc.MipLODBias = state.tm0.lod_bias / 256.f;

  if (state.tm0.anisotropic_filtering)
  {
    sampdc.Filter = D3D11_FILTER_ANISOTROPIC;
    sampdc.MaxAnisotropy = 1u << g_ActiveConfig.iMaxAnisotropy;
  }

  ComPtr<ID3D11SamplerState> res;
  HRESULT hr = D3D::device->CreateSamplerState(&sampdc, res.GetAddressOf());
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Creating D3D sampler state failed: {}", DX11HRWrap(hr));
  return m_sampler.emplace(state, std::move(res)).first->second.Get();
}

ID3D11BlendState* StateCache::Get(BlendingState state)
{
  std::lock_guard<std::mutex> guard(m_lock);
  auto it = m_blend.find(state.hex);
  if (it != m_blend.end())
    return it->second.Get();

  if (state.logicopenable && g_ActiveConfig.backend_info.bSupportsLogicOp)
  {
    D3D11_BLEND_DESC1 desc = {};
    D3D11_RENDER_TARGET_BLEND_DESC1& tdesc = desc.RenderTarget[0];
    if (state.colorupdate)
      tdesc.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN |
                                    D3D11_COLOR_WRITE_ENABLE_BLUE;
    else
      tdesc.RenderTargetWriteMask = 0;
    if (state.alphaupdate)
      tdesc.RenderTargetWriteMask |= D3D11_COLOR_WRITE_ENABLE_ALPHA;

    static constexpr std::array<D3D11_LOGIC_OP, 16> logic_ops = {
        {D3D11_LOGIC_OP_CLEAR, D3D11_LOGIC_OP_AND, D3D11_LOGIC_OP_AND_REVERSE, D3D11_LOGIC_OP_COPY,
         D3D11_LOGIC_OP_AND_INVERTED, D3D11_LOGIC_OP_NOOP, D3D11_LOGIC_OP_XOR, D3D11_LOGIC_OP_OR,
         D3D11_LOGIC_OP_NOR, D3D11_LOGIC_OP_EQUIV, D3D11_LOGIC_OP_INVERT, D3D11_LOGIC_OP_OR_REVERSE,
         D3D11_LOGIC_OP_COPY_INVERTED, D3D11_LOGIC_OP_OR_INVERTED, D3D11_LOGIC_OP_NAND,
         D3D11_LOGIC_OP_SET}};
    tdesc.LogicOpEnable = TRUE;
    tdesc.LogicOp = logic_ops[u32(state.logicmode.Value())];

    ComPtr<ID3D11BlendState1> res;
    HRESULT hr = D3D::device1->CreateBlendState1(&desc, res.GetAddressOf());
    if (SUCCEEDED(hr))
    {
      return m_blend.emplace(state.hex, std::move(res)).first->second.Get();
    }
    WARN_LOG_FMT(VIDEO, "Creating D3D blend state failed with an error: {}", DX11HRWrap(hr));
  }

  D3D11_BLEND_DESC desc = {};
  desc.AlphaToCoverageEnable = FALSE;
  desc.IndependentBlendEnable = FALSE;

  D3D11_RENDER_TARGET_BLEND_DESC& tdesc = desc.RenderTarget[0];
  tdesc.BlendEnable = state.blendenable;

  if (state.colorupdate)
    tdesc.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN |
                                  D3D11_COLOR_WRITE_ENABLE_BLUE;
  else
    tdesc.RenderTargetWriteMask = 0;
  if (state.alphaupdate)
    tdesc.RenderTargetWriteMask |= D3D11_COLOR_WRITE_ENABLE_ALPHA;

  const bool use_dual_source = state.usedualsrc;
  const std::array<D3D11_BLEND, 8> src_factors = {
      {D3D11_BLEND_ZERO, D3D11_BLEND_ONE, D3D11_BLEND_DEST_COLOR, D3D11_BLEND_INV_DEST_COLOR,
       use_dual_source ? D3D11_BLEND_SRC1_ALPHA : D3D11_BLEND_SRC_ALPHA,
       use_dual_source ? D3D11_BLEND_INV_SRC1_ALPHA : D3D11_BLEND_INV_SRC_ALPHA,
       D3D11_BLEND_DEST_ALPHA, D3D11_BLEND_INV_DEST_ALPHA}};
  const std::array<D3D11_BLEND, 8> dst_factors = {
      {D3D11_BLEND_ZERO, D3D11_BLEND_ONE, D3D11_BLEND_SRC_COLOR, D3D11_BLEND_INV_SRC_COLOR,
       use_dual_source ? D3D11_BLEND_SRC1_ALPHA : D3D11_BLEND_SRC_ALPHA,
       use_dual_source ? D3D11_BLEND_INV_SRC1_ALPHA : D3D11_BLEND_INV_SRC_ALPHA,
       D3D11_BLEND_DEST_ALPHA, D3D11_BLEND_INV_DEST_ALPHA}};

  tdesc.SrcBlend = src_factors[u32(state.srcfactor.Value())];
  tdesc.SrcBlendAlpha = src_factors[u32(state.srcfactoralpha.Value())];
  tdesc.DestBlend = dst_factors[u32(state.dstfactor.Value())];
  tdesc.DestBlendAlpha = dst_factors[u32(state.dstfactoralpha.Value())];
  tdesc.BlendOp = state.subtract ? D3D11_BLEND_OP_REV_SUBTRACT : D3D11_BLEND_OP_ADD;
  tdesc.BlendOpAlpha = state.subtractAlpha ? D3D11_BLEND_OP_REV_SUBTRACT : D3D11_BLEND_OP_ADD;

  ComPtr<ID3D11BlendState> res;
  HRESULT hr = D3D::device->CreateBlendState(&desc, res.GetAddressOf());
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Creating D3D blend state failed: {}", DX11HRWrap(hr));
  return m_blend.emplace(state.hex, std::move(res)).first->second.Get();
}

ID3D11RasterizerState* StateCache::Get(RasterizationState state)
{
  std::lock_guard<std::mutex> guard(m_lock);
  auto it = m_raster.find(state.hex);
  if (it != m_raster.end())
    return it->second.Get();

  static constexpr std::array<D3D11_CULL_MODE, 4> cull_modes = {
      {D3D11_CULL_NONE, D3D11_CULL_BACK, D3D11_CULL_FRONT, D3D11_CULL_BACK}};

  D3D11_RASTERIZER_DESC desc = {};
  desc.FillMode = D3D11_FILL_SOLID;
  desc.CullMode = cull_modes[u32(state.cullmode.Value())];
  desc.ScissorEnable = TRUE;

  ComPtr<ID3D11RasterizerState> res;
  HRESULT hr = D3D::device->CreateRasterizerState(&desc, res.GetAddressOf());
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Creating D3D rasterizer state failed: {}", DX11HRWrap(hr));
  return m_raster.emplace(state.hex, std::move(res)).first->second.Get();
}

ID3D11DepthStencilState* StateCache::Get(DepthState state)
{
  std::lock_guard<std::mutex> guard(m_lock);
  auto it = m_depth.find(state.hex);
  if (it != m_depth.end())
    return it->second.Get();

  D3D11_DEPTH_STENCIL_DESC depthdc = CD3D11_DEPTH_STENCIL_DESC(CD3D11_DEFAULT());

  depthdc.DepthEnable = TRUE;
  depthdc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
  depthdc.DepthFunc = D3D11_COMPARISON_GREATER;
  depthdc.StencilEnable = FALSE;
  depthdc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
  depthdc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;

  // Less/greater are swapped due to inverted depth.
  constexpr D3D11_COMPARISON_FUNC d3dCmpFuncs[8] = {
      D3D11_COMPARISON_NEVER,         D3D11_COMPARISON_GREATER, D3D11_COMPARISON_EQUAL,
      D3D11_COMPARISON_GREATER_EQUAL, D3D11_COMPARISON_LESS,    D3D11_COMPARISON_NOT_EQUAL,
      D3D11_COMPARISON_LESS_EQUAL,    D3D11_COMPARISON_ALWAYS};

  if (state.testenable)
  {
    depthdc.DepthEnable = TRUE;
    depthdc.DepthWriteMask =
        state.updateenable ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    depthdc.DepthFunc = d3dCmpFuncs[u32(state.func.Value())];
  }
  else
  {
    // if the test is disabled write is disabled too
    depthdc.DepthEnable = FALSE;
    depthdc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
  }

  ComPtr<ID3D11DepthStencilState> res;
  HRESULT hr = D3D::device->CreateDepthStencilState(&depthdc, res.GetAddressOf());
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Creating D3D depth stencil state failed: {}", DX11HRWrap(hr));
  return m_depth.emplace(state.hex, std::move(res)).first->second.Get();
}

D3D11_PRIMITIVE_TOPOLOGY StateCache::GetPrimitiveTopology(PrimitiveType primitive)
{
  static constexpr std::array<D3D11_PRIMITIVE_TOPOLOGY, 4> primitives = {
      {D3D11_PRIMITIVE_TOPOLOGY_POINTLIST, D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
       D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP}};
  return primitives[static_cast<u32>(primitive)];
}

}  // namespace DX11
