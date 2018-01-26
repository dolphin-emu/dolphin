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
void ClearScreen(const EFBRectangle& rc, bool new_frame_just_rendered);
void OnPixelFormatChange();
void SetInterlacingMode(const BPCmd& bp);
}
