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


// ------------------------------------------
// Video plugin must define these functions
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
void SetGenerationMode(const BPCmd &bp);
void SetScissor(const BPCmd &bp);
void SetLineWidth(const BPCmd &bp);
void SetDepthMode(const BPCmd &bp);
void SetBlendMode(const BPCmd &bp);
void SetDitherMode(const BPCmd &bp);
void SetLogicOpMode(const BPCmd &bp);
void SetColorMask(const BPCmd &bp);
void CopyEFB(const BPCmd &bp, const EFBRectangle &rc, const u32 &address, const bool &fromZBuffer, const bool &isIntensityFmt, const u32 &copyfmt, const int &scaleByHalf);
void ClearScreen(const BPCmd &bp, const EFBRectangle &rc);
void RestoreRenderState(const BPCmd &bp);
u8 *GetPointer(const u32 &address);
bool GetConfig(const int &type);
void SetTextureMode(const BPCmd &bp);
void SetInterlacingMode(const BPCmd &bp);
};

#endif // _BPFUNCTIONS_H
