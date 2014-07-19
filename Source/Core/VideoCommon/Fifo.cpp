// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Atomic.h"
#include "Common/ChunkFile.h"
#include "Common/FPURoundMode.h"
#include "Common/MemoryUtil.h"
#include "Common/Thread.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/Memmap.h"

#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/VideoConfig.h"

volatile bool g_bSkipCurrentFrame = false;

namespace
{
static volatile bool GpuRunningState = false;
static volatile bool EmuRunningState = false;
static std::mutex m_csHWVidOccupied;
// STATE_TO_SAVE
static u8* s_video_buffer;  // Size: FIFO_SIZE.
static int s_buffer_write_pos = 0;
}  // namespace

void Fifo_DoState(PointerWrap &p)
{
	p.DoArray(s_video_buffer, FIFO_SIZE);
	p.Do(s_buffer_write_pos);
	p.DoPointer(g_pVideoData, s_video_buffer);
	p.Do(g_bSkipCurrentFrame);
}

void Fifo_PauseAndLock(bool doLock, bool unpauseOnUnlock)
{
	if (doLock)
	{
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
	s_buffer_write_pos = 0;
	GpuRunningState = false;
	Common::AtomicStore(CommandProcessor::VITicks, CommandProcessor::m_cpClockOrigin);
}

void Fifo_Shutdown()
{
	if (GpuRunningState) PanicAlert("Fifo shutting down while active");
	FreeMemoryPages(s_video_buffer, FIFO_SIZE);
	s_video_buffer = nullptr;
}

u8* GetVideoBufferStartPtr()
{
	return s_video_buffer;
}

u8* GetVideoBufferEndPtr()
{
	return &s_video_buffer[s_buffer_write_pos];
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


// Description: RunGpuLoop() sends data through this function. Returns the
// number of bytes successfully written.
static u32 ReadDataFromFifo(u8* data, u32 len)
{
	if (s_buffer_write_pos + len >= FIFO_SIZE)
	{
		// No more space in the FIFO, try to relocate the data that is
		// currently waiting to be processed (it starts at g_pVideoData and
		// ends at our current writing position).
		//
		//           R       W
		// [01234567890123456789]
		//
		// If we want to add 4 bytes, we will copy (W - R) bytes to index 0.
		u32 read_pos = (u32)(g_pVideoData - s_video_buffer);
		s_buffer_write_pos -= read_pos;

		// Still not enough space in the FIFO. Write as much data as we can.
		if (s_buffer_write_pos == FIFO_SIZE)
		{
			PanicAlert("FIFO out of bounds (wi = %i, len = %i at %08x)",
			           s_buffer_write_pos, len, read_pos);
		}
		else if (s_buffer_write_pos + len > FIFO_SIZE)
		{
			len = FIFO_SIZE - s_buffer_write_pos;
		}

		// Note: we already substracted the read pos from the write pos here.
		memmove(s_video_buffer, g_pVideoData, s_buffer_write_pos);
		g_pVideoData = s_video_buffer;
	}
	// Copy new video instructions to s_video_buffer for future use in rendering the new picture
	memcpy(s_video_buffer + s_buffer_write_pos, data, len);
	s_buffer_write_pos += len;

	return len;
}

void ResetVideoBuffer()
{
	g_pVideoData = s_video_buffer;
	s_buffer_write_pos = 0;
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

		CommandProcessor::SetCpStatus();

		Common::AtomicStore(CommandProcessor::VITicks, CommandProcessor::m_cpClockOrigin);

		// check if we are able to run this buffer
		while (GpuRunningState && !CommandProcessor::interruptWaiting && fifo.bFF_GPReadEnable && fifo.CPReadWriteDistance && !AtBreakpoint())
		{
			fifo.isGpuReadingData = true;
			CommandProcessor::isPossibleWaitingSetDrawDone = fifo.bFF_GPLinkEnable ? true : false;

			if (!Core::g_CoreStartupParameter.bSyncGPU || Common::AtomicLoad(CommandProcessor::VITicks) > CommandProcessor::m_cpClockOrigin)
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

				cyclesExecuted = OpcodeDecoder_Run(g_bSkipCurrentFrame);

				if (Core::g_CoreStartupParameter.bSyncGPU && Common::AtomicLoad(CommandProcessor::VITicks) > cyclesExecuted)
					Common::AtomicAdd(CommandProcessor::VITicks, -(s32)cyclesExecuted);

				Common::AtomicStore(fifo.CPReadPointer, readPtr);
				Common::AtomicAdd(fifo.CPReadWriteDistance, -32);
				if ((GetVideoBufferEndPtr() - g_pVideoData) == 0)
					Common::AtomicStore(fifo.SafeCPReadPointer, fifo.CPReadPointer);
			}

			CommandProcessor::SetCpStatus();

			// This call is pretty important in DualCore mode and must be called in the FIFO Loop.
			// If we don't, s_swapRequested or s_efbAccessRequested won't be set to false
			// leading the CPU thread to wait in Video_BeginField or Video_AccessEFB thus slowing things down.
			VideoFifo_CheckAsyncRequest();
			CommandProcessor::isPossibleWaitingSetDrawDone = false;
		}

		fifo.isGpuReadingData = false;

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
}


bool AtBreakpoint()
{
	SCPFifoStruct &fifo = CommandProcessor::fifo;
	return fifo.bFF_BPEnable && (fifo.CPReadPointer == fifo.CPBreakpoint);
}

void RunGpu()
{
	SCPFifoStruct &fifo = CommandProcessor::fifo;
	while (fifo.bFF_GPReadEnable && fifo.CPReadWriteDistance && !AtBreakpoint() )
	{
		u8 *uData = Memory::GetPointer(fifo.CPReadPointer);

		FPURoundMode::SaveSIMDState();
		FPURoundMode::LoadDefaultSIMDState();
		ReadDataFromFifo(uData, 32);
		OpcodeDecoder_Run(g_bSkipCurrentFrame);
		FPURoundMode::LoadSIMDState();

		//DEBUG_LOG(COMMANDPROCESSOR, "Fifo wraps to base");

		if (fifo.CPReadPointer == fifo.CPEnd)
			fifo.CPReadPointer = fifo.CPBase;
		else
			fifo.CPReadPointer += 32;

		fifo.CPReadWriteDistance -= 32;
	}
	CommandProcessor::SetCpStatus();
}
