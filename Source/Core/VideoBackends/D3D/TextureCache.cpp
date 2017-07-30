// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/D3D/TextureCache.h"

#include <algorithm>
#include <memory>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DShader.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/D3DTexture.h"
#include "VideoBackends/D3D/D3DUtil.h"
#include "VideoBackends/D3D/DXTexture.h"
#include "VideoBackends/D3D/FramebufferManager.h"
#include "VideoBackends/D3D/GeometryShaderCache.h"
#include "VideoBackends/D3D/PSTextureEncoder.h"
#include "VideoBackends/D3D/PixelShaderCache.h"
#include "VideoBackends/D3D/VertexShaderCache.h"

#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/TextureConfig.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{
static const size_t MAX_COPY_BUFFERS = 32;
static ID3D11Buffer* s_efbcopycbuf[MAX_COPY_BUFFERS] = {0};
static std::unique_ptr<PSTextureEncoder> g_encoder;

std::unique_ptr<AbstractTexture> TextureCache::CreateTexture(const TextureConfig& config)
{
  return std::make_unique<DXTexture>(config);
}

void TextureCache::CopyEFB(u8* dst, const EFBCopyParams& params, u32 native_width,
                           u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
                           const EFBRectangle& src_rect, bool scale_by_half)
{
  g_encoder->Encode(dst, params, native_width, bytes_per_row, num_blocks_y, memory_stride, src_rect,
                    scale_by_half);
}

const char palette_shader[] =
    R"HLSL(
sampler samp0 : register(s0);
Texture2DArray Tex0 : register(t0);
Buffer<uint> Tex1 : register(t1);
uniform float Multiply;

uint Convert3To8(uint v)
{
	// Swizzle bits: 00000123 -> 12312312
	return (v << 5) | (v << 2) | (v >> 1);
}

uint Convert4To8(uint v)
{
	// Swizzle bits: 00001234 -> 12341234
	return (v << 4) | v;
}

uint Convert5To8(uint v)
{
	// Swizzle bits: 00012345 -> 12345123
	return (v << 3) | (v >> 2);
}

uint Convert6To8(uint v)
{
	// Swizzle bits: 00123456 -> 12345612
	return (v << 2) | (v >> 4);
}

float4 DecodePixel_RGB5A3(uint val)
{
	int r,g,b,a;
	if ((val&0x8000))
	{
		r=Convert5To8((val>>10) & 0x1f);
		g=Convert5To8((val>>5 ) & 0x1f);
		b=Convert5To8((val    ) & 0x1f);
		a=0xFF;
	}
	else
	{
		a=Convert3To8((val>>12) & 0x7);
		r=Convert4To8((val>>8 ) & 0xf);
		g=Convert4To8((val>>4 ) & 0xf);
		b=Convert4To8((val    ) & 0xf);
	}
	return float4(r, g, b, a) / 255;
}

float4 DecodePixel_RGB565(uint val)
{
	int r, g, b, a;
	r = Convert5To8((val >> 11) & 0x1f);
	g = Convert6To8((val >> 5) & 0x3f);
	b = Convert5To8((val) & 0x1f);
	a = 0xFF;
	return float4(r, g, b, a) / 255;
}

float4 DecodePixel_IA8(uint val)
{
	int i = val & 0xFF;
	int a = val >> 8;
	return float4(i, i, i, a) / 255;
}

void main(
	out float4 ocol0 : SV_Target,
	in float4 pos : SV_Position,
	in float3 uv0 : TEXCOORD0)
{
	uint src = round(Tex0.Sample(samp0,uv0) * Multiply).r;
	src = Tex1.Load(src);
	src = ((src << 8) & 0xFF00) | (src >> 8);
	ocol0 = DECODE(src);
}
)HLSL";

