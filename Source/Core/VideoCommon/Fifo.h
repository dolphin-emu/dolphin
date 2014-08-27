// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/VideoBackendBase.h"

class PointerWrap;

#define FIFO_SIZE (2*1024*1024)

extern bool g_bSkipCurrentFrame;


void Fifo_Init();
void Fifo_Shutdown();

u8* GetVideoBufferStartPtr();
u8* GetVideoBufferEndPtr();

void Fifo_DoState(PointerWrap &f);
void Fifo_PauseAndLock(bool doLock, bool unpauseOnUnlock);

void RunGpu();
void RunGpuLoop();
void ExitGpuLoop();
void EmulatorState(bool running);
bool AtBreakpoint();
void ResetVideoBuffer();
void Fifo_SetRendering(bool bEnabled);


// Implemented by the Video Backend
void VideoFifo_CheckAsyncRequest();
