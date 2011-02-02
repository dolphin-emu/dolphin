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
#include "HW/Memmap.h"

volatile bool g_bSkipCurrentFrame = false;
extern u8* g_pVideoData;

namespace
{
static volatile bool fifoStateRun = false;
static volatile bool EmuRunning = false;
static u8 *videoBuffer;
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
	fifoStateRun = false;
}

void Fifo_Shutdown()
{
	if (fifoStateRun) PanicAlert("Fifo shutting down while active");
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

// May be executed from any thread, even the graphics thread.
// Created to allow for self shutdown.
void Fifo_ExitLoop()
{
	// This should break the wait loop in CPU thread
	CommandProcessor::fifo.bFF_GPReadEnable = false;
	CommandProcessor::SetFifoIdleFromVideoPlugin();
	// Terminate GPU thread loop
	fifoStateRun = false;
	EmuRunning = true;
}

void Fifo_RunLoop(bool run)
{
	EmuRunning = run;
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

void ResetVideoBuffer()
{
	g_pVideoData = videoBuffer;
	size = 0;
}


// Description: Main FIFO update loop
// Purpose: Keep the Core HW updated about the CPU-GPU distance
void Fifo_EnterLoop()
{
	fifoStateRun = true;
	SCPFifoStruct &_fifo = CommandProcessor::fifo;
	s32 distToSend;

	while (fifoStateRun)
	{
		g_video_backend->PeekMessages();

		VideoFifo_CheckAsyncRequest();

		// check if we are able to run this buffer		
		
		CommandProcessor::SetStatus();

		while (!CommandProcessor::interruptWaiting && _fifo.bFF_GPReadEnable &&
			_fifo.CPReadWriteDistance && (!AtBreakpoint() || CommandProcessor::OnOverflow))
		{
			// while the FIFO is processing data we activate this for sync with emulator thread.
			
			CommandProcessor::isFifoBusy = true;

			if (!fifoStateRun) break;

			CommandProcessor::FifoCriticalEnter();

			// Create pointer to video data and send it to the VideoPlugin
			u32 readPtr = _fifo.CPReadPointer;
			u8 *uData = Memory::GetPointer(readPtr);

			distToSend = 32;

			if (readPtr == _fifo.CPEnd) 
				readPtr = _fifo.CPBase;
			else
				readPtr += 32;
			
			_assert_msg_(COMMANDPROCESSOR, (s32)_fifo.CPReadWriteDistance - distToSend >= 0 ,
			"Negative fifo.CPReadWriteDistance = %i in FIFO Loop !\nThat can produce inestabilty in the game. Please report it.", _fifo.CPReadWriteDistance - distToSend);
			
			// Execute new instructions found in uData
			Fifo_SendFifoData(uData, distToSend);	
			
			OpcodeDecoder_Run(g_bSkipCurrentFrame);	

			Common::AtomicStore(_fifo.CPReadPointer, readPtr);
			Common::AtomicAdd(_fifo.CPReadWriteDistance, -distToSend);						
		    			
			CommandProcessor::SetStatus();

			CommandProcessor::FifoCriticalLeave();
		
			// This call is pretty important in DualCore mode and must be called in the FIFO Loop.
			// If we don't, s_swapRequested or s_efbAccessRequested won't be set to false
			// leading the CPU thread to wait in Video_BeginField or Video_AccessEFB thus slowing things down.
			VideoFifo_CheckAsyncRequest();					
		}

		CommandProcessor::isFifoBusy = false;
		CommandProcessor::SetFifoIdleFromVideoPlugin();

		if (EmuRunning)
			Common::YieldCPU();
		else
		{
			// While the emu is paused, we still handle async request such as Savestates then sleep.
			while (!EmuRunning)
			{
				g_video_backend->PeekMessages();
				VideoFifo_CheckAsyncRequest();
				Common::SleepCurrentThread(10);
			}
		}
	}
}


bool AtBreakpoint()
{
	SCPFifoStruct &_fifo = CommandProcessor::fifo;
	return _fifo.bFF_BPEnable && (_fifo.CPReadPointer == _fifo.CPBreakpoint);
}
