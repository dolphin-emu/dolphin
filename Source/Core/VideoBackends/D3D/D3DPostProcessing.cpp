// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/D3D/D3DPostProcessing.h"
#include "VideoBackends/D3D/D3DUtil.h"
#include "VideoBackends/D3D/PixelShaderCache.h"
#include "VideoBackends/D3D/VertexShaderCache.h"

namespace DX11
{

D3DPostProcessing::D3DPostProcessing()
{
}

D3DPostProcessing::~D3DPostProcessing()
{
}

void D3DPostProcessing::PostProcessTexture(ID3D11ShaderResourceView* srv, RECT* rect, u32 src_width,
                                           u32 src_height, D3D11_VIEWPORT viewport, u32 slice)
{
  D3D::drawShadedTexQuad(srv, rect, src_width, src_height,
                         PixelShaderCache::GetColorCopyProgram(false),
                         VertexShaderCache::GetSimpleVertexShader(),
                         VertexShaderCache::GetSimpleInputLayout(), nullptr, slice);
}

}  // namespace DX11
