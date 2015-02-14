// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Common/Flag.h"

extern bool s_BackendInitialized;
extern Common::Flag s_swapRequested;

void VideoFifo_CheckEFBAccess();
void VideoFifo_CheckSwapRequestAt(u32 xfbAddr, u32 fbWidth, u32 fbHeight);
