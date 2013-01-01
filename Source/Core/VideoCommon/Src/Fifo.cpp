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

#include "VideoConfig.h"
#include "MemoryUtil.h"
#include "Thread.h"
#include "Atomic.h"
#include "OpcodeDecoding.h"
#include "CommandProcessor.h"
#include "PixelEngine.h"
#include "ChunkFile.h"
#include "Fifo.h"
#include "HW/Memmap.h"
#include "Core.h"

volatile bool g_bSkipCurrentFrame = false;
extern u8* g_pVideoData;

namespace
{
static volatile bool GpuRunningState = false;
static volatile bool EmuRunningState = false;
static std::mutex m_csHWVidOccupied;
// STATE_TO_SAVE
static u8 *videoBuffer;
static int size = 0;
}  // namespace

void Fifo_DoState(PointerWrap &p) 
{
    p.DoArray(videoBuffer, FIFO_SIZE);
    p.Do(size);
    p.DoPointer(g_pVideoData, videoBuffer);
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
    videoBuffer = (u8*)AllocateMemoryPages(FIFO_SIZE);
	size = 0;
	GpuRunningState = false;
}

void Fifo_Shutdown()
{
	if (GpuRunningState) PanicAlert("Fifo shutting down while active");
    FreeMemoryPages(videoBuffer, FIFO_SIZE);
}

u8* GetVideoBufferStartPtr()
{
    return videoBuffer;
}

u8* GetVideoBufferEndPtr()
{
	return &videoBuffer[size];
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
	while(fifo.isGpuReadingData) Common::YieldCPU();
	// Terminate GPU thread loop
	GpuRunningState = false;
	EmuRunningState = true;
}

void EmulatorState(bool running)
{
	EmuRunningState = running;
}


// Description: RunGpuLoop() sends data through this function.
void ReadDataFromFifo(u8* _uData, u32 len)
{
    if (size + len >= FIFO_SIZE)
    {
		int pos = (int)(g_pVideoData - videoBuffer);
		size -= pos;
        if (size + len > FIFO_SIZE)
        {
            PanicAlert("FIFO out of bounds (sz = %i, at %08x)", size, pos);
        }
        memmove(&videoBuffer[0], &videoBuffer[pos], size);
		g_pVideoData = videoBuffer;
    }
	// Copy new video instructions to videoBuffer for future use in rendering the new picture
    memcpy(videoBuffer + size, _uData, len);
    size += len;
}

void ResetVideoBuffer()
{
	g_pVideoData = videoBuffer;
	size = 0;
}


// Description: Main FIFO update loop
// Purpose: Keep the Core HW updated about the CPU-GPU distance
void RunGpuLoop()
{
	std::lock_guard<std::mutex> lk(m_csHWVidOccupied);
	GpuRunningState = true;
	SCPFifoStruct &fifo = CommandProcessor::fifo;

	while (GpuRunningState)
	{
		g_video_backend->PeekMessages();

		VideoFifo_CheckAsyncRequest();
				
		CommandProcessor::SetCpStatus();
		// check if we are able to run this buffer	
		while (GpuRunningState && !CommandProcessor::interruptWaiting && fifo.bFF_GPReadEnable && fifo.CPReadWriteDistance && !AtBreakpoint() && !PixelEngine::WaitingForPEInterrupt())
		{
			if (!GpuRunningState) break;

			fifo.isGpuReadingData = true;
			CommandProcessor::isPossibleWaitingSetDrawDone = fifo.bFF_GPLinkEnable ? true : false;
			
			u32 readPtr = fifo.CPReadPointer;
			u8 *uData = Memory::GetPointer(readPtr);

			if (readPtr == fifo.CPEnd) readPtr = fifo.CPBase;
				else readPtr += 32;
			
			_assert_msg_(COMMANDPROCESSOR, (s32)fifo.CPReadWriteDistance - 32 >= 0 ,
			"Negative fifo.CPReadWriteDistance = %i in FIFO Loop !\nThat can produce inestabilty in the game. Please report it.", fifo.CPReadWriteDistance - 32);
			
			ReadDataFromFifo(uData, 32);	
			
			OpcodeDecoder_Run(g_bSkipCurrentFrame);	

			Common::AtomicStore(fifo.CPReadPointer, readPtr);
			Common::AtomicAdd(fifo.CPReadWriteDistance, -32);						
			if((GetVideoBufferEndPtr() - g_pVideoData) == 0)
				Common::AtomicStore(fifo.SafeCPReadPointer, fifo.CPReadPointer);
			CommandProcessor::SetCpStatus();
		
			// This call is pretty important in DualCore mode and must be called in the FIFO Loop.
			// If we don't, s_swapRequested or s_efbAccessRequested won't be set to false
			// leading the CPU thread to wait in Video_BeginField or Video_AccessEFB thus slowing things down.
			VideoFifo_CheckAsyncRequest();		
			CommandProcessor::isPossibleWaitingSetDrawDone = false;
		}
		
		fifo.isGpuReadingData = false;
		

		if (EmuRunningState)
			Common::YieldCPU();
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
			
			SaveSSEState();
			LoadDefaultSSEState();
			ReadDataFromFifo(uData, 32);				
			OpcodeDecoder_Run(g_bSkipCurrentFrame);	
			LoadSSEState();

			//DEBUG_LOG(COMMANDPROCESSOR, "Fifo wraps to base");

			if (fifo.CPReadPointer == fifo.CPEnd) fifo.CPReadPointer = fifo.CPBase;
				else fifo.CPReadPointer += 32;

			fifo.CPReadWriteDistance -= 32;											
		}
		CommandProcessor::SetCpStatus();
}
