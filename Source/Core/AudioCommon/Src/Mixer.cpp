// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Atomic.h"
#include "Mixer.h"
#include "AudioCommon.h"
#include "CPUDetect.h"
#include "../../Core/Src/Host.h"

#include "../../Core/Src/HW/AudioInterface.h"

// UGLINESS
#include "../../Core/Src/PowerPC/PowerPC.h"

#if _M_SSE >= 0x301 && !(defined __GNUC__ && !defined __SSSE3__)
#include <tmmintrin.h>
#endif

#include <soundtouch/SoundTouch.h>

soundtouch::SAMPLETYPE m_buffer[MAX_SAMPLES * 2];
soundtouch::SoundTouch m_soundtouch;
	
CMixer::CMixer(unsigned int AISampleRate, unsigned int DACSampleRate, unsigned int BackendSampleRate)
	: m_aiSampleRate(AISampleRate)
	, m_dacSampleRate(DACSampleRate)
	, m_bits(16)
	, m_channels(2)
	, m_HLEready(false)
	, m_logAudio(0)
	, m_indexW(0)
	, m_indexR(0)
	, m_AIplaying(true)
	, m_last_speed(1.0)
{
	// AyuanX: The internal (Core & DSP) sample rate is fixed at 32KHz
	// So when AI/DAC sample rate differs than 32KHz, we have to do re-sampling
	m_sampleRate = BackendSampleRate;

	memset(m_buffer, 0, sizeof(m_buffer));
	
	m_soundtouch.setChannels(m_channels);
	m_soundtouch.setSampleRate(m_sampleRate);
	m_soundtouch.setRate(32000. / m_sampleRate);
	m_soundtouch.setTempo(1.0);
	m_soundtouch.setSetting(SETTING_USE_QUICKSEEK, 0);
	m_soundtouch.setSetting(SETTING_USE_AA_FILTER, 0);
	m_soundtouch.setSetting(SETTING_SEQUENCE_MS, 1);
	m_soundtouch.setSetting(SETTING_SEEKWINDOW_MS, 28);
	m_soundtouch.setSetting(SETTING_OVERLAP_MS, 12);
	m_soundtouch.clear();
	
	INFO_LOG(AUDIO_INTERFACE, "Mixer is initialized (AISampleRate:%i, DACSampleRate:%i)", AISampleRate, DACSampleRate);
}

// Executed from sound stream thread
unsigned int CMixer::Mix(short* samples, unsigned int numSamples)
{
	if (!samples)
		return 0;

	std::lock_guard<std::mutex> lk(m_csMixing);

	if (PowerPC::GetState() != PowerPC::CPU_RUNNING)
	{
		// Silence
		memset(samples, 0, numSamples * 4);
		return numSamples;
	}

	// cache access in non-volatile variable
	u32 indexR = m_indexR;
	u32 indexW = m_indexW;
	float speed = m_speed;
	
	if(speed != m_last_speed)
	{
		m_last_speed = speed;
		speed = std::min<float>(std::max<float>(speed, 0.1), 10.0);
		m_soundtouch.setSetting(SETTING_SEQUENCE_MS, (int)(1 / (speed * speed)));
		m_soundtouch.setTempo(speed);
	}
	
	// convert samples in buffer
	if((indexW & INDEX_MASK) < (indexR & INDEX_MASK)) {
		// rollover, so convert to end of buffer
		u32 read_samples = std::min<int>((2*MAX_SAMPLES-(indexR & INDEX_MASK))/2, std::max<int>(0, MAX_SAMPLES-m_soundtouch.numSamples()));
		m_soundtouch.putSamples(m_buffer + (indexR & INDEX_MASK), read_samples);
		indexR += 2*read_samples;
	}
	if((indexW & INDEX_MASK) > (indexR & INDEX_MASK)) {
		u32 read_samples = std::min<int>(((indexW-indexR) & INDEX_MASK)/2, std::max<int>(0, MAX_SAMPLES-m_soundtouch.numSamples()));
		m_soundtouch.putSamples(m_buffer + (indexR & INDEX_MASK), read_samples);
		indexR += 2*read_samples;
	}
	m_indexR = indexR;
	
	if (m_AIplaying) {
		if (m_soundtouch.numSamples() < numSamples) //cannot do much about this
			m_AIplaying = false;
	} else {
		if (m_soundtouch.numSamples() > MAX_SAMPLES/2) //high watermark
			m_AIplaying = true;
	}
	
	if (m_AIplaying) {
		soundtouch::SAMPLETYPE *buffer = (soundtouch::SAMPLETYPE*)alloca(2 * numSamples * sizeof(soundtouch::SAMPLETYPE));
		m_soundtouch.receiveSamples(buffer, numSamples);
		
		for(u32 i=0; i<2*numSamples; i++)
		{
			samples[i] = buffer[i];
		}
		
		m_last_sample[0] = buffer[2*numSamples-2];
		m_last_sample[1] = buffer[2*numSamples-1];
	}
	else
	{
		// padding
		for(u32 i=0; i<2*numSamples; i+=2)
		{
			samples[i] = m_last_sample[0];
			samples[i+1] = m_last_sample[1];
		}
	}

	//when logging, also throttle HLE audio
	if (m_logAudio) {
		if (m_AIplaying) {
			Premix(samples, numSamples);

			AudioInterface::Callback_GetStreaming(samples, numSamples, m_sampleRate);

			g_wave_writer.AddStereoSamples(samples, numSamples);
		}
	}
	else { 	//or mix as usual
		// Add the DSPHLE sound, re-sampling is done inside
		Premix(samples, numSamples);

		// Add the DTK Music
		// Re-sampling is done inside
		AudioInterface::Callback_GetStreaming(samples, numSamples, m_sampleRate);
	}

	return numSamples;
}


void CMixer::PushSamples(const short *samples, unsigned int num_samples)
{
	// cache access in non-volatile variable
	u32 indexW = m_indexW;
	
	if (m_throttle)
	{
		// The auto throttle function. This loop will put a ceiling on the CPU MHz.
		while (num_samples * 2 + ((indexW - m_indexR) & INDEX_MASK) >= MAX_SAMPLES * 2)
		{
			if (*PowerPC::GetStatePtr() != PowerPC::CPU_RUNNING || soundStream->IsMuted()) 
				break;
			// Shortcut key for Throttle Skipping
			if (Host_GetKeyState('\t'))
				break;
			SLEEP(1);
			soundStream->Update();
		}
	}

	// Check if we have enough free space
	// indexW == m_indexR results in empty buffer, so indexR must always be smaller than indexW
	if (num_samples * 2 + ((indexW - m_indexR) & INDEX_MASK) >= MAX_SAMPLES * 2)
		return;

	for(u32 i=0; i<num_samples; i++) {
		m_buffer[(indexW + 2*i) & INDEX_MASK] = float(s16(Common::swap16(samples[2*i])));
		m_buffer[(indexW + 2*i + 1) & INDEX_MASK] = float(s16(Common::swap16(samples[2*i + 1])));
	}

	m_indexW += num_samples * 2;
	
	return;
}

