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

#define BUFFER_SIZE 4096
#define BUFFER_SIZE_BYTES (BUFFER_SIZE * 4)

PulseAudio::PulseAudio(CMixer *mixer)
	: SoundStream(mixer), thread_running(false), mainloop(NULL)
	, context(NULL), stream(NULL), iVolume(100)
{
	mix_buffer = new u8[BUFFER_SIZE_BYTES];
}

PulseAudio::~PulseAudio()
{
	delete [] mix_buffer;
}

bool PulseAudio::Start()
{
	thread_running = true;
	thread = std::thread(std::mem_fun(&PulseAudio::SoundLoop), this);
	return true;
}

void PulseAudio::Stop()
{
	thread_running = false;
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

	thread_running = PulseInit();

	while (thread_running)
	{
		int frames_to_deliver = 512;
		m_mixer->Mix((short *)mix_buffer, frames_to_deliver);
		if (!Write(mix_buffer, frames_to_deliver * 4))
			ERROR_LOG(AUDIO, "PulseAudio failure writing data");
	}
	PulseShutdown();
}

bool PulseAudio::PulseInit()
{
	// The Sample format to use
	const pa_sample_spec ss =
	{
		PA_SAMPLE_S16LE,
		m_mixer->GetSampleRate(),
		2
	};

	mainloop = pa_threaded_mainloop_new();

	context = pa_context_new(pa_threaded_mainloop_get_api(mainloop), "dolphin-emu");
	pa_context_set_state_callback(context, ContextStateCB, this);

	if (pa_context_connect(context, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0)
	{
		ERROR_LOG(AUDIO, "PulseAudio failed to connect context: %s",
				pa_strerror(pa_context_errno(context)));
		return false;
	}

	pa_threaded_mainloop_lock(mainloop);
	pa_threaded_mainloop_start(mainloop);

	for (;;)
	{
		pa_context_state_t state;

		state = pa_context_get_state(context);

		if (state == PA_CONTEXT_READY)
			break;

		if (!PA_CONTEXT_IS_GOOD(state))
		{
			ERROR_LOG(AUDIO, "PulseAudio context state failure: %s",
					pa_strerror(pa_context_errno(context)));
			pa_threaded_mainloop_unlock(mainloop);
			return false;
		}

		// Wait until the context is ready
		pa_threaded_mainloop_wait(mainloop);
	}

	if (!(stream = pa_stream_new(context, "emulator", &ss, NULL)))
	{
		ERROR_LOG(AUDIO, "PulseAudio failed to create playback stream: %s",
				pa_strerror(pa_context_errno(context)));
		pa_threaded_mainloop_unlock(mainloop);
		return false;
	}

	// Set callbacks for the playback stream
	pa_stream_set_state_callback(stream, StreamStateCB, this);
	pa_stream_set_write_callback(stream, StreamWriteCB, this);

	if (pa_stream_connect_playback(stream, NULL, NULL, PA_STREAM_NOFLAGS, NULL, NULL) < 0)
	{
		ERROR_LOG(AUDIO, "PulseAudio failed to connect playback stream: %s",
				pa_strerror(pa_context_errno(context)));
		pa_threaded_mainloop_unlock(mainloop);
		return false;
	}

	for (;;)
	{
		pa_stream_state_t state;

		state = pa_stream_get_state(stream);

		if (state == PA_STREAM_READY)
			break;

		if (!PA_STREAM_IS_GOOD(state))
		{
			ERROR_LOG(AUDIO, "PulseAudio stream state failure: %s",
					pa_strerror(pa_context_errno(context)));
			pa_threaded_mainloop_unlock(mainloop);
			return false;
		}

		// Wait until the stream is ready
		pa_threaded_mainloop_wait(mainloop);
	}

	pa_threaded_mainloop_unlock(mainloop);

	SetVolume(iVolume);

	NOTICE_LOG(AUDIO, "Pulse successfully initialized.");
	return true;
}

void PulseAudio::PulseShutdown()
{
	if (mainloop)
		pa_threaded_mainloop_stop(mainloop);

	if (stream)
		pa_stream_unref(stream);

	if (context)
	{
		pa_context_disconnect(context);
		pa_context_unref(context);
	}

	if (mainloop)
		pa_threaded_mainloop_free(mainloop);
}

void PulseAudio::SignalMainLoop()
{
	pa_threaded_mainloop_signal(mainloop, 0);
}

void PulseAudio::ContextStateCB(pa_context *c, void *userdata)
{
	switch (pa_context_get_state(c))
	{
		case PA_CONTEXT_READY:
		case PA_CONTEXT_TERMINATED:
		case PA_CONTEXT_FAILED:
			((PulseAudio *)userdata)->SignalMainLoop();
			break;

		default:
			break;
	}
}

void PulseAudio::StreamStateCB(pa_stream *s, void * userdata)
{
	switch (pa_stream_get_state(s))
	{
		case PA_STREAM_READY:
		case PA_STREAM_TERMINATED:
		case PA_STREAM_FAILED:
			((PulseAudio *)userdata)->SignalMainLoop();
			break;

		default:
			break;
	}
}

void PulseAudio::StreamWriteCB(pa_stream *s, size_t length, void *userdata)
{
	((PulseAudio *)userdata)->SignalMainLoop();
}

static bool StateIsGood(pa_context *context, pa_stream *stream)
{
	if (!context || !PA_CONTEXT_IS_GOOD(pa_context_get_state(context)) ||
			!stream || !PA_STREAM_IS_GOOD(pa_stream_get_state(stream)))
	{
		if ((context && pa_context_get_state(context) == PA_CONTEXT_FAILED) ||
				(stream && pa_stream_get_state(stream) == PA_STREAM_FAILED))
		{
			ERROR_LOG(AUDIO, "PulseAudio state failure: %s",
					pa_strerror(pa_context_errno(context)));
		}
		else
		{
			ERROR_LOG(AUDIO, "PulseAudio state failure: %s",
					pa_strerror(PA_ERR_BADSTATE));
		}
		return false;
	}
	return true;
}

bool PulseAudio::Write(const void *data, size_t length)
{
	if (!data || length == 0 || !stream)
		return false;

	pa_threaded_mainloop_lock(mainloop);

	if (!StateIsGood(context, stream))
	{
		pa_threaded_mainloop_unlock(mainloop);
		return false;
	}

	while (length > 0)
	{
		size_t l;
		int r;

		while (!(l = pa_stream_writable_size(stream)))
		{
			pa_threaded_mainloop_wait(mainloop);
			if (!StateIsGood(context, stream))
			{
				pa_threaded_mainloop_unlock(mainloop);
				return false;
			}
		}

		if (l == (size_t)-1)
		{
			ERROR_LOG(AUDIO, "PulseAudio invalid stream:  %s",
					pa_strerror(pa_context_errno(context)));
			pa_threaded_mainloop_unlock(mainloop);
			return false;
		}

		if (l > length)
			l = length;

		r = pa_stream_write(stream, data, l, NULL, 0LL, PA_SEEK_RELATIVE);
		if (r < 0)
		{
			ERROR_LOG(AUDIO, "PulseAudio error writing to stream:  %s",
					pa_strerror(pa_context_errno(context)));
			pa_threaded_mainloop_unlock(mainloop);
			return false;
		}

		data = (const uint8_t*) data + l;
		length -= l;
	}

	pa_threaded_mainloop_unlock(mainloop);
	return true;
}

void PulseAudio::SetVolume(int volume)
{
	iVolume = volume;

	if (!stream)
		return;

	pa_cvolume cvolume;
	const pa_channel_map *channels = pa_stream_get_channel_map(stream);
	pa_cvolume_set(&cvolume, channels->channels,
			iVolume * (PA_VOLUME_NORM - PA_VOLUME_MUTED) / 100);

	pa_context_set_sink_input_volume(context, pa_stream_get_index(stream),
			&cvolume, NULL, this);
}
