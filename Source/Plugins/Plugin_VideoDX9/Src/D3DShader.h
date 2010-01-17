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
	LPDIRECT3DVERTEXSHADER9 CreateVertexShaderFromByteCode(const u8 *bytecode, int len);
	LPDIRECT3DPIXELSHADER9 CreatePixelShaderFromByteCode(const u8 *bytecode, int len);

	// The returned bytecode buffers should be delete[]-d.
	bool CompileVertexShader(const char *code, int len, u8 **bytecode, int *bytecodelen);
	bool CompilePixelShader(const char *code, int len, u8 **bytecode, int *bytecodelen);

	// Utility functions
	LPDIRECT3DVERTEXSHADER9 CompileAndCreateVertexShader(const char *code, int len);
	LPDIRECT3DPIXELSHADER9 CompileAndCreatePixelShader(const char *code, int len);
}