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

#ifndef _ALSA_SOUND_STREAM_H
#define _ALSA_SOUND_STREAM_H

#if defined(HAVE_ALSA) && HAVE_ALSA
#include <alsa/asoundlib.h>
#endif

#include "Common.h"
#include "SoundStream.h"

#include "Thread.h"

class AlsaSound : public SoundStream
{
#if defined(HAVE_ALSA) && HAVE_ALSA
public:
	AlsaSound(CMixer *mixer);
	virtual ~AlsaSound();

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
	bool AlsaInit();
	void AlsaShutdown();

	u8 *mix_buffer;
	Common::Thread *thread;
	// 0 = continue
	// 1 = shutdown
	// 2 = done shutting down.
	volatile int thread_data;

	snd_pcm_t *handle;
#else
public:
	AlsaSound(CMixer *mixer) : SoundStream(mixer) {}
#endif
};

#endif

