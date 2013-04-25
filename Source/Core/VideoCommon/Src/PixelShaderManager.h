// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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

	static void SetConstants(u32 components); // sets pixel shader constants

	// constant management, should be called after memory is committed
	static void SetColorChanged(int type, int index, bool high);
	static void SetAlpha(const AlphaTest& alpha);
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
