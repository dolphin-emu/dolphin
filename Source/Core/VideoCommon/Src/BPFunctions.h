// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


// ------------------------------------------
// Video backend must define these functions
// ------------------------------------------

#ifndef _BPFUNCTIONS_H
#define _BPFUNCTIONS_H

#include "BPMemory.h"
#include "VideoCommon.h"

namespace BPFunctions
{

enum
{
	CONFIG_ISWII = 0,
	CONFIG_DISABLEFOG,
	CONFIG_SHOWEFBREGIONS
};

void FlushPipeline();
void SetGenerationMode();
void SetScissor();
void SetLineWidth();
void SetDepthMode();
void SetBlendMode();
void SetDitherMode();
void SetLogicOpMode();
void SetColorMask();
void CopyEFB(u32 dstAddr, unsigned int dstFormat, unsigned int srcFormat,
	const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf);
void ClearScreen(const EFBRectangle &rc);
void OnPixelFormatChange();
u8 *GetPointer(const u32 &address);
bool GetConfig(const int &type);
void SetTextureMode(const BPCmd &bp);
void SetInterlacingMode(const BPCmd &bp);
};

#endif // _BPFUNCTIONS_H
