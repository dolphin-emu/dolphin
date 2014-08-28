// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Atomic.h"
#include "Common/ChunkFile.h"
#include "Common/FPURoundMode.h"
#include "Common/MemoryUtil.h"
#include "Common/Thread.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/Memmap.h"

#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/VideoConfig.h"

bool g_bSkipCurrentFrame = false;

static volatile bool GpuRunningState = false;
static volatile bool EmuRunningState = false;
static std::mutex m_csHWVidOccupied;

// Most of this array is unlikely to be faulted in...
static u8 s_fifo_aux_data[FIFO_SIZE];
static u8* s_fifo_aux_write_ptr;
static u8* s_fifo_aux_read_ptr;

bool g_use_deterministic_gpu_thread = true; // XXX

// STATE_TO_SAVE
static std::mutex s_video_buffer_lock;
static std::condition_variable s_video_buffer_cond;
static u8* s_video_buffer;
u8* g_video_buffer_read_ptr;
static std::atomic<u8*> s_video_buffer_write_ptr;
static std::atomic<u8*> s_video_buffer_seen_ptr;
u8* g_video_buffer_pp_read_ptr;
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

void Fifo_DoState(PointerWrap &p)
{
	p.DoArray(s_video_buffer, FIFO_SIZE);
	u8* write_ptr = s_video_buffer_write_ptr;
	p.DoPointer(write_ptr, s_video_buffer);
	s_video_buffer_write_ptr = write_ptr;
	p.DoPointer(g_video_buffer_read_ptr, s_video_buffer);
	if (p.mode == PointerWrap::MODE_READ && g_use_deterministic_gpu_thread)
	{
		// We're good and paused, right?
		s_video_buffer_seen_ptr = g_video_buffer_pp_read_ptr = g_video_buffer_read_ptr;
	}
	p.Do(g_bSkipCurrentFrame);
}

void Fifo_PauseAndLock(bool doLock, bool unpauseOnUnlock)
{
	if (doLock)
	{
		SyncGPU(SYNC_GPU_OTHER);
		EmulatorState(false);
		if (!Core::IsGPUThread())
			m_csHWVidOccupied.lock();
		_dbg_assert_(COMMON, !CommandProcessor::fifo.isGpuReadingData);
	}
	else
	{
		if (unpauseOnUnlock)
			EmulatorState(true);
		if (!Core::IsGPUThread())
			m_csHWVidOccupied.unlock();
	}
}


void Fifo_Init()
{
	s_video_buffer = (u8*)AllocateMemoryPages(FIFO_SIZE);
	ResetVideoBuffer();
	GpuRunningState = false;
	Common::AtomicStore(CommandProcessor::VITicks, CommandProcessor::m_cpClockOrigin);
}

void Fifo_Shutdown()
{
	if (GpuRunningState) PanicAlert("Fifo shutting down while active");
	FreeMemoryPages(s_video_buffer, FIFO_SIZE);
	s_video_buffer = nullptr;
	s_video_buffer_write_ptr = nullptr;
	g_video_buffer_pp_read_ptr = nullptr;
	g_video_buffer_read_ptr = nullptr;
	s_video_buffer_seen_ptr = nullptr;
	s_fifo_aux_write_ptr = nullptr;
	s_fifo_aux_read_ptr = nullptr;
}

u8* GetVideoBufferStartPtr()
{
	return s_video_buffer;
}

u8* GetVideoBufferEndPtr()
{
	return s_video_buffer_write_ptr;
}

void Fifo_SetRendering(bool enabled)
{
	g_bSkipCurrentFrame = !enabled;
}

// May be executed from any thread, even the graphics thread.
// Created to allow for self shutdown.
void ExitGpuLoop()
{
	// This should break the wait loop in CPU thread
	CommandProcessor::fifo.bFF_GPReadEnable = false;
	SCPFifoStruct &fifo = CommandProcessor::fifo;
	while (fifo.isGpuReadingData) Common::YieldCPU();
	// Terminate GPU thread loop
	GpuRunningState = false;
	EmuRunningState = true;
}

void EmulatorState(bool running)
{
	EmuRunningState = running;
}

