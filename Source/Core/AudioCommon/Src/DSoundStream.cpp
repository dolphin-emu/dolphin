// Copyright (C) 2003-2009 Dolphin Project.

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

#include <windows.h>
#include <dxerr.h>
#include "DSoundStream.h"

//extern bool log_ai;

bool DSound::CreateBuffer()
{
	PCMWAVEFORMAT pcmwf;
	DSBUFFERDESC dsbdesc;

	memset(&pcmwf, 0, sizeof(PCMWAVEFORMAT));
	memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));

	pcmwf.wf.wFormatTag = WAVE_FORMAT_PCM;
	pcmwf.wf.nChannels = 2;
	pcmwf.wf.nSamplesPerSec = sampleRate;
	pcmwf.wf.nBlockAlign = 4;
	pcmwf.wf.nAvgBytesPerSec = pcmwf.wf.nSamplesPerSec * pcmwf.wf.nBlockAlign;
	pcmwf.wBitsPerSample = 16;

	// Fill out DSound buffer description.
	dsbdesc.dwSize  = sizeof(DSBUFFERDESC);
	dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_STICKYFOCUS;
	dsbdesc.dwBufferBytes = bufferSize = BUFSIZE;
	dsbdesc.lpwfxFormat = (WAVEFORMATEX *)&pcmwf;

	HRESULT res = ds->CreateSoundBuffer(&dsbdesc, &dsBuffer, NULL);
	if (SUCCEEDED(res))
	{
		dsBuffer->SetCurrentPosition(0);
		return true;
	}
	else
	{
		// Failed.
		PanicAlert("Sound buffer creation failed: %s", DXGetErrorString(res)); 
		dsBuffer = NULL;
		return false;
	}
}

bool DSound::WriteDataToBuffer(DWORD dwOffset,                  // Our own write cursor.
		char* soundData, // Start of our data.
		DWORD dwSoundBytes) // Size of block to copy.
{
	// I want to record the regular audio to, how do I do that?
	// Well, it's gonna be a bit tricky. For future work :)
	//std::string Data = ArrayToString((const u8*)soundData, dwSoundBytes);
	//Console::Print("Data: %s\n\n", Data.c_str());
	//if (log_ai) g_wave_writer.AddStereoSamples((const short*)soundData, dwSoundBytes);

	void *ptr1, *ptr2;
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
			memcpy(ptr2, soundData + numBytes1, numBytes2);

		// Release the data back to DirectSound.
		dsBuffer->Unlock(ptr1, numBytes1, ptr2, numBytes2);
		return true;
	}

	return false;
}

// The audio thread.
DWORD WINAPI soundThread(void* args)
{
	(reinterpret_cast<DSound *>(args))->SoundLoop();
	return 0;
}

void DSound::SoundLoop()
{
	currentPos = 0;
	lastPos = 0;
	dsBuffer->Play(0, 0, DSBPLAY_LOOPING);
	while (!threadData)
	{
		// No blocking inside the csection
		soundCriticalSection.Enter();
		dsBuffer->GetCurrentPosition((DWORD*)&currentPos, 0);
		int numBytesToRender = FIX128(ModBufferSize(currentPos - lastPos));
		if (numBytesToRender >= 256)
		{
			if (numBytesToRender > sizeof(realtimeBuffer))
				PanicAlert("soundThread: too big render call");
			m_mixer->Mix(realtimeBuffer, numBytesToRender >> 2);
			WriteDataToBuffer(lastPos, (char*)realtimeBuffer, numBytesToRender);
			currentPos = ModBufferSize(lastPos + numBytesToRender);
			totalRenderedBytes += numBytesToRender;
			lastPos = currentPos;
		}

		soundCriticalSection.Leave();
		if (threadData)
			break;
		soundSyncEvent.Wait();
	}

	dsBuffer->Stop();
}

bool DSound::Start()
{
	soundSyncEvent.Init();
	if (FAILED(DirectSoundCreate8(0, &ds, 0)))
        return false;
	if (hWnd)
		ds->SetCooperativeLevel((HWND)hWnd, DSSCL_NORMAL);
	if (!CreateBuffer())
		return false;

	DWORD num1;
	short* p1;
	dsBuffer->Lock(0, bufferSize, (void* *)&p1, &num1, 0, 0, 0);
	memset(p1, 0, num1);
	dsBuffer->Unlock(p1, num1, 0, 0);
	totalRenderedBytes = -bufferSize;
	thread = new Common::Thread(soundThread, (void *)this);
	return true;
}

void DSound::Update()
{
	soundSyncEvent.Set();
}

void DSound::Stop()
{
	soundCriticalSection.Enter();
	threadData = 1;
	// kick the thread if it's waiting
	soundSyncEvent.Set();
	soundCriticalSection.Leave();
	delete thread;

	dsBuffer->Release();
	ds->Release();

	soundSyncEvent.Shutdown();
	thread = NULL;
}
