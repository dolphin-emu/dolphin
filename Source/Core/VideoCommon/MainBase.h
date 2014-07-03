#pragma once

#include "Common/CommonTypes.h"

extern bool s_BackendInitialized;
extern u32 s_efbAccessRequested;
extern volatile u32 s_FifoShuttingDown;
extern volatile u32 s_swapRequested;

void VideoFifo_CheckEFBAccess();
void VideoFifo_CheckSwapRequestAt(u32 xfbAddr, u32 fbWidth, u32 fbHeight);
