// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <d3d11.h>

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/DXTextureRaw.h"

namespace DX11
{
DXTextureRaw::DXTextureRaw(const u8* data, u32 stride, u32 width, u32 height,
                           ID3D11Texture2D* texture)
    : AbstractRawTexture(data, stride, width, height), m_stagingTexture(texture)
{
}

DXTextureRaw::~DXTextureRaw()
{
  D3D::context->Unmap(m_stagingTexture, 0);
  m_stagingTexture->Release();
}

}  // namespace DX11
