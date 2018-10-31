// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/D3DTexture.h"
#include "VideoBackends/D3D/D3DUtil.h"
#include "VideoBackends/D3D/DXTexture.h"
#include "VideoBackends/D3D/FramebufferManager.h"
#include "VideoBackends/D3D/GeometryShaderCache.h"
#include "VideoBackends/D3D/PixelShaderCache.h"
#include "VideoBackends/D3D/TextureCache.h"
#include "VideoBackends/D3D/VertexShaderCache.h"

#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/TextureConfig.h"

namespace DX11
{
namespace
{
DXGI_FORMAT GetDXGIFormatForHostFormat(AbstractTextureFormat format)
{
  switch (format)
  {
  case AbstractTextureFormat::DXT1:
    return DXGI_FORMAT_BC1_UNORM;
  case AbstractTextureFormat::DXT3:
    return DXGI_FORMAT_BC2_UNORM;
  case AbstractTextureFormat::DXT5:
    return DXGI_FORMAT_BC3_UNORM;
  case AbstractTextureFormat::BPTC:
    return DXGI_FORMAT_BC7_UNORM;
  case AbstractTextureFormat::RGBA8:
    return DXGI_FORMAT_R8G8B8A8_UNORM;
  case AbstractTextureFormat::BGRA8:
    return DXGI_FORMAT_B8G8R8A8_UNORM;
  case AbstractTextureFormat::R16:
    return DXGI_FORMAT_R16_UNORM;
  case AbstractTextureFormat::R32F:
    return DXGI_FORMAT_R32_FLOAT;
  case AbstractTextureFormat::D16:
    return DXGI_FORMAT_R16_TYPELESS;
  case AbstractTextureFormat::D24_S8:
    return DXGI_FORMAT_R24G8_TYPELESS;
  case AbstractTextureFormat::D32F:
    return DXGI_FORMAT_R32_TYPELESS;
  case AbstractTextureFormat::D32F_S8:
    return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
  default:
    PanicAlert("Unhandled texture format.");
    return DXGI_FORMAT_R8G8B8A8_UNORM;
  }
}
DXGI_FORMAT GetSRVFormatForHostFormat(AbstractTextureFormat format)
{
  switch (format)
  {
  case AbstractTextureFormat::D16:
    return DXGI_FORMAT_R16_UNORM;
  case AbstractTextureFormat::D24_S8:
    return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
  case AbstractTextureFormat::D32F:
    return DXGI_FORMAT_R32_FLOAT;
  case AbstractTextureFormat::D32F_S8:
    return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
  default:
    return GetDXGIFormatForHostFormat(format);
  }
}
DXGI_FORMAT GetDSVFormatForHostFormat(AbstractTextureFormat format)
{
  switch (format)
  {
  case AbstractTextureFormat::D16:
    return DXGI_FORMAT_D16_UNORM;
  case AbstractTextureFormat::D24_S8:
    return DXGI_FORMAT_D24_UNORM_S8_UINT;
  case AbstractTextureFormat::D32F:
    return DXGI_FORMAT_D32_FLOAT;
  case AbstractTextureFormat::D32F_S8:
    return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
  default:
    return GetDXGIFormatForHostFormat(format);
  }
}
}  // Anonymous namespace

DXTexture::DXTexture(const TextureConfig& tex_config) : AbstractTexture(tex_config)
{
  DXGI_FORMAT tex_format = GetDXGIFormatForHostFormat(m_config.format);
  DXGI_FORMAT srv_format = GetSRVFormatForHostFormat(m_config.format);
  DXGI_FORMAT rtv_format = DXGI_FORMAT_UNKNOWN;
  DXGI_FORMAT dsv_format = DXGI_FORMAT_UNKNOWN;
  UINT bind_flags = D3D11_BIND_SHADER_RESOURCE;
  if (tex_config.rendertarget)
  {
    if (IsDepthFormat(tex_config.format))
    {
      bind_flags |= D3D11_BIND_DEPTH_STENCIL;
      dsv_format = GetDSVFormatForHostFormat(m_config.format);
    }
    else
    {
      bind_flags |= D3D11_BIND_RENDER_TARGET;
      rtv_format = tex_format;
    }
  }

  CD3D11_TEXTURE2D_DESC texdesc(tex_format, tex_config.width, tex_config.height, tex_config.layers,
                                tex_config.levels, bind_flags, D3D11_USAGE_DEFAULT, 0,
                                tex_config.samples, 0, 0);

  ID3D11Texture2D* pTexture;
  HRESULT hr = D3D::device->CreateTexture2D(&texdesc, nullptr, &pTexture);
  CHECK(SUCCEEDED(hr), "Create backing DXTexture");

  m_texture = new D3DTexture2D(pTexture, static_cast<D3D11_BIND_FLAG>(bind_flags), srv_format,
                               dsv_format, rtv_format, tex_config.samples > 1);

  SAFE_RELEASE(pTexture);
}

DXTexture::~DXTexture()
{
  g_renderer->UnbindTexture(this);
  m_texture->Release();
}

D3DTexture2D* DXTexture::GetRawTexIdentifier() const
{
  return m_texture;
}

void DXTexture::CopyRectangleFromTexture(const AbstractTexture* src,
                                         const MathUtil::Rectangle<int>& src_rect, u32 src_layer,
                                         u32 src_level, const MathUtil::Rectangle<int>& dst_rect,
                                         u32 dst_layer, u32 dst_level)
{
  const DXTexture* srcentry = static_cast<const DXTexture*>(src);
  ASSERT(src_rect.GetWidth() == dst_rect.GetWidth() &&
         src_rect.GetHeight() == dst_rect.GetHeight());

  D3D11_BOX src_box;
  src_box.left = src_rect.left;
  src_box.top = src_rect.top;
  src_box.right = src_rect.right;
  src_box.bottom = src_rect.bottom;
  src_box.front = 0;
  src_box.back = 1;

  D3D::context->CopySubresourceRegion(
      m_texture->GetTex(), D3D11CalcSubresource(dst_level, dst_layer, m_config.levels),
      dst_rect.left, dst_rect.top, 0, srcentry->m_texture->GetTex(),
      D3D11CalcSubresource(src_level, src_layer, srcentry->m_config.levels), &src_box);
}

void DXTexture::ScaleRectangleFromTexture(const AbstractTexture* source,
                                          const MathUtil::Rectangle<int>& srcrect,
                                          const MathUtil::Rectangle<int>& dstrect)
{
  const DXTexture* srcentry = static_cast<const DXTexture*>(source);
  ASSERT(m_config.rendertarget);

  g_renderer->ResetAPIState();  // reset any game specific settings

  const D3D11_VIEWPORT vp = CD3D11_VIEWPORT(float(dstrect.left), float(dstrect.top),
                                            float(dstrect.GetWidth()), float(dstrect.GetHeight()));

  D3D::stateman->UnsetTexture(m_texture->GetSRV());
  D3D::stateman->Apply();

  D3D::context->OMSetRenderTargets(1, &m_texture->GetRTV(), nullptr);
  D3D::context->RSSetViewports(1, &vp);
  D3D::SetLinearCopySampler();
  D3D11_RECT srcRC;
  srcRC.left = srcrect.left;
  srcRC.right = srcrect.right;
  srcRC.top = srcrect.top;
  srcRC.bottom = srcrect.bottom;
  D3D::drawShadedTexQuad(
      srcentry->m_texture->GetSRV(), &srcRC, srcentry->m_config.width, srcentry->m_config.height,
      PixelShaderCache::GetColorCopyProgram(false), VertexShaderCache::GetSimpleVertexShader(),
      VertexShaderCache::GetSimpleInputLayout(), GeometryShaderCache::GetCopyGeometryShader(), 0);

  g_renderer->RestoreAPIState();
}

void DXTexture::ResolveFromTexture(const AbstractTexture* src, const MathUtil::Rectangle<int>& rect,
                                   u32 layer, u32 level)
{
  const DXTexture* srcentry = static_cast<const DXTexture*>(src);
  DEBUG_ASSERT(m_config.samples > 1 && m_config.width == srcentry->m_config.width &&
               m_config.height == srcentry->m_config.height && m_config.samples == 1);
  DEBUG_ASSERT(rect.left + rect.GetWidth() <= static_cast<int>(srcentry->m_config.width) &&
               rect.top + rect.GetHeight() <= static_cast<int>(srcentry->m_config.height));

  D3D::context->ResolveSubresource(
      m_texture->GetTex(), D3D11CalcSubresource(level, layer, m_config.levels),
      srcentry->m_texture->GetTex(), D3D11CalcSubresource(level, layer, srcentry->m_config.levels),
      GetDXGIFormatForHostFormat(m_config.format));
}

void DXTexture::Load(u32 level, u32 width, u32 height, u32 row_length, const u8* buffer,
                     size_t buffer_size)
{
  size_t src_pitch = CalculateStrideForFormat(m_config.format, row_length);
  D3D::context->UpdateSubresource(m_texture->GetTex(), level, nullptr, buffer,
                                  static_cast<UINT>(src_pitch), 0);
}

DXStagingTexture::DXStagingTexture(StagingTextureType type, const TextureConfig& config,
                                   ID3D11Texture2D* tex)
    : AbstractStagingTexture(type, config), m_tex(tex)
{
}

DXStagingTexture::~DXStagingTexture()
{
  if (IsMapped())
    DXStagingTexture::Unmap();
  SAFE_RELEASE(m_tex);
}

std::unique_ptr<DXStagingTexture> DXStagingTexture::Create(StagingTextureType type,
                                                           const TextureConfig& config)
{
  D3D11_USAGE usage;
  UINT cpu_flags;
  if (type == StagingTextureType::Readback)
  {
    usage = D3D11_USAGE_STAGING;
    cpu_flags = D3D11_CPU_ACCESS_READ;
  }
  else if (type == StagingTextureType::Upload)
  {
    usage = D3D11_USAGE_DYNAMIC;
    cpu_flags = D3D11_CPU_ACCESS_WRITE;
  }
  else
  {
    usage = D3D11_USAGE_STAGING;
    cpu_flags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
  }

  CD3D11_TEXTURE2D_DESC desc(GetDXGIFormatForHostFormat(config.format), config.width, config.height,
                             1, 1, 0, usage, cpu_flags);

  ID3D11Texture2D* texture;
  HRESULT hr = D3D::device->CreateTexture2D(&desc, nullptr, &texture);
  CHECK(SUCCEEDED(hr), "Create staging texture");
  if (FAILED(hr))
    return nullptr;

  return std::unique_ptr<DXStagingTexture>(new DXStagingTexture(type, config, texture));
}

void DXStagingTexture::CopyFromTexture(const AbstractTexture* src,
                                       const MathUtil::Rectangle<int>& src_rect, u32 src_layer,
                                       u32 src_level, const MathUtil::Rectangle<int>& dst_rect)
{
  ASSERT(m_type == StagingTextureType::Readback);
  ASSERT(src_rect.GetWidth() == dst_rect.GetWidth() &&
         src_rect.GetHeight() == dst_rect.GetHeight());
  ASSERT(src_rect.left >= 0 && static_cast<u32>(src_rect.right) <= src->GetConfig().width &&
         src_rect.top >= 0 && static_cast<u32>(src_rect.bottom) <= src->GetConfig().height);
  ASSERT(dst_rect.left >= 0 && static_cast<u32>(dst_rect.right) <= m_config.width &&
         dst_rect.top >= 0 && static_cast<u32>(dst_rect.bottom) <= m_config.height);

  if (IsMapped())
    DXStagingTexture::Unmap();

  CD3D11_BOX src_box(src_rect.left, src_rect.top, 0, src_rect.right, src_rect.bottom, 1);
  D3D::context->CopySubresourceRegion(
      m_tex, 0, static_cast<u32>(dst_rect.left), static_cast<u32>(dst_rect.top), 0,
      static_cast<const DXTexture*>(src)->GetRawTexIdentifier()->GetTex(),
      D3D11CalcSubresource(src_level, src_layer, src->GetConfig().levels), &src_box);

  m_needs_flush = true;
}

void DXStagingTexture::CopyToTexture(const MathUtil::Rectangle<int>& src_rect, AbstractTexture* dst,
                                     const MathUtil::Rectangle<int>& dst_rect, u32 dst_layer,
                                     u32 dst_level)
{
  ASSERT(m_type == StagingTextureType::Upload);
  ASSERT(src_rect.GetWidth() == dst_rect.GetWidth() &&
         src_rect.GetHeight() == dst_rect.GetHeight());
  ASSERT(src_rect.left >= 0 && static_cast<u32>(src_rect.right) <= m_config.width &&
         src_rect.top >= 0 && static_cast<u32>(src_rect.bottom) <= m_config.height);
  ASSERT(dst_rect.left >= 0 && static_cast<u32>(dst_rect.right) <= dst->GetConfig().width &&
         dst_rect.top >= 0 && static_cast<u32>(dst_rect.bottom) <= dst->GetConfig().height);

  if (IsMapped())
    DXStagingTexture::Unmap();

  CD3D11_BOX src_box(src_rect.left, src_rect.top, 0, src_rect.right, src_rect.bottom, 1);
  D3D::context->CopySubresourceRegion(
      static_cast<const DXTexture*>(dst)->GetRawTexIdentifier()->GetTex(),
      D3D11CalcSubresource(dst_level, dst_layer, dst->GetConfig().levels),
      static_cast<u32>(dst_rect.left), static_cast<u32>(dst_rect.top), 0, m_tex, 0, &src_box);
}

bool DXStagingTexture::Map()
{
  if (m_map_pointer)
    return true;

  D3D11_MAP map_type;
  if (m_type == StagingTextureType::Readback)
    map_type = D3D11_MAP_READ;
  else if (m_type == StagingTextureType::Upload)
    map_type = D3D11_MAP_WRITE;
  else
    map_type = D3D11_MAP_READ_WRITE;

  D3D11_MAPPED_SUBRESOURCE sr;
  HRESULT hr = D3D::context->Map(m_tex, 0, map_type, 0, &sr);
  CHECK(SUCCEEDED(hr), "Map readback texture");
  if (FAILED(hr))
    return false;

  m_map_pointer = reinterpret_cast<char*>(sr.pData);
  m_map_stride = sr.RowPitch;
  return true;
}

void DXStagingTexture::Unmap()
{
  if (!m_map_pointer)
    return;

  D3D::context->Unmap(m_tex, 0);
  m_map_pointer = nullptr;
}

void DXStagingTexture::Flush()
{
  // Flushing is handled by the API.
  m_needs_flush = false;
}

DXFramebuffer::DXFramebuffer(AbstractTextureFormat color_format, AbstractTextureFormat depth_format,
                             u32 width, u32 height, u32 layers, u32 samples,
                             ID3D11RenderTargetView* rtv, ID3D11DepthStencilView* dsv)
    : AbstractFramebuffer(color_format, depth_format, width, height, layers, samples), m_rtv(rtv),
      m_dsv(dsv)
{
}

DXFramebuffer::~DXFramebuffer()
{
  if (m_rtv)
    m_rtv->Release();
  if (m_dsv)
    m_dsv->Release();
}

std::unique_ptr<DXFramebuffer> DXFramebuffer::Create(const DXTexture* color_attachment,
                                                     const DXTexture* depth_attachment)
{
  if (!ValidateConfig(color_attachment, depth_attachment))
    return nullptr;

  const AbstractTextureFormat color_format =
      color_attachment ? color_attachment->GetFormat() : AbstractTextureFormat::Undefined;
  const AbstractTextureFormat depth_format =
      depth_attachment ? depth_attachment->GetFormat() : AbstractTextureFormat::Undefined;
  const DXTexture* either_attachment = color_attachment ? color_attachment : depth_attachment;
  const u32 width = either_attachment->GetWidth();
  const u32 height = either_attachment->GetHeight();
  const u32 layers = either_attachment->GetLayers();
  const u32 samples = either_attachment->GetSamples();

  ID3D11RenderTargetView* rtv = nullptr;
  if (color_attachment)
  {
    D3D11_RENDER_TARGET_VIEW_DESC desc;
    desc.Format = GetDXGIFormatForHostFormat(color_attachment->GetConfig().format);
    if (color_attachment->GetConfig().IsMultisampled())
    {
      desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
      desc.Texture2DMSArray.ArraySize = color_attachment->GetConfig().layers;
      desc.Texture2DMSArray.FirstArraySlice = 0;
    }
    else
    {
      desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
      desc.Texture2DArray.ArraySize = color_attachment->GetConfig().layers;
      desc.Texture2DArray.FirstArraySlice = 0;
      desc.Texture2DArray.MipSlice = 0;
    }

    HRESULT hr = D3D::device->CreateRenderTargetView(
        color_attachment->GetRawTexIdentifier()->GetTex(), &desc, &rtv);
    CHECK(SUCCEEDED(hr), "Create render target view for framebuffer");
  }

  ID3D11DepthStencilView* dsv = nullptr;
  if (depth_attachment)
  {
    D3D11_DEPTH_STENCIL_VIEW_DESC desc;
    desc.Format = GetDXGIFormatForHostFormat(depth_attachment->GetConfig().format);
    if (depth_attachment->GetConfig().IsMultisampled())
    {
      desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
      desc.Texture2DMSArray.ArraySize = depth_attachment->GetConfig().layers;
      desc.Texture2DMSArray.FirstArraySlice = 0;
    }
    else
    {
      desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
      desc.Texture2DArray.ArraySize = depth_attachment->GetConfig().layers;
      desc.Texture2DArray.FirstArraySlice = 0;
      desc.Texture2DArray.MipSlice = 0;
    }

    HRESULT hr = D3D::device->CreateDepthStencilView(
        depth_attachment->GetRawTexIdentifier()->GetTex(), &desc, &dsv);
    CHECK(SUCCEEDED(hr), "Create depth stencil view for framebuffer");
  }

  return std::make_unique<DXFramebuffer>(color_format, depth_format, width, height, layers, samples,
                                         rtv, dsv);
}

}  // namespace DX11
