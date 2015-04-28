// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Atomic.h"
#include "Common/ChunkFile.h"
#include "Common/CPUDetect.h"
#include "Common/Event.h"
#include "Common/FPURoundMode.h"
#include "Common/MemoryUtil.h"
#include "Common/Thread.h"

#include "Core/ARBruteForcer.h"
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
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VR.h"

bool g_bSkipCurrentFrame = false;

static volatile bool GpuRunningState = false;
static volatile bool EmuRunningState = false;

// Most of this array is unlikely to be faulted in...
static u8 s_fifo_aux_data[FIFO_SIZE];
static u8* s_fifo_aux_write_ptr;
static u8* s_fifo_aux_read_ptr;

bool g_use_deterministic_gpu_thread;

// STATE_TO_SAVE
static std::mutex s_video_buffer_lock;
static std::condition_variable s_video_buffer_cond;
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

static Common::Flag s_gpu_is_running; // If this one is set, the gpu loop will be called at least once again
static Common::Event s_gpu_new_work_event;

static Common::Flag s_gpu_is_pending; // If this one is set, there might still be work to do
static Common::Event s_gpu_done_event;

void Fifo_DoState(PointerWrap &p)
{
	if (!s_video_buffer && ARBruteForcer::ch_bruteforce)
		Core::KillDolphinAndRestart();
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

void Fifo_PauseAndLock(bool doLock, bool unpauseOnUnlock)
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


void Fifo_Init()
{
	// Padded so that SIMD overreads in the vertex loader are safe
	s_video_buffer = (u8*)AllocateMemoryPages(FIFO_SIZE + 4);
	ResetVideoBuffer();
	GpuRunningState = false;
	Common::AtomicStore(CommandProcessor::VITicks, CommandProcessor::m_cpClockOrigin);
}

void Fifo_Shutdown()
{
	if (GpuRunningState)
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
	FlushGpu();

	// Terminate GPU thread loop
	GpuRunningState = false;
	EmuRunningState = true;
	s_gpu_new_work_event.Set();
}

void EmulatorState(bool running)
{
	EmuRunningState = running;
	s_gpu_new_work_event.Set();
}

void SyncGPU(SyncGPUReason reason, bool may_move_read_ptr)
{
	if (g_use_deterministic_gpu_thread)
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
		if (!GpuRunningState)
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
			PanicAlert("FIFO out of bounds (existing %lu + new %lu > %lu)", (unsigned long) existing_len, (unsigned long) len, (unsigned long) FIFO_SIZE);
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
		if (!GpuRunningState)
		{
			// GPU is shutting down
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
			PanicAlert("FIFO out of bounds (existing %lu + new %lu > %lu)", (unsigned long) existing_len, (unsigned long) len, (unsigned long) FIFO_SIZE);
			return;
		}
	}
	Memory::CopyFromEmu(s_video_buffer_write_ptr, readPtr, len);
	s_video_buffer_pp_read_ptr = OpcodeDecoder_Run<true>(DataReader(s_video_buffer_pp_read_ptr, write_ptr + len), nullptr, false);

#ifdef INLINE_OPCODE
	//Render Extra Headtracking Frames for VR.
	if (g_new_frame_just_rendered && g_has_hmd)
	{
		OpcodeReplayBufferInline();
	}
	g_new_frame_just_rendered = false;
#endif

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
	GpuRunningState = true;
	SCPFifoStruct &fifo = CommandProcessor::fifo;
	u32 cyclesExecuted = 0;

	AsyncRequests::GetInstance()->SetEnable(true);
	AsyncRequests::GetInstance()->SetPassthrough(false);

	while (GpuRunningState)
	{
		g_video_backend->PeekMessages();

		if (g_use_deterministic_gpu_thread && EmuRunningState)
		{
			AsyncRequests::GetInstance()->PullEvents();

			// All the fifo/CP stuff is on the CPU.  We just need to run the opcode decoder.
			u8* seen_ptr = s_video_buffer_seen_ptr;
			u8* write_ptr = s_video_buffer_write_ptr;
			// See comment in SyncGPU
			if (write_ptr > seen_ptr)
			{
				s_video_buffer_read_ptr = OpcodeDecoder_Run(DataReader(s_video_buffer_read_ptr, write_ptr), nullptr, false);

#ifdef INLINE_OPCODE
				//Render Extra Headtracking Frames for VR.
				if (g_new_frame_just_rendered && g_has_hmd)
				{
					OpcodeReplayBufferInline();
				}
				g_new_frame_just_rendered = false;
#endif

				{
					std::lock_guard<std::mutex> vblk(s_video_buffer_lock);
					s_video_buffer_seen_ptr = write_ptr;
					s_video_buffer_cond.notify_all();
				}
			}
		}
		else if (EmuRunningState)
		{
			AsyncRequests::GetInstance()->PullEvents();

			CommandProcessor::SetCPStatusFromGPU();

			if (!fifo.isGpuReadingData)
			{
				Common::AtomicStore(CommandProcessor::VITicks, CommandProcessor::m_cpClockOrigin);
			}

			bool run_loop = true;

			// check if we are able to run this buffer
			while (run_loop && !CommandProcessor::interruptWaiting && fifo.bFF_GPReadEnable && fifo.CPReadWriteDistance && !AtBreakpoint())
			{
				fifo.isGpuReadingData = true;

				if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bSyncGPU || Common::AtomicLoad(CommandProcessor::VITicks) > CommandProcessor::m_cpClockOrigin)
				{
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

#ifdef INLINE_OPCODE
					//Render Extra Headtracking Frames for VR.
					if (g_new_frame_just_rendered && g_has_hmd)
					{
						OpcodeReplayBufferInline();
					}
					g_new_frame_just_rendered = false;
#endif

					if (SConfig::GetInstance().m_LocalCoreStartupParameter.bSyncGPU && Common::AtomicLoad(CommandProcessor::VITicks) >= cyclesExecuted)
						Common::AtomicAdd(CommandProcessor::VITicks, -(s32)cyclesExecuted);

					Common::AtomicStore(fifo.CPReadPointer, readPtr);
					Common::AtomicAdd(fifo.CPReadWriteDistance, -32);
					if ((write_ptr - s_video_buffer_read_ptr) == 0)
						Common::AtomicStore(fifo.SafeCPReadPointer, fifo.CPReadPointer);
				}
				else
				{
					run_loop = false;
				}

				CommandProcessor::SetCPStatusFromGPU();

				// This call is pretty important in DualCore mode and must be called in the FIFO Loop.
				// If we don't, s_swapRequested or s_efbAccessRequested won't be set to false
				// leading the CPU thread to wait in Video_BeginField or Video_AccessEFB thus slowing things down.
				AsyncRequests::GetInstance()->PullEvents();
			}

			// don't release the GPU running state on sync GPU waits
			fifo.isGpuReadingData = !run_loop;
		}

		s_gpu_is_pending.Clear();
		s_gpu_done_event.Set();

		if (s_gpu_is_running.IsSet())
		{
			if (CommandProcessor::s_gpuMaySleep.IsSet())
			{
				// Reset the atomic flag. But as the CPU thread might have pushed some new data, we have to rerun the GPU loop
				s_gpu_is_pending.Set();
				s_gpu_is_running.Clear();
				CommandProcessor::s_gpuMaySleep.Clear();
			}
		}
		else
		{
			s_gpu_new_work_event.WaitFor(std::chrono::milliseconds(100));
		}
	}
	// wake up SyncGPU if we were interrupted
	s_video_buffer_cond.notify_all();
	AsyncRequests::GetInstance()->SetEnable(false);
	AsyncRequests::GetInstance()->SetPassthrough(true);
}

