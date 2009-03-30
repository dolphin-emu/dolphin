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
			}
		}
		delete pDeviceList;
	}
	return bReturn;
}

void OpenALStream::Stop()
{
	ALCcontext *pContext;
	ALCdevice *pDevice;

	delete thread;

	pContext = alcGetCurrentContext();
	pDevice = alcGetContextsDevice(pContext);

	alcMakeContextCurrent(NULL);
	alcDestroyContext(pContext);
	alcCloseDevice(pDevice);
}

void OpenALStream::Update()
{

}

// The audio thread.
#ifdef _WIN32
DWORD WINAPI OpenALStream::ThreadFunc(void* args)
#else
void* OpenALStream::ThreadFunc(void* args)
#endif
{
	(reinterpret_cast<OpenALStream *>(args))->SoundLoop();
	return 0;
}

void OpenALStream::SoundLoop()
{

}
