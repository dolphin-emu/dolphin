// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/D3D/DXPipeline.h"

#include "Common/Assert.h"
#include "Common/Logging/Log.h"

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DGfx.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/D3DVertexManager.h"
#include "VideoBackends/D3D/DXShader.h"
#include "VideoBackends/D3D/DXTexture.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{
DXPipeline::DXPipeline(const AbstractPipelineConfig& config, ID3D11InputLayout* input_layout,
                       ID3D11VertexShader* vertex_shader, ID3D11GeometryShader* geometry_shader,
                       ID3D11PixelShader* pixel_shader, ID3D11RasterizerState* rasterizer_state,
                       ID3D11DepthStencilState* depth_state, ID3D11BlendState* blend_state,
                       D3D11_PRIMITIVE_TOPOLOGY primitive_topology, bool use_logic_op)
    : AbstractPipeline(config), m_input_layout(input_layout), m_vertex_shader(vertex_shader),
      m_geometry_shader(geometry_shader), m_pixel_shader(pixel_shader),
      m_rasterizer_state(rasterizer_state), m_depth_state(depth_state), m_blend_state(blend_state),
      m_primitive_topology(primitive_topology), m_use_logic_op(use_logic_op)
{
}

DXPipeline::~DXPipeline() = default;

std::unique_ptr<DXPipeline> DXPipeline::Create(const AbstractPipelineConfig& config)
{
  StateCache& state_cache = static_cast<Gfx*>(g_gfx.get())->GetStateCache();
  ID3D11RasterizerState* rasterizer_state = state_cache.Get(config.rasterization_state);
  ID3D11DepthStencilState* depth_state = state_cache.Get(config.depth_state);
  ID3D11BlendState* blend_state = state_cache.Get(config.blending_state);
  D3D11_PRIMITIVE_TOPOLOGY primitive_topology =
      StateCache::GetPrimitiveTopology(config.rasterization_state.primitive);
  if (!rasterizer_state || !depth_state || !blend_state)
    return nullptr;

  const DXShader* vertex_shader = static_cast<const DXShader*>(config.vertex_shader);
  const DXShader* geometry_shader = static_cast<const DXShader*>(config.geometry_shader);
  const DXShader* pixel_shader = static_cast<const DXShader*>(config.pixel_shader);
  ASSERT(vertex_shader != nullptr && pixel_shader != nullptr);

  ID3D11InputLayout* input_layout =
      config.vertex_format ?
          const_cast<D3DVertexFormat*>(static_cast<const D3DVertexFormat*>(config.vertex_format))
              ->GetInputLayout(vertex_shader->GetByteCode().data(),
                               vertex_shader->GetByteCode().size()) :
          nullptr;

  // Only use the integer RTV if logic op is supported, and enabled.
  const bool use_logic_op =
      config.blending_state.logicopenable && g_ActiveConfig.backend_info.bSupportsLogicOp;

  return std::make_unique<DXPipeline>(config, input_layout, vertex_shader->GetD3DVertexShader(),
                                      geometry_shader ? geometry_shader->GetD3DGeometryShader() :
                                                        nullptr,
                                      pixel_shader->GetD3DPixelShader(), rasterizer_state,
                                      depth_state, blend_state, primitive_topology, use_logic_op);
}
}  // namespace DX11
