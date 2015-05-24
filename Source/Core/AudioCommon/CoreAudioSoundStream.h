// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
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
	bool Start() override;
	void SetVolume(int volume) override;
	void SoundLoop() override;
	void Stop() override;
	void Update() override;

	static bool isValid()
	{
		return true;
	}

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
