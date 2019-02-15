// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DBlob.h"

#include "VideoCommon/AsyncShaderCompiler.h"
#include "VideoCommon/UberShaderVertex.h"
#include "VideoCommon/VertexShaderGen.h"

namespace DX11
{
class D3DVertexFormat;

class VertexShaderCache
{
public:
  static void Init();
  static void Shutdown();

  static ID3D11Buffer*& GetConstantBuffer();

  static ID3D11VertexShader* GetSimpleVertexShader();
  static ID3D11VertexShader* GetClearVertexShader();
  static ID3D11InputLayout* GetSimpleInputLayout();
  static ID3D11InputLayout* GetClearInputLayout();
};

}  // namespace DX11
