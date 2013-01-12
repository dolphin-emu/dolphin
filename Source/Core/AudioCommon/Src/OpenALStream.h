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
#include <OpenAL/include/al.h>
#include <OpenAL/include/alc.h>
#include <OpenAL/include/alext.h>
#elif defined __APPLE__
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#endif

#include "Core.h"
#include "HW/SystemTimers.h"
#include "HW/AudioInterface.h"
#include <soundtouch/SoundTouch.h>
#include <soundtouch/STTypes.h>

// 16 bit Stereo
#define SFX_MAX_SOURCE		1
#define OAL_MAX_BUFFERS		32
#define OAL_MAX_SAMPLES		256
#define SURROUND_CHANNELS	6	// number of channels in surround mode
#define SIZE_FLOAT			4   // size of a float in bytes
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

private:
	std::thread thread;
	Common::Event soundSyncEvent;

	short realtimeBuffer[OAL_MAX_SAMPLES * 2];
	soundtouch::SAMPLETYPE sampleBuffer[OAL_MAX_SAMPLES * SIZE_FLOAT * SURROUND_CHANNELS * OAL_MAX_BUFFERS];
	ALuint uiBuffers[OAL_MAX_BUFFERS];
	ALuint uiSource;
	ALfloat fVolume;

	u8 numBuffers;
#else
public:
	OpenALStream(CMixer *mixer, void *hWnd = NULL): SoundStream(mixer) {}
#endif // HAVE_OPENAL
};

#endif // OPENALSTREAM
