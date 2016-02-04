// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "VideoBackends/D3D12/D3DBase.h"

class D3DBlob;

namespace DX12
{

namespace D3D
{

// The returned bytecode buffers should be Release()d.
bool CompileVertexShader(const std::string& code, ID3DBlob** blob);
bool CompileGeometryShader(const std::string& code, ID3DBlob** blob, const D3D_SHADER_MACRO* defines = nullptr);
bool CompilePixelShader(const std::string& code, ID3DBlob** blob, const D3D_SHADER_MACRO* defines = nullptr);

}

}  // namespace DX12
