// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.


// ------------------------------------------
// Video backend must define these functions
// ------------------------------------------

#pragma once

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/VideoCommon.h"

namespace BPFunctions
{

void FlushPipeline();
void SetGenerationMode();
void SetScissor();
void SetDepthMode();
void SetBlendMode();
void SetDitherMode();
void SetLogicOpMode();
void SetColorMask();
void CopyEFB(u32 dstAddr, const EFBRectangle& srcRect,
             unsigned int dstFormat, PEControl::PixelFormat srcFormat,
             bool isIntensity, bool scaleByHalf);
void ClearScreen(const EFBRectangle &rc);
void OnPixelFormatChange();
void SetInterlacingMode(const BPCmd &bp);
}
