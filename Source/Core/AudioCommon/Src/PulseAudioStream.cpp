// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#if defined(HAVE_PULSEAUDIO) && HAVE_PULSEAUDIO

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

PulseAudioStream::PulseAudioStream(CMixer *mixer):
	CBaseSoundStream(mixer),
	m_pa(nullptr),
	m_join(false),
	m_mix_buffer(BUFFER_SIZE)
{
}

bool PulseAudioStream::OnPreThreadStart()
{
	m_join = false;
	return true;
}

// Called on audio thread.
void PulseAudioStream::SoundLoop()
{
	Common::SetCurrentThreadName("Audio thread - pulse");

	if (PulseInit())
	{
		CMixer *mixer = CBaseSoundStream::GetMixer();
		while (!m_join)
		{
			mixer->Mix(&m_mix_buffer[0], m_mix_buffer.size() / CHANNEL_COUNT);
			Write(&m_mix_buffer[0], m_mix_buffer.size() * sizeof(s16));
		}

		PulseShutdown();
	}
}

void PulseAudioStream::OnPreThreadJoin()
{
	m_join = true;
}

bool PulseAudioStream::PulseInit()
{
	pa_sample_spec ss = {};
	ss.format = PA_SAMPLE_S16LE;
	ss.channels = 2;
	ss.rate = CBaseSoundStream::GetMixer()->GetSampleRate();

	int error;
	m_pa = pa_simple_new(nullptr, "dolphin-emu", PA_STREAM_PLAYBACK,
		nullptr, "audio", &ss, nullptr, nullptr, &error);

	if (m_pa)
	{
		NOTICE_LOG(AUDIO, "Pulse successfully initialized.");
		return true;
	}
	else
	{
		ERROR_LOG(AUDIO, "PulseAudio failed to initialize: %s",
			pa_strerror(error));
		return false;
	}
}

void PulseAudioStream::PulseShutdown()
{
	pa_simple_free(m_pa);
}

void PulseAudioStream::Write(const void *data, size_t length)
{
	int error;
	if (pa_simple_write(m_pa, data, length, &error) < 0)
	{
		ERROR_LOG(AUDIO, "PulseAudio failed to write data: %s",
			pa_strerror(error));
	}
}

#endif // HAVE_PULSEAUDIO
