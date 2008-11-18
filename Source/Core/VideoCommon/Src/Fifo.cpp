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

#include "MemoryUtil.h"
#include "Thread.h"
#include "OpcodeDecoding.h"

#include "Fifo.h"

extern u8* g_pVideoData;

bool fifoStateRun = true;

// STATE_TO_SAVE
static u8 *videoBuffer;
static int size = 0;

void Fifo_DoState(PointerWrap &p) 
{
    p.DoArray(videoBuffer, FIFO_SIZE);
    p.Do(size);
	int pos = (int)(g_pVideoData-videoBuffer); // get offset
	p.Do(pos); // read or write offset (depends on the mode afaik)
	g_pVideoData = &videoBuffer[pos]; // overwrite g_pVideoData -> expected no change when load ss and change when save ss
}

void Fifo_Init()
{
    videoBuffer = (u8*)AllocateMemoryPages(FIFO_SIZE);
    fifoStateRun = true;
}

void Fifo_Shutdown()
{
    FreeMemoryPages(videoBuffer, FIFO_SIZE);
    fifoStateRun = false;
}

void Fifo_Stop() 
{
    fifoStateRun = false;
}

u8* FAKE_GetFifoStartPtr()
{
    return videoBuffer;
}

u8* FAKE_GetFifoEndPtr()
{
	return &videoBuffer[size];
}

void Video_SendFifoData(u8* _uData, u32 len)
{
    if (size + len >= FIFO_SIZE)
    {
		int pos = (int)(g_pVideoData-videoBuffer);
        if (size-pos > pos)
        {
            PanicAlert("FIFO out of bounds (sz = %i, at %08x)", size, pos);
        }
        memmove(&videoBuffer[0], &videoBuffer[pos], size - pos );
        size -= pos;
		g_pVideoData = FAKE_GetFifoStartPtr();
    }
    memcpy(videoBuffer + size, _uData, len);
    size += len;
    OpcodeDecoder_Run();
}

// for GP watchdog hack
THREAD_RETURN GPWatchdogThread(void *pArg)
{
	const SCPFifoStruct &_fifo = *(SCPFifoStruct*)pArg;
	u32 lastToken = 0;
	u32 currentToken = 0;
	int FourMsCount = 0;

	Common::SetCurrentThreadName("GPWatchdogThread");
	
	while (_fifo.bFF_GPReadEnable != ~0UL) // blah
	{
		// 4 ms should be enough insignificant
		Common::SleepCurrentThread(4);
		currentToken = _fifo.Fake_GPWDToken;
		if (lastToken == currentToken)
		{
			FourMsCount++;
			// Threshold quite arbitrary.
			// Assuming the PPC-frame-finish-watchdog use RTC(TOCHECK) and throw its exception after several times the normal frame rate
			// I tested higher frame-periode-factor but 3 might be safe enough for DC stability for everyone.
			// I may be wrong, so TOTEST on different machine like hell !!!
			if (FourMsCount >= 3*16/4)//  frame_periode_factor(3) * frame_periode(~16ms) / ms_step(4)
			{
#ifdef _WIN32
				InterlockedExchange((LONG*)&_fifo.Fake_GPWDInterrupt, 1); 
#else
                Common::InterlockedExchange((int*)&_fifo.Fake_GPWDInterrupt, 1);
#endif 
			//__Log(LogTypes::VIDEO,"!!! Watchdog hit",_fifo.CPReadWriteDistance);
			}
		}
		else
			FourMsCount = 0;

		lastToken = currentToken;
	}
	return 0;
}


