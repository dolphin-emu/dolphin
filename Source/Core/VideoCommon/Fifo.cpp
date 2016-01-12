// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <atomic>

#include "Common/Atomic.h"
#include "Common/BlockingLoop.h"
#include "Common/ChunkFile.h"
#include "Common/CPUDetect.h"
#include "Common/Event.h"
#include "Common/FPURoundMode.h"
#include "Common/MemoryUtil.h"
#include "Common/Thread.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/NetPlayProto.h"
#include "Core/HW/Memmap.h"

#include "VideoCommon/AsyncRequests.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoConfig.h"

namespace Fifo
{

static constexpr u32 FIFO_SIZE = 2 * 1024 * 1024;

bool g_bSkipCurrentFrame = false;

static Common::BlockingLoop s_gpu_mainloop;

static std::atomic<bool> s_emu_running_state;

// Most of this array is unlikely to be faulted in...
static u8 s_fifo_aux_data[FIFO_SIZE];
static u8* s_fifo_aux_write_ptr;
static u8* s_fifo_aux_read_ptr;

bool g_use_deterministic_gpu_thread;

// STATE_TO_SAVE
static u8* s_video_buffer;
static u8* s_video_buffer_read_ptr;
static std::atomic<u8*> s_video_buffer_write_ptr;
static std::atomic<u8*> s_video_buffer_seen_ptr;
static u8* s_video_buffer_pp_read_ptr;
// The read_ptr is always owned by the GPU thread.  In normal mode, so is the
// write_ptr, despite it being atomic.  In g_use_deterministic_gpu_thread mode,
// things get a bit more complicated:
// - The seen_ptr is written by the GPU thread, and points to what it's already
// processed as much of as possible - in the case of a partial command which
// caused it to stop, not the same as the read ptr.  It's written by the GPU,
// under the lock, and updating the cond.
// - The write_ptr is written by the CPU thread after it copies data from the
// FIFO.  Maybe someday it will be under the lock.  For now, because RunGpuLoop
// polls, it's just atomic.
// - The pp_read_ptr is the CPU preprocessing version of the read_ptr.

static std::atomic<int> s_sync_ticks;
static Common::Event s_sync_wakeup_event;

void DoState(PointerWrap &p)
{
	p.DoArray(s_video_buffer, FIFO_SIZE);
	u8* write_ptr = s_video_buffer_write_ptr;
	p.DoPointer(write_ptr, s_video_buffer);
	s_video_buffer_write_ptr = write_ptr;
	p.DoPointer(s_video_buffer_read_ptr, s_video_buffer);
	if (p.mode == PointerWrap::MODE_READ && g_use_deterministic_gpu_thread)
	{
		// We're good and paused, right?
		s_video_buffer_seen_ptr = s_video_buffer_pp_read_ptr = s_video_buffer_read_ptr;
	}
	p.Do(g_bSkipCurrentFrame);
}

void PauseAndLock(bool doLock, bool unpauseOnUnlock)
{
	if (doLock)
	{
		SyncGPU(SYNC_GPU_OTHER);
		EmulatorState(false);
		FlushGpu();
	}
	else
	{
		if (unpauseOnUnlock)
			EmulatorState(true);
	}
}


void Init()
{
	// Padded so that SIMD overreads in the vertex loader are safe
	s_video_buffer = (u8*)AllocateMemoryPages(FIFO_SIZE + 4);
	ResetVideoBuffer();
	if (SConfig::GetInstance().bCPUThread)
		s_gpu_mainloop.Prepare();
	s_sync_ticks.store(0);
}

void Shutdown()
{
	if (s_gpu_mainloop.IsRunning())
		PanicAlert("Fifo shutting down while active");

	FreeMemoryPages(s_video_buffer, FIFO_SIZE + 4);
	s_video_buffer = nullptr;
	s_video_buffer_write_ptr = nullptr;
	s_video_buffer_pp_read_ptr = nullptr;
	s_video_buffer_read_ptr = nullptr;
	s_video_buffer_seen_ptr = nullptr;
	s_fifo_aux_write_ptr = nullptr;
	s_fifo_aux_read_ptr = nullptr;
}

void SetRendering(bool enabled)
{
	g_bSkipCurrentFrame = !enabled;
}

// May be executed from any thread, even the graphics thread.
// Created to allow for self shutdown.
void ExitGpuLoop()
{
	// This should break the wait loop in CPU thread
	CommandProcessor::fifo.bFF_GPReadEnable = false;
	FlushGpu();

	// Terminate GPU thread loop
	s_emu_running_state.store(true);
	s_gpu_mainloop.Stop(false);
}

void EmulatorState(bool running)
{
	s_emu_running_state.store(running);
	s_gpu_mainloop.Wakeup();
}

void SyncGPU(SyncGPUReason reason, bool may_move_read_ptr)
{
	if (g_use_deterministic_gpu_thread)
	{
		s_gpu_mainloop.Wait();
		if (!s_gpu_mainloop.IsRunning())
			return;

		// Opportunistically reset FIFOs so we don't wrap around.
		if (may_move_read_ptr && s_fifo_aux_write_ptr != s_fifo_aux_read_ptr)
			PanicAlert("aux fifo not synced (%p, %p)", s_fifo_aux_write_ptr, s_fifo_aux_read_ptr);

		memmove(s_fifo_aux_data, s_fifo_aux_read_ptr, s_fifo_aux_write_ptr - s_fifo_aux_read_ptr);
		s_fifo_aux_write_ptr -= (s_fifo_aux_read_ptr - s_fifo_aux_data);
		s_fifo_aux_read_ptr = s_fifo_aux_data;

		if (may_move_read_ptr)
		{
			u8* write_ptr = s_video_buffer_write_ptr;

			// what's left over in the buffer
			size_t size = write_ptr - s_video_buffer_pp_read_ptr;

			memmove(s_video_buffer, s_video_buffer_pp_read_ptr, size);
			// This change always decreases the pointers.  We write seen_ptr
			// after write_ptr here, and read it before in RunGpuLoop, so
			// 'write_ptr > seen_ptr' there cannot become spuriously true.
			s_video_buffer_write_ptr = write_ptr = s_video_buffer + size;
			s_video_buffer_pp_read_ptr = s_video_buffer;
			s_video_buffer_read_ptr = s_video_buffer;
			s_video_buffer_seen_ptr = write_ptr;
		}
	}
}

void PushFifoAuxBuffer(void* ptr, size_t size)
{
	if (size > (size_t) (s_fifo_aux_data + FIFO_SIZE - s_fifo_aux_write_ptr))
	{
		SyncGPU(SYNC_GPU_AUX_SPACE, /* may_move_read_ptr */ false);
		if (!s_gpu_mainloop.IsRunning())
		{
			// GPU is shutting down
			return;
		}
		if (size > (size_t) (s_fifo_aux_data + FIFO_SIZE - s_fifo_aux_write_ptr))
		{
			// That will sync us up to the last 32 bytes, so this short region
			// of FIFO would have to point to a 2MB display list or something.
			PanicAlert("absurdly large aux buffer");
			return;
		}
	}
	memcpy(s_fifo_aux_write_ptr, ptr, size);
	s_fifo_aux_write_ptr += size;
}

void* PopFifoAuxBuffer(size_t size)
{
	void* ret = s_fifo_aux_read_ptr;
	s_fifo_aux_read_ptr += size;
	return ret;
}

// Description: RunGpuLoop() sends data through this function.
static void ReadDataFromFifo(u32 readPtr)
{
	size_t len = 32;
	if (len > (size_t)(s_video_buffer + FIFO_SIZE - s_video_buffer_write_ptr))
	{
		size_t existing_len = s_video_buffer_write_ptr - s_video_buffer_read_ptr;
		if (len > (size_t)(FIFO_SIZE - existing_len))
		{
			PanicAlert("FIFO out of bounds (existing %zu + new %zu > %lu)", existing_len, len, (unsigned long) FIFO_SIZE);
			return;
		}
		memmove(s_video_buffer, s_video_buffer_read_ptr, existing_len);
		s_video_buffer_write_ptr = s_video_buffer + existing_len;
		s_video_buffer_read_ptr = s_video_buffer;
	}
	// Copy new video instructions to s_video_buffer for future use in rendering the new picture
	Memory::CopyFromEmu(s_video_buffer_write_ptr, readPtr, len);
	s_video_buffer_write_ptr += len;
}

// The deterministic_gpu_thread version.
static void ReadDataFromFifoOnCPU(u32 readPtr)
{
	size_t len = 32;
	u8 *write_ptr = s_video_buffer_write_ptr;
	if (len > (size_t)(s_video_buffer + FIFO_SIZE - write_ptr))
	{
		// We can't wrap around while the GPU is working on the data.
		// This should be very rare due to the reset in SyncGPU.
		SyncGPU(SYNC_GPU_WRAPAROUND);
		if (!s_gpu_mainloop.IsRunning())
		{
			// GPU is shutting down, so the next asserts may fail
			return;
		}

		if (s_video_buffer_pp_read_ptr != s_video_buffer_read_ptr)
		{
			PanicAlert("desynced read pointers");
			return;
		}
		write_ptr = s_video_buffer_write_ptr;
		size_t existing_len = write_ptr - s_video_buffer_pp_read_ptr;
		if (len > (size_t)(FIFO_SIZE - existing_len))
		{
			PanicAlert("FIFO out of bounds (existing %zu + new %zu > %lu)", existing_len, len, (unsigned long) FIFO_SIZE);
			return;
		}
	}
	Memory::CopyFromEmu(s_video_buffer_write_ptr, readPtr, len);
	s_video_buffer_pp_read_ptr = OpcodeDecoder_Run<true>(DataReader(s_video_buffer_pp_read_ptr, write_ptr + len), nullptr, false);
	// This would have to be locked if the GPU thread didn't spin.
	s_video_buffer_write_ptr = write_ptr + len;
}

void ResetVideoBuffer()
{
	s_video_buffer_read_ptr = s_video_buffer;
	s_video_buffer_write_ptr = s_video_buffer;
	s_video_buffer_seen_ptr = s_video_buffer;
	s_video_buffer_pp_read_ptr = s_video_buffer;
	s_fifo_aux_write_ptr = s_fifo_aux_data;
	s_fifo_aux_read_ptr = s_fifo_aux_data;
}


// Description: Main FIFO update loop
// Purpose: Keep the Core HW updated about the CPU-GPU distance
void RunGpuLoop()
{

	AsyncRequests::GetInstance()->SetEnable(true);
	AsyncRequests::GetInstance()->SetPassthrough(false);

	s_gpu_mainloop.Run(
	[] {
		const SConfig& param = SConfig::GetInstance();

		g_video_backend->PeekMessages();

		// Do nothing while paused
		if (!s_emu_running_state.load())
			return;

		if (g_use_deterministic_gpu_thread)
		{
			AsyncRequests::GetInstance()->PullEvents();

			// All the fifo/CP stuff is on the CPU.  We just need to run the opcode decoder.
			u8* seen_ptr = s_video_buffer_seen_ptr;
			u8* write_ptr = s_video_buffer_write_ptr;
			// See comment in SyncGPU
			if (write_ptr > seen_ptr)
			{
				s_video_buffer_read_ptr = OpcodeDecoder_Run(DataReader(s_video_buffer_read_ptr, write_ptr), nullptr, false);
				s_video_buffer_seen_ptr = write_ptr;
			}
		}
		else
		{
			SCPFifoStruct &fifo = CommandProcessor::fifo;

			AsyncRequests::GetInstance()->PullEvents();

			CommandProcessor::SetCPStatusFromGPU();

			// check if we are able to run this buffer
			while (!CommandProcessor::IsInterruptWaiting() && fifo.bFF_GPReadEnable && fifo.CPReadWriteDistance && !AtBreakpoint())
			{
				if (param.bSyncGPU && s_sync_ticks.load() < param.iSyncGpuMinDistance)
					break;

				u32 cyclesExecuted = 0;
				u32 readPtr = fifo.CPReadPointer;
				ReadDataFromFifo(readPtr);

				if (readPtr == fifo.CPEnd)
					readPtr = fifo.CPBase;
				else
					readPtr += 32;

				_assert_msg_(COMMANDPROCESSOR, (s32)fifo.CPReadWriteDistance - 32 >= 0 ,
					"Negative fifo.CPReadWriteDistance = %i in FIFO Loop !\nThat can produce instability in the game. Please report it.", fifo.CPReadWriteDistance - 32);

				u8* write_ptr = s_video_buffer_write_ptr;
				s_video_buffer_read_ptr = OpcodeDecoder_Run(DataReader(s_video_buffer_read_ptr, write_ptr), &cyclesExecuted, false);

				Common::AtomicStore(fifo.CPReadPointer, readPtr);
				Common::AtomicAdd(fifo.CPReadWriteDistance, -32);
				if ((write_ptr - s_video_buffer_read_ptr) == 0)
					Common::AtomicStore(fifo.SafeCPReadPointer, fifo.CPReadPointer);

				CommandProcessor::SetCPStatusFromGPU();

				if (param.bSyncGPU)
				{
					cyclesExecuted = (int)(cyclesExecuted / param.fSyncGpuOverclock);
					int old = s_sync_ticks.fetch_sub(cyclesExecuted);
					if (old > 0 && old - (int)cyclesExecuted <= 0)
						s_sync_wakeup_event.Set();
				}

				// This call is pretty important in DualCore mode and must be called in the FIFO Loop.
				// If we don't, s_swapRequested or s_efbAccessRequested won't be set to false
				// leading the CPU thread to wait in Video_BeginField or Video_AccessEFB thus slowing things down.
				AsyncRequests::GetInstance()->PullEvents();
			}

			// fast skip remaining GPU time if fifo is empty
			if (s_sync_ticks.load() > 0)
			{
				int old = s_sync_ticks.exchange(0);
				if (old > 0)
					s_sync_wakeup_event.Set();
			}

			// The fifo is empty and it's unlikely we will get any more work in the near future.
			// Make sure VertexManager finishes drawing any primitives it has stored in it's buffer.
			VertexManagerBase::Flush();
		}
	}, 100);

	AsyncRequests::GetInstance()->SetEnable(false);
	AsyncRequests::GetInstance()->SetPassthrough(true);
}

void FlushGpu()
{
	const SConfig& param = SConfig::GetInstance();

	if (!param.bCPUThread || g_use_deterministic_gpu_thread)
		return;

	s_gpu_mainloop.Wait();
}

void GpuMaySleep()
{
	s_gpu_mainloop.AllowSleep();
}

bool AtBreakpoint()
{
	SCPFifoStruct &fifo = CommandProcessor::fifo;
	return fifo.bFF_BPEnable && (fifo.CPReadPointer == fifo.CPBreakpoint);
}

void RunGpu()
{
	SCPFifoStruct &fifo = CommandProcessor::fifo;
	const SConfig& param = SConfig::GetInstance();

	// execute GPU
	if (!param.bCPUThread || g_use_deterministic_gpu_thread)
	{
		bool reset_simd_state = false;
		while (fifo.bFF_GPReadEnable && fifo.CPReadWriteDistance && !AtBreakpoint() )
		{
			if (g_use_deterministic_gpu_thread)
			{
				ReadDataFromFifoOnCPU(fifo.CPReadPointer);
				s_gpu_mainloop.Wakeup();
			}
			else
			{
				if (!reset_simd_state)
				{
					FPURoundMode::SaveSIMDState();
					FPURoundMode::LoadDefaultSIMDState();
					reset_simd_state = true;
				}
				ReadDataFromFifo(fifo.CPReadPointer);
				s_video_buffer_read_ptr = OpcodeDecoder_Run(DataReader(s_video_buffer_read_ptr, s_video_buffer_write_ptr), nullptr, false);
			}

			//DEBUG_LOG(COMMANDPROCESSOR, "Fifo wraps to base");

			if (fifo.CPReadPointer == fifo.CPEnd)
				fifo.CPReadPointer = fifo.CPBase;
			else
				fifo.CPReadPointer += 32;

			fifo.CPReadWriteDistance -= 32;
		}
		CommandProcessor::SetCPStatusFromGPU();

		if (reset_simd_state)
		{
			FPURoundMode::LoadSIMDState();
		}
	}

	// wake up GPU thread
	if (param.bCPUThread)
	{
		s_gpu_mainloop.Wakeup();
	}
}

void UpdateWantDeterminism(bool want)
{
	// We are paused (or not running at all yet), so
	// it should be safe to change this.
	const SConfig& param = SConfig::GetInstance();
	bool gpu_thread = false;
	switch (param.m_GPUDeterminismMode)
	{
		case GPU_DETERMINISM_AUTO:
			gpu_thread = want;

			// Hack: For now movies are an exception to this being on (but not
			// to wanting determinism in general).  Once vertex arrays are
			// fixed, there should be no reason to want this off for movies by
			// default, so this can be removed.
			if (!NetPlay::IsNetPlayRunning())
				gpu_thread = false;

			break;
		case GPU_DETERMINISM_NONE:
			gpu_thread = false;
			break;
		case GPU_DETERMINISM_FAKE_COMPLETION:
			gpu_thread = true;
			break;
	}

	gpu_thread = gpu_thread && param.bCPUThread;

	if (g_use_deterministic_gpu_thread != gpu_thread)
	{
		g_use_deterministic_gpu_thread = gpu_thread;
		if (gpu_thread)
		{
			// These haven't been updated in non-deterministic mode.
			s_video_buffer_seen_ptr = s_video_buffer_pp_read_ptr = s_video_buffer_read_ptr;
			CopyPreprocessCPStateFromMain();
			VertexLoaderManager::MarkAllDirty();
		}
	}
}

int Update(int ticks)
{
	const SConfig& param = SConfig::GetInstance();

	if (ticks == 0)
	{
		FlushGpu();
		return param.iSyncGpuMaxDistance;
	}

	// GPU is sleeping, so no need for synchronization
	if (s_gpu_mainloop.IsDone() || g_use_deterministic_gpu_thread)
	{
		if (s_sync_ticks.load() < 0)
		{
			int old = s_sync_ticks.fetch_add(ticks);
			if (old < param.iSyncGpuMinDistance && old + ticks >= param.iSyncGpuMinDistance)
				RunGpu();
		}
		return param.iSyncGpuMaxDistance;
	}

	int old = s_sync_ticks.fetch_add(ticks);
	if (old < param.iSyncGpuMinDistance && old + ticks >= param.iSyncGpuMinDistance)
		RunGpu();

	if (s_sync_ticks.load() >= param.iSyncGpuMaxDistance)
	{
		while (s_sync_ticks.load() > 0)
		{
			s_sync_wakeup_event.Wait();
		}
	}

	return param.iSyncGpuMaxDistance - s_sync_ticks.load();
}

}
