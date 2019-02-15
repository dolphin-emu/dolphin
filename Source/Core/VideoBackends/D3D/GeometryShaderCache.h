// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d11.h>
#include <map>

#include "VideoCommon/GeometryShaderGen.h"

namespace DX11
{
class GeometryShaderCache
{
public:
  static void Init();
  static void Shutdown();

  static ID3D11GeometryShader* GetClearGeometryShader();
  static ID3D11GeometryShader* GetCopyGeometryShader();

  static ID3D11Buffer*& GetConstantBuffer();
};

}  // namespace DX11
