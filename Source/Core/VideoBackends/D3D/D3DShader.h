// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DBlob.h"

struct ID3D11PixelShader;
struct ID3D11VertexShader;

namespace DX11
{

namespace D3D
{
	ID3D11VertexShader* CreateVertexShaderFromByteCode(const void* bytecode, unsigned int len);
	ID3D11GeometryShader* CreateGeometryShaderFromByteCode(const void* bytecode, unsigned int len);
	ID3D11PixelShader* CreatePixelShaderFromByteCode(const void* bytecode, unsigned int len);

	// The returned bytecode buffers should be Release()d.
	bool CompileVertexShader(const std::string& code, D3DBlob** blob);
	bool CompileGeometryShader(const std::string& code, D3DBlob** blob, const D3D_SHADER_MACRO* pDefines = nullptr);
	bool CompilePixelShader(const std::string& code, D3DBlob** blob, const D3D_SHADER_MACRO* pDefines = nullptr);

	// Utility functions
	ID3D11VertexShader* CompileAndCreateVertexShader(const std::string& code);
	ID3D11GeometryShader* CompileAndCreateGeometryShader(const std::string& code, const D3D_SHADER_MACRO* pDefines = nullptr);
	ID3D11PixelShader* CompileAndCreatePixelShader(const std::string& code);

	inline ID3D11VertexShader* CreateVertexShaderFromByteCode(D3DBlob* bytecode)
	{ return CreateVertexShaderFromByteCode(bytecode->Data(), bytecode->Size()); }
	inline ID3D11GeometryShader* CreateGeometryShaderFromByteCode(D3DBlob* bytecode)
	{ return CreateGeometryShaderFromByteCode(bytecode->Data(), bytecode->Size()); }
	inline ID3D11PixelShader* CreatePixelShaderFromByteCode(D3DBlob* bytecode)
	{ return CreatePixelShaderFromByteCode(bytecode->Data(), bytecode->Size()); }

	inline ID3D11VertexShader* CompileAndCreateVertexShader(D3DBlob* code)
	{
		return CompileAndCreateVertexShader((const char*)code->Data());
	}

	inline ID3D11GeometryShader* CompileAndCreateGeometryShader(D3DBlob* code, const D3D_SHADER_MACRO* pDefines = nullptr)
	{
		return CompileAndCreateGeometryShader((const char*)code->Data(), pDefines);
	}

	inline ID3D11PixelShader* CompileAndCreatePixelShader(D3DBlob* code)
	{
		return CompileAndCreatePixelShader((const char*)code->Data());
	}
}

}  // namespace DX11
