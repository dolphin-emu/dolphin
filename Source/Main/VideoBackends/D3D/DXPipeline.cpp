// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>

#include "Common/Assert.h"
#include "Common/Logging/Log.h"

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/DXPipeline.h"
#include "VideoBackends/D3D/DXShader.h"
#include "VideoBackends/D3D/DXTexture.h"
#include "VideoBackends/D3D/Render.h"
#include "VideoBackends/D3D/VertexManager.h"

namespace DX11
{
DXPipeline::DXPipeline(ID3D11InputLayout* input_layout, ID3D11VertexShader* vertex_shader,
                       ID3D11GeometryShader* geometry_shader, ID3D11PixelShader* pixel_shader,
                       ID3D11RasterizerState* rasterizer_state,
                       ID3D11DepthStencilState* depth_state, ID3D11BlendState* blend_state,
                       D3D11_PRIMITIVE_TOPOLOGY primitive_topology)
    : m_input_layout(input_layout), m_vertex_shader(vertex_shader),
      m_geometry_shader(geometry_shader), m_pixel_shader(pixel_shader),
      m_rasterizer_state(rasterizer_state), m_depth_state(depth_state), m_blend_state(blend_state),
      m_primitive_topology(primitive_topology)
{
  if (m_input_layout)
    m_input_layout->AddRef();
  if (m_vertex_shader)
    m_vertex_shader->AddRef();
  if (m_geometry_shader)
    m_geometry_shader->AddRef();
  if (m_pixel_shader)
    m_pixel_shader->AddRef();
  if (m_rasterizer_state)
    m_rasterizer_state->AddRef();
  if (m_depth_state)
    m_depth_state->AddRef();
  if (m_blend_state)
    m_blend_state->AddRef();
}

DXPipeline::~DXPipeline()
{
  if (m_input_layout)
    m_input_layout->Release();
  if (m_vertex_shader)
    m_vertex_shader->Release();
  if (m_geometry_shader)
    m_geometry_shader->Release();
  if (m_pixel_shader)
    m_pixel_shader->Release();
  if (m_rasterizer_state)
    m_rasterizer_state->Release();
  if (m_depth_state)
    m_depth_state->Release();
  if (m_blend_state)
    m_blend_state->Release();
}

std::unique_ptr<DXPipeline> DXPipeline::Create(const AbstractPipelineConfig& config)
{
  StateCache& state_cache = static_cast<Renderer*>(g_renderer.get())->GetStateCache();
  ID3D11RasterizerState* rasterizer_state = state_cache.Get(config.rasterization_state);
  ID3D11DepthStencilState* depth_state = state_cache.Get(config.depth_state);
  ID3D11BlendState* blend_state = state_cache.Get(config.blending_state);
  D3D11_PRIMITIVE_TOPOLOGY primitive_topology =
      StateCache::GetPrimitiveTopology(config.rasterization_state.primitive);
  if (!rasterizer_state || !depth_state || !blend_state)
  {
    SAFE_RELEASE(rasterizer_state);
    SAFE_RELEASE(depth_state);
    SAFE_RELEASE(blend_state);
    return nullptr;
  }

  const DXShader* vertex_shader = static_cast<const DXShader*>(config.vertex_shader);
  const DXShader* geometry_shader = static_cast<const DXShader*>(config.geometry_shader);
  const DXShader* pixel_shader = static_cast<const DXShader*>(config.pixel_shader);
  ASSERT(vertex_shader != nullptr && pixel_shader != nullptr);

  ID3D11InputLayout* input_layout =
      const_cast<D3DVertexFormat*>(static_cast<const D3DVertexFormat*>(config.vertex_format))
          ->GetInputLayout(vertex_shader->GetByteCode());

  return std::make_unique<DXPipeline>(input_layout, vertex_shader->GetD3DVertexShader(),
                                      geometry_shader ? geometry_shader->GetD3DGeometryShader() :
                                                        nullptr,
                                      pixel_shader->GetD3DPixelShader(), rasterizer_state,
                                      depth_state, blend_state, primitive_topology);
}
}  // namespace DX11
