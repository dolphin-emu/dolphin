// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cmath>
#include <functional>

#include <windows.h>
#include <dxerr.h>

#include "AudioCommon.h"
#include "DSoundStream.h"

DSoundStream::DSoundStream(CMixer *mixer, void *hWnd /*= NULL*/):
	CBaseSoundStream(mixer),
	m_hWnd(hWnd),
	m_ds(NULL),
	m_dsBuffer(NULL),
	m_bufferSize(0),
	m_join(false),
	m_currentPos(0),
	m_lastPos(0)
{
}

DSoundStream::~DSoundStream()
{
}

void DSoundStream::OnSetVolume(u32 volume)
{
	if (m_dsBuffer)
	{
		m_dsBuffer->SetVolume(ConvertVolume(volume));
	}
}

void DSoundStream::OnUpdate()
{
	m_soundSyncEvent.Set();
}

void DSoundStream::OnFlushBuffers(bool mute)
{
	if (m_dsBuffer)
	{
		if (mute)
		{
			m_dsBuffer->Stop();
		}
		else
		{
			m_dsBuffer->Play(0, 0, DSBPLAY_LOOPING);
		}
	}
}

bool DSoundStream::OnPreThreadStart()
{
	m_join = false;
	if (FAILED(DirectSoundCreate8(0, &m_ds, 0)))
		return false;
	if (m_hWnd)
	{
		HRESULT hr = m_ds->SetCooperativeLevel((HWND)m_hWnd, DSSCL_PRIORITY);
	}
	if (!CreateBuffer())
		return false;

	DWORD num1;
	short* p1;
	m_dsBuffer->Lock(0, m_bufferSize, (void**)&p1, &num1, 0, 0, DSBLOCK_ENTIREBUFFER);
	memset(p1, 0, num1);
	m_dsBuffer->Unlock(p1, num1, 0, 0);
	return true;
}

// The audio thread.
void DSoundStream::SoundLoop()
{
	Common::SetCurrentThreadName("Audio thread - dsound");

	m_currentPos = 0;
	m_lastPos = 0;
	m_dsBuffer->Play(0, 0, DSBPLAY_LOOPING);

	while (!m_join)
	{
		// No blocking inside the csection
		m_dsBuffer->GetCurrentPosition((DWORD*)&m_currentPos, 0);
		int numBytesToRender = FIX128(ModBufferSize(m_currentPos - m_lastPos));
		if (numBytesToRender >= 256)
		{
			if (numBytesToRender > sizeof(m_realtimeBuffer))
				PanicAlert("soundThread: too big render call");
			CBaseSoundStream::GetMixer()->Mix(m_realtimeBuffer, numBytesToRender / 4);
			WriteDataToBuffer(m_lastPos, (char*)m_realtimeBuffer, numBytesToRender);
			m_lastPos = ModBufferSize(m_lastPos + numBytesToRender);
		}
		m_soundSyncEvent.Wait();
	}
}

void DSoundStream::OnPreThreadJoin()
{
	m_join = true;
	m_soundSyncEvent.Set();
}

void DSoundStream::OnPostThreadJoin()
{
	m_dsBuffer->Stop();
	m_dsBuffer->Release();
	m_ds->Release();
}

bool DSoundStream::CreateBuffer()
{
	PCMWAVEFORMAT pcmwf;
	DSBUFFERDESC dsbdesc;

	memset(&pcmwf, 0, sizeof(PCMWAVEFORMAT));
	memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));

	pcmwf.wf.wFormatTag = WAVE_FORMAT_PCM;
	pcmwf.wf.nChannels = 2;
	pcmwf.wf.nSamplesPerSec = CBaseSoundStream::GetMixer()->GetSampleRate();
	pcmwf.wf.nBlockAlign = 4;
	pcmwf.wf.nAvgBytesPerSec = pcmwf.wf.nSamplesPerSec * pcmwf.wf.nBlockAlign;
	pcmwf.wBitsPerSample = 16;

	// Fill out DSound buffer description.
	dsbdesc.dwSize  = sizeof(DSBUFFERDESC);
	dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLVOLUME | DSBCAPS_GLOBALFOCUS;
	dsbdesc.dwBufferBytes = m_bufferSize = BUFSIZE;
	dsbdesc.lpwfxFormat = (WAVEFORMATEX *)&pcmwf;
	dsbdesc.guid3DAlgorithm = DS3DALG_DEFAULT;

	HRESULT res = m_ds->CreateSoundBuffer(&dsbdesc, &m_dsBuffer, NULL);
	if (SUCCEEDED(res))
	{
		m_dsBuffer->SetCurrentPosition(0);
		u32 vol = CBaseSoundStream::GetVolume();
		m_dsBuffer->SetVolume(ConvertVolume(vol));
		return true;
	}
	else
	{
		// Failed.
		PanicAlertT("Sound buffer creation failed: %s", DXGetErrorString(res)); 
		m_dsBuffer = NULL;
		return false;
	}
}

bool DSoundStream::WriteDataToBuffer(DWORD dwOffset,                  // Our own write cursor.
		char* soundData, // Start of our data.
		DWORD dwSoundBytes) // Size of block to copy.
{
	// I want to record the regular audio to, how do I do that?

	void *ptr1, *ptr2;
	DWORD numBytes1, numBytes2;
	// Obtain memory address of write block. This will be in two parts if the block wraps around.
	HRESULT hr = m_dsBuffer->Lock(dwOffset, dwSoundBytes, &ptr1, &numBytes1, &ptr2, &numBytes2, 0);

	// If the buffer was lost, restore and retry lock.
	if (DSERR_BUFFERLOST == hr)
	{
		m_dsBuffer->Restore();
		hr = m_dsBuffer->Lock(dwOffset, dwSoundBytes, &ptr1, &numBytes1, &ptr2, &numBytes2, 0);
	}
	if (SUCCEEDED(hr))
	{
		memcpy(ptr1, soundData, numBytes1);
		if (ptr2)
		{
			memcpy(ptr2, soundData + numBytes1, numBytes2);
		}

		// Release the data back to DirectSound.
		m_dsBuffer->Unlock(ptr1, numBytes1, ptr2, numBytes2);
		return true;
	}

	return false;
}
