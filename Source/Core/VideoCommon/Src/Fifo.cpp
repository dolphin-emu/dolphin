// Copyright (C) 2003-2008 Dolphin Project.

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

#include "Setup.h"
#ifdef SETUP_TIMER_WAITING
	#include "../../../Plugins/Plugin_VideoOGL/Src/OS/Win32.h"
#endif
#include "MemoryUtil.h"
#include "Thread.h"
#include "OpcodeDecoding.h"
#include "ConsoleWindow.h"

#include "Fifo.h"


// TODO (mb2): move/rm this global
volatile u32 g_XFBUpdateRequested = FALSE;
extern u8* g_pVideoData;

namespace {

static volatile bool fifoStateRun = false;
static u8 *videoBuffer;
static Common::Event fifo_exit_event;
// STATE_TO_SAVE
static int size = 0;

}  // namespace

void Fifo_DoState(PointerWrap &p) 
{
    p.DoArray(videoBuffer, FIFO_SIZE);
    p.Do(size);
	int pos = (int)(g_pVideoData - videoBuffer); // get offset
	p.Do(pos); // read or write offset (depends on the mode afaik)
	g_pVideoData = &videoBuffer[pos]; // overwrite g_pVideoData -> expected no change when load ss and change when save ss
}

void Fifo_Init()
{
    videoBuffer = (u8*)AllocateMemoryPages(FIFO_SIZE);
	fifo_exit_event.Init();
	fifoStateRun = false;
	g_XFBUpdateRequested = FALSE;
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

// The loop in EnterLoop sends data through this function.
// TODO: Possibly inline it? This one is exported so it will likely not be inlined at all.
void Video_SendFifoData(u8* _uData, u32 len)
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
    memcpy(videoBuffer + size, _uData, len);
    size += len;
    OpcodeDecoder_Run();
}

// Executed from another thread, no the graphics thread!
// Basically, all it does is set a flag so that the loop will eventually exit, then
// waits for the event to be set, which happens when the loop does exit.
// If we look stuck in here, then the video thread is stuck in something and won't exit
// the loop. Switch to the video thread and investigate.
void Fifo_ExitLoop()
{
    fifoStateRun = false;
	#ifndef SETUP_TIMER_WAITING
		fifo_exit_event.MsgWait();
		fifo_exit_event.Shutdown();
	#else
	//Console::Print("Video: Fifo_ExitLoop: Done:%i\n", fifo_exit_event.DoneWait());
		if (fifo_exit_event.TimerWait(Fifo_ExitLoop))
		{
			//Console::Print("Video: Fifo_Shutdown: Done:%i\n\n", fifo_exit_event.DoneWait());
			fifo_exit_event.Shutdown();
			PostMessage(EmuWindow::GetParentWnd(), WM_USER, OPENGL_VIDEO_STOP, 0);
		}
	#endif
}

// May be executed from any thread, even the graphics thread.
// Created to allow for self shutdown.
void Fifo_ExitLoopNonBlocking() {
	fifoStateRun = false;
}

// 
void Fifo_EnterLoop(const SVideoInitialize &video_initialize)
{
    fifoStateRun = true;
    SCPFifoStruct &_fifo = *video_initialize.pCPFifo;
	s32 distToSend;

    while (fifoStateRun)
    {
#if defined(_WIN32) && !defined(SETUP_AVOID_OPENGL_SCREEN_MESSAGE_HANG)
		video_initialize.pPeekMessages();
#endif

        if (_fifo.CPReadWriteDistance == 0)
			Common::SleepCurrentThread(1);

		// Draw XFB if CP/GPfifo isn't used
		if (g_XFBUpdateRequested)
		{
			Video_UpdateXFB(NULL, 0, 0, 0, FALSE);
			video_initialize.pCopiedToXFB();
		}

        // check if we are able to run this buffer
        if ((_fifo.bFF_GPReadEnable) && _fifo.CPReadWriteDistance && !(_fifo.bFF_BPEnable && _fifo.bFF_Breakpoint))
        {
			Common::SyncInterlockedExchange((LONG*)&_fifo.CPReadIdle, 0);
			//video_initialize.pLog("RUN...........................",FALSE);
			int peek_counter = 0;
            while (_fifo.bFF_GPReadEnable && _fifo.CPReadWriteDistance)
			{
				peek_counter++;
				if (peek_counter == 1000) {
					video_initialize.pPeekMessages();
					peek_counter = 0;
				}
                // read the data and send it to the VideoPlugin
				u32 readPtr = _fifo.CPReadPointer;
                u8 *uData = video_initialize.pGetMemoryPointer(readPtr);

				// if we are on BP mode we must send 32B chunks to Video plugin for BP checking
				// TODO (mb2): test & check if MP1/MP2 realy need this now.
				if (_fifo.bFF_BPEnable)
                {
					if (readPtr == _fifo.CPBreakpoint)
                    {
						video_initialize.pLog("!!! BP irq raised",FALSE);
						Common::SyncInterlockedExchange((LONG*)&_fifo.bFF_Breakpoint, 1);

                        video_initialize.pUpdateInterrupts();
                        break;
                    }
					distToSend = 32;
					readPtr += 32;
					if ( readPtr >= _fifo.CPEnd) 
						readPtr = _fifo.CPBase;
                }
				else
				{
#if 0 // ugly random GP slowdown for testing DC robustness... TODO: remove when completly sure DC is ok
					int r = rand();
					if ((r & 0xF) == r)
						Common::SleepCurrentThread(r);
					distToSend = 32;
					readPtr += 32;
					if (readPtr >= _fifo.CPEnd) 
						readPtr = _fifo.CPBase;
#else
					distToSend = _fifo.CPReadWriteDistance;
					// send 1024B chunk max length to have better control over PeekMessages' period
					distToSend = distToSend > 1024 ? 1024 : distToSend;
					if ((distToSend + readPtr) >= _fifo.CPEnd) // TODO: better?
					{
						distToSend =_fifo.CPEnd - readPtr;
						readPtr = _fifo.CPBase;
					}
					else
						readPtr += distToSend; 
#endif
				}
				Video_SendFifoData(uData, distToSend);
                Common::SyncInterlockedExchange((LONG*)&_fifo.CPReadPointer, readPtr);
                Common::SyncInterlockedExchangeAdd((LONG*)&_fifo.CPReadWriteDistance, -distToSend);
			}
			//video_initialize.pLog("..........................IDLE",FALSE);
			Common::SyncInterlockedExchange((LONG*)&_fifo.CPReadIdle, 1);
        }
    }
	fifo_exit_event.Set();
	#ifdef SETUP_TIMER_WAITING
		fifo_exit_event.SetTimer();
	#endif
}
