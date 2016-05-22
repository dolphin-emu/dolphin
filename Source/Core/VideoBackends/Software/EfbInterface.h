// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/PerfQueryBase.h"
#include "VideoCommon/VideoCommon.h"

namespace EfbInterface
{
	const int DEPTH_BUFFER_START = EFB_WIDTH * EFB_HEIGHT * 3;

	// xfb color format - packed so the compiler doesn't mess with alignment
#pragma pack(push,1)
	struct yuv422_packed
	{
		u8 Y;
		u8 UV;
	};
#pragma pack(pop)

	// But this struct is only used internally, so we could optimise alignment
	struct yuv444
	{
		u8 Y;
		s8 U;
		s8 V;
	};

	enum
	{
		ALP_C,
		BLU_C,
		GRN_C,
		RED_C
	};

	// color order is ABGR in order to emulate RGBA on little-endian hardware

	// does full blending of an incoming pixel
	void BlendTev(u16 x, u16 y, u8 *color);

	// compare z at location x,y
	// writes it if it passes
	// returns result of compare.
	bool ZCompare(u16 x, u16 y, u32 z);

	// sets the color and alpha
	void SetColor(u16 x, u16 y, u8 *color);
	void SetDepth(u16 x, u16 y, u32 depth);

	void GetColor(u16 x, u16 y, u8 *color);
	void GetColorYUV(u16 x, u16 y, yuv444 *color);
	u32 GetDepth(u16 x, u16 y);

	u8* GetPixelPointer(u16 x, u16 y, bool depth);

	void CopyToXFB(yuv422_packed* xfb_in_ram, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc, float Gamma);
	void BypassXFB(u8* texture, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc, float Gamma);

	extern u32 perf_values[PQ_NUM_MEMBERS];
	inline void IncPerfCounterQuadCount(PerfQueryType type)
	{
		// NOTE: hardware doesn't process individual pixels but quads instead.
		// Current software renderer architecture works on pixels though, so
		// we have this "quad" hack here to only increment the registers on
		// every fourth rendered pixel
		static u32 quad[PQ_NUM_MEMBERS];
		if (++quad[type] != 3)
			return;
		quad[type] = 0;
		++perf_values[type];
	}
}
