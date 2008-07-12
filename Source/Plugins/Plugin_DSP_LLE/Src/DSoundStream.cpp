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

#include "stdafx.h"

#include <mmsystem.h>
#include <dsound.h>

#include "dsoundstream.h"

namespace DSound
{
#define BUFSIZE 32768
#define MAXWAIT 70   //ms

//THE ROCK SOLID SYNCED DSOUND ENGINE :)


//våran kritiska sektion och vår syncevent-handle
CRITICAL_SECTION soundCriticalSection;
HANDLE soundSyncEvent;
HANDLE hThread;

StreamCallback callback;

//lite mojs
IDirectSound8* ds;
IDirectSoundBuffer* dsBuffer;

//tja.. behövs
int bufferSize; //i bytes
int totalRenderedBytes;
int sampleRate;

//med den här synkar vi stängning..
//0=vi spelar oväsen, 1=stäng tråden NU! 2=japp,tråden är stängd så fortsätt
volatile int threadData;


//ser till så X kan delas med 32
inline int FIX32(int x)
{
	return(x & (~127));
}


int DSound_GetSampleRate()
{
	return(sampleRate);
}


//Dags att skapa vår directsound buffert
bool createBuffer()
{
	PCMWAVEFORMAT pcmwf;
	DSBUFFERDESC dsbdesc;

	//ljudformatet
	memset(&pcmwf, 0, sizeof(PCMWAVEFORMAT));
	memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));

	pcmwf.wf.wFormatTag = WAVE_FORMAT_PCM;
	pcmwf.wf.nChannels = 2;
	pcmwf.wf.nSamplesPerSec = sampleRate;
	pcmwf.wf.nBlockAlign = 4;
	pcmwf.wf.nAvgBytesPerSec = pcmwf.wf.nSamplesPerSec * pcmwf.wf.nBlockAlign;
	pcmwf.wBitsPerSample = 16;

	//buffer description
	dsbdesc.dwSize  = sizeof(DSBUFFERDESC);
	dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_STICKYFOCUS; //VIKTIGT //DSBCAPS_CTRLPAN | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY;
	dsbdesc.dwBufferBytes = bufferSize = BUFSIZE; //FIX32(pcmwf.wf.nAvgBytesPerSec);   //ändra för att ställa in bufferstorlek
	dsbdesc.lpwfxFormat = (WAVEFORMATEX*)&pcmwf;
	// nu skapar vi bufferjäveln

	if (SUCCEEDED(ds->CreateSoundBuffer(&dsbdesc, &dsBuffer, NULL)))
	{
		dsBuffer->SetCurrentPosition(0);
		return(true);
	}
	else
	{
		// Failed.
		dsBuffer = NULL;
		return(false);
	}
}


bool writeDataToBuffer(DWORD dwOffset,              // Our own write cursor.
		char* soundData, // Start of our data.
		DWORD dwSoundBytes) // Size of block to copy.
{
	void* ptr1, * ptr2;
	DWORD numBytes1, numBytes2;
	// Obtain memory address of write block. This will be in two parts if the block wraps around.
	HRESULT hr = dsBuffer->Lock(dwOffset, dwSoundBytes, &ptr1, &numBytes1, &ptr2, &numBytes2, 0);

	// If the buffer was lost, restore and retry lock.

	if (DSERR_BUFFERLOST == hr)
	{
		dsBuffer->Restore();
		hr = dsBuffer->Lock(dwOffset, dwSoundBytes, &ptr1, &numBytes1, &ptr2, &numBytes2, 0);
	}

	if (SUCCEEDED(hr))
	{
		memcpy(ptr1, soundData, numBytes1);

		if (ptr2 != 0)
		{
			memcpy(ptr2, soundData + numBytes1, numBytes2);
		}

		// Release the data back to DirectSound.
		dsBuffer->Unlock(ptr1, numBytes1, ptr2, numBytes2);
		return(true);
	} /*
        else
        {
        char temp[8];
        sprintf(temp,"%i\n",hr);
        OutputDebugString(temp);
        }*/

	return(false);
}


