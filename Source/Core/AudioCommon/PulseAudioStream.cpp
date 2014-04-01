// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <functional>

#include "AudioCommon/PulseAudioStream.h"
#include "Common/Common.h"
#include "Common/Thread.h"

namespace
{
const size_t BUFFER_SAMPLES = 512; // ~10 ms
const size_t CHANNEL_COUNT = 2;
const size_t BUFFER_SIZE = BUFFER_SAMPLES * CHANNEL_COUNT * sizeof(s16);
}

PulseAudio::PulseAudio(CMixer *mixer)
	: SoundStream(mixer)
	, m_thread()
	, m_run_thread()
{}

bool PulseAudio::Start()
{
	m_run_thread = true;
	m_thread = std::thread(std::mem_fn(&PulseAudio::SoundLoop), this);
	return true;
}

void PulseAudio::Stop()
{
	m_run_thread = false;
	m_thread.join();
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
		while (m_run_thread.load() && m_pa_connected == 1 && m_pa_error >= 0)
			m_pa_error = pa_mainloop_iterate(m_pa_ml, 1, nullptr);

		if (m_pa_error < 0)
			ERROR_LOG(AUDIO, "PulseAudio error: %s", pa_strerror(m_pa_error));

		PulseShutdown();
	}
}

bool PulseAudio::PulseInit()
{
	m_pa_error = 0;
	m_pa_connected = 0;

	// create pulseaudio main loop and context
	// also register the async state callback which is called when the connection to the pa server has changed
	m_pa_ml = pa_mainloop_new();
	m_pa_mlapi = pa_mainloop_get_api(m_pa_ml);
	m_pa_ctx = pa_context_new(m_pa_mlapi, "dolphin-emu");
	m_pa_error = pa_context_connect(m_pa_ctx, nullptr, PA_CONTEXT_NOFLAGS, nullptr);
	pa_context_set_state_callback(m_pa_ctx, StateCallback, this);

	// wait until we're connected to the pulseaudio server
	while (m_pa_connected == 0 && m_pa_error >= 0)
		m_pa_error = pa_mainloop_iterate(m_pa_ml, 1, nullptr);

	if (m_pa_connected == 2 || m_pa_error < 0)
	{
		ERROR_LOG(AUDIO, "PulseAudio failed to initialize: %s", pa_strerror(m_pa_error));
		return false;
	}

	// create a new audio stream with our sample format
	// also connect the callbacks for this stream
	pa_sample_spec ss;
	ss.format = PA_SAMPLE_S16LE;
	ss.channels = 2;
	ss.rate = m_mixer->GetSampleRate();
	m_pa_s = pa_stream_new(m_pa_ctx, "Playback", &ss, nullptr);
	pa_stream_set_write_callback(m_pa_s, WriteCallback, this);
	pa_stream_set_underflow_callback(m_pa_s, UnderflowCallback, this);

	// connect this audio stream to the default audio playback
	// limit buffersize to reduce latency
	m_pa_ba.fragsize = -1;
	m_pa_ba.maxlength = -1;          // max buffer, so also max latency
	m_pa_ba.minreq = -1;             // don't read every byte, try to group them _a bit_
	m_pa_ba.prebuf = -1;             // start as early as possible
	m_pa_ba.tlength = BUFFER_SIZE;   // designed latency, only change this flag for low latency output
	pa_stream_flags flags = pa_stream_flags(PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_ADJUST_LATENCY | PA_STREAM_AUTO_TIMING_UPDATE);
	m_pa_error = pa_stream_connect_playback(m_pa_s, nullptr, &m_pa_ba, flags, nullptr, nullptr);
	if (m_pa_error < 0)
	{
		ERROR_LOG(AUDIO, "PulseAudio failed to initialize: %s", pa_strerror(m_pa_error));
		return false;
	}

	INFO_LOG(AUDIO, "Pulse successfully initialized");
	return true;
}

void PulseAudio::PulseShutdown()
{
	pa_context_disconnect(m_pa_ctx);
	pa_context_unref(m_pa_ctx);
	pa_mainloop_free(m_pa_ml);
}

void PulseAudio::StateCallback(pa_context* c)
{
	pa_context_state_t state = pa_context_get_state(c);
	switch (state)
	{
	case PA_CONTEXT_FAILED:
	case PA_CONTEXT_TERMINATED:
		m_pa_connected = 2;
		break;
	case PA_CONTEXT_READY:
		m_pa_connected = 1;
		break;
	default:
		break;
	}
}
// on underflow, increase pulseaudio latency in ~10ms steps
void PulseAudio::UnderflowCallback(pa_stream* s)
{
	m_pa_ba.tlength += BUFFER_SIZE;
	pa_stream_set_buffer_attr(s, &m_pa_ba, nullptr, nullptr);

	WARN_LOG(AUDIO, "pulseaudio underflow, new latency: %d bytes", m_pa_ba.tlength);
}

void PulseAudio::WriteCallback(pa_stream* s, size_t length)
{
	// fetch dst buffer directly from pulseaudio, so no memcpy is needed
	void* buffer;
	m_pa_error = pa_stream_begin_write(s, &buffer, &length);

	if (!buffer || m_pa_error < 0)
		return; // error will be printed from main loop

	m_mixer->Mix((s16*) buffer, length / sizeof(s16) / CHANNEL_COUNT);
	m_pa_error = pa_stream_write(s, buffer, length, nullptr, 0, PA_SEEK_RELATIVE);
}

// Callbacks that forward to internal methods (required because PulseAudio is a C API).

void PulseAudio::StateCallback(pa_context* c, void* userdata)
{
	PulseAudio* p = (PulseAudio*) userdata;
	p->StateCallback(c);
}

void PulseAudio::UnderflowCallback(pa_stream* s, void* userdata)
{
	PulseAudio* p = (PulseAudio*) userdata;
	p->UnderflowCallback(s);
}

void PulseAudio::WriteCallback(pa_stream* s, size_t length, void* userdata)
{
	PulseAudio* p = (PulseAudio*) userdata;
	p->WriteCallback(s, length);
}
