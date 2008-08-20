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

#include <stdio.h>
#include <stdlib.h>

#include "Common.h"
#include "Utils.h"
#include "Fifo.h"
#include "main.h"
#include "OpcodeDecoding.h"

#define FIFO_SIZE (1024*1024)

FifoReader fifo;
static u8 *videoBuffer;

int size = 0;
int readptr = 0;

void Fifo_Init()
{
	//VirtualFree((LPVOID)buffer,CMDBUFFER_SIZE,MEM_RELEASE);
	videoBuffer = (u8*)VirtualAlloc(0, FIFO_SIZE, MEM_COMMIT, PAGE_READWRITE);
	fifo.Init(videoBuffer, videoBuffer); //zero length. there is no data yet.
}

void Fifo_Shutdown()
{
	VirtualFree(videoBuffer, FIFO_SIZE, MEM_RELEASE);
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
	return _byteswap_ushort(*(u16*)&videoBuffer[readptr + _uOffset]);
}

u32 FAKE_PeekFifo32(u32 _uOffset)
{
	return _byteswap_ulong(*(u32*)&videoBuffer[readptr + _uOffset]);
}

u8 FAKE_ReadFifo8()
{
	return videoBuffer[readptr++];
}

u16 FAKE_ReadFifo16()
{
	u16 val = _byteswap_ushort(*(u16*)(videoBuffer+readptr));
	readptr += 2;
	return val;
}

u32 FAKE_ReadFifo32()
{
	u32 val = _byteswap_ulong(*(u32*)(videoBuffer+readptr));
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
			PanicAlert("FIFO out of bounds", "video-plugin", MB_OK);
			exit(1);
		}

		DebugLog("FAKE BUFFER LOOPS");
		memmove(&videoBuffer[0], &videoBuffer[readptr], FAKE_GetFifoSize());
		//		memset(&videoBuffer[FAKE_GetFifoSize()], 0, FIFO_SIZE - FAKE_GetFifoSize());
		size = FAKE_GetFifoSize();
		readptr = 0;
	}
	OpcodeDecoder_Run();
}


//TODO - turn inside out, have the "reader" ask for bytes instead
// See Core.cpp for threading idea
void Video_EnterLoop()
{
	SCPFifoStruct &fifo = *g_VideoInitialize.pCPFifo;

	// TODO(ector): Don't peek so often!
	while (g_VideoInitialize.pPeekMessages())
	{
		if (fifo.CPReadWriteDistance < 1) //fifo.CPLoWatermark)
			Sleep(1);
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
					//MessageBox(0,"Breakpoint enabled",0,0);
					if (fifo.CPReadPointer == fifo.CPBreakpoint)
					{
						fifo.bFF_Breakpoint = 1; 
						g_VideoInitialize.pUpdateInterrupts(); 
						break;
					}
				}

				// read the data and send it to the VideoPlugin

				u8 *uData = Memory_GetPtr(fifo.CPReadPointer);

				EnterCriticalSection(&fifo.sync);
				fifo.CPReadPointer += 32;
				Video_SendFifoData(uData);
				InterlockedExchangeAdd((LONG*)&fifo.CPReadWriteDistance, -32);
				LeaveCriticalSection(&fifo.sync);

				// increase the ReadPtr
				if (fifo.CPReadPointer >= fifo.CPEnd)
				{
					fifo.CPReadPointer = fifo.CPBase;				
					//LOG(COMMANDPROCESSOR, "BUFFER LOOP");
					// MessageBox(NULL, "loop", "now", MB_OK);
				}
				count--;
			}
		}

	}
}

