// Copyright 2009 Dolphin Emulator Project
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
void ClearScreen(const EFBRectangle &rc, bool new_frame_just_rendered);
void OnPixelFormatChange();
void SetInterlacingMode(const BPCmd &bp);
}
