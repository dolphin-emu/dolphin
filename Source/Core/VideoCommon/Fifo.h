// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <cstddef>
#include "Common/CommonTypes.h"

class PointerWrap;

namespace Fifo
{

extern bool g_bSkipCurrentFrame;

// This could be in SConfig, but it depends on multiple settings
// and can change at runtime.
extern bool g_use_deterministic_gpu_thread;
extern std::atomic<u8*> g_video_buffer_write_ptr_xthread;

void Init();
void Shutdown();
void Prepare(); // Must be called from the CPU thread.
void DoState(PointerWrap &f);
void PauseAndLock(bool doLock, bool unpauseOnUnlock);
void UpdateWantDeterminism(bool want);

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
void SetRendering(bool bEnabled);

};