void SyncGPU(SyncGPUReason reason, bool may_move_read_ptr)
{
	if (g_use_deterministic_gpu_thread && GpuRunningState)
	{
		std::unique_lock<std::mutex> lk(s_video_buffer_lock);
		u8* write_ptr = s_video_buffer_write_ptr;
		s_video_buffer_cond.wait(lk, [&]() {
			return !GpuRunningState || s_video_buffer_seen_ptr == write_ptr;
		});
		if (!GpuRunningState)
			return;

		// Opportunistically reset FIFOs so we don't wrap around.
		if (may_move_read_ptr && s_fifo_aux_write_ptr != s_fifo_aux_read_ptr)
			PanicAlert("aux fifo not synced (%p, %p)", s_fifo_aux_write_ptr, s_fifo_aux_read_ptr);

		memmove(s_fifo_aux_data, s_fifo_aux_read_ptr, s_fifo_aux_write_ptr - s_fifo_aux_read_ptr);
		s_fifo_aux_write_ptr -= (s_fifo_aux_read_ptr - s_fifo_aux_data);
		s_fifo_aux_read_ptr = s_fifo_aux_data;

		if (may_move_read_ptr)
		{
			// what's left over in the buffer
			size_t size = write_ptr - g_video_buffer_pp_read_ptr;

			memmove(s_video_buffer, g_video_buffer_pp_read_ptr, size);
			// This change always decreases the pointers.  We write seen_ptr
			// after write_ptr here, and read it before in RunGpuLoop, so
			// 'write_ptr > seen_ptr' there cannot become spuriously true.
			s_video_buffer_write_ptr = write_ptr = s_video_buffer + size;
			g_video_buffer_pp_read_ptr = s_video_buffer;
			g_video_buffer_read_ptr = s_video_buffer;
			s_video_buffer_seen_ptr = write_ptr;
		}
	}
}

