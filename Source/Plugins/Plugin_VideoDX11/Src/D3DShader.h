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

namespace DX11
{

namespace D3D
{

// returns bytecode
SharedPtr<ID3D10Blob> CompileVertexShader(const char* code, unsigned int len);
SharedPtr<ID3D10Blob> CompileGeometryShader(const char* code, unsigned int len,
	const D3D_SHADER_MACRO* pDefines = NULL);
SharedPtr<ID3D10Blob> CompilePixelShader(const char* code, unsigned int len,
	const D3D_SHADER_MACRO* pDefines = NULL);

SharedPtr<ID3D11VertexShader> CreateVertexShaderFromByteCode(const void* bytecode, unsigned int len);
SharedPtr<ID3D11GeometryShader> CreateGeometryShaderFromByteCode(const void* bytecode, unsigned int len);
SharedPtr<ID3D11PixelShader> CreatePixelShaderFromByteCode(const void* bytecode, unsigned int len);

inline SharedPtr<ID3D11VertexShader> CreateVertexShaderFromByteCode(SharedPtr<ID3D10Blob> bytecode)
{
	return CreateVertexShaderFromByteCode(bytecode->GetBufferPointer(), (unsigned int)bytecode->GetBufferSize());
}

inline SharedPtr<ID3D11GeometryShader> CreateGeometryShaderFromByteCode(SharedPtr<ID3D10Blob> bytecode)
{
	return CreateGeometryShaderFromByteCode(bytecode->GetBufferPointer(), (unsigned int)bytecode->GetBufferSize());
}

inline SharedPtr<ID3D11PixelShader> CreatePixelShaderFromByteCode(SharedPtr<ID3D10Blob> bytecode)
{
	return CreatePixelShaderFromByteCode(bytecode->GetBufferPointer(), (unsigned int)bytecode->GetBufferSize());
}

// Utility functions, optionally return the bytecode if "bytecode" is non-null
SharedPtr<ID3D11VertexShader> CompileAndCreateVertexShader(const char* code, unsigned int len,
	SharedPtr<ID3D10Blob>* bytecode = nullptr);
SharedPtr<ID3D11GeometryShader> CompileAndCreateGeometryShader(const char* code, unsigned int len,
	const D3D_SHADER_MACRO* pDefines = nullptr, SharedPtr<ID3D10Blob>* bytecode = nullptr);
SharedPtr<ID3D11PixelShader> CompileAndCreatePixelShader(const char* code, unsigned int len,
	const D3D_SHADER_MACRO* pDefines = nullptr, SharedPtr<ID3D10Blob>* bytecode = nullptr);

}

}  // namespace DX11