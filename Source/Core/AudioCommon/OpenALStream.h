// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <thread>

#include "AudioCommon/SoundStream.h"
#include "Common/Event.h"
#include "Core/Core.h"
#include "Core/HW/AudioInterface.h"
#include "Core/HW/SystemTimers.h"

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

#ifdef __APPLE__
// Avoid conflict with objc.h (on Windows, ST uses the system BOOL type, so this doesn't work)
#define BOOL SoundTouch_BOOL
#endif

#include <soundtouch/SoundTouch.h>
#include <soundtouch/STTypes.h>

#ifdef __APPLE__
#undef BOOL
#endif

// 16 bit Stereo
#define SFX_MAX_SOURCE          1
#define OAL_MAX_BUFFERS         32
#define OAL_MAX_SAMPLES         256
#define STEREO_CHANNELS         2
#define SURROUND_CHANNELS       6   // number of channels in surround mode
#define SIZE_SHORT              2
#define SIZE_FLOAT              4   // size of a float in bytes
#define FRAME_STEREO_SHORT      STEREO_CHANNELS * SIZE_SHORT
#define FRAME_STEREO_FLOAT      STEREO_CHANNELS * SIZE_FLOAT
#define FRAME_SURROUND_FLOAT    SURROUND_CHANNELS * SIZE_FLOAT
#define FRAME_SURROUND_SHORT    SURROUND_CHANNELS * SIZE_SHORT
#endif

class OpenALStream final : public SoundStream
{
#if defined HAVE_OPENAL && HAVE_OPENAL
public:
	OpenALStream() : uiSource(0)
	{
	}

	bool Start() override;
	void SoundLoop() override;
	void SetVolume(int volume) override;
	void Stop() override;
	void Clear(bool mute) override;
	void Update() override;

	static bool isValid() { return true; }

private:
	std::thread thread;
	std::atomic<bool> m_run_thread;

	Common::Event soundSyncEvent;

	short realtimeBuffer[OAL_MAX_SAMPLES * STEREO_CHANNELS];
	soundtouch::SAMPLETYPE sampleBuffer[OAL_MAX_SAMPLES * SURROUND_CHANNELS * OAL_MAX_BUFFERS];
	ALuint uiBuffers[OAL_MAX_BUFFERS];
	ALuint uiSource;
	ALfloat fVolume;

	u8 numBuffers;
#endif // HAVE_OPENAL
};
