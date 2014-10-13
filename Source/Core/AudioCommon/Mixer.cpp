// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "AudioCommon/AudioCommon.h"
#include "AudioCommon/Mixer.h"
#include "Common/Atomic.h"
#include "Common/CPUDetect.h"
#include "Common/MathUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/AudioInterface.h"
#include "Core/HW/VideoInterface.h"

// UGLINESS
#include "Core/PowerPC/PowerPC.h"

#if _M_SSE >= 0x301 && !(defined __GNUC__ && !defined __SSSE3__)
#include <tmmintrin.h>
#endif

// Executed from sound stream thread
unsigned int CMixer::MixerFifo::Mix(short* samples, unsigned int numSamples, bool consider_framelimit)
{
	unsigned int currentSample = 0;

	// Cache access in non-volatile variable
	// This is the only function changing the read value, so it's safe to
	// cache it locally although it's written here.
	// The writing pointer will be modified outside, but it will only increase,
	// so we will just ignore new written data while interpolating.
	// Without this cache, the compiler wouldn't be allowed to optimize the
	// interpolation loop.
	u32 indexR = Common::AtomicLoad(m_indexR);
	u32 indexW = Common::AtomicLoad(m_indexW);

	float numLeft = (float)(((indexW - indexR) & INDEX_MASK) / 2);
	m_numLeftI = (numLeft + m_numLeftI*(CONTROL_AVG-1)) / CONTROL_AVG;
	float offset = (m_numLeftI - LOW_WATERMARK) * CONTROL_FACTOR;
	if (offset > MAX_FREQ_SHIFT) offset = MAX_FREQ_SHIFT;
	if (offset < -MAX_FREQ_SHIFT) offset = -MAX_FREQ_SHIFT;

	//render numleft sample pairs to samples[]
	//advance indexR with sample position
	//remember fractional offset

	u32 framelimit = SConfig::GetInstance().m_Framelimit;
	float aid_sample_rate = m_input_sample_rate + offset;
	if (consider_framelimit && framelimit > 1)
	{
		aid_sample_rate = aid_sample_rate * (framelimit - 1) * 5 / VideoInterface::TargetRefreshRate;
	}

	const u32 ratio = (u32)( 65536.0f * aid_sample_rate / (float)m_mixer->m_sampleRate );

	s32 lvolume = m_LVolume;
	s32 rvolume = m_RVolume;

	// TODO: consider a higher-quality resampling algorithm.
	for (; currentSample < numSamples*2 && ((indexW-indexR) & INDEX_MASK) > 2; currentSample+=2)
	{
		u32 indexR2 = indexR + 2; //next sample

		s16 l1 = Common::swap16(m_buffer[indexR & INDEX_MASK]); //current
		s16 l2 = Common::swap16(m_buffer[indexR2 & INDEX_MASK]); //next
		int sampleL = ((l1 << 16) + (l2 - l1) * (u16)m_frac)  >> 16;
		sampleL = (sampleL * lvolume) >> 8;
		sampleL += samples[currentSample + 1];
		MathUtil::Clamp(&sampleL, -32767, 32767);
		samples[currentSample+1] = sampleL;

		s16 r1 = Common::swap16(m_buffer[(indexR + 1) & INDEX_MASK]); //current
		s16 r2 = Common::swap16(m_buffer[(indexR2 + 1) & INDEX_MASK]); //next
		int sampleR = ((r1 << 16) + (r2 - r1) * (u16)m_frac)  >> 16;
		sampleR = (sampleR * rvolume) >> 8;
		sampleR += samples[currentSample];
		MathUtil::Clamp(&sampleR, -32767, 32767);
		samples[currentSample] = sampleR;

		m_frac += ratio;
		indexR += 2 * (u16)(m_frac >> 16);
		m_frac &= 0xffff;
	}

	// Padding
	short s[2];
	s[0] = Common::swap16(m_buffer[(indexR - 1) & INDEX_MASK]);
	s[1] = Common::swap16(m_buffer[(indexR - 2) & INDEX_MASK]);
	s[0] = (s[0] * rvolume) >> 8;
	s[1] = (s[1] * lvolume) >> 8;
	for (; currentSample < numSamples * 2; currentSample += 2)
	{
		int sampleR = s[0] + samples[currentSample];
		MathUtil::Clamp(&sampleR, -32767, 32767);
		samples[currentSample] = sampleR;
		int sampleL = s[1] + samples[currentSample + 1];
		MathUtil::Clamp(&sampleL, -32767, 32767);
		samples[currentSample + 1] = sampleL;
	}

	// Flush cached variable
	Common::AtomicStore(m_indexR, indexR);

	return numSamples;
}

