// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/D3D/PSTextureEncoder.h"

#include "Common/Logging/Log.h"
#include "Core/HW/Memmap.h"
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DShader.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/D3DUtil.h"
#include "VideoBackends/D3D/FramebufferManager.h"
#include "VideoBackends/D3D/Render.h"
#include "VideoBackends/D3D/TextureCache.h"
#include "VideoBackends/D3D/VertexShaderCache.h"

#include "VideoCommon/TextureConversionShader.h"
#include "VideoCommon/VideoCommon.h"

namespace DX11
{
struct EFBEncodeParams
{
  DWORD SrcLeft;
  DWORD SrcTop;
  DWORD DestWidth;
  DWORD ScaleFactor;
};

void PSTextureEncoder::Init()
{
  m_ready = false;

  HRESULT hr;

  // Create output texture RGBA format
  D3D11_TEXTURE2D_DESC t2dd = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_B8G8R8A8_UNORM, EFB_WIDTH * 4,
                                                    EFB_HEIGHT / 4, 1, 1, D3D11_BIND_RENDER_TARGET);
  hr = D3D::device->CreateTexture2D(&t2dd, nullptr, &m_out);
  CHECK(SUCCEEDED(hr), "create efb encode output texture");
  D3D::SetDebugObjectName(m_out.Get(), "efb encoder output texture");

  // Create output render target view
  D3D11_RENDER_TARGET_VIEW_DESC rtvd = CD3D11_RENDER_TARGET_VIEW_DESC(
      m_out.Get(), D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_B8G8R8A8_UNORM);
  hr = D3D::device->CreateRenderTargetView(m_out.Get(), &rtvd, &m_outRTV);
  CHECK(SUCCEEDED(hr), "create efb encode output render target view");
  D3D::SetDebugObjectName(m_outRTV.Get(), "efb encoder output rtv");

  // Create output staging buffer
  t2dd.Usage = D3D11_USAGE_STAGING;
  t2dd.BindFlags = 0;
  t2dd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  hr = D3D::device->CreateTexture2D(&t2dd, nullptr, &m_outStage);
  CHECK(SUCCEEDED(hr), "create efb encode output staging buffer");
  D3D::SetDebugObjectName(m_outStage.Get(), "efb encoder output staging buffer");

  // Create constant buffer for uploading data to shaders
  D3D11_BUFFER_DESC bd = CD3D11_BUFFER_DESC(sizeof(EFBEncodeParams), D3D11_BIND_CONSTANT_BUFFER);
  hr = D3D::device->CreateBuffer(&bd, nullptr, &m_encodeParams);
  CHECK(SUCCEEDED(hr), "create efb encode params buffer");
  D3D::SetDebugObjectName(m_encodeParams.Get(), "efb encoder params buffer");

  m_ready = true;
}

void PSTextureEncoder::Shutdown()
{
  m_ready = false;

  m_encoding_shaders.clear();

  m_encodeParams.Reset();
  m_outStage.Reset();
  m_outRTV.Reset();
  m_out.Reset();
}

