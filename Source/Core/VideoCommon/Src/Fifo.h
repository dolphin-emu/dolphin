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

#ifndef _FIFO_H
#define _FIFO_H

#include "pluginspecs_video.h"
#include "Common.h"

class PointerWrap;

#define FIFO_SIZE (1024*1024)

extern volatile bool g_bSkipCurrentFrame;

void Fifo_Init();
void Fifo_Shutdown();

void Fifo_SendFifoData(u8* _uData, u32 len);

// These two are for dual core mode only.
void Fifo_EnterLoop(const SVideoInitialize &video_initialize);
void Fifo_ExitLoop();
void Fifo_ExitLoopNonBlocking();
void Fifo_RunLoop(bool run);

void Fifo_DoState(PointerWrap &f);

void Fifo_SetRendering(bool bEnabled);

// Implemented by the Video Plugin
void VideoFifo_CheckSwapRequest();
void VideoFifo_CheckSwapRequestAt(u32 xfbAddr, u32 fbWidth, u32 fbHeight);
void VideoFifo_CheckEFBAccess();

#endif // _FIFO_H
