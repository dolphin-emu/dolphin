// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// ------------------------------------------
// Video backend must define these functions
// ------------------------------------------

#pragma once

#include "Common/MathUtil.h"

struct BPCmd;

namespace BPFunctions
{
void FlushPipeline();
void SetGenerationMode();
void SetScissor();
void SetViewport();
void SetDepthMode();
void SetBlendMode();
void ClearScreen(const MathUtil::Rectangle<int>& rc);
void OnPixelFormatChange();
void SetInterlacingMode(const BPCmd& bp);
}  // namespace BPFunctions
