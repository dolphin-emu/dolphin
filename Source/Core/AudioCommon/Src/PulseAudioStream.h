// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _PULSE_AUDIO_STREAM_H
#define _PULSE_AUDIO_STREAM_H

#if defined(HAVE_PULSEAUDIO) && HAVE_PULSEAUDIO
#include <pulse/simple.h>
#include <pulse/error.h>
#endif

#include "Common.h"
#include "SoundStream.h"

#include "Thread.h"

#include <vector>

class PulseAudio : public SoundStream
{
#if defined(HAVE_PULSEAUDIO) && HAVE_PULSEAUDIO
public:
	PulseAudio(CMixer *mixer);

	virtual bool Start();
	virtual void Stop(); 

	static bool isValid() {return true;}

	virtual bool usesMixer() const {return true;}

	virtual void Update();

private:
	virtual void SoundLoop();

	bool PulseInit();
	void PulseShutdown();
	void Write(const void *data, size_t bytes);

	std::vector<s16> mix_buffer;
	std::thread thread;
	volatile bool run_thread;

	pa_simple* pa;
#else
public:
	PulseAudio(CMixer *mixer) : SoundStream(mixer) {}
#endif
};

#endif