unsigned int CMixer::Mix(short* samples, unsigned int num_samples, bool consider_framelimit)
{
	if (!samples)
		return 0;

	std::lock_guard<std::mutex> lk(m_csMixing);

	memset(samples, 0, num_samples * 2 * sizeof(short));

	if (PowerPC::GetState() != PowerPC::CPU_RUNNING)
	{
		// Silence
		return num_samples;
	}

	m_dma_mixer.Mix(samples, num_samples, consider_framelimit);
	m_streaming_mixer.Mix(samples, num_samples, consider_framelimit);
	m_wiimote_speaker_mixer.Mix(samples, num_samples, consider_framelimit);
	return num_samples;
}

void CMixer::MixerFifo::PushSamples(const short *samples, unsigned int num_samples)
{
	// Cache access in non-volatile variable
	// indexR isn't allowed to cache in the audio throttling loop as it
	// needs to get updates to not deadlock.
	u32 indexW = Common::AtomicLoad(m_indexW);

	// Check if we have enough free space
	// indexW == m_indexR results in empty buffer, so indexR must always be smaller than indexW
	if (num_samples * 2 + ((indexW - Common::AtomicLoad(m_indexR)) & INDEX_MASK) >= MAX_SAMPLES * 2)
		return;

	// AyuanX: Actual re-sampling work has been moved to sound thread
	// to alleviate the workload on main thread
	// and we simply store raw data here to make fast mem copy
	int over_bytes = num_samples * 4 - (MAX_SAMPLES * 2 - (indexW & INDEX_MASK)) * sizeof(short);
	if (over_bytes > 0)
	{
		memcpy(&m_buffer[indexW & INDEX_MASK], samples, num_samples * 4 - over_bytes);
		memcpy(&m_buffer[0], samples + (num_samples * 4 - over_bytes) / sizeof(short), over_bytes);
	}
	else
	{
		memcpy(&m_buffer[indexW & INDEX_MASK], samples, num_samples * 4);
	}

	Common::AtomicAdd(m_indexW, num_samples * 2);

	return;
}

void CMixer::PushSamples(const short *samples, unsigned int num_samples)
{
	m_dma_mixer.PushSamples(samples, num_samples);
	if (m_log_dsp_audio)
		g_wave_writer_dsp.AddStereoSamplesBE(samples, num_samples);
}

void CMixer::PushStreamingSamples(const short *samples, unsigned int num_samples)
{
	m_streaming_mixer.PushSamples(samples, num_samples);
	if (m_log_dtk_audio)
		g_wave_writer_dtk.AddStereoSamplesBE(samples, num_samples);
}

void CMixer::PushWiimoteSpeakerSamples(const short *samples, unsigned int num_samples, unsigned int sample_rate)
{
	short samples_stereo[MAX_SAMPLES * 2];

	if (num_samples < MAX_SAMPLES)
	{
		m_wiimote_speaker_mixer.SetInputSampleRate(sample_rate);

		for (unsigned int i = 0; i < num_samples; ++i)
		{
			samples_stereo[i * 2] = Common::swap16(samples[i]);
			samples_stereo[i * 2 + 1] = Common::swap16(samples[i]);
		}

		m_wiimote_speaker_mixer.PushSamples(samples_stereo, num_samples);
	}
}

void CMixer::SetDMAInputSampleRate(unsigned int rate)
{
	m_dma_mixer.SetInputSampleRate(rate);
}

void CMixer::SetStreamInputSampleRate(unsigned int rate)
{
	m_streaming_mixer.SetInputSampleRate(rate);
}

void CMixer::SetStreamingVolume(unsigned int lvolume, unsigned int rvolume)
{
	m_streaming_mixer.SetVolume(lvolume, rvolume);
}

void CMixer::SetWiimoteSpeakerVolume(unsigned int lvolume, unsigned int rvolume)
{
	m_wiimote_speaker_mixer.SetVolume(lvolume, rvolume);
}

void CMixer::MixerFifo::SetInputSampleRate(unsigned int rate)
{
	m_input_sample_rate = rate;
}

void CMixer::MixerFifo::SetVolume(unsigned int lvolume, unsigned int rvolume)
{
	m_LVolume = lvolume + (lvolume >> 7);
	m_RVolume = rvolume + (rvolume >> 7);
}
