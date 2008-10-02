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

#if defined(DATAREADER_INLINE)
extern u32 g_pVideoData;
#else
FifoReader fifo;
#endif

bool fifoStateRun = true;

// STATE_TO_SAVE
static u8 *videoBuffer;
static int size = 0;
static int readptr = 0;

void Fifo_DoState(PointerWrap &p) {
    p.DoArray(videoBuffer, FIFO_SIZE);
    p.Do(size);
    p.Do(readptr);
}

void Fifo_Init()
{
    videoBuffer = (u8*)AllocateMemoryPages(FIFO_SIZE);
#ifndef DATAREADER_INLINE
    fifo.Init(videoBuffer, videoBuffer);  //zero length. there is no data yet.
#endif
    fifoStateRun = true;
}

void Fifo_Shutdown()
{
    FreeMemoryPages(videoBuffer, FIFO_SIZE);
    fifoStateRun = false;
}

void Fifo_Stop() {
    fifoStateRun = false;
}

u32 FAKE_GetFifoStartPtr()
{
    return (int)videoBuffer;
}

int FAKE_GetFifoSize()
{
    if (size < readptr)
    {
        PanicAlert("GFX Fifo underrun encountered (size = %i, readptr = %i)", size, readptr);
    }
    return (size - readptr);
}
int FAKE_GetFifoEndAddr()
{
    return (int)(videoBuffer+size);
}

u8 FAKE_PeekFifo8(u32 _uOffset)
{
    return videoBuffer[readptr + _uOffset];
}

u16 FAKE_PeekFifo16(u32 _uOffset)
{
    return Common::swap16(*(u16*)&videoBuffer[readptr + _uOffset]);
}

u32 FAKE_PeekFifo32(u32 _uOffset)
{
    return Common::swap32(*(u32*)&videoBuffer[readptr + _uOffset]);
}

u8 FAKE_ReadFifo8()
{
    return videoBuffer[readptr++];
}

int FAKE_GetPosition()
{
    return readptr;
}

int FAKE_GetRealPtr()
{
	return (int)(videoBuffer+readptr);
}

u16 FAKE_ReadFifo16()
{
    u16 val = Common::swap16(*(u16*)(videoBuffer+readptr));
    readptr += 2;
    return val;
}

u32 FAKE_ReadFifo32()
{
    u32 val = Common::swap32(*(u32*)(videoBuffer+readptr));
    readptr += 4;
    return val;
}

void FAKE_SkipFifo(u32 skip)
{
    readptr += skip;
}

void Video_SendFifoData(u8* _uData)
{
	// TODO (mb2): unrolled loop faster than memcpy here?
    memcpy(videoBuffer + size, _uData, 32);
    size += 32;
    if (size + 32 >= FIFO_SIZE)
    {
		// TODO (mb2): Better and DataReader inline for DX9 
#ifdef DATAREADER_INLINE
		if (g_pVideoData) // for DX9 plugin "compatibility"
			readptr = g_pVideoData-(u32)videoBuffer;
#endif
        if (FAKE_GetFifoSize() > readptr)
        {
            PanicAlert("FIFO out of bounds (sz = %i, at %08x)", FAKE_GetFifoSize(), readptr);
        }
//        DebugLog("FAKE BUFFER LOOPS");
        memmove(&videoBuffer[0], &videoBuffer[readptr], FAKE_GetFifoSize());
        //		memset(&videoBuffer[FAKE_GetFifoSize()], 0, FIFO_SIZE - FAKE_GetFifoSize());
        size = FAKE_GetFifoSize();
        readptr = 0;
#ifdef DATAREADER_INLINE
		if (g_pVideoData) // for DX9 plugin "compatibility"
			g_pVideoData = FAKE_GetFifoStartPtr();
#endif
    }
    OpcodeDecoder_Run();
}


//TODO - turn inside out, have the "reader" ask for bytes instead
// See Core.cpp for threading idea
void Fifo_EnterLoop(const SVideoInitialize &video_initialize)
{
    SCPFifoStruct &_fifo = *video_initialize.pCPFifo;
#if defined(THREAD_VIDEO_WAKEUP_ONIDLE) && defined(_WIN32)
	HANDLE hEventOnIdle= OpenEventA(EVENT_ALL_ACCESS,FALSE,(LPCSTR)"EventOnIdle");
	if (hEventOnIdle==NULL) PanicAlert("Fifo_EnterLoop() -> EventOnIdle NULL");
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
        if (_fifo.CPReadWriteDistance < 1) //fifo.CPLoWatermark)
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
              int count = 200;
            while(_fifo.CPReadWriteDistance > 0 && count)
#endif
			{
                // check if we are on a breakpoint
                if (_fifo.bFF_BPEnable)
                {
                    if (_fifo.CPReadPointer == _fifo.CPBreakpoint)
                    {
                        _fifo.bFF_Breakpoint = 1; 
                        video_initialize.pUpdateInterrupts(); 
                        break;
                    }
                }

                // read the data and send it to the VideoPlugin
				u8 *uData = video_initialize.pGetMemoryPointer(_fifo.CPReadPointer);
#ifdef _WIN32
                EnterCriticalSection(&_fifo.sync);
#else
                _fifo.sync->Enter();
#endif
                _fifo.CPReadPointer += 32;
                Video_SendFifoData(uData);
#ifdef _WIN32
                InterlockedExchangeAdd((LONG*)&_fifo.CPReadWriteDistance, -32);
                LeaveCriticalSection(&_fifo.sync);
#else 
                  _fifo.CPReadWriteDistance -= 32;
                  _fifo.sync->Leave();
#endif
                // increase the ReadPtr
                if (_fifo.CPReadPointer >= _fifo.CPEnd)
                {
                    _fifo.CPReadPointer = _fifo.CPBase;				
                    //LOG(COMMANDPROCESSOR, "BUFFER LOOP");
                }
#ifndef THREAD_VIDEO_WAKEUP_ONIDLE
                count--;
#endif
            }
        }

    }
#if defined(THREAD_VIDEO_WAKEUP_ONIDLE) && defined(_WIN32)
	CloseHandle(hEventOnIdle);
#endif
}

