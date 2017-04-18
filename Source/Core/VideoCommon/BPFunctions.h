// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// ------------------------------------------
// Video backend must define these functions
// ------------------------------------------

#pragma once

#include "VideoCommon/VideoCommon.h"

struct BPCmd;

namespace BPFunctions
{
void FlushPipeline();
void SetGenerationMode();
void SetScissor();
void SetDepthMode();
void SetBlendMode();
void SetLogicOpMode();
void SetColorMask();
void ClearScreen(const EFBRectangle& rc);
void OnPixelFormatChange();
void SetInterlacingMode(const BPCmd& bp);
}