void PushFifoAuxBuffer(void* ptr, size_t size)
{
	if (size > (size_t) (s_fifo_aux_data + FIFO_SIZE - s_fifo_aux_write_ptr))
	{
		SyncGPU(SYNC_GPU_AUX_SPACE, /* may_move_read_ptr */ false);
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
static void ReadDataFromFifo(u8* _uData, u32 len)
{
	if (len > (s_video_buffer + FIFO_SIZE - s_video_buffer_write_ptr))
	{
		size_t size = s_video_buffer_write_ptr - g_video_buffer_read_ptr;
		if (len > FIFO_SIZE - size)
		{
			PanicAlert("FIFO out of bounds (existing %lu + new %lu > %lu)", (unsigned long) size, (unsigned long) len, (unsigned long) FIFO_SIZE);
			return;
		}
		memmove(s_video_buffer, g_video_buffer_read_ptr, size);
		s_video_buffer_write_ptr = s_video_buffer + size;
		g_video_buffer_read_ptr = s_video_buffer;
	}
	// Copy new video instructions to s_video_buffer for future use in rendering the new picture
	memcpy(s_video_buffer_write_ptr, _uData, len);
	s_video_buffer_write_ptr += len;
}

// The deterministic_gpu_thread version.
static void ReadDataFromFifoOnCPU(u8* _uData, u32 len)
{
	u8 *write_ptr = s_video_buffer_write_ptr;
	if (len > (s_video_buffer + FIFO_SIZE - write_ptr))
	{
		// We can't wrap around while the GPU is working on the data.
		// This should be very rare due to the reset in SyncGPU.
		SyncGPU(SYNC_GPU_WRAPAROUND);
		if (g_video_buffer_pp_read_ptr != g_video_buffer_read_ptr)
		{
			PanicAlert("desynced read pointers");
			return;
		}
		write_ptr = s_video_buffer_write_ptr;
		size_t size = write_ptr - g_video_buffer_pp_read_ptr;
		if (len > FIFO_SIZE - size)
		{
			PanicAlert("FIFO out of bounds (existing %lu + new %lu > %lu)", (unsigned long) size, (unsigned long) len, (unsigned long) FIFO_SIZE);
			return;
		}
	}
	memcpy(write_ptr, _uData, len);
	OpcodeDecoder_Preprocess(write_ptr + len);
	// This would have to be locked if the GPU thread didn't spin.
	s_video_buffer_write_ptr = write_ptr + len;
}

void ResetVideoBuffer()
{
	g_video_buffer_read_ptr = s_video_buffer;
	s_video_buffer_write_ptr = s_video_buffer;
	s_video_buffer_seen_ptr = s_video_buffer;
	g_video_buffer_pp_read_ptr = s_video_buffer;
	s_fifo_aux_write_ptr = s_fifo_aux_data;
	s_fifo_aux_read_ptr = s_fifo_aux_data;
}


// Description: Main FIFO update loop
// Purpose: Keep the Core HW updated about the CPU-GPU distance
void RunGpuLoop()
{
	std::lock_guard<std::mutex> lk(m_csHWVidOccupied);
	GpuRunningState = true;
	SCPFifoStruct &fifo = CommandProcessor::fifo;
	u32 cyclesExecuted = 0;

	while (GpuRunningState)
	{
		g_video_backend->PeekMessages();

		VideoFifo_CheckAsyncRequest();
		if (g_use_deterministic_gpu_thread)
		{
			// All the fifo/CP stuff is on the CPU.  We just need to run the opcode decoder.
			u8* seen_ptr = s_video_buffer_seen_ptr;
			u8* write_ptr = s_video_buffer_write_ptr;
			// See comment in SyncGPU
			if (write_ptr > seen_ptr)
			{
				OpcodeDecoder_Run(write_ptr);

				{
					std::lock_guard<std::mutex> vblk(s_video_buffer_lock);
					s_video_buffer_seen_ptr = write_ptr;
					s_video_buffer_cond.notify_all();
				}
			}
		}
		else
		{
			CommandProcessor::SetCPStatusFromGPU();

			Common::AtomicStore(CommandProcessor::VITicks, CommandProcessor::m_cpClockOrigin);

			// check if we are able to run this buffer
			while (GpuRunningState && EmuRunningState && !CommandProcessor::interruptWaiting && fifo.bFF_GPReadEnable && fifo.CPReadWriteDistance && !AtBreakpoint())
			{
				fifo.isGpuReadingData = true;
				CommandProcessor::isPossibleWaitingSetDrawDone = fifo.bFF_GPLinkEnable ? true : false;

				if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bSyncGPU || Common::AtomicLoad(CommandProcessor::VITicks) > CommandProcessor::m_cpClockOrigin)
				{
					u32 readPtr = fifo.CPReadPointer;
					u8 *uData = Memory::GetPointer(readPtr);

					if (readPtr == fifo.CPEnd)
						readPtr = fifo.CPBase;
					else
						readPtr += 32;

					_assert_msg_(COMMANDPROCESSOR, (s32)fifo.CPReadWriteDistance - 32 >= 0 ,
						"Negative fifo.CPReadWriteDistance = %i in FIFO Loop !\nThat can produce instability in the game. Please report it.", fifo.CPReadWriteDistance - 32);

					ReadDataFromFifo(uData, 32);

					u8* write_ptr = s_video_buffer_write_ptr;

					cyclesExecuted = OpcodeDecoder_Run(write_ptr);


					if (SConfig::GetInstance().m_LocalCoreStartupParameter.bSyncGPU && Common::AtomicLoad(CommandProcessor::VITicks) >= cyclesExecuted)
						Common::AtomicAdd(CommandProcessor::VITicks, -(s32)cyclesExecuted);

					Common::AtomicStore(fifo.CPReadPointer, readPtr);
					Common::AtomicAdd(fifo.CPReadWriteDistance, -32);
					if ((write_ptr - g_video_buffer_read_ptr) == 0)
						Common::AtomicStore(fifo.SafeCPReadPointer, fifo.CPReadPointer);
				}

				CommandProcessor::SetCPStatusFromGPU();

				// This call is pretty important in DualCore mode and must be called in the FIFO Loop.
				// If we don't, s_swapRequested or s_efbAccessRequested won't be set to false
				// leading the CPU thread to wait in Video_BeginField or Video_AccessEFB thus slowing things down.
				VideoFifo_CheckAsyncRequest();
				CommandProcessor::isPossibleWaitingSetDrawDone = false;
			}

			fifo.isGpuReadingData = false;
		}

		if (EmuRunningState)
		{
			// NOTE(jsd): Calling SwitchToThread() on Windows 7 x64 is a hot spot, according to profiler.
			// See https://docs.google.com/spreadsheet/ccc?key=0Ah4nh0yGtjrgdFpDeF9pS3V6RUotRVE3S3J4TGM1NlE#gid=0
			// for benchmark details.
#if 0
			Common::YieldCPU();
#endif
		}
		else
		{
			// While the emu is paused, we still handle async requests then sleep.
			while (!EmuRunningState)
			{
				g_video_backend->PeekMessages();
				m_csHWVidOccupied.unlock();
				Common::SleepCurrentThread(1);
				m_csHWVidOccupied.lock();
			}
		}
	}
	// wake up SyncGPU if we were interrupted
	s_video_buffer_cond.notify_all();
}


bool AtBreakpoint()
{
	SCPFifoStruct &fifo = CommandProcessor::fifo;
	return fifo.bFF_BPEnable && (fifo.CPReadPointer == fifo.CPBreakpoint);
}

void RunGpu()
{
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread &&
	    !g_use_deterministic_gpu_thread)
		return;

	SCPFifoStruct &fifo = CommandProcessor::fifo;
	while (fifo.bFF_GPReadEnable && fifo.CPReadWriteDistance && !AtBreakpoint() )
	{
		u8 *uData = Memory::GetPointer(fifo.CPReadPointer);

		if (g_use_deterministic_gpu_thread)
		{
			ReadDataFromFifoOnCPU(uData, 32);
		}
		else
		{
			FPURoundMode::SaveSIMDState();
			FPURoundMode::LoadDefaultSIMDState();
			ReadDataFromFifo(uData, 32);
			OpcodeDecoder_Run(s_video_buffer_write_ptr);
			FPURoundMode::LoadSIMDState();
		}

		//DEBUG_LOG(COMMANDPROCESSOR, "Fifo wraps to base");

		if (fifo.CPReadPointer == fifo.CPEnd)
			fifo.CPReadPointer = fifo.CPBase;
		else
			fifo.CPReadPointer += 32;

		fifo.CPReadWriteDistance -= 32;
	}
	CommandProcessor::SetCPStatusFromGPU();
}
