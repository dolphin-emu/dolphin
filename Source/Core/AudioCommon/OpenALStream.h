// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "AudioCommon/SoundStream.h"
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

#endif

class OpenALStream final : public SoundStream
{
#if defined HAVE_OPENAL && HAVE_OPENAL
protected:
	void InitializeSoundLoop() override;
	u32 SamplesNeeded() override;
	void WriteSamples(s16 *src, u32 numsamples) override;
	bool SupportSurroundOutput() override;
public:
	OpenALStream() : uiSource(0)
	{
	}

	bool Start() override;
	void SetVolume(int volume) override;
	void Stop() override;
	void Clear(bool mute) override;

	static bool isValid() { return true; }

private:
	ALuint uiBuffers[SOUND_BUFFER_COUNT + 32];
	ALuint uiSource;
	ALfloat fVolume;

	u8 numBuffers;
	u32 ulFrequency;
	ALenum format;
	u32 samplesize;
#else
public:
	OpenALStream() {}
#endif
};
