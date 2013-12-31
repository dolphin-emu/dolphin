// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _EFB_INTERFACE_H_
#define _EFB_INTERFACE_H_

#include "VideoCommon.h"

namespace EfbInterface
{
	const int DEPTH_BUFFER_START = EFB_WIDTH * EFB_HEIGHT * 3;

	enum { ALP_C, BLU_C, GRN_C, RED_C };

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
	u32 GetDepth(u16 x, u16 y);

	u8* GetPixelPointer(u16 x, u16 y, bool depth);

	void UpdateColorTexture();
	extern u8 efbColorTexture[EFB_WIDTH*EFB_HEIGHT*4]; // RGBA format
	void DoState(PointerWrap &p);
}

#endif
