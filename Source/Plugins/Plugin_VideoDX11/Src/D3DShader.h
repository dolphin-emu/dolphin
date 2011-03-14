// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#pragma once

#include "D3DBase.h"
#include "D3DBlob.h"

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
	bool CompileVertexShader(const char* code, unsigned int len,
		D3DBlob** blob);
	bool CompileGeometryShader(const char* code, unsigned int len,
		D3DBlob** blob, const D3D_SHADER_MACRO* pDefines = NULL);
	bool CompilePixelShader(const char* code, unsigned int len,
		D3DBlob** blob, const D3D_SHADER_MACRO* pDefines = NULL);

	// Utility functions
	ID3D11VertexShader* CompileAndCreateVertexShader(const char* code,
		unsigned int len);
	ID3D11GeometryShader* CompileAndCreateGeometryShader(const char* code,
		unsigned int len, const D3D_SHADER_MACRO* pDefines = NULL);
	ID3D11PixelShader* CompileAndCreatePixelShader(const char* code,
		unsigned int len);

	inline ID3D11VertexShader* CreateVertexShaderFromByteCode(D3DBlob* bytecode)
	{ return CreateVertexShaderFromByteCode(bytecode->Data(), bytecode->Size()); }
	inline ID3D11GeometryShader* CreateGeometryShaderFromByteCode(D3DBlob* bytecode)
	{ return CreateGeometryShaderFromByteCode(bytecode->Data(), bytecode->Size()); }
	inline ID3D11PixelShader* CreatePixelShaderFromByteCode(D3DBlob* bytecode)
	{ return CreatePixelShaderFromByteCode(bytecode->Data(), bytecode->Size()); }

	inline ID3D11VertexShader* CompileAndCreateVertexShader(D3DBlob* code)
	{ return CompileAndCreateVertexShader((const char*)code->Data(), code->Size()); }
	inline ID3D11GeometryShader* CompileAndCreateGeometryShader(D3DBlob* code, const D3D_SHADER_MACRO* pDefines = NULL)
	{ return CompileAndCreateGeometryShader((const char*)code->Data(), code->Size(), pDefines); }
	inline ID3D11PixelShader* CompileAndCreatePixelShader(D3DBlob* code)
	{ return CompileAndCreatePixelShader((const char*)code->Data(), code->Size()); }
}

}  // namespace DX11