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

namespace D3D
{
	ID3D11VertexShader* CreateVertexShaderFromByteCode(void* bytecode, unsigned int len);
	ID3D11PixelShader* CreatePixelShaderFromByteCode(void* bytecode, unsigned int len);

	// The returned bytecode buffers should be Release()d.
	bool CompileVertexShader(const char* code, unsigned int len, ID3D10Blob** blob);
	bool CompilePixelShader(const char* code, unsigned int len, ID3D10Blob** blob);

	// Utility functions
	ID3D11VertexShader* CompileAndCreateVertexShader(const char* code, unsigned int len);
	ID3D11PixelShader* CompileAndCreatePixelShader(const char* code, unsigned int len);

	inline ID3D11VertexShader* CreateVertexShaderFromByteCode(ID3D10Blob* bytecode) { return CreateVertexShaderFromByteCode(bytecode->GetBufferPointer(), bytecode->GetBufferSize()); }
	inline ID3D11PixelShader* CreatePixelShaderFromByteCode(ID3D10Blob* bytecode) { return CreatePixelShaderFromByteCode(bytecode->GetBufferPointer(), bytecode->GetBufferSize()); }
	inline ID3D11VertexShader* CompileAndCreateVertexShader(ID3D10Blob* code) { return CompileAndCreateVertexShader((const char*)code->GetBufferPointer(), code->GetBufferSize()); }
	inline ID3D11PixelShader* CompileAndCreatePixelShader(ID3D10Blob* code) { return CompileAndCreatePixelShader((const char*)code->GetBufferPointer(), code->GetBufferSize()); }
}