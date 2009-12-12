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
#include "../../../PluginSpecs/pluginspecs_dsp.h"

#if defined HAVE_OPENAL && HAVE_OPENAL

#define AUDIO_NUMBUFFERS			(4)
//#define	AUDIO_SERVICE_UPDATE_PERIOD	(20)

extern DSPInitialize g_dspInitialize;

bool OpenALStream::Start()
{
	ALDeviceList *pDeviceList = NULL;
	ALCcontext *pContext = NULL;
	ALCdevice *pDevice = NULL;
	bool bReturn = false;
	
	g_uiSource = 0;

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
		} else {
			PanicAlert("OpenAL: can't open device %s", pDevice);
		}

		
		delete pDeviceList;
	} else {
		PanicAlert("OpenAL: can't find sound devices");
	}

	return bReturn;
}

void OpenALStream::Stop()
{
	ALCcontext *pContext;
	ALCdevice *pDevice;

	soundCriticalSection.Enter();
	threadData = 1;
	// kick the thread if it's waiting
	soundSyncEvent.Set();
	soundCriticalSection.Leave();
	delete thread;
	
	pContext = alcGetCurrentContext();
	pDevice = alcGetContextsDevice(pContext);

	alcMakeContextCurrent(NULL);
	alcDestroyContext(pContext);
	alcCloseDevice(pDevice);

	soundSyncEvent.Shutdown();
	thread = NULL;
}

void OpenALStream::Update()
{
	//if (m_mixer->GetDataSize())	//here need debug
	{
		soundSyncEvent.Set();
	}
}

void OpenALStream::Clear()
{
	if(!*g_dspInitialize.pEmulatorState)
	{
		alSourcePlay(g_uiSource);
	}
	else
	{
		alSourceStop(g_uiSource);
	}
}

THREAD_RETURN OpenALStream::ThreadFunc(void* args)
{
	(reinterpret_cast<OpenALStream *>(args))->SoundLoop();
	return 0;
}

void OpenALStream::SoundLoop()
{
	ALuint		    uiBuffers[AUDIO_NUMBUFFERS] = {0};
	ALuint		    uiSource = 0;
	ALenum err;
	u32 ulFrequency = m_mixer->GetSampleRate();
	// Generate some AL Buffers for streaming
	alGenBuffers(AUDIO_NUMBUFFERS, (ALuint *)uiBuffers);
	// Generate a Source to playback the Buffers
	alGenSources(1, &uiSource);

	memset(realtimeBuffer, 0, OAL_BUFFER_SIZE * sizeof(short));
//*
	for (int iLoop = 0; iLoop < AUDIO_NUMBUFFERS; iLoop++)
	{
		// pay load fake data
		alBufferData(uiBuffers[iLoop], AL_FORMAT_STEREO16, realtimeBuffer, 1024, ulFrequency);
		alSourceQueueBuffers(uiSource, 1, &uiBuffers[iLoop]);
	}
//*/

	g_uiSource = uiSource;

	alSourcePlay(uiSource);
	err = alGetError();

	while (!threadData) 
	{
		soundCriticalSection.Enter();
		int numBytesToRender = 32768;	//ya, this is a hack, we need real data count
		/*int numBytesRender =*/ m_mixer->Mix(realtimeBuffer, numBytesToRender >> 2);
		soundCriticalSection.Leave();

		//if (numBytesRender)	//here need debug
		{
			ALint iBuffersProcessed = 0;
			alGetSourcei(uiSource, AL_BUFFERS_PROCESSED, &iBuffersProcessed);

			if (iBuffersProcessed)
			{
				// Remove the Buffer from the Queue.  (uiBuffer contains the Buffer ID for the unqueued Buffer)
				ALuint uiTempBuffer = 0;
				alSourceUnqueueBuffers(uiSource, 1, &uiTempBuffer);
/*
				soundCriticalSection.Enter();
				int numBytesToRender = 32768;	//ya, this is a hack, we need real data count
				 m_mixer->Mix(realtimeBuffer, numBytesToRender >> 2);
				soundCriticalSection.Leave();

				unsigned long	ulBytesWritten = 0;
*/
				//if (numBytesRender)
				{
					alBufferData(uiTempBuffer, AL_FORMAT_STEREO16, realtimeBuffer, numBytesToRender, ulFrequency);
				}
				alSourceQueueBuffers(uiSource, 1, &uiTempBuffer);
			}
		}
		
		if (!threadData)
			soundSyncEvent.Wait();
	}
	alSourceStop(uiSource);
	alSourcei(uiSource, AL_BUFFER, 0);

	// Clean up buffers and sources
	alDeleteSources(1, &uiSource);
	alDeleteBuffers(AUDIO_NUMBUFFERS, (ALuint *)uiBuffers);

}

#endif //HAVE_OPENAL

