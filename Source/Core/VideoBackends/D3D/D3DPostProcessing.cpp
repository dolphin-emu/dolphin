// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Align.h"
#include "VideoBackends/D3D/D3DPostProcessing.h"
#include "VideoBackends/D3D/D3DShader.h"
#include "VideoBackends/D3D/D3DUtil.h"
#include "VideoBackends/D3D/PixelShaderCache.h"
#include "VideoBackends/D3D/VertexShaderCache.h"

namespace DX11
{
// Dummy SV_Position variable is needed because GetSimpleVertexShader outputs TEXCOORD in v1
static const char s_default_shader[] = R"(
float4 main(float4 _ : SV_Position, float2 coord : TEXCOORD) : SV_Target
{
  return g_FrameBuffer.Sample(g_DefaultSampler, coord);
}
)";

static const char s_hlsl_prefix[] = R"(
Texture2D g_FrameBuffer;
SamplerState g_DefaultSampler;
cbuffer Globals : register(b0)
{
  uint2 g_Resolution;
  float2 g_ResolutionF;
  float2 g_InvResolutionF;
  uint g_Time;
}
)";

#pragma pack(push, 4)
struct CBGlobal
{
  u32 resX, resY;
  float resFX, resFY;
  float invResX, invResY;
  u32 time;
};
#pragma pack(pop)

D3DPostProcessing::D3DPostProcessing()
{
  D3D11_BUFFER_DESC bdesc;
  bdesc.ByteWidth = static_cast<UINT>(Common::AlignUp(sizeof(CBGlobal), 16));
  bdesc.Usage = D3D11_USAGE_DYNAMIC;
  bdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  bdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  bdesc.MiscFlags = 0;
  bdesc.StructureByteStride = 0;
  HRESULT hr;
  hr = D3D::device->CreateBuffer(&bdesc, nullptr, &m_globals_cb);
  CHECK(SUCCEEDED(hr), "Create D3D Post Processing CB.");
  D3D::SetDebugObjectName(m_globals_cb, "constant buffer of D3D Post Processing");
}

D3DPostProcessing::~D3DPostProcessing()
{
  if (m_current_ps)
    m_current_ps->Release();
  m_globals_cb->Release();
}

void D3DPostProcessing::PostProcessTexture(ID3D11ShaderResourceView* srv, RECT* rect, u32 src_width,
                                           u32 src_height, D3D11_VIEWPORT viewport, u32 slice)
{
  UpdateConfig();

  D3D11_MAPPED_SUBRESOURCE msr;
  HRESULT hr;
  hr = D3D::context->Map(m_globals_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
  CHECK(SUCCEEDED(hr), "Map D3D Post Processing CB.");
  CBGlobal* pCb = reinterpret_cast<CBGlobal*>(msr.pData);
  pCb->resX = src_width;
  pCb->resY = src_height;
  pCb->resFX = src_width;
  pCb->resFY = src_height;
  pCb->invResX = 1.0f / src_width;
  pCb->invResY = 1.0f / src_height;
  pCb->time = static_cast<u32>(m_timer.GetTimeElapsed());
  D3D::context->Unmap(m_globals_cb, 0);

  D3D::context->PSSetConstantBuffers(0, 1, &m_globals_cb);

  D3D::drawShadedTexQuad(srv, rect, src_width, src_height, m_current_ps,
                         VertexShaderCache::GetSimpleVertexShader(),
                         VertexShaderCache::GetSimpleInputLayout(), nullptr, slice);
}

bool D3DPostProcessing::IsActive()
{
  UpdateConfig();
  return !m_ps_is_default;
}

ID3D11PixelShader* D3DPostProcessing::CompileShader(const std::string& source)
{
  return D3D::CompileAndCreatePixelShader(s_hlsl_prefix + source);
}

void D3DPostProcessing::UpdateConfig()
{
  if (m_config.GetShader() == g_ActiveConfig.sPostProcessingShader)
    return;

  std::string source = m_config.LoadShader(APIType::D3D);
  if (source.empty())
  {
    m_ps_is_default = true;
    source = s_default_shader;
  }
  else
  {
    m_ps_is_default = false;
  }

  if (m_current_ps)
    m_current_ps->Release();

  m_current_ps = CompileShader(source);
  D3D::SetDebugObjectName(
      m_current_ps, (std::string("D3D Post Processing shader: ") + m_config.GetShader()).c_str());
}

}  // namespace DX11
