// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#ifdef __APPLE__
#include <AudioUnit/AudioUnit.h>
#endif

#include "AudioCommon/SoundStream.h"

class CoreAudioSound final : public SoundStream
{
#ifdef __APPLE__
public:
	virtual bool Start();
	virtual void SetVolume(int volume);
	virtual void SoundLoop();
	virtual void Stop();

	static bool isValid()
	{
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
#endif
};
