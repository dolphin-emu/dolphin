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

#include <functional>

#include "Common.h"
#include "Thread.h"

#include "PulseAudioStream.h"

namespace
{
const size_t BUFFER_SAMPLES = 512;
const size_t CHANNEL_COUNT = 2;
const size_t BUFFER_SIZE = BUFFER_SAMPLES * CHANNEL_COUNT;
}

PulseAudio::PulseAudio(CMixer *mixer)
	: SoundStream(mixer)
	, mix_buffer(BUFFER_SIZE)
	, thread()
	, run_thread()
	, pa()
{}

bool PulseAudio::Start()
{
	run_thread = true;
	thread = std::thread(std::mem_fun(&PulseAudio::SoundLoop), this);
	return true;
}

void PulseAudio::Stop()
{
	run_thread = false;
	thread.join();
}

void PulseAudio::Update()
{
	// don't need to do anything here.
}

// Called on audio thread.
void PulseAudio::SoundLoop()
{
	Common::SetCurrentThreadName("Audio thread - pulse");

	if (PulseInit())
	{
		while (run_thread)
		{
			m_mixer->Mix(&mix_buffer[0], mix_buffer.size() / CHANNEL_COUNT);
			Write(&mix_buffer[0], mix_buffer.size() * sizeof(s16));
		}

		PulseShutdown();
	}
}

bool PulseAudio::PulseInit()
{
	pa_sample_spec ss = {};
	ss.format = PA_SAMPLE_S16LE;
	ss.channels = 2;
	ss.rate = m_mixer->GetSampleRate();

	int error;
	pa = pa_simple_new(nullptr, "dolphin-emu", PA_STREAM_PLAYBACK,
		nullptr, "audio", &ss, nullptr, nullptr, &error);

	if (!pa)
	{
		ERROR_LOG(AUDIO, "PulseAudio failed to initialize: %s",
			pa_strerror(error));
		return false;
	}
	else
	{
		NOTICE_LOG(AUDIO, "Pulse successfully initialized.");
		return true;
	}
}

void PulseAudio::PulseShutdown()
{
	pa_simple_free(pa);
}

void PulseAudio::Write(const void *data, size_t length)
{
	int error;
	if (pa_simple_write(pa, data, length, &error) < 0)
	{
		ERROR_LOG(AUDIO, "PulseAudio failed to write data: %s",
			pa_strerror(error));
	}
}
