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

#ifndef _OPENALSTREAM_H_
#define _OPENALSTREAM_H_

#include "Common.h"
#include "SoundStream.h"
#include "Thread.h"

#if defined HAVE_OPENAL && HAVE_OPENAL
#ifdef _WIN32
#include "../../../../Externals/OpenAL/include/al.h"
#include "../../../../Externals/OpenAL/include/alc.h"
#elif defined(__APPLE__)
#include "openal/al.h"
#include "openal/alc.h"
#else
#include "AL/al.h"
#include "AL/alc.h"
#endif // WIN32
// public use
#define SFX_MAX_SOURCE		1
#define OAL_NUM_BUFFERS		8
#define OAL_BUFFER_SIZE		(512 * 4)
#endif

class OpenALStream: public SoundStream
{
#if defined HAVE_OPENAL && HAVE_OPENAL
public:
	OpenALStream(CMixer *mixer, void *hWnd = NULL)
		: SoundStream(mixer)
		, uiSource(0)
	{};

	virtual ~OpenALStream() {};

	virtual bool Start();
	virtual void SoundLoop();
	virtual void SetVolume(int volume);
	virtual void Stop();
	virtual void Clear(bool mute);
	static bool isValid() { return true; }
	virtual bool usesMixer() const { return true; }
	virtual void Update();

	static THREAD_RETURN ThreadFunc(void* args);

private:
	Common::Thread *thread;
	Common::CriticalSection soundCriticalSection;
	Common::Event soundSyncEvent;
	
	short realtimeBuffer[OAL_BUFFER_SIZE/sizeof(short)];
	ALuint uiBuffers[OAL_NUM_BUFFERS];
	ALuint uiSource;
	ALfloat fVolume;
#else
public:
	OpenALStream(CMixer *mixer, void *hWnd = NULL): SoundStream(mixer) {}
#endif // HAVE_OPENAL
};

#endif // OPENALSTREAM
