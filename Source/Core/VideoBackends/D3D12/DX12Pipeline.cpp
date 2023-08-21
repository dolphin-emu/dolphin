// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/D3D12/DX12Pipeline.h"

#include "Common/Assert.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/D3D12/Common.h"
#include "VideoBackends/D3D12/DX12Context.h"
#include "VideoBackends/D3D12/DX12Shader.h"
#include "VideoBackends/D3D12/DX12Texture.h"
#include "VideoBackends/D3D12/DX12VertexFormat.h"

namespace DX12
{
DXPipeline::DXPipeline(const AbstractPipelineConfig& config, ID3D12PipelineState* pipeline,
                       ID3D12RootSignature* root_signature, AbstractPipelineUsage usage,
                       D3D12_PRIMITIVE_TOPOLOGY primitive_topology, bool use_integer_rtv)
    : AbstractPipeline(config), m_pipeline(pipeline), m_root_signature(root_signature),
      m_usage(usage), m_primitive_topology(primitive_topology), m_use_integer_rtv(use_integer_rtv)
{
}

DXPipeline::~DXPipeline()
{
  m_pipeline->Release();
}

static D3D12_PRIMITIVE_TOPOLOGY GetD3DTopology(const RasterizationState& state)
{
  switch (state.primitive)
  {
  case PrimitiveType::Points:
    return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
  case PrimitiveType::Lines:
    return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
  case PrimitiveType::Triangles:
    return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
  case PrimitiveType::TriangleStrip:
  default:
    return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
  }
}

static D3D12_PRIMITIVE_TOPOLOGY_TYPE GetD3DTopologyType(const RasterizationState& state)
{
  switch (state.primitive)
  {
  case PrimitiveType::Points:
    return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
  case PrimitiveType::Lines:
    return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
  case PrimitiveType::Triangles:
  case PrimitiveType::TriangleStrip:
  default:
    return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  }
}

static void GetD3DRasterizerDesc(D3D12_RASTERIZER_DESC* desc, const RasterizationState& rs_state,
                                 const FramebufferState& fb_state)
{
  // No CULL_ALL here.
  static constexpr std::array<D3D12_CULL_MODE, 4> cull_modes = {
      {D3D12_CULL_MODE_NONE, D3D12_CULL_MODE_BACK, D3D12_CULL_MODE_FRONT, D3D12_CULL_MODE_FRONT}};

  desc->FillMode = D3D12_FILL_MODE_SOLID;
  desc->CullMode = cull_modes[u32(rs_state.cullmode.Value())];
  desc->MultisampleEnable = fb_state.samples > 1;
}

static void GetD3DDepthDesc(D3D12_DEPTH_STENCIL_DESC* desc, const DepthState& state)
{
  // Less/greater are swapped due to inverted depth.
  static constexpr std::array<D3D12_COMPARISON_FUNC, 8> compare_funcs = {
      {D3D12_COMPARISON_FUNC_NEVER, D3D12_COMPARISON_FUNC_GREATER, D3D12_COMPARISON_FUNC_EQUAL,
       D3D12_COMPARISON_FUNC_GREATER_EQUAL, D3D12_COMPARISON_FUNC_LESS,
       D3D12_COMPARISON_FUNC_NOT_EQUAL, D3D12_COMPARISON_FUNC_LESS_EQUAL,
       D3D12_COMPARISON_FUNC_ALWAYS}};

  desc->DepthEnable = state.testenable;
  desc->DepthFunc = compare_funcs[u32(state.func.Value())];
  desc->DepthWriteMask =
      state.updateenable ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
}

static void GetD3DBlendDesc(D3D12_BLEND_DESC* desc, const BlendingState& state)
{
  static constexpr std::array<D3D12_BLEND, 8> src_dual_src_factors = {
      {D3D12_BLEND_ZERO, D3D12_BLEND_ONE, D3D12_BLEND_DEST_COLOR, D3D12_BLEND_INV_DEST_COLOR,
       D3D12_BLEND_SRC1_ALPHA, D3D12_BLEND_INV_SRC1_ALPHA, D3D12_BLEND_DEST_ALPHA,
       D3D12_BLEND_INV_DEST_ALPHA}};
  static constexpr std::array<D3D12_BLEND, 8> dst_dual_src_factors = {
      {D3D12_BLEND_ZERO, D3D12_BLEND_ONE, D3D12_BLEND_SRC_COLOR, D3D12_BLEND_INV_SRC_COLOR,
       D3D12_BLEND_SRC1_ALPHA, D3D12_BLEND_INV_SRC1_ALPHA, D3D12_BLEND_DEST_ALPHA,
       D3D12_BLEND_INV_DEST_ALPHA}};
  static constexpr std::array<D3D12_BLEND, 8> src_factors = {
      {D3D12_BLEND_ZERO, D3D12_BLEND_ONE, D3D12_BLEND_DEST_COLOR, D3D12_BLEND_INV_DEST_COLOR,
       D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_DEST_ALPHA,
       D3D12_BLEND_INV_DEST_ALPHA}};

  static constexpr std::array<D3D12_BLEND, 8> dst_factors = {
      {D3D12_BLEND_ZERO, D3D12_BLEND_ONE, D3D12_BLEND_SRC_COLOR, D3D12_BLEND_INV_SRC_COLOR,
       D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_DEST_ALPHA,
       D3D12_BLEND_INV_DEST_ALPHA}};

  static constexpr std::array<D3D12_LOGIC_OP, 16> logic_ops = {
      {D3D12_LOGIC_OP_CLEAR, D3D12_LOGIC_OP_AND, D3D12_LOGIC_OP_AND_REVERSE, D3D12_LOGIC_OP_COPY,
       D3D12_LOGIC_OP_AND_INVERTED, D3D12_LOGIC_OP_NOOP, D3D12_LOGIC_OP_XOR, D3D12_LOGIC_OP_OR,
       D3D12_LOGIC_OP_NOR, D3D12_LOGIC_OP_EQUIV, D3D12_LOGIC_OP_INVERT, D3D12_LOGIC_OP_OR_REVERSE,
       D3D12_LOGIC_OP_COPY_INVERTED, D3D12_LOGIC_OP_OR_INVERTED, D3D12_LOGIC_OP_NAND,
       D3D12_LOGIC_OP_SET}};

  desc->AlphaToCoverageEnable = FALSE;
  desc->IndependentBlendEnable = FALSE;

  D3D12_RENDER_TARGET_BLEND_DESC* rtblend = &desc->RenderTarget[0];
  if (state.colorupdate)
  {
    rtblend->RenderTargetWriteMask |= D3D12_COLOR_WRITE_ENABLE_RED |
                                      D3D12_COLOR_WRITE_ENABLE_GREEN |
                                      D3D12_COLOR_WRITE_ENABLE_BLUE;
  }
  if (state.alphaupdate)
  {
    rtblend->RenderTargetWriteMask |= D3D12_COLOR_WRITE_ENABLE_ALPHA;
  }

  // blend takes precedence over logic op
  rtblend->BlendEnable = state.blendenable;
  if (state.blendenable)
  {
    rtblend->BlendOp = state.subtract ? D3D12_BLEND_OP_REV_SUBTRACT : D3D12_BLEND_OP_ADD;
    rtblend->BlendOpAlpha = state.subtractAlpha ? D3D12_BLEND_OP_REV_SUBTRACT : D3D12_BLEND_OP_ADD;
    if (state.usedualsrc)
    {
      rtblend->SrcBlend = src_dual_src_factors[u32(state.srcfactor.Value())];
      rtblend->SrcBlendAlpha = src_dual_src_factors[u32(state.srcfactoralpha.Value())];
      rtblend->DestBlend = dst_dual_src_factors[u32(state.dstfactor.Value())];
      rtblend->DestBlendAlpha = dst_dual_src_factors[u32(state.dstfactoralpha.Value())];
    }
    else
    {
      rtblend->SrcBlend = src_factors[u32(state.srcfactor.Value())];
      rtblend->SrcBlendAlpha = src_factors[u32(state.srcfactoralpha.Value())];
      rtblend->DestBlend = dst_factors[u32(state.dstfactor.Value())];
      rtblend->DestBlendAlpha = dst_factors[u32(state.dstfactoralpha.Value())];
    }
  }
  else
  {
    rtblend->LogicOpEnable = state.logicopenable;
    if (state.logicopenable)
      rtblend->LogicOp = logic_ops[u32(state.logicmode.Value())];
  }
}

std::unique_ptr<DXPipeline> DXPipeline::Create(const AbstractPipelineConfig& config,
                                               const void* cache_data, size_t cache_data_size)
{
  DEBUG_ASSERT(config.vertex_shader && config.pixel_shader);

  D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
  switch (config.usage)
  {
  case AbstractPipelineUsage::GX:
  case AbstractPipelineUsage::GXUber:
    desc.pRootSignature = g_dx_context->GetGXRootSignature();
    break;
  case AbstractPipelineUsage::Utility:
    desc.pRootSignature = g_dx_context->GetUtilityRootSignature();
    break;
  default:
    PanicAlertFmt("Unknown pipeline layout.");
    return nullptr;
  }

  if (config.vertex_shader)
    desc.VS = static_cast<const DXShader*>(config.vertex_shader)->GetD3DByteCode();
  if (config.geometry_shader)
    desc.GS = static_cast<const DXShader*>(config.geometry_shader)->GetD3DByteCode();
  if (config.pixel_shader)
    desc.PS = static_cast<const DXShader*>(config.pixel_shader)->GetD3DByteCode();

  GetD3DBlendDesc(&desc.BlendState, config.blending_state);
  desc.SampleMask = 0xFFFFFFFF;
  GetD3DRasterizerDesc(&desc.RasterizerState, config.rasterization_state, config.framebuffer_state);
  GetD3DDepthDesc(&desc.DepthStencilState, config.depth_state);
  if (config.vertex_format)
    static_cast<const DXVertexFormat*>(config.vertex_format)->GetInputLayoutDesc(&desc.InputLayout);
  desc.IBStripCutValue = config.rasterization_state.primitive == PrimitiveType::TriangleStrip ?
                             D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF :
                             D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
  desc.PrimitiveTopologyType = GetD3DTopologyType(config.rasterization_state);
  if (config.framebuffer_state.color_texture_format != AbstractTextureFormat::Undefined)
  {
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = D3DCommon::GetRTVFormatForAbstractFormat(
        config.framebuffer_state.color_texture_format, config.blending_state.logicopenable);
  }
  if (config.framebuffer_state.depth_texture_format != AbstractTextureFormat::Undefined)
    desc.DSVFormat =
        D3DCommon::GetDSVFormatForAbstractFormat(config.framebuffer_state.depth_texture_format);
  desc.SampleDesc.Count = config.framebuffer_state.samples;
  desc.NodeMask = 1;
  desc.CachedPSO.pCachedBlob = cache_data;
  desc.CachedPSO.CachedBlobSizeInBytes = cache_data_size;

  ID3D12PipelineState* pso;
  HRESULT hr = g_dx_context->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso));
  if (FAILED(hr))
  {
    WARN_LOG_FMT(VIDEO, "CreateGraphicsPipelineState() {}failed: {}",
                 cache_data ? "with cache data " : "", DX12HRWrap(hr));
    return nullptr;
  }

  const bool use_integer_rtv =
      !config.blending_state.blendenable && config.blending_state.logicopenable;
  return std::make_unique<DXPipeline>(config, pso, desc.pRootSignature, config.usage,
                                      GetD3DTopology(config.rasterization_state), use_integer_rtv);
}

AbstractPipeline::CacheData DXPipeline::GetCacheData() const
{
  ComPtr<ID3DBlob> blob;
  HRESULT hr = m_pipeline->GetCachedBlob(&blob);
  if (FAILED(hr))
  {
    WARN_LOG_FMT(VIDEO, "ID3D12Pipeline::GetCachedBlob() failed: {}", DX12HRWrap(hr));
    return {};
  }

  CacheData data(blob->GetBufferSize());
  std::memcpy(data.data(), blob->GetBufferPointer(), blob->GetBufferSize());
  return data;
}
}  // namespace DX12