void TextureCache::ConvertTexture(TCacheEntry* destination, TCacheEntry* source,
                                  const void* palette, TLUTFormat format)
{
  DXTexture* source_texture = static_cast<DXTexture*>(source->texture.get());
  DXTexture* destination_texture = static_cast<DXTexture*>(destination->texture.get());
  g_renderer->ResetAPIState();

  // stretch picture with increased internal resolution
  const D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, static_cast<float>(source->GetWidth()),
                                            static_cast<float>(source->GetHeight()));
  D3D::context->RSSetViewports(1, &vp);

  D3D11_BOX box{0, 0, 0, 512, 1, 1};
  D3D::context->UpdateSubresource(palette_buf, 0, &box, palette, 0, 0);

  D3D::stateman->SetTexture(1, palette_buf_srv);

  // TODO: Add support for C14X2 format.  (Different multiplier, more palette entries.)
  float params[4] = {source->format == TextureFormat::I4 ? 15.f : 255.f};
  D3D::context->UpdateSubresource(palette_uniform, 0, nullptr, &params, 0, 0);
  D3D::stateman->SetPixelConstants(palette_uniform);

  const D3D11_RECT sourcerect = CD3D11_RECT(0, 0, source->GetWidth(), source->GetHeight());

  D3D::SetPointCopySampler();

  // Make sure we don't draw with the texture set as both a source and target.
  // (This can happen because we don't unbind textures when we free them.)
  D3D::stateman->UnsetTexture(destination_texture->GetRawTexIdentifier()->GetSRV());
  D3D::stateman->Apply();

  D3D::context->OMSetRenderTargets(1, &destination_texture->GetRawTexIdentifier()->GetRTV(),
                                   nullptr);

  // Create texture copy
  D3D::drawShadedTexQuad(
      source_texture->GetRawTexIdentifier()->GetSRV(), &sourcerect, source->GetWidth(),
      source->GetHeight(), palette_pixel_shader[static_cast<int>(format)],
      VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout(),
      GeometryShaderCache::GetCopyGeometryShader());

  D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(),
                                   FramebufferManager::GetEFBDepthTexture()->GetDSV());

  g_renderer->RestoreAPIState();
}

ID3D11PixelShader* GetConvertShader(const char* Type)
{
  std::string shader = "#define DECODE DecodePixel_";
  shader.append(Type);
  shader.append("\n");
  shader.append(palette_shader);
  return D3D::CompileAndCreatePixelShader(shader);
}

TextureCache::TextureCache()
{
  // FIXME: Is it safe here?
  g_encoder = std::make_unique<PSTextureEncoder>();
  g_encoder->Init();

  palette_buf = nullptr;
  palette_buf_srv = nullptr;
  palette_uniform = nullptr;
  palette_pixel_shader[static_cast<int>(TLUTFormat::IA8)] = GetConvertShader("IA8");
  palette_pixel_shader[static_cast<int>(TLUTFormat::RGB565)] = GetConvertShader("RGB565");
  palette_pixel_shader[static_cast<int>(TLUTFormat::RGB5A3)] = GetConvertShader("RGB5A3");
  auto lutBd = CD3D11_BUFFER_DESC(sizeof(u16) * 256, D3D11_BIND_SHADER_RESOURCE);
  HRESULT hr = D3D::device->CreateBuffer(&lutBd, nullptr, &palette_buf);
  CHECK(SUCCEEDED(hr), "create palette decoder lut buffer");
  D3D::SetDebugObjectName(palette_buf, "texture decoder lut buffer");
  // TODO: C14X2 format.
  auto outlutUavDesc =
      CD3D11_SHADER_RESOURCE_VIEW_DESC(palette_buf, DXGI_FORMAT_R16_UINT, 0, 256, 0);
  hr = D3D::device->CreateShaderResourceView(palette_buf, &outlutUavDesc, &palette_buf_srv);
  CHECK(SUCCEEDED(hr), "create palette decoder lut srv");
  D3D::SetDebugObjectName(palette_buf_srv, "texture decoder lut srv");
  const D3D11_BUFFER_DESC cbdesc =
      CD3D11_BUFFER_DESC(16, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DEFAULT);
  hr = D3D::device->CreateBuffer(&cbdesc, nullptr, &palette_uniform);
  CHECK(SUCCEEDED(hr), "Create palette decoder constant buffer");
  D3D::SetDebugObjectName((ID3D11DeviceChild*)palette_uniform,
                          "a constant buffer used in TextureCache::CopyRenderTargetToTexture");
}

