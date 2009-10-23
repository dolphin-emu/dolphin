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

#ifndef _COREAUDIO_SOUND_STREAM_H
#define _COREAUDIO_SOUND_STREAM_H

#include "Common.h"
#include "SoundStream.h"
#if defined(__APPLE__)
#include <CoreAudio/AudioHardware.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreServices/CoreServices.h>
#endif

#include "Thread.h"

class CoreAudioSound : public SoundStream
{
#if defined(__APPLE__)

        Common::Thread *thread;
        Common::CriticalSection soundCriticalSection;
        Common::Event soundSyncEvent;

public:
	CoreAudioSound(CMixer *mixer);
	virtual ~CoreAudioSound();
	
	virtual bool Start();
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
	bool CoreAudioInit();
#else
public:
	CoreAudioSound(CMixer *mixer) : SoundStream(mixer) {}
#endif
};

#endif

