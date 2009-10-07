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

#include <string.h>

#include "VideoConfig.h"
#include "Setup.h"
#include "MemoryUtil.h"
#include "Thread.h"
#include "Atomic.h"
#include "OpcodeDecoding.h"

#include "Fifo.h"

volatile bool g_bSkipCurrentFrame = false;
volatile bool g_EFBAccessRequested = false;
extern u8* g_pVideoData;

namespace
{
static volatile bool fifoStateRun = false;
static u8 *videoBuffer;
static Common::Event fifo_exit_event;
static Common::CriticalSection s_criticalFifo;
// STATE_TO_SAVE
static int size = 0;
}  // namespace

void Fifo_DoState(PointerWrap &p) 
{
	s_criticalFifo.Enter();

    p.DoArray(videoBuffer, FIFO_SIZE);
    p.Do(size);
	int pos = (int)(g_pVideoData - videoBuffer); // get offset
	p.Do(pos); // read or write offset (depends on the mode afaik)
	g_pVideoData = &videoBuffer[pos]; // overwrite g_pVideoData -> expected no change when load ss and change when save ss

	s_criticalFifo.Leave();
}

void Fifo_Init()
{
    videoBuffer = (u8*)AllocateMemoryPages(FIFO_SIZE);
	fifo_exit_event.Init();
	fifoStateRun = false;
}

void Fifo_Shutdown()
{
	if (fifoStateRun)
		PanicAlert("Fifo shutting down while active");
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
    fifoStateRun = false;

	fifo_exit_event.MsgWait();
	fifo_exit_event.Shutdown();
}

// May be executed from any thread, even the graphics thread.
// Created to allow for self shutdown.
void Fifo_ExitLoopNonBlocking()
{
	fifoStateRun = false;
}

// Description: Fifo_EnterLoop() sends data through this function.
void Fifo_SendFifoData(u8* _uData, u32 len)
{
    if (size + len >= FIFO_SIZE)
    {
		int pos = (int)(g_pVideoData - videoBuffer);
        if (size - pos > pos)
        {
            PanicAlert("FIFO out of bounds (sz = %i, at %08x)", size, pos);
        }
        memmove(&videoBuffer[0], &videoBuffer[pos], size - pos);
        size -= pos;
		g_pVideoData = FAKE_GetFifoStartPtr();
    }
	// Copy new video instructions to videoBuffer for future use in rendering the new picture
    memcpy(videoBuffer + size, _uData, len);
    size += len;
    OpcodeDecoder_Run(g_bSkipCurrentFrame);
}

// Description: Main FIFO update loop
// Purpose: Keep the Core HW updated about the CPU-GPU distance
void Fifo_EnterLoop(const SVideoInitialize &video_initialize)
{
    fifoStateRun = true;
    SCPFifoStruct &_fifo = *video_initialize.pCPFifo;
	s32 distToSend;

    while (fifoStateRun)
    {
		video_initialize.pPeekMessages();
		if (g_ActiveConfig.bEFBAccessEnable)
			VideoFifo_CheckEFBAccess();
		VideoFifo_CheckSwapRequest();

		s_criticalFifo.Enter();

        // check if we are able to run this buffer
        if ((_fifo.bFF_GPReadEnable) && _fifo.CPReadWriteDistance && !(_fifo.bFF_BPEnable && _fifo.bFF_Breakpoint))
        {
			Common::AtomicStore(_fifo.CPReadIdle, 0);

            do
			{
				if (!fifoStateRun)
					break;

				// Get the Read Pointer
                // Create pointer to video data and send it to the VideoPlugin
				u32 readPtr = _fifo.CPReadPointer;

				// Set distance
				// if we are on BP mode we must send 32B chunks to Video plugin for BP checking
				distToSend  = _fifo.bFF_BPEnable ? 32 : _fifo.CPReadWriteDistance;
				// send 1024B chunk max length to have better control over PeekMessages' period
				//distToSend = distToSend > 1024 ? 1024 : distToSend;

				// Get Data based on pointer
				u8 *uData = video_initialize.pGetMemoryPointer(readPtr);

				// Check Pointer and move pointer
				if ((distToSend + readPtr) >= _fifo.CPEnd)
				{
					distToSend =_fifo.CPEnd - readPtr;
					readPtr = _fifo.CPBase;
				}
				else
					readPtr += distToSend;

				// Execute new instructions found in uData
				Fifo_SendFifoData(uData, distToSend);

				//Check BP Mode - Pokemon XD wants this to be called after the pointer move
				if (_fifo.bFF_BPEnable && (readPtr == _fifo.CPBreakpoint))
                {
						Common::AtomicStore(_fifo.bFF_Breakpoint, 1);
                        video_initialize.pUpdateInterrupts();
                }

				// Update CPReadPointer and RWDistance
                Common::AtomicStore(_fifo.CPReadPointer, readPtr);
                Common::AtomicAdd(_fifo.CPReadWriteDistance, -distToSend);


			} while (_fifo.bFF_GPReadEnable && _fifo.CPReadWriteDistance  && !(_fifo.bFF_BPEnable && _fifo.bFF_Breakpoint));

			Common::AtomicStore(_fifo.CPReadIdle, 1);
			video_initialize.pSetFifoIdle();
        }
		else
		{
			Common::YieldCPU();
		}

		s_criticalFifo.Leave();
    }
	fifo_exit_event.Set();
}

