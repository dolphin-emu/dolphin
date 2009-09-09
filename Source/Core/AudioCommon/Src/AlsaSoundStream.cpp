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

#include "Common.h"
#include "Thread.h"

#include "AlsaSoundStream.h"

#define BUFFER_SIZE 4096
#define BUFFER_SIZE_BYTES (BUFFER_SIZE*2*2)

AlsaSound::AlsaSound(CMixer *mixer) : SoundStream(mixer), thread_data(0), handle(NULL)
{
	mix_buffer = new u8[BUFFER_SIZE_BYTES];
}

AlsaSound::~AlsaSound()
{
	delete [] mix_buffer;
}

static void *ThreadTrampoline(void *args)
{
	reinterpret_cast<AlsaSound *>(args)->SoundLoop();
	return NULL;
}

bool AlsaSound::Start()
{
	thread = new Common::Thread(&ThreadTrampoline, this);
	thread_data = 0;
	return true;
}

void AlsaSound::Stop()
{
	thread_data = 1;
	delete thread;
	thread = NULL;
}

void AlsaSound::Update()
{
	// don't need to do anything here.
}

// Called on audio thread.
void AlsaSound::SoundLoop()
{
	AlsaInit();
	while (!thread_data)
	{
		int frames_to_deliver = 512;
		m_mixer->Mix(reinterpret_cast<short *>(mix_buffer), frames_to_deliver);
		int rc = snd_pcm_writei(handle, mix_buffer, frames_to_deliver);
		if (rc == -EPIPE)
		{
			// Underrun
			snd_pcm_prepare(handle);
		}
		else if (rc < 0) 
		{
			ERROR_LOG(AUDIO, "writei fail: %s", snd_strerror(rc));
		}
	}
	AlsaShutdown();
	thread_data = 2;
}

bool AlsaSound::AlsaInit()
{
	unsigned int sample_rate = m_mixer->GetSampleRate();
	int err;
	int dir;
	snd_pcm_sw_params_t *swparams;
	snd_pcm_hw_params_t *hwparams;
	
	err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (err < 0) 
	{
		ERROR_LOG(AUDIO, "Audio open error: %s\n", snd_strerror(err));
		return false;
	}

	snd_pcm_hw_params_alloca(&hwparams);
	
	err = snd_pcm_hw_params_any(handle, hwparams);
	if (err < 0) 
	{
		ERROR_LOG(AUDIO, "Broken configuration for this PCM: %s\n", snd_strerror(err));
		return false;
	}
	
	err = snd_pcm_hw_params_set_access(handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) 
	{
		ERROR_LOG(AUDIO, "Access type not available: %s\n", snd_strerror(err));
		return false;
	}
	
    err = snd_pcm_hw_params_set_format(handle, hwparams, SND_PCM_FORMAT_S16_LE);
	if (err < 0) 
	{
		ERROR_LOG(AUDIO, "Sample format not available: %s\n", snd_strerror(err));
		return false;
	}

	// This is weird - if I do pass in a pointer to a variable, like the header wants me to,
	// the sample rate goes mad. It seems that the alsa header doesn't match the library we link in :(
	// If anyone know why, i'd appreciate if you let me know - ector.
	err = snd_pcm_hw_params_set_rate_near(handle, hwparams, (unsigned int *)sample_rate, &dir);
	if (err < 0) 
	{
		ERROR_LOG(AUDIO, "Rate not available: %s\n", snd_strerror(err));
		return false;
	}
	
    err = snd_pcm_hw_params_set_channels(handle, hwparams, 2);
	if (err < 0) 
	{
		ERROR_LOG(AUDIO, "Channels count not available: %s\n", snd_strerror(err));
		return false;
	}

	err = snd_pcm_hw_params(handle, hwparams);
	if (err < 0) 
	{
	    ERROR_LOG(AUDIO, "Unable to install hw params: %s\n", snd_strerror(err));
		return false;
	}
    
	snd_pcm_sw_params_alloca(&swparams);

	err = snd_pcm_sw_params_current(handle, swparams);
	if (err < 0) 
	{
	    ERROR_LOG(AUDIO, "cannot init sw params: %s\n", snd_strerror(err));
		return false;
	}
	
	err = snd_pcm_sw_params_set_avail_min(handle, swparams, BUFFER_SIZE);
	if (err < 0) 
	{
	    ERROR_LOG(AUDIO, "cannot set avail min: %s\n", snd_strerror(err));
		return false;
	}
	
	err = snd_pcm_sw_params_set_start_threshold(handle, swparams, 0U);
	if (err < 0) 
	{
	    ERROR_LOG(AUDIO, "cannot set start thresh: %s\n", snd_strerror(err));
		return false;
	}
	
	err = snd_pcm_sw_params(handle, swparams);
	if (err < 0) 
	{
	    ERROR_LOG(AUDIO, "cannot set sw params: %s\n", snd_strerror(err));
		return false;
	}

	err = snd_pcm_prepare(handle);
	if (err < 0) 
	{
		ERROR_LOG(AUDIO, "Unable to prepare: %s\n", snd_strerror(err));
		return false;
	}
	NOTICE_LOG(AUDIO, "ALSA successfully initialized.\n");
	return true;
}

void AlsaSound::AlsaShutdown()
{
	if (handle != NULL)
	{
		snd_pcm_drop(handle);
		snd_pcm_close(handle);
		handle = NULL;
	}
}

