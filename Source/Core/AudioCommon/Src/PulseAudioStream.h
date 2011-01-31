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

#ifndef _PULSE_AUDIO_STREAM_H
#define _PULSE_AUDIO_STREAM_H

#if defined(HAVE_PULSEAUDIO) && HAVE_PULSEAUDIO
#include <pulse/pulseaudio.h>
#endif

#include "Common.h"
#include "SoundStream.h"

#include "Thread.h"

class PulseAudio : public SoundStream
{
#if defined(HAVE_PULSEAUDIO) && HAVE_PULSEAUDIO
public:
	PulseAudio(CMixer *mixer);
	virtual ~PulseAudio();

	virtual bool Start();
	virtual void Stop(); 
	virtual void SetVolume(int volume);

	static bool isValid() {return true;}

	virtual bool usesMixer() const {return true;}

	virtual void Update();

private:
	virtual void SoundLoop();
	bool PulseInit();
	void PulseShutdown();
	bool Write(const void *data, size_t bytes);
	void SignalMainLoop();
	static void ContextStateCB(pa_context *c, void *userdata);
	static void StreamStateCB(pa_stream *s, void * userdata);
	static void StreamWriteCB(pa_stream *s, size_t length, void *userdata);

	u8 *mix_buffer;
	std::thread thread;
	volatile bool thread_running;

	pa_threaded_mainloop *mainloop;
	pa_context *context;
	pa_stream *stream;
	int iVolume;
#else
public:
	PulseAudio(CMixer *mixer) : SoundStream(mixer) {}
#endif
};

#endif
