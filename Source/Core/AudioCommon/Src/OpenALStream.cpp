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

#include "aldlist.h"
#include "OpenALStream.h"

#if defined HAVE_OPENAL && HAVE_OPENAL

#define AUDIO_NUMBUFFERS			(4)

bool OpenALStream::Start()
{
	ALDeviceList *pDeviceList = NULL;
	ALCcontext *pContext = NULL;
	ALCdevice *pDevice = NULL;
	bool bReturn = false;
	
	pDeviceList = new ALDeviceList();
	if ((pDeviceList) && (pDeviceList->GetNumDevices()))
	{
		pDevice = alcOpenDevice(pDeviceList->GetDeviceName(pDeviceList->GetDefaultDevice()));
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
	soundSyncEvent.Set();
}

THREAD_RETURN OpenALStream::ThreadFunc(void* args)
{
	(reinterpret_cast<OpenALStream *>(args))->SoundLoop();
	return 0;
}

void OpenALStream::SoundLoop()
{
	while (!threadData) {
		soundCriticalSection.Enter();

		// Add sound playing here
		// m_mixer->Mix(realtimeBuffer, numBytesToRender >> 2);
		soundCriticalSection.Leave();

		if (!threadData)
			soundSyncEvent.Wait();
	}
}

#endif //HAVE_OPENAL
