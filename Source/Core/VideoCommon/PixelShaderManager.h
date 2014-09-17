// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/ConstantManager.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/XFMemory.h"

class PointerWrap;



// The non-API dependent parts.
class PixelShaderManager
{
public:
	static void Init();
	static void Dirty();
	static void Shutdown();
	static void DoState(PointerWrap &p);

	static void SetConstants(); // sets pixel shader constants

	// constant management
	// Some of these functions grab the constant values from global state,
	// so make sure to call them after memory is committed
	static void SetTevColor(int index, int component, s32 value);
	static void SetTevKonstColor(int index, int component, s32 value);
	static void SetAlpha();
	static void SetDestAlpha();
	static void SetTexDims(int texmapid, u32 width, u32 height, u32 wraps, u32 wrapt);
	static void SetZTextureBias();
	static void SetViewportChanged();
	static void SetIndMatrixChanged(int matrixidx);
	static void SetTevKSelChanged(int id);
	static void SetZTextureTypeChanged();
	static void SetIndTexScaleChanged(bool high);
	static void SetTexCoordChanged(u8 texmapid);
	static void SetFogColorChanged();
	static void SetFogParamChanged();
	static void SetFogRangeAdjustChanged();

	static PixelShaderConstants constants;
	static bool dirty;

	static bool s_bFogRangeAdjustChanged;
	static bool s_bViewPortChanged;

	// These colors aren't available from global BP state,
	// hence we keep a copy of them around.
	static std::array<int4,4> s_tev_color;
	static std::array<int4,4> s_tev_konst_color;
};
