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

#include "Common.h"
#include "VideoBackendBase.h"

class PointerWrap;

#define FIFO_SIZE (1024*1024)

extern volatile bool g_bSkipCurrentFrame;


void Fifo_Init();
void Fifo_Shutdown();

void Fifo_SendFifoData(u8* _uData, u32 len);

// These two are for dual core mode only.
void Fifo_EnterLoop();
void Fifo_ExitLoop();
void Fifo_RunLoop(bool run);
bool AtBreakpoint();
void Fifo_DoState(PointerWrap &f);
void ResetVideoBuffer();
void Fifo_SetRendering(bool bEnabled);
bool IsFifoProcesingData();

// Implemented by the Video Plugin
void VideoFifo_CheckAsyncRequest();
void VideoFifo_CheckStateRequest();

#endif // _FIFO_H
