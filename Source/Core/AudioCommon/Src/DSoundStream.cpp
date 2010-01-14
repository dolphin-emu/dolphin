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

#include <windows.h>
#include <cmath>
#include <dxerr.h>
#include "AudioCommon.h"
#include "DSoundStream.h"

bool DSound::CreateBuffer()
{
	PCMWAVEFORMAT pcmwf;
	DSBUFFERDESC dsbdesc;

	memset(&pcmwf, 0, sizeof(PCMWAVEFORMAT));
	memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));

	pcmwf.wf.wFormatTag = WAVE_FORMAT_PCM;
	pcmwf.wf.nChannels = 2;
	pcmwf.wf.nSamplesPerSec = m_mixer->GetSampleRate();
	pcmwf.wf.nBlockAlign = 4;
	pcmwf.wf.nAvgBytesPerSec = pcmwf.wf.nSamplesPerSec * pcmwf.wf.nBlockAlign;
	pcmwf.wBitsPerSample = 16;

	// Fill out DSound buffer description.
	dsbdesc.dwSize  = sizeof(DSBUFFERDESC);
	dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_STICKYFOCUS | DSBCAPS_CTRLVOLUME;
	dsbdesc.dwBufferBytes = bufferSize = BUFSIZE;
	dsbdesc.lpwfxFormat = (WAVEFORMATEX *)&pcmwf;
	dsbdesc.guid3DAlgorithm = DS3DALG_DEFAULT;

	HRESULT res = ds->CreateSoundBuffer(&dsbdesc, &dsBuffer, NULL);
	if (SUCCEEDED(res))
	{
		dsBuffer->SetCurrentPosition(0);
		dsBuffer->SetVolume(m_volume);
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
THREAD_RETURN soundThread(void* args)
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
		dsBuffer->GetCurrentPosition((DWORD*)&currentPos, 0);
		int numBytesToRender = FIX128(ModBufferSize(currentPos - lastPos));
		if (numBytesToRender >= 256)
		{
			if (numBytesToRender > sizeof(realtimeBuffer))
				PanicAlert("soundThread: too big render call");
			m_mixer->Mix(realtimeBuffer, numBytesToRender / 4);
			WriteDataToBuffer(lastPos, (char*)realtimeBuffer, numBytesToRender);
			lastPos = ModBufferSize(lastPos + numBytesToRender);
		}
		soundSyncEvent.Wait();
	}
}

bool DSound::Start()
{
	soundSyncEvent.Init();

	if (FAILED(DirectSoundCreate8(0, &ds, 0)))
        return false;
	if (g_dspInitialize.hWnd)
	{
		HRESULT hr = ds->SetCooperativeLevel((HWND)g_dspInitialize.hWnd, DSSCL_PRIORITY);
	}
	if (!CreateBuffer())
		return false;

	DWORD num1;
	short* p1;
	dsBuffer->Lock(0, bufferSize, (void* *)&p1, &num1, 0, 0, 0);
	memset(p1, 0, num1);
	dsBuffer->Unlock(p1, num1, 0, 0);
	thread = new Common::Thread(soundThread, (void *)this);
	return true;
}

void DSound::SetVolume(int volume)
{
	// This is in "dBA attenuation" from 0 to -10000, logarithmic
	m_volume = (int)floor(log10((float)volume) * 5000.0f) - 10000;

	if (dsBuffer != NULL)
		dsBuffer->SetVolume(m_volume);
}

void DSound::Update()
{
	soundSyncEvent.Set();
}

void DSound::Clear(bool mute)
{
	m_muted = mute;

	if (m_muted)
	{
		dsBuffer->Stop();
	}
	else
	{
		dsBuffer->Play(0, 0, DSBPLAY_LOOPING);
	}
}

void DSound::Stop()
{
	threadData = 1;
	// kick the thread if it's waiting
	soundSyncEvent.Set();

	delete thread;
	thread = NULL;
	dsBuffer->Stop();
	dsBuffer->Release();
	ds->Release();

	soundSyncEvent.Shutdown();
}

