// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>

#include "Common/CommonTypes.h"
#include "VideoCommon/TextureConversionShader.h"
#include "VideoCommon/VideoCommon.h"

struct ID3D11Texture2D;
struct ID3D11RenderTargetView;
struct ID3D11Buffer;
struct ID3D11InputLayout;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11ClassLinkage;
struct ID3D11ClassInstance;
struct ID3D11BlendState;
struct ID3D11DepthStencilState;
struct ID3D11RasterizerState;
struct ID3D11SamplerState;

namespace DX11
{
class PSTextureEncoder final
{
public:
  PSTextureEncoder();

  void Init();
  void Shutdown();
  void Encode(u8* dst, const EFBCopyParams& params, u32 native_width, u32 bytes_per_row,
              u32 num_blocks_y, u32 memory_stride, const EFBRectangle& src_rect,
              bool scale_by_half);

private:
  ID3D11PixelShader* GetEncodingPixelShader(const EFBCopyParams& params);

  bool m_ready;

  ID3D11Texture2D* m_out;
  ID3D11RenderTargetView* m_outRTV;
  ID3D11Texture2D* m_outStage;
  ID3D11Buffer* m_encodeParams;
  std::map<EFBCopyParams, ID3D11PixelShader*> m_encoding_shaders;
};
}
