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
#include "XFMemory.h"
#include "PixelShaderGen.h"

class PointerWrap;

// The non-API dependent parts.
class PixelShaderManager
{
	static void SetPSTextureDims(int texid);
public:
	static void Init();
	static void Dirty();
	static void Shutdown();
	static void DoState(PointerWrap &p);

	static void SetConstants(); // sets pixel shader constants

	// constant management, should be called after memory is committed
	static void SetColorChanged(int type, int index, bool high);
	static void SetAlpha(const AlphaFunc& alpha);
	static void SetDestAlpha(const ConstantAlpha& alpha);
	static void SetTexDims(int texmapid, u32 width, u32 height, u32 wraps, u32 wrapt);
	static void SetZTextureBias(u32 bias);
	static void SetViewportChanged();
	static void SetIndMatrixChanged(int matrixidx);
	static void SetTevKSelChanged(int id);
	static void SetZTextureTypeChanged();
	static void SetIndTexScaleChanged(u8 stagemask);
	static void SetTexCoordChanged(u8 texmapid);
	static void SetFogColorChanged();
	static void SetFogParamChanged();
	static void SetFogRangeAdjustChanged();
	static void SetColorMatrix(const float* pmatrix);
	static void InvalidateXFRange(int start, int end);
	static void SetMaterialColorChanged(int index);
	
};


#endif // _PIXELSHADERMANAGER_H
