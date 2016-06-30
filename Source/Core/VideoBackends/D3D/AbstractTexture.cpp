// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>

#include "VideoBackends/D3D/AbstractTexture.h"
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/D3DUtil.h"
#include "VideoBackends/D3D/FramebufferManager.h"
#include "VideoBackends/D3D/GeometryShaderCache.h"
#include "VideoBackends/D3D/PixelShaderCache.h"
#include "VideoBackends/D3D/VertexShaderCache.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/RenderBase.h"

namespace DX11
{
const size_t MAX_COPY_BUFFERS = 32;
ID3D11Buffer* efbcopycbuf[MAX_COPY_BUFFERS] = { 0 };

AbstractTexture::AbstractTexture(const AbstractTextureBase::TextureConfig& config) : AbstractTextureBase(config)
{
  usage = D3D11_USAGE_DEFAULT;
  if (config.rendertarget)
  {
    texture = D3DTexture2D::Create(config.width, config.height,
      (D3D11_BIND_FLAG)((int)D3D11_BIND_RENDER_TARGET | (int)D3D11_BIND_SHADER_RESOURCE),
                                   usage, DXGI_FORMAT_R8G8B8A8_UNORM, 1, config.layers);
  }
  else
  {
    D3D11_CPU_ACCESS_FLAG cpu_access = (D3D11_CPU_ACCESS_FLAG)0;

    if (config.levels == 1)
    {
      usage = D3D11_USAGE_DYNAMIC;
      cpu_access = D3D11_CPU_ACCESS_WRITE;
    }

    const D3D11_TEXTURE2D_DESC texdesc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM,
                                                               config.width, config.height, 1, config.levels, D3D11_BIND_SHADER_RESOURCE, usage, cpu_access);

    ID3D11Texture2D *pTexture;
    const HRESULT hr = D3D::device->CreateTexture2D(&texdesc, nullptr, &pTexture);
    CHECK(SUCCEEDED(hr), "Create texture of the TextureCache");

    texture = new D3DTexture2D(pTexture, D3D11_BIND_SHADER_RESOURCE);

    // TODO: better debug names
    D3D::SetDebugObjectName((ID3D11DeviceChild*)texture->GetTex(), "a texture of the TextureCache");
    D3D::SetDebugObjectName((ID3D11DeviceChild*)texture->GetSRV(), "shader resource view of a texture of the TextureCache");

    SAFE_RELEASE(pTexture);
  }
}

AbstractTexture::~AbstractTexture()
{
  texture->Release();
}

void AbstractTexture::CopyRectangleFromTexture(
  const AbstractTextureBase* source,
  const MathUtil::Rectangle<int> &srcrect,
  const MathUtil::Rectangle<int> &dstrect)
{
  auto* source_tex = static_cast<const AbstractTexture*>(source);

  if (srcrect.GetWidth() == dstrect.GetWidth()
      && srcrect.GetHeight() == dstrect.GetHeight())
  {
    D3D11_BOX srcbox;
    srcbox.left = srcrect.left;
    srcbox.top = srcrect.top;
    srcbox.right = srcrect.right;
    srcbox.bottom = srcrect.bottom;
    srcbox.front = 0;
    srcbox.back = source_tex->config.layers;

    D3D::context->CopySubresourceRegion(
      texture->GetTex(),
      0,
      dstrect.left,
      dstrect.top,
      0,
      source_tex->texture->GetTex(),
      0,
      &srcbox);
    return;
  }
  else if (!config.rendertarget)
  {
    return;
  }
  g_renderer->ResetAPIState(); // reset any game specific settings

  const D3D11_VIEWPORT vp = CD3D11_VIEWPORT(
    float(dstrect.left),
    float(dstrect.top),
    float(dstrect.GetWidth()),
    float(dstrect.GetHeight()));

  D3D::stateman->UnsetTexture(texture->GetSRV());
  D3D::stateman->Apply();

  D3D::context->OMSetRenderTargets(1, &texture->GetRTV(), nullptr);
  D3D::context->RSSetViewports(1, &vp);
  D3D::SetLinearCopySampler();
  D3D11_RECT srcRC;
  srcRC.left = srcrect.left;
  srcRC.right = srcrect.right;
  srcRC.top = srcrect.top;
  srcRC.bottom = srcrect.bottom;
  D3D::drawShadedTexQuad(source_tex->texture->GetSRV(), &srcRC,
                         source_tex->config.width, source_tex->config.height,
                         PixelShaderCache::GetColorCopyProgram(false),
                         VertexShaderCache::GetSimpleVertexShader(),
                         VertexShaderCache::GetSimpleInputLayout(), GeometryShaderCache::GetCopyGeometryShader(), 1.0,
                         0);

  D3D::context->OMSetRenderTargets(1,
                                   &FramebufferManager::GetEFBColorTexture()->GetRTV(),
                                   FramebufferManager::GetEFBDepthTexture()->GetDSV());

  g_renderer->RestoreAPIState();
}

void AbstractTexture::Load(u8* data, unsigned int width, unsigned int height,
                           unsigned int expanded_width, unsigned int level)
{
  unsigned int src_pitch = 4 * expanded_width;
  D3D::ReplaceRGBATexture2D(texture->GetTex(), data, width, height, src_pitch, level,
                            usage);
}

