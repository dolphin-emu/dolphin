// Copyright (C) 2003-2009 Dolphin Project.

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


// ------------------------------------------
// The plugins has to define these functions
// ------------------------------------------

#ifndef _BPFUNCTIONS_H_
#define _BPFUNCTIONS_H_

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
void SetGenerationMode(const Bypass &bp);
void SetScissor(const Bypass &bp);
void SetLineWidth(const Bypass &bp);
void SetDepthMode(const Bypass &bp);
void SetBlendMode(const Bypass &bp);
void SetDitherMode(const Bypass &bp);
void SetLogicOpMode(const Bypass &bp);
void SetColorMask(const Bypass &bp);
float GetRendererTargetScaleX();
float GetRendererTargetScaleY();
void CopyEFB(const Bypass &bp, const TRectangle &rc, const u32 &address, const bool &fromZBuffer, const bool &isIntensityFmt, const u32 &copyfmt, const bool &scaleByHalf);
void RenderToXFB(const Bypass &bp, const TRectangle &multirc, const float &yScale, const float &xfbLines, u8* pXFB, const u32 &dstWidth, const u32 &dstHeight);
void ClearScreen(const Bypass &bp, const TRectangle &multirc);
void RestoreRenderState(const Bypass &bp);
u8 *GetPointer(const u32 &address);
bool GetConfig(const int &type);
void SetSamplerState(const Bypass &bp);
void SetInterlacingMode(const Bypass &bp);
};

#endif // _BPFUNCTIONS_H_