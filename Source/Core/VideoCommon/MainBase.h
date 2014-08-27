#pragma once

#include "Common/CommonTypes.h"
#include "Common/Flag.h"

extern bool s_BackendInitialized;
extern Common::Flag s_swapRequested;

void VideoFifo_CheckEFBAccess();
void VideoFifo_CheckSwapRequestAt(u32 xfbAddr, u32 fbWidth, u32 fbHeight);