void AbstractTexture::FromRenderTarget(u8* dst, PEControl::PixelFormat srcFormat, const
                                       EFBRectangle& srcRect,
                                       bool scaleByHalf, unsigned int cbufid, const float *colmat)
{
  // When copying at half size, in multisampled mode, resolve the color/depth buffer first.
  // This is because multisampled texture reads go through Load, not Sample, and the linear
  // filter is ignored.
  bool multisampled = (g_ActiveConfig.iMultisamples > 1);
  ID3D11ShaderResourceView* efbTexSRV = (srcFormat == PEControl::Z24) ?
    FramebufferManager::GetEFBDepthTexture()->GetSRV() :
    FramebufferManager::GetEFBColorTexture()->GetSRV();
  if (multisampled && scaleByHalf)
  {
    multisampled = false;
    efbTexSRV = (srcFormat == PEControl::Z24) ?
      FramebufferManager::GetResolvedEFBDepthTexture()->GetSRV() :
      FramebufferManager::GetResolvedEFBColorTexture()->GetSRV();
  }

  g_renderer->ResetAPIState();

  // stretch picture with increased internal resolution
  const D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, (float)config.width, (float)config.height);
  D3D::context->RSSetViewports(1, &vp);

  // set transformation
  if (efbcopycbuf[cbufid] == nullptr)
  {
    const D3D11_BUFFER_DESC cbdesc = CD3D11_BUFFER_DESC(28 * sizeof(float),
                                                        D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DEFAULT);
    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = colmat;
    HRESULT hr = D3D::device->CreateBuffer(&cbdesc, &data, &efbcopycbuf[cbufid]);
    CHECK(SUCCEEDED(hr), "Create efb copy constant buffer %d", cbufid);
    D3D::SetDebugObjectName((ID3D11DeviceChild*)efbcopycbuf[cbufid], "a constant buffer used in AbstractTexture::CopyRenderTargetToTexture");
  }
  D3D::stateman->SetPixelConstants(efbcopycbuf[cbufid]);

  const TargetRectangle targetSource = g_renderer->ConvertEFBRectangle(srcRect);
  // TODO: try targetSource.asRECT();
  const D3D11_RECT sourcerect = CD3D11_RECT(targetSource.left, targetSource.top, targetSource.right,
                                            targetSource.bottom);

  // Use linear filtering if (bScaleByHalf), use point filtering otherwise
  if (scaleByHalf)
    D3D::SetLinearCopySampler();
  else
    D3D::SetPointCopySampler();

  // Make sure we don't draw with the texture set as both a source and target.
  // (This can happen because we don't unbind textures when we free them.)
  D3D::stateman->UnsetTexture(texture->GetSRV());
  D3D::stateman->Apply();

  D3D::context->OMSetRenderTargets(1, &texture->GetRTV(), nullptr);

  // Create texture copy
  D3D::drawShadedTexQuad(
    efbTexSRV,
    &sourcerect, Renderer::GetTargetWidth(),
    Renderer::GetTargetHeight(),
    srcFormat == PEControl::Z24 ? PixelShaderCache::GetDepthMatrixProgram(multisampled) :
    PixelShaderCache::GetColorMatrixProgram(multisampled),
    VertexShaderCache::GetSimpleVertexShader(),
    VertexShaderCache::GetSimpleInputLayout(),
    GeometryShaderCache::GetCopyGeometryShader());

  D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(),
                                   FramebufferManager::GetEFBDepthTexture()->GetDSV());

  g_renderer->RestoreAPIState();
}

void AbstractTexture::Bind(unsigned int stage)
{
  D3D::stateman->SetTexture(stage, texture->GetSRV());
}

bool AbstractTexture::Save(const std::string& filename, unsigned int level)
{
  // Create a staging/readback texture with the dimensions of the specified mip level.
  u32 mip_width = std::max(config.width >> level, 1u);
  u32 mip_height = std::max(config.height >> level, 1u);
  CD3D11_TEXTURE2D_DESC staging_texture_desc(DXGI_FORMAT_R8G8B8A8_UNORM, mip_width, mip_height, 1,
                                             1, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ);

  ID3D11Texture2D* staging_texture;
  HRESULT hr = D3D::device->CreateTexture2D(&staging_texture_desc, nullptr, &staging_texture);
  if (FAILED(hr))
  {
    WARN_LOG(VIDEO, "Failed to create texture dumping readback texture: %X", static_cast<u32>(hr));
    return false;
  }

  // Copy the selected mip level to the staging texture.
  CD3D11_BOX src_box(0, 0, 0, mip_width, mip_height, 1);
  D3D::context->CopySubresourceRegion(staging_texture, 0, 0, 0, 0, texture->GetTex(),
                                      D3D11CalcSubresource(level, 0, config.levels), &src_box);

  // Map the staging texture to client memory, and encode it as a .png image.
  D3D11_MAPPED_SUBRESOURCE map;
  hr = D3D::context->Map(staging_texture, 0, D3D11_MAP_READ, 0, &map);
  if (FAILED(hr))
  {
    WARN_LOG(VIDEO, "Failed to map texture dumping readback texture: %X", static_cast<u32>(hr));
    staging_texture->Release();
    return false;
  }

  bool encode_result =
    TextureToPng(reinterpret_cast<u8*>(map.pData), map.RowPitch, filename, mip_width, mip_height);
  D3D::context->Unmap(staging_texture, 0);
  staging_texture->Release();

  return encode_result;
}

void AbstractTexture::Shutdown()
{
  for (unsigned int k = 0; k < MAX_COPY_BUFFERS; ++k)
    SAFE_RELEASE(efbcopycbuf[k]);
}

}
