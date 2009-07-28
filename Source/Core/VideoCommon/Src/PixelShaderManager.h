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

#ifndef _PIXELSHADERMANAGER_H
#define _PIXELSHADERMANAGER_H

#include "BPMemory.h"
#include "PixelShaderGen.h"

void SetPSConstant4f(int const_number, float f1, float f2, float f3, float f4);
void SetPSConstant4fv(int const_number, const float *f);

// The non-API dependent parts.
class PixelShaderManager
{
	static void SetPSTextureDims(int texid);
public:
	static void Init();
	static void Shutdown();

	static void SetConstants(); // sets pixel shader constants

	// constant management, should be called after memory is committed
	static void SetColorChanged(int type, int index);
	static void SetAlpha(const AlphaFunc& alpha);
	static void SetDestAlpha(const ConstantAlpha& alpha);
	static void SetTexDims(int texmapid, u32 width, u32 height, u32 wraps, u32 wrapt);
	static void SetCustomTexScale(int texmapid, float x, float y);
	static void SetZTextureBias(u32 bias);
	static void SetViewport(float* viewport);
	static void SetIndMatrixChanged(int matrixidx);
	static void SetTevKSelChanged(int id);
	static void SetZTextureTypeChanged();
	static void SetIndTexScaleChanged(u8 stagemask);
	static void SetTexturesUsed(u32 nonpow2tex);
	static void SetTexCoordChanged(u8 texmapid);
    static void SetFogColorChanged();
    static void SetFogParamChanged();
	static void SetColorMatrix(const float* pmatrix, const float* pfConstAdd);
	static u32 GetTextureMask();
};


#endif // _PIXELSHADERMANAGER_H
