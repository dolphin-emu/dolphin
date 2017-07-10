// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DShader.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/D3DTexture.h"
#include "VideoBackends/D3D/D3DUtil.h"
#include "VideoBackends/D3D/DXPixelShader.h"
#include "VideoBackends/D3D/DXTexture.h"
#include "VideoBackends/D3D/FramebufferManager.h"
#include "VideoBackends/D3D/VertexShaderCache.h"

namespace DX11
{
struct EFBEncodeParams
{
  DWORD SrcLeft;
  DWORD SrcTop;
  DWORD placeholder;
  DWORD ScaleFactor;
  DWORD DestinationWidth;
  DWORD DestinationHeight;
  DWORD placeholders[2];
};
DXPixelShader::DXPixelShader(const std::string& shader_source) : AbstractPixelShader(shader_source), m_shader_params(nullptr)
{
  D3DBlob* bytecode = nullptr;
  if (!D3D::CompilePixelShader(shader_source, &bytecode))
  {
    PanicAlert("Failed to compile texture encoding shader.");
    // TODO: Error
  }

  HRESULT hr =
    D3D::device->CreatePixelShader(bytecode->Data(), bytecode->Size(), nullptr, &m_shader);
  CHECK(SUCCEEDED(hr), "create pixel shader");

  // Create constant buffer for uploading data to shaders
  D3D11_BUFFER_DESC bd = CD3D11_BUFFER_DESC(sizeof(EFBEncodeParams), D3D11_BIND_CONSTANT_BUFFER);
  hr = D3D::device->CreateBuffer(&bd, nullptr, &m_shader_params);
  CHECK(SUCCEEDED(hr), "create shader params buffer");
  D3D::SetDebugObjectName(m_shader_params, "shader params buffer");
}

DXPixelShader::~DXPixelShader()
{
  SAFE_RELEASE(m_shader);
  SAFE_RELEASE(m_shader_params);
}

void DXPixelShader::ApplyTo(AbstractTexture* texture)
{
  // Reset API
  g_renderer->ResetAPIState();

  DXTexture* d3d_texture = static_cast<DXTexture*>(texture);
  D3DTexture2D* src_texture = d3d_texture->GetRawTexIdentifier();
  D3DTexture2D* render_target_texture = d3d_texture->GetRenderTargetCopy();

  
  D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, FLOAT(texture->GetConfig().width), FLOAT(texture->GetConfig().height));
  D3D::context->RSSetViewports(1, &vp);

  // Make sure we don't draw with the texture set as both a source and target.
  // (This can happen because we don't unbind textures when we free them.)
  D3D::stateman->UnsetTexture(render_target_texture->GetSRV());
  D3D::stateman->Apply();

  D3D::context->OMSetRenderTargets(1, &render_target_texture->GetRTV(), nullptr);

  auto uniform = GetUniform(0);

  EFBEncodeParams params;
  params.SrcLeft = uniform[0];
  params.SrcTop = uniform[1];
  // Skip placeholder
  params.ScaleFactor = uniform[3];
  params.DestinationWidth = uniform[4];
  params.DestinationWidth = uniform[5];
  D3D::context->UpdateSubresource(m_shader_params, 0, nullptr, &params, 0, 0);
  D3D::stateman->SetPixelConstants(m_shader_params);

  // We also linear filtering for both box filtering and downsampling higher resolutions to 1x
  // TODO: This only produces perfect downsampling for 1.5x and 2x IR, other resolution will
  //       need more complex down filtering to average all pixels and produce the correct result.
  // Also, box filtering won't be correct for anything other than 1x IR
  if (g_ActiveConfig.iEFBScale != SCALE_1X)
    D3D::SetLinearCopySampler();
  else
    D3D::SetPointCopySampler();

  const D3D11_RECT source_rect = CD3D11_RECT(0, 0, texture->GetConfig().width, texture->GetConfig().height);
  D3D::drawShadedTexQuad(src_texture->GetSRV(), &source_rect, texture->GetConfig().width,
    texture->GetConfig().height, m_shader,
    VertexShaderCache::GetSimpleVertexShader(),
    VertexShaderCache::GetSimpleInputLayout());

  // Now copy it back..
  D3D::context->CopyResource(src_texture->GetTex(), render_target_texture->GetTex());

  // Restore API
  g_renderer->RestoreAPIState();
  D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(),
    FramebufferManager::GetEFBDepthTexture()->GetDSV());
}
} // namespace DX11
