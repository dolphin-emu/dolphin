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

#include "aldlist.h"
#include "OpenALStream.h"

#if defined HAVE_OPENAL && HAVE_OPENAL

bool OpenALStream::Start()
{
	ALDeviceList *pDeviceList = NULL;
	ALCcontext *pContext = NULL;
	ALCdevice *pDevice = NULL;
	bool bReturn = false;

	soundSyncEvent.Init();

	pDeviceList = new ALDeviceList();
	if ((pDeviceList) && (pDeviceList->GetNumDevices()))
	{
		pDevice = alcOpenDevice((const ALCchar *)pDeviceList->GetDeviceName(pDeviceList->GetDefaultDevice()));
		if (pDevice)
		{
			pContext = alcCreateContext(pDevice, NULL);
			if (pContext)
			{
				alcMakeContextCurrent(pContext);
				thread = new Common::Thread(OpenALStream::ThreadFunc, (void *)this);
				bReturn = true;
			}
			else
			{
				alcCloseDevice(pDevice);
				PanicAlert("OpenAL: can't create context for device %s", pDevice);
			}
		}
		else
		{
			PanicAlert("OpenAL: can't open device %s", pDevice);
		}
		delete pDeviceList;
	}
	else
	{
		PanicAlert("OpenAL: can't find sound devices");
	}

	return bReturn;
}

void OpenALStream::Stop()
{
	threadData = 1;
	// kick the thread if it's waiting
	soundSyncEvent.Set();

// AyuanX: Spec says OpenAL1.1 is thread safe already
//	soundCriticalSection.Enter();
	delete thread;
	thread = NULL;

	alSourceStop(uiSource);
	alSourcei(uiSource, AL_BUFFER, 0);

	// Clean up buffers and sources
	alDeleteSources(1, &uiSource);
	alDeleteBuffers(OAL_NUM_BUFFERS, uiBuffers);

	ALCcontext *pContext = alcGetCurrentContext();
	ALCdevice *pDevice = alcGetContextsDevice(pContext);

	alcMakeContextCurrent(NULL);
	alcDestroyContext(pContext);
	alcCloseDevice(pDevice);
//	soundCriticalSection.Leave();

	soundSyncEvent.Shutdown();
}

void OpenALStream::Update()
{
	soundSyncEvent.Set();
}

void OpenALStream::Clear(bool mute)
{
	m_muted = mute;

//	soundCriticalSection.Enter();
	if(m_muted)
	{
		alSourceStop(uiSource);
	}
	else
	{
		alSourcePlay(uiSource);
	}
//	soundCriticalSection.Leave();
}

THREAD_RETURN OpenALStream::ThreadFunc(void* args)
{
	(reinterpret_cast<OpenALStream *>(args))->SoundLoop();
	return 0;
}

void OpenALStream::SoundLoop()
{
	ALenum err;
	u32 ulFrequency = m_mixer->GetSampleRate();

	memset(uiBuffers, 0, OAL_NUM_BUFFERS * sizeof(ALuint));
	uiSource = 0;

	// Generate some AL Buffers for streaming
	alGenBuffers(OAL_NUM_BUFFERS, (ALuint *)uiBuffers);
	// Generate a Source to playback the Buffers
	alGenSources(1, &uiSource);

	memset(realtimeBuffer, 0, OAL_BUFFER_SIZE);
	for (int i = 0; i < OAL_NUM_BUFFERS; i++)
		alBufferData(uiBuffers[i], AL_FORMAT_STEREO16, realtimeBuffer, OAL_BUFFER_SIZE, ulFrequency);

	alSourceQueueBuffers(uiSource, OAL_NUM_BUFFERS, uiBuffers);
	alSourcePlay(uiSource);
	err = alGetError();
	// TODO: Error handling

	ALint iBuffersFilled = 0;
	ALint iBuffersProcessed = 0;
	ALuint uiBufferTemp[OAL_NUM_BUFFERS] = {0};

	while (!threadData) 
	{
//		soundCriticalSection.Enter();
		if (iBuffersProcessed == iBuffersFilled)
		{
			alGetSourcei(uiSource, AL_BUFFERS_PROCESSED, &iBuffersProcessed);
			iBuffersFilled = 0;
		}
		int numSamples = m_mixer->GetNumSamples();
		numSamples &= ~0x400;

		if (iBuffersProcessed && numSamples)
		{
			numSamples = (numSamples > OAL_BUFFER_SIZE / 4) ? OAL_BUFFER_SIZE / 4 : numSamples;
			// Remove the Buffer from the Queue.  (uiBuffer contains the Buffer ID for the unqueued Buffer)
			if (iBuffersFilled == 0)
				alSourceUnqueueBuffers(uiSource, iBuffersProcessed, uiBufferTemp);

			m_mixer->Mix(realtimeBuffer, numSamples);
			alBufferData(uiBufferTemp[iBuffersFilled], AL_FORMAT_STEREO16, realtimeBuffer, numSamples * 4, ulFrequency);
			alSourceQueueBuffers(uiSource, 1, &uiBufferTemp[iBuffersFilled]);
			iBuffersFilled++;

			if (iBuffersFilled == OAL_NUM_BUFFERS)
				alSourcePlay(uiSource);
		}
		else
		{
			ALint state = 0;
			alGetSourcei(uiSource, AL_SOURCE_STATE, &state);
			if (state != AL_PLAYING)
				alSourcePlay(uiSource);
		}
//		soundCriticalSection.Leave();
		soundSyncEvent.Wait();
	}
}

#endif //HAVE_OPENAL

