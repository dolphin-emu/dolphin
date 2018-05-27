// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d11.h>
#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/VideoCommon.h"

namespace DX11
{
class D3DPostProcessing : public PostProcessingShaderImplementation
{
public:
  D3DPostProcessing();
  ~D3DPostProcessing();

  void PostProcessTexture(ID3D11ShaderResourceView* srv, RECT* rect, u32 src_width, u32 src_height,
                          D3D11_VIEWPORT viewport, u32 slice);

private:
};

}  // namespace DX11