TextureCache::~TextureCache()
{
  for (unsigned int k = 0; k < MAX_COPY_BUFFERS; ++k)
    SAFE_RELEASE(s_efbcopycbuf[k]);

  g_encoder->Shutdown();
  g_encoder.reset();

  SAFE_RELEASE(palette_buf);
  SAFE_RELEASE(palette_buf_srv);
  SAFE_RELEASE(palette_uniform);
  for (ID3D11PixelShader*& shader : palette_pixel_shader)
    SAFE_RELEASE(shader);
}

void TextureCache::CopyEFBToCacheEntry(TCacheEntry* entry, bool is_depth_copy,
                                       const EFBRectangle& src_rect, bool scale_by_half,
                                       unsigned int cbuf_id, const float* colmat)
{
  auto* destination_texture = static_cast<DXTexture*>(entry->texture.get());

  // When copying at half size, in multisampled mode, resolve the color/depth buffer first.
  // This is because multisampled texture reads go through Load, not Sample, and the linear
  // filter is ignored.
  bool multisampled = (g_ActiveConfig.iMultisamples > 1);
  ID3D11ShaderResourceView* efbTexSRV = is_depth_copy ?
                                            FramebufferManager::GetEFBDepthTexture()->GetSRV() :
                                            FramebufferManager::GetEFBColorTexture()->GetSRV();
  if (multisampled && scale_by_half)
  {
    multisampled = false;
    efbTexSRV = is_depth_copy ? FramebufferManager::GetResolvedEFBDepthTexture()->GetSRV() :
                                FramebufferManager::GetResolvedEFBColorTexture()->GetSRV();
  }

  g_renderer->ResetAPIState();

  // stretch picture with increased internal resolution
  const D3D11_VIEWPORT vp =
      CD3D11_VIEWPORT(0.f, 0.f, static_cast<float>(destination_texture->GetConfig().width),
                      static_cast<float>(destination_texture->GetConfig().height));
  D3D::context->RSSetViewports(1, &vp);

  // set transformation
  if (nullptr == s_efbcopycbuf[cbuf_id])
  {
    const D3D11_BUFFER_DESC cbdesc =
        CD3D11_BUFFER_DESC(28 * sizeof(float), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DEFAULT);
    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = colmat;
    HRESULT hr = D3D::device->CreateBuffer(&cbdesc, &data, &s_efbcopycbuf[cbuf_id]);
    CHECK(SUCCEEDED(hr), "Create efb copy constant buffer %d", cbuf_id);
    D3D::SetDebugObjectName((ID3D11DeviceChild*)s_efbcopycbuf[cbuf_id],
                            "a constant buffer used in TextureCache::CopyRenderTargetToTexture");
  }
  D3D::stateman->SetPixelConstants(s_efbcopycbuf[cbuf_id]);

  const TargetRectangle targetSource = g_renderer->ConvertEFBRectangle(src_rect);
  // TODO: try targetSource.asRECT();
  const D3D11_RECT sourcerect =
      CD3D11_RECT(targetSource.left, targetSource.top, targetSource.right, targetSource.bottom);

  // Use linear filtering if (bScaleByHalf), use point filtering otherwise
  if (scale_by_half)
    D3D::SetLinearCopySampler();
  else
    D3D::SetPointCopySampler();

  // Make sure we don't draw with the texture set as both a source and target.
  // (This can happen because we don't unbind textures when we free them.)
  D3D::stateman->UnsetTexture(destination_texture->GetRawTexIdentifier()->GetSRV());
  D3D::stateman->Apply();

  D3D::context->OMSetRenderTargets(1, &destination_texture->GetRawTexIdentifier()->GetRTV(),
                                   nullptr);

  // Create texture copy
  D3D::drawShadedTexQuad(
      efbTexSRV, &sourcerect, g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight(),
      is_depth_copy ? PixelShaderCache::GetDepthMatrixProgram(multisampled) :
                      PixelShaderCache::GetColorMatrixProgram(multisampled),
      VertexShaderCache::GetSimpleVertexShader(), VertexShaderCache::GetSimpleInputLayout(),
      GeometryShaderCache::GetCopyGeometryShader());

  D3D::context->OMSetRenderTargets(1, &FramebufferManager::GetEFBColorTexture()->GetRTV(),
                                   FramebufferManager::GetEFBDepthTexture()->GetDSV());

  g_renderer->RestoreAPIState();
}
}