void PSTextureEncoder::Encode(u8* dst, const EFBCopyParams& params, u32 native_width,
                              u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
                              const EFBRectangle& src_rect, bool scale_by_half)
{
  if (!m_ready)  // Make sure we initialized OK
    return;

  HRESULT hr;

  // Resolve MSAA targets before copying.
  // FIXME: Instead of resolving EFB, it would be better to pick out a
  // single sample from each pixel. The game may break if it isn't
  // expecting the blurred edges around multisampled shapes.
  ID3D11ShaderResourceView* pEFB = params.depth ?
                                       FramebufferManager::GetResolvedEFBDepthTexture().GetSRV() :
                                       FramebufferManager::GetResolvedEFBColorTexture().GetSRV();

  // Reset API
  g_renderer->ResetAPIState();

  // Set up all the state for EFB encoding
  {
    const u32 words_per_row = bytes_per_row / sizeof(u32);

    D3D11_VIEWPORT vp = CD3D11_VIEWPORT(0.f, 0.f, FLOAT(words_per_row), FLOAT(num_blocks_y));
    D3D::context->RSSetViewports(1, &vp);

    constexpr EFBRectangle fullSrcRect(0, 0, EFB_WIDTH, EFB_HEIGHT);
    TargetRectangle targetRect = g_renderer->ConvertEFBRectangle(fullSrcRect);

    D3D::SetRenderTarget(m_outRTV.Get());

    EFBEncodeParams encode_params;
    encode_params.SrcLeft = src_rect.left;
    encode_params.SrcTop = src_rect.top;
    encode_params.DestWidth = native_width;
    encode_params.ScaleFactor = scale_by_half ? 2 : 1;
    D3D::context->UpdateSubresource(m_encodeParams.Get(), 0, nullptr, &encode_params, 0, 0);
    D3D::stateman->SetPixelConstants(m_encodeParams.Get());

    // We also linear filtering for both box filtering and downsampling higher resolutions to 1x
    // TODO: This only produces perfect downsampling for 1.5x and 2x IR, other resolution will
    //       need more complex down filtering to average all pixels and produce the correct result.
    // Also, box filtering won't be correct for anything other than 1x IR
    if (scale_by_half || g_ActiveConfig.iEFBScale != SCALE_1X)
      D3D::SetLinearCopySampler();
    else
      D3D::SetPointCopySampler();

    D3D::drawShadedTexQuad(pEFB, targetRect.AsRECT(), g_renderer->GetTargetWidth(),
                           g_renderer->GetTargetHeight(), GetEncodingPixelShader(params),
                           VertexShaderCache::GetSimpleVertexShader(),
                           VertexShaderCache::GetSimpleInputLayout());

    // Copy to staging buffer
    D3D11_BOX srcBox = CD3D11_BOX(0, 0, 0, words_per_row, num_blocks_y, 1);
    D3D::context->CopySubresourceRegion(m_outStage.Get(), 0, 0, 0, 0, m_out.Get(), 0, &srcBox);

    // Transfer staging buffer to GameCube/Wii RAM
    D3D11_MAPPED_SUBRESOURCE map = {0};
    hr = D3D::context->Map(m_outStage.Get(), 0, D3D11_MAP_READ, 0, &map);
    CHECK(SUCCEEDED(hr), "map staging buffer (0x%x)", hr);

    u8* src = (u8*)map.pData;
    u32 readStride = std::min(bytes_per_row, map.RowPitch);
    for (unsigned int y = 0; y < num_blocks_y; ++y)
    {
      memcpy(dst, src, readStride);
      dst += memory_stride;
      src += map.RowPitch;
    }

    D3D::context->Unmap(m_outStage.Get(), 0);
  }

  // Restore API
  g_renderer->RestoreAPIState();
  D3D::SetRenderTarget(FramebufferManager::GetEFBColorTexture().GetRTV(),
                       FramebufferManager::GetEFBDepthTexture().GetDSV());
}

ID3D11PixelShader* PSTextureEncoder::GetEncodingPixelShader(const EFBCopyParams& params)
{
  auto iter = m_encoding_shaders.find(params);
  if (iter != m_encoding_shaders.end())
    return iter->second.Get();

  D3DBlob* bytecode = nullptr;
  const char* shader = TextureConversionShader::GenerateEncodingShader(params, APIType::D3D);
  if (!D3D::CompilePixelShader(shader, &bytecode))
  {
    PanicAlert("Failed to compile texture encoding shader.");
    m_encoding_shaders[params] = nullptr;
    return nullptr;
  }

  ComPtr<ID3D11PixelShader> newShader;
  HRESULT hr =
      D3D::device->CreatePixelShader(bytecode->Data(), bytecode->Size(), nullptr, &newShader);
  CHECK(SUCCEEDED(hr), "create efb encoder pixel shader");

  m_encoding_shaders.emplace(params, newShader);
  return newShader.Get();
}
}
