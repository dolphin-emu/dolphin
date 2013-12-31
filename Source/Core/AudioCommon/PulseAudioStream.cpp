// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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
