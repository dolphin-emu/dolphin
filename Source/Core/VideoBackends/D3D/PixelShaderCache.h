// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d11.h>
#include <map>

#include "VideoCommon/AsyncShaderCompiler.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/UberShaderPixel.h"

namespace DX11
{
class D3DBlob;

class PixelShaderCache
{
public:
  static void Init();
  static void Shutdown();

  static ID3D11Buffer* GetConstantBuffer();

  static ID3D11PixelShader* GetColorCopyProgram(bool multisampled);
  static ID3D11PixelShader* GetClearProgram();
  static ID3D11PixelShader* GetAnaglyphProgram();
  static ID3D11PixelShader* GetDepthResolveProgram();
  static ID3D11PixelShader* ReinterpRGBA6ToRGB8(bool multisampled);
  static ID3D11PixelShader* ReinterpRGB8ToRGBA6(bool multisampled);

  static void InvalidateMSAAShaders();
};

}  // namespace DX11
