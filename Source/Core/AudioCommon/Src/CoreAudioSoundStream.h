// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _COREAUDIO_SOUND_STREAM_H
#define _COREAUDIO_SOUND_STREAM_H

#ifdef __APPLE__
#include <AudioUnit/AudioUnit.h>
#endif

#include "SoundStream.h"

class CoreAudioSound: public CBaseSoundStream
{
#ifdef __APPLE__
public:
	CoreAudioSound(CMixer *mixer);
	virtual ~CoreAudioSound();

	static inline bool IsValid() { return true; }

private:
	virtual void OnSetVolume(u32 volume) override;

	virtual bool OnPreThreadStart() override;
	virtual void OnPreThreadJoin() override;

private:
	static AudioUnitParameterValue ConvertVolume(u32 volume);
	static OSStatus callback(
		void *inRefCon, 
		AudioUnitRenderActionFlags *ioActionFlags,
		const AudioTimeStamp *inTimeStamp,
		UInt32 inBusNumber,
		UInt32 inNumberFrames,
		AudioBufferList *ioData);

private:
	AudioUnit m_audioUnit;

#else
public:
	CoreAudioSound(CMixer *mixer):
		CBaseSoundStream(mixer)
	{
	}
#endif
};

#endif