inline int ModBufferSize(int x)
{
	return((x + bufferSize) % bufferSize);
}


int currentPos;
int lastPos;
short realtimeBuffer[1024 * 1024];


DWORD WINAPI soundThread(void*)
{
	currentPos = 0;
	lastPos = 0;
	//writeDataToBuffer(0,realtimeBuffer,bufferSize);
	//  dsBuffer->Lock(0, bufferSize, (void **)&p1, &num1, (void **)&p2, &num2, 0);

	dsBuffer->Play(0, 0, DSBPLAY_LOOPING);

	while (!threadData)
	{
		EnterCriticalSection(&soundCriticalSection);

		dsBuffer->GetCurrentPosition((DWORD*)&currentPos, 0);
		int numBytesToRender = FIX32(ModBufferSize(currentPos - lastPos));

		//renderStuff(numBytesToRender/2);
		//if (numBytesToRender>bufferSize/2) numBytesToRender=0;

		if (numBytesToRender >= 256)
		{
			(*callback)(realtimeBuffer, numBytesToRender >> 2, 16, 44100, 2);

			writeDataToBuffer(lastPos, (char*)realtimeBuffer, numBytesToRender);

			currentPos = ModBufferSize(lastPos + numBytesToRender);
			totalRenderedBytes += numBytesToRender;

			lastPos = currentPos;
		}

		LeaveCriticalSection(&soundCriticalSection);
		WaitForSingleObject(soundSyncEvent, MAXWAIT);
	}

	dsBuffer->Stop();

	threadData = 2;
	return(0); //hurra!
}


bool DSound_StartSound(HWND window, int _sampleRate, StreamCallback _callback)
{
	callback   = _callback;
	threadData = 0;
	sampleRate = _sampleRate;

	//no security attributes, automatic resetting, init state nonset, untitled
	soundSyncEvent = CreateEvent(0, false, false, 0);

	//vi initierar den...........
	InitializeCriticalSection(&soundCriticalSection);

	//vi vill ha access till DSOUND så...
	if (FAILED(DirectSoundCreate8(0, &ds, 0)))
	{
		return(false);
	}

	//samarbetsvillig? nää :)
	ds->SetCooperativeLevel(window, DSSCL_NORMAL); //DSSCL_PRIORITY?

	//så.. skapa buffern
	if (!createBuffer())
	{
		return(false);
	}

	//rensa den.. ?
	DWORD num1;
	short* p1;

	dsBuffer->Lock(0, bufferSize, (void* *)&p1, &num1, 0, 0, 0);

	memset(p1, 0, num1);
	dsBuffer->Unlock(p1, num1, 0, 0);
	totalRenderedBytes = -bufferSize;
	DWORD h;
	hThread = CreateThread(0, 0, soundThread, 0, 0, &h);
	SetThreadPriority(hThread, THREAD_PRIORITY_ABOVE_NORMAL);
	return(true);
}


void DSound_UpdateSound()
{
	SetEvent(soundSyncEvent);
}


void DSound_StopSound()
{
	EnterCriticalSection(&soundCriticalSection);
	threadData = 1;
	//kick the thread if it's waiting
	SetEvent(soundSyncEvent);
	LeaveCriticalSection(&soundCriticalSection);
	WaitForSingleObject(hThread, INFINITE);
	CloseHandle(hThread);

	dsBuffer->Release();
	ds->Release();

	CloseHandle(soundSyncEvent);
}


int DSound_GetCurSample()
{
	EnterCriticalSection(&soundCriticalSection);
	int playCursor;
	dsBuffer->GetCurrentPosition((DWORD*)&playCursor, 0);
	playCursor = ModBufferSize(playCursor - lastPos) + totalRenderedBytes;
	LeaveCriticalSection(&soundCriticalSection);
	return(playCursor);
}


float DSound_GetTimer()
{
	return((float)DSound_GetCurSample() * (1.0f / (4.0f * 44100.0f)));
}
}
