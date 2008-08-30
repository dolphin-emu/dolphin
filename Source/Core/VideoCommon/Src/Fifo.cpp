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

#define FIFO_SIZE (1024*1024)

FifoReader fifo;

// STATE_TO_SAVE
static u8 *videoBuffer;
static int size = 0;
static int readptr = 0;

void Fifo_DoState(PointerWrap &p) {
    p.DoArray(videoBuffer, size);
    p.Do(size);
    p.Do(readptr);
}

void Fifo_Init()
{
    videoBuffer = (u8*)AllocateMemoryPages(FIFO_SIZE);
    fifo.Init(videoBuffer, videoBuffer);  //zero length. there is no data yet.
}

void Fifo_Shutdown()
{
    FreeMemoryPages(videoBuffer, FIFO_SIZE);
}

int FAKE_GetFifoSize()
{
    if (size < readptr)
    {
        PanicAlert("GFX Fifo underrun encountered (size = %i, readptr = %i)", size, readptr);
    }
    return (size - readptr);
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

void Video_SendFifoData(BYTE *_uData)
{
    memcpy(videoBuffer + size, _uData, 32);
    size += 32;
    if (size + 32 >= FIFO_SIZE)
    {
        if (FAKE_GetFifoSize() > readptr)
        {
            PanicAlert("FIFO out of bounds (sz = %i, at %08x)", FAKE_GetFifoSize(), readptr);
        }
//        DebugLog("FAKE BUFFER LOOPS");
        memmove(&videoBuffer[0], &videoBuffer[readptr], FAKE_GetFifoSize());
        //		memset(&videoBuffer[FAKE_GetFifoSize()], 0, FIFO_SIZE - FAKE_GetFifoSize());
        size = FAKE_GetFifoSize();
        readptr = 0;
    }
    OpcodeDecoder_Run();
}


//TODO - turn inside out, have the "reader" ask for bytes instead
// See Core.cpp for threading idea
void Fifo_EnterLoop(const SVideoInitialize &video_initialize)
{
    SCPFifoStruct &fifo = *video_initialize.pCPFifo;

    // TODO(ector): Don't peek so often!
    while (video_initialize.pPeekMessages())
    {
        if (fifo.CPReadWriteDistance < 1) //fifo.CPLoWatermark)
            Common::SleepCurrentThread(1);
        //etc...

        // check if we are able to run this buffer
        if ((fifo.bFF_GPReadEnable) && !(fifo.bFF_BPEnable && fifo.bFF_Breakpoint))
        {
            int count = 200;
            while(fifo.CPReadWriteDistance > 0 && count)
            {
                // check if we are on a breakpoint
                if (fifo.bFF_BPEnable)
                {
                    if (fifo.CPReadPointer == fifo.CPBreakpoint)
                    {
                        fifo.bFF_Breakpoint = 1; 
                        video_initialize.pUpdateInterrupts(); 
                        break;
                    }
                }

                // read the data and send it to the VideoPlugin
				u8 *uData = video_initialize.pGetMemoryPointer(fifo.CPReadPointer);
#ifdef _WIN32
                EnterCriticalSection(&fifo.sync);
#endif
                fifo.CPReadPointer += 32;
                Video_SendFifoData(uData);
#ifdef _WIN32
                InterlockedExchangeAdd((LONG*)&fifo.CPReadWriteDistance, -32);
                LeaveCriticalSection(&fifo.sync);
#endif
                // increase the ReadPtr
                if (fifo.CPReadPointer >= fifo.CPEnd)
                {
                    fifo.CPReadPointer = fifo.CPBase;				
                    //LOG(COMMANDPROCESSOR, "BUFFER LOOP");
                }
                count--;
            }
        }

    }
}

