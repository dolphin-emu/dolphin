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
#include "Setup.h"
#include "MemoryUtil.h"
#include "Thread.h"
#include "Atomic.h"
#include "OpcodeDecoding.h"
#include "CommandProcessor.h"
#include "ChunkFile.h"
#include "Fifo.h"

volatile bool g_bSkipCurrentFrame = false;
volatile bool g_EFBAccessRequested = false;
extern u8* g_pVideoData;

namespace
{
static volatile bool fifoStateRun = false;
static volatile bool EmuRunning = false;
static u8 *videoBuffer;
static Common::EventEx fifo_run_event;
// STATE_TO_SAVE
static int size = 0;
}  // namespace

void Fifo_DoState(PointerWrap &p) 
{
	CommandProcessor::FifoCriticalEnter();

    p.DoArray(videoBuffer, FIFO_SIZE);
    p.Do(size);
	int pos = (int)(g_pVideoData - videoBuffer); // get offset
	p.Do(pos); // read or write offset (depends on the mode afaik)
	g_pVideoData = &videoBuffer[pos]; // overwrite g_pVideoData -> expected no change when load ss and change when save ss

	CommandProcessor::FifoCriticalLeave();
}

void Fifo_Init()
{
    videoBuffer = (u8*)AllocateMemoryPages(FIFO_SIZE);
	fifo_run_event.Init();
	fifoStateRun = false;
}

void Fifo_Shutdown()
{
	if (fifoStateRun) PanicAlert("Fifo shutting down while active");
	fifo_run_event.Shutdown();
    FreeMemoryPages(videoBuffer, FIFO_SIZE);
}

u8* FAKE_GetFifoStartPtr()
{
    return videoBuffer;
}

u8* FAKE_GetFifoEndPtr()
{
	return &videoBuffer[size];
}

void Fifo_SetRendering(bool enabled)
{
	g_bSkipCurrentFrame = !enabled;
}

// Executed from another thread, no the graphics thread!
// Basically, all it does is set a flag so that the loop will eventually exit, then
// waits for the event to be set, which happens when the loop does exit.
// If we look stuck in here, then the video thread is stuck in something and won't exit
// the loop. Switch to the video thread and investigate.
void Fifo_ExitLoop()
{
	Fifo_ExitLoopNonBlocking();
}

// May be executed from any thread, even the graphics thread.
// Created to allow for self shutdown.
void Fifo_ExitLoopNonBlocking()
{
	// This should break the wait loop in CPU thread
	CommandProcessor::fifo.bFF_GPReadEnable = false;
	CommandProcessor::SetFifoIdleFromVideoPlugin();
	// Terminate GPU thread loop
	fifoStateRun = false;
	fifo_run_event.Set();
}

void Fifo_RunLoop(bool run)
{
	EmuRunning = run;
	if (run)
		fifo_run_event.Set();
}

// Description: Fifo_EnterLoop() sends data through this function.
void Fifo_SendFifoData(u8* _uData, u32 len)
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


// Description: Main FIFO update loop
// Purpose: Keep the Core HW updated about the CPU-GPU distance
void Fifo_EnterLoop(const SVideoInitialize &video_initialize)
{

	fifoStateRun = true;
	SCPFifoStruct &_fifo = CommandProcessor::fifo;
	s32 distToSend;

	while (fifoStateRun)
	{
		video_initialize.pPeekMessages();

		VideoFifo_CheckAsyncRequest();

		// check if we are able to run this buffer		
		
		CommandProcessor::SetStatus();

		while (!CommandProcessor::interruptWaiting && _fifo.bFF_GPReadEnable &&
			(_fifo.CPReadWriteDistance && !AtBreakpoint() || CommandProcessor::OnOverflow))
		{
			// while the FIFO is processing data we activate this for sync with emulator thread.
			

			if (!fifoStateRun) break;

			
			CommandProcessor::FifoCriticalEnter();
			// Create pointer to video data and send it to the VideoPlugin
			u32 readPtr = _fifo.CPReadPointer;
			u8 *uData = video_initialize.pGetMemoryPointer(readPtr);

			distToSend = 32;

			if (readPtr == _fifo.CPEnd) 
				readPtr = _fifo.CPBase;
			else
				readPtr += 32;
			
			_assert_msg_(COMMANDPROCESSOR, (s32)_fifo.CPReadWriteDistance - distToSend >= 0 ,
			"Negative fifo.CPReadWriteDistance = %i in FIFO Loop !\nThat can produce inestabilty in the game. Please report it.", _fifo.CPReadWriteDistance - distToSend);
			

			// Execute new instructions found in uData
			Fifo_SendFifoData(uData, distToSend);											
			Common::AtomicStore(_fifo.CPReadPointer, readPtr);
			Common::AtomicAdd(_fifo.CPReadWriteDistance, -distToSend);						
		    
			CommandProcessor::isFifoBusy = true;
			CommandProcessor::SetStatus();
			
			_fifo.CPCmdIdle = false;

			OpcodeDecoder_Run(g_bSkipCurrentFrame);
			
			_fifo.CPCmdIdle = true;
			
			CommandProcessor::isFifoBusy = false;
			CommandProcessor::FifoCriticalLeave();

			// Those two are pretty important and must be called in the FIFO Loop.
			// If we don't, s_swapRequested (OGL only) or s_efbAccessRequested won't be set to false
			// leading the CPU thread to wait in Video_BeginField or Video_AccessEFB thus slowing things down.
			
			VideoFifo_CheckAsyncRequest();
									
		}
		
		
		CommandProcessor::SetFifoIdleFromVideoPlugin();
		if (EmuRunning)
			Common::YieldCPU();
		else
			fifo_run_event.MsgWait();
	}
}


bool AtBreakpoint()
{
	SCPFifoStruct &_fifo = CommandProcessor::fifo;
	return _fifo.bFF_BPEnable && (_fifo.CPReadPointer == _fifo.CPBreakpoint);
}
