// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/VideoBackendBase.h"

class PointerWrap;

#define FIFO_SIZE (2*1024*1024)

extern bool g_bSkipCurrentFrame;

// This could be in SConfig, but it depends on multiple settings
// and can change at runtime.
extern bool g_use_deterministic_gpu_thread;
extern std::atomic<u8*> g_video_buffer_write_ptr_xthread;

void Fifo_Init();
void Fifo_Shutdown();
void Fifo_DoState(PointerWrap &f);
void Fifo_PauseAndLock(bool doLock, bool unpauseOnUnlock);
void Fifo_UpdateWantDeterminism(bool want);

// Used for diagnostics.
enum SyncGPUReason
{
	SYNC_GPU_OTHER,
	SYNC_GPU_WRAPAROUND,
	SYNC_GPU_EFB_POKE,
	SYNC_GPU_PERFQUERY,
	SYNC_GPU_BBOX,
	SYNC_GPU_SWAP,
	SYNC_GPU_AUX_SPACE,
};
// In g_use_deterministic_gpu_thread mode, waits for the GPU to be done with pending work.
void SyncGPU(SyncGPUReason reason, bool may_move_read_ptr = true);

void PushFifoAuxBuffer(void* ptr, size_t size);
void* PopFifoAuxBuffer(size_t size);

void FlushGpu();
void RunGpu();
void GpuMaySleep();
void RunGpuLoop();
void ExitGpuLoop();
void EmulatorState(bool running);
bool AtBreakpoint();
void ResetVideoBuffer();
void Fifo_SetRendering(bool bEnabled);
int Fifo_Update(int ticks);
