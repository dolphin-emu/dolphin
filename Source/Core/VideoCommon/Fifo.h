// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/Common.h"
#include "VideoCommon/VideoBackendBase.h"

class PointerWrap;

#define FIFO_SIZE (2*1024*1024)

extern volatile bool g_bSkipCurrentFrame;


void Fifo_Init();
void Fifo_Shutdown();

void Fifo_DoState(PointerWrap &f);
void Fifo_PauseAndLock(bool doLock, bool unpauseOnUnlock);

void ReadDataFromFifo(u8* _uData, u32 len);

void RunGpu();
void RunGpuLoop();
void ExitGpuLoop();
void EmulatorState(bool running);
bool AtBreakpoint();
void ResetVideoBuffer();
void Fifo_SetRendering(bool bEnabled);


// Implemented by the Video Backend
void VideoFifo_CheckAsyncRequest();
