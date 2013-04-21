// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "AlsaSoundStream.h"

#if defined(HAVE_ALSA) && HAVE_ALSA

#include <functional>

#include "Common.h"
#include "Thread.h"

#define FRAME_COUNT_MIN 256
#define BUFFER_SIZE_MAX 8192
#define BUFFER_SIZE_BYTES (BUFFER_SIZE_MAX*2*2)

AlsaSoundStream::AlsaSoundStream(CMixer *mixer):
	CBaseSoundStream(mixer),
	m_mix_buffer(new u8[BUFFER_SIZE_BYTES]),
	m_handle(NULL),
	m_frames_to_deliver(FRAME_COUNT_MIN),
	m_join(false)
{
}

AlsaSoundStream::~AlsaSoundStream()
{
}

bool AlsaSoundStream::OnPreThreadStart()
{
	m_join = false;
	return true;
}

// Called on audio thread.
void AlsaSoundStream::SoundLoop()
{
	if (!AlsaInit())
	{
		return;
	}

	Common::SetCurrentThreadName("Audio thread - alsa");

	CMixer *mixer = CBaseSoundStream::GetMixer();
	while (!m_join)
	{
		mixer->Mix(reinterpret_cast<short*>(m_mix_buffer.get()), m_frames_to_deliver);
		if (!CBaseSoundStream::IsMuted())
		{
			snd_pcm_sframes_t frames_written = snd_pcm_writei(m_handle, m_mix_buffer.get(), m_frames_to_deliver);
			if (frames_written == -EPIPE)
			{
				// Underrun
				snd_pcm_prepare(m_handle);
			}
			else if (frames_written < 0)
			{
				ERROR_LOG(AUDIO, "writei fail: %s\n", snd_strerror(frames_written));
			}
		}
	}
	AlsaShutdown();
}

void AlsaSoundStream::OnPreThreadJoin()
{
	m_join = true;
}

bool AlsaSoundStream::AlsaInit()
{
	int err = snd_pcm_open(&m_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (err < 0) 
	{
		ERROR_LOG(AUDIO, "Audio open error: %s\n", snd_strerror(err));
		return false;
	}

	snd_pcm_hw_params_t *hwparams = NULL;
	snd_pcm_hw_params_alloca(&hwparams);

	err = snd_pcm_hw_params_any(m_handle, hwparams);
	if (err < 0) 
	{
		ERROR_LOG(AUDIO, "Broken configuration for this PCM: %s\n", snd_strerror(err));
		return false;
	}
	
	err = snd_pcm_hw_params_set_access(m_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) 
	{
		ERROR_LOG(AUDIO, "Access type not available: %s\n", snd_strerror(err));
		return false;
	}

	err = snd_pcm_hw_params_set_format(m_handle, hwparams, SND_PCM_FORMAT_S16_LE);
	if (err < 0) 
	{
		ERROR_LOG(AUDIO, "Sample format not available: %s\n", snd_strerror(err));
		return false;
	}

	int dir = 0;
	unsigned int sample_rate = CBaseSoundStream::GetMixer()->GetSampleRate();
	err = snd_pcm_hw_params_set_rate_near(m_handle, hwparams, &sample_rate, &dir);
	if (err < 0) 
	{
		ERROR_LOG(AUDIO, "Rate not available: %s\n", snd_strerror(err));
		return false;
	}

	err = snd_pcm_hw_params_set_channels(m_handle, hwparams, 2);
	if (err < 0) 
	{
		ERROR_LOG(AUDIO, "Channels count not available: %s\n", snd_strerror(err));
		return false;
	}

	unsigned int periods = BUFFER_SIZE_MAX / FRAME_COUNT_MIN;
	err = snd_pcm_hw_params_set_periods_max(m_handle, hwparams, &periods, &dir);
	if (err < 0) 
	{
		ERROR_LOG(AUDIO, "Cannot set Minimum periods: %s\n", snd_strerror(err));
		return false;
	}

	snd_pcm_uframes_t buffer_size_max = BUFFER_SIZE_MAX;
	err = snd_pcm_hw_params_set_buffer_size_max(m_handle, hwparams, &buffer_size_max);
	if (err < 0) 
	{
		ERROR_LOG(AUDIO, "Cannot set minimum buffer size: %s\n", snd_strerror(err));
		return false;
	}

	err = snd_pcm_hw_params(m_handle, hwparams);
	if (err < 0) 
	{
		ERROR_LOG(AUDIO, "Unable to install hw params: %s\n", snd_strerror(err));
		return false;
	}

	snd_pcm_uframes_t buffer_size = 0;
	err = snd_pcm_hw_params_get_buffer_size(hwparams, &buffer_size);
	if (err < 0) 
	{
		ERROR_LOG(AUDIO, "Cannot get buffer size: %s\n", snd_strerror(err));
		return false;
	}

	err = snd_pcm_hw_params_get_periods_max(hwparams, &periods, &dir);
	if (err < 0) 
	{
		ERROR_LOG(AUDIO, "Cannot get periods: %s\n", snd_strerror(err));
		return false;
	}

	//periods is the number of fragments alsa can wait for during one 
	//buffer_size
	m_frames_to_deliver = buffer_size / periods;
	//limit the minimum size. pulseaudio advertises a minimum of 32 samples.
	if (m_frames_to_deliver < FRAME_COUNT_MIN)
	{
		m_frames_to_deliver = FRAME_COUNT_MIN;
	}
	//it is probably a bad idea to try to send more than one buffer of data
	if (frames_to_deliver > buffer_size)
	{
		frames_to_deliver = buffer_size;
	}
	NOTICE_LOG(AUDIO, "ALSA gave us a %ld sample \"hardware\" buffer with %d periods. Will send %d samples per fragments.\n", buffer_size, periods, frames_to_deliver);

	snd_pcm_sw_params_t *swparams = NULL;
	snd_pcm_sw_params_alloca(&swparams);

	err = snd_pcm_sw_params_current(handle, swparams);
	if (err < 0) 
	{
		ERROR_LOG(AUDIO, "cannot init sw params: %s\n", snd_strerror(err));
		return false;
	}

	err = snd_pcm_sw_params_set_start_threshold(handle, swparams, 0U);
	if (err < 0) 
	{
		ERROR_LOG(AUDIO, "cannot set start thresh: %s\n", snd_strerror(err));
		return false;
	}

	err = snd_pcm_sw_params(m_handle, swparams);
	if (err < 0) 
	{
		ERROR_LOG(AUDIO, "cannot set sw params: %s\n", snd_strerror(err));
		return false;
	}

	err = snd_pcm_prepare(m_handle);
	if (err < 0) 
	{
		ERROR_LOG(AUDIO, "Unable to prepare: %s\n", snd_strerror(err));
		return false;
	}
	NOTICE_LOG(AUDIO, "ALSA successfully initialized.\n");
	return true;
}

void AlsaSoundStream::AlsaShutdown()
{
	if (m_handle)
	{
		snd_pcm_drop(m_handle);
		snd_pcm_close(m_handle);
		m_handle = NULL;
	}
}

#endif
