// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _EFB_INTERFACE_H_
#define _EFB_INTERFACE_H_

#include "VideoCommon.h"

namespace EfbInterface
{
	const int DEPTH_BUFFER_START = EFB_WIDTH * EFB_HEIGHT * 3;

	// color order is ABGR in order to emulate RGBA on little-endian hardware
	enum { ALP_C, BLU_C, GRN_C, RED_C };

	// packed so the compiler doesn't mess with alignment
	typedef struct __attribute__ ((packed)) {
		u8 Y;
		u8 UV;
	} yuv422_packed;

	// But this one is only used internally, so we can let the compiler pack it however it likes.
	typedef struct __attribute__ ((aligned (4))){
		u8 Y;
		s8 U;
		s8 V;
	} yuv444;

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

	void DoState(PointerWrap &p);
}

#endif
