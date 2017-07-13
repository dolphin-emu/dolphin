// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "VideoBackends/D3D/D3DBase.h"

struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11PixelShader;
struct ID3D11SamplerState;

namespace DX11
{
class Television
{
public:
  void Init();
  void Shutdown();

  // Submit video data to be drawn. This will change the current state of the
  // TV. xfbAddr points to YUYV data stored in GameCube/Wii RAM, but the XFB
  // may be virtualized when rendering so the RAM may not actually be read.
  void Submit(u32 xfbAddr, u32 stride, u32 width, u32 height);

  // Render the current state of the TV.
  void Render();

private:
  // Properties of last Submit call
  u32 m_curAddr;
  u32 m_curWidth;
  u32 m_curHeight;

  // Used for real XFB mode

  ComPtr<ID3D11Texture2D> m_yuyvTexture;
  ComPtr<ID3D11ShaderResourceView> m_yuyvTextureSRV;
  ComPtr<ID3D11PixelShader> m_pShader;
  ComPtr<ID3D11SamplerState> m_samplerState;
};
}
