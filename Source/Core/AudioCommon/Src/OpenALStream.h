// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _OPENALSTREAM_H_
#define _OPENALSTREAM_H_

// needs to be before HAVE_OPENAL check!!!
#include "Common.h"
#include "SoundStream.h"

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

class OpenALSoundStream: public CBaseSoundStream
{
#if defined HAVE_OPENAL && HAVE_OPENAL
public:
	OpenALSoundStream(CMixer *mixer, void *hWnd = NULL);
	virtual ~OpenALSoundStream();

	static inline bool IsValid() { return true; }

private:
	virtual void OnSetVolume(u32 volume) override;

	virtual void OnUpdate() override;
	virtual void OnFlushBuffers(bool mute) override;

	virtual bool OnPreThreadStart() override;
	virtual void SoundLoop() override;
	virtual void OnPreThreadJoin() override;
	virtual void OnPostThreadJoin() override;

private:
	static inline ALfloat ConvertVolume(u32 volume)
	{
		return (ALfloat)volume / 100.0f;
	}

private:
	Common::Event m_soundSyncEvent;

	short m_realtimeBuffer[OAL_MAX_SAMPLES * 2];
	soundtouch::SAMPLETYPE m_sampleBuffer[OAL_MAX_SAMPLES * SIZE_FLOAT * SURROUND_CHANNELS * OAL_MAX_BUFFERS];
	ALuint m_uiBuffers[OAL_MAX_BUFFERS];
	ALuint m_uiSource;

	u8 m_numBuffers;
	volatile bool m_join;

#else
public:
	OpenALSoundStream(CMixer *mixer, void *hWnd = NULL):
		CBaseSoundStream(mixer)
	{
	}
#endif // HAVE_OPENAL
};

#endif // OPENALSTREAM
