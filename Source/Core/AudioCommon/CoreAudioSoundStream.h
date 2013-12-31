// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _COREAUDIO_SOUND_STREAM_H
#define _COREAUDIO_SOUND_STREAM_H

#ifdef __APPLE__
#include <AudioUnit/AudioUnit.h>
#endif

#include "SoundStream.h"

class CoreAudioSound : public SoundStream
{
#ifdef __APPLE__
public:
	CoreAudioSound(CMixer *mixer);
	virtual ~CoreAudioSound();

	virtual bool Start();
	virtual void SetVolume(int volume);
	virtual void SoundLoop();
	virtual void Stop();

	static bool isValid() {
		return true;
	}
	virtual bool usesMixer() const {
		return true;
	}

	virtual void Update();

private:
	AudioUnit audioUnit;
    int m_volume;

	static OSStatus callback(void *inRefCon,
		AudioUnitRenderActionFlags *ioActionFlags,
		const AudioTimeStamp *inTimeStamp,
		UInt32 inBusNumber, UInt32 inNumberFrames,
		AudioBufferList *ioData);
#else
public:
	CoreAudioSound(CMixer *mixer) : SoundStream(mixer) {}
#endif
};

#endif