void FlushGpu()
{
	if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread || g_use_deterministic_gpu_thread)
		return;

	while (s_gpu_is_running.IsSet() || s_gpu_is_pending.IsSet())
	{
		CommandProcessor::s_gpuMaySleep.Set();
		s_gpu_done_event.Wait();
	}
}

bool AtBreakpoint()
{
	SCPFifoStruct &fifo = CommandProcessor::fifo;
	return fifo.bFF_BPEnable && (fifo.CPReadPointer == fifo.CPBreakpoint);
}

void RunGpu()
{
	SCPFifoStruct &fifo = CommandProcessor::fifo;

	// execute GPU
	if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread || g_use_deterministic_gpu_thread)
	{
		bool reset_simd_state = false;
		while (fifo.bFF_GPReadEnable && fifo.CPReadWriteDistance && !AtBreakpoint() )
		{
			if (g_use_deterministic_gpu_thread)
			{
				ReadDataFromFifoOnCPU(fifo.CPReadPointer);
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

#ifdef INLINE_OPCODE
				//Render Extra Headtracking Frames for VR.
				if (g_new_frame_just_rendered && g_has_hmd)
				{
					OpcodeReplayBufferInline();
				}
				g_new_frame_just_rendered = false;
#endif
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
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread && !s_gpu_is_running.IsSet())
	{
		s_gpu_is_pending.Set();
		s_gpu_is_running.Set();
		s_gpu_new_work_event.Set();
	}
}

void Fifo_UpdateWantDeterminism(bool want)
{
	// We are paused (or not running at all yet), so
	// it should be safe to change this.
	const SCoreStartupParameter& param = SConfig::GetInstance().m_LocalCoreStartupParameter;
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

	gpu_thread = gpu_thread && SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread;

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
