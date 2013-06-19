// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "D3DBase.h"

namespace DX9
{

namespace D3D
{
	LPDIRECT3DVERTEXSHADER9 CreateVertexShaderFromByteCode(const u8 *bytecode, int len);
	LPDIRECT3DPIXELSHADER9 CreatePixelShaderFromByteCode(const u8 *bytecode, int len);

	// The returned bytecode buffers should be delete[]-d.
	bool CompileVertexShader(const char *code, int len, u8 **bytecode, int *bytecodelen);
	bool CompilePixelShader(const char *code, int len, u8 **bytecode, int *bytecodelen);

	// Utility functions
	LPDIRECT3DVERTEXSHADER9 CompileAndCreateVertexShader(const char *code, int len);
	LPDIRECT3DPIXELSHADER9 CompileAndCreatePixelShader(const char *code, unsigned int len);
}

}  // namespace DX9