void Fifo_EnterLoop(const SVideoInitialize &video_initialize)
{
    SCPFifoStruct &_fifo = *video_initialize.pCPFifo;
	u32 distToSend;
#if defined(THREAD_VIDEO_WAKEUP_ONIDLE) && defined(_WIN32)
	HANDLE hEventOnIdle= OpenEventA(EVENT_ALL_ACCESS,FALSE,(LPCSTR)"EventOnIdle");
	if (hEventOnIdle==NULL) PanicAlert("Fifo_EnterLoop() -> EventOnIdle NULL");
#endif

	// for GP watchdog hack
	Common::Thread *watchdogThread = NULL;
	watchdogThread = new Common::Thread(GPWatchdogThread, (void*)&_fifo);
	// TODO (mb2): figure out why doesn't work on core 2 ???
	// may have to force it for DualCores
	//watchdogThread->SetAffinity(1);
#ifdef _WIN32
	SetThreadPriority(watchdogThread, THREAD_PRIORITY_ABOVE_NORMAL);
#else
	//TODO
#endif

#ifdef _WIN32
    // TODO(ector): Don't peek so often!
    while (video_initialize.pPeekMessages())
#else 
    while (fifoStateRun)
#endif
    {
#if defined(THREAD_VIDEO_WAKEUP_ONIDLE) && defined(_WIN32)
	if (MsgWaitForMultipleObjects(1, &hEventOnIdle, FALSE, 1L, QS_ALLEVENTS) == WAIT_ABANDONED)
		break;
#endif
        //if (_fifo.CPReadWriteDistance < 1) 
        if (_fifo.CPReadWriteDistance < _fifo.CPLoWatermark)
#if defined(THREAD_VIDEO_WAKEUP_ONIDLE) && defined(_WIN32)
            continue;
#else
			Common::SleepCurrentThread(1);
#endif
        //etc...

        // check if we are able to run this buffer
        if ((_fifo.bFF_GPReadEnable) && !(_fifo.bFF_BPEnable && _fifo.bFF_Breakpoint))
        {
#if defined(THREAD_VIDEO_WAKEUP_ONIDLE) && defined(_WIN32)
            while(_fifo.CPReadWriteDistance > 0)
#else
            while(_fifo.bFF_GPReadEnable && (_fifo.CPReadWriteDistance > 0) )
#endif
			{
                // read the data and send it to the VideoPlugin
                u8 *uData = video_initialize.pGetMemoryPointer(_fifo.CPReadPointer);

				u32 readPtr = _fifo.CPReadPointer;
				// if we are on BP mode we must send 32B chunks to Video plugin
				// for BP checking
				if (_fifo.bFF_BPEnable)
                {
					distToSend = 32;
					readPtr += 32;
					if (readPtr == _fifo.CPBreakpoint)
                    {
						video_initialize.pLog("!!! BP irq raised",FALSE);
						//PanicAlert("!!! BP irq raised");
                        #ifdef _WIN32
						InterlockedExchange((LONG*)&_fifo.bFF_Breakpoint, 1);
						#else
						Common::InterlockedExchange((int*)&_fifo.bFF_Breakpoint, 1);
						#endif
                        video_initialize.pUpdateInterrupts();
                        break;
                    }
                }
				else
				{
					distToSend = _fifo.CPReadWriteDistance;
					if ( (distToSend+readPtr) > _fifo.CPEnd) // TODO: better
					{
						distToSend =_fifo.CPEnd - readPtr;
						readPtr = _fifo.CPBase;
					}
					else
						readPtr += distToSend; 
				}
				// TODO (mb2): add warning comments here for BP irq
				// sending the whole CPReadWriteDistance most of the time
				Video_SendFifoData(uData, distToSend);
#ifdef _WIN32
                InterlockedExchange((LONG*)&_fifo.CPReadPointer, readPtr);
                InterlockedExchangeAdd((LONG*)&_fifo.CPReadWriteDistance, -distToSend);
#else 
                Common::InterlockedExchange((int*)&_fifo.CPReadPointer, readPtr);
                Common::InterlockedExchangeAdd((int*)&_fifo.CPReadWriteDistance, -distToSend);
#endif
			}
        }
    }
#if defined(THREAD_VIDEO_WAKEUP_ONIDLE) && defined(_WIN32)
	CloseHandle(hEventOnIdle);
#endif
	// for GP watchdog DC hack
	// dummy finish signal to watchdog
	_fifo.bFF_GPReadEnable = ~0;
	if(watchdogThread)
		watchdogThread->WaitForDeath();

}

