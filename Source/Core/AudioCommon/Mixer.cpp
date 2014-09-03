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

float twos2float(u16 s) {
	int n = (s16)s;
	if (n > 0) {
		return (float) (n / (float) 0x7fff);
	}
	else {
		return (float) (n / (float) 0x8000);
	}
}

s16 float2stwos(float f) {
	int n;
	if (f > 0) {
		n = (s16) (f * 0x7fff);
	}
	else {
		n = (s16) (f * 0x8000);
	}
	return (s16) n;
}

// Executed from sound stream thread
unsigned int CMixer::MixerFifo::Mix(short* samples, unsigned int numSamples, bool consider_framelimit)
{
	srand((u32)time(NULL));
	unsigned int currentSample = 0;
	int rand1 = 0, rand2 = 0, rand3 = 0, rand4 = 0;
	float errorL1 = 0, errorL2 = 0;
	float errorR1 = 0, errorR2 = 0;
	float shape = 0.5f;
	float w = (0xFFFF);
	float width = 1.f / w;
	float dither = width / RAND_MAX;
	float doffset = width * 0.5f;
	float temp;
	// Cache access in non-volatile variable
	// This is the only function changing the read value, so it's safe to
	// cache it locally although it's written here.
	// The writing pointer will be modified outside, but it will only increase,
	// so we will just ignore new written data while interpolating.
	// Without this cache, the compiler wouldn't be allowed to optimize the
	// interpolation loop.
	u32 indexR = Common::AtomicLoad(m_indexR);
	u32 startR = indexR;
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

	// if not framelimit, then ratio = 0x10000
	//const u32 ratio = (u32)( 65536.0f * aid_sample_rate / (float)m_mixer->m_sampleRate );	//ratio of aid over sample, scaled to be 0 - 0x10000
	float ratio = aid_sample_rate / (float) m_mixer->m_sampleRate;
	
	float lvolume = (m_LVolume / 256.f);
	float rvolume = (m_RVolume / 256.f);

	for (u32 i = indexR; i < indexW - 2; i++) {
		float_buffer[i & INDEX_MASK] = twos2float(Common::swap16(m_buffer[i & INDEX_MASK]));
	}

	// TODO: consider a higher-quality resampling algorithm.
	for (; currentSample < numSamples*2 && ((indexW-indexR) & INDEX_MASK) > 2; currentSample+=2)
	{
		
		u32 indexRp = indexR - 2; //previous sample
		u32 indexR2 = indexR + 2; //next
		u32 indexR4 = indexR + 4; //nextnext
		float f2 = (m_frac * m_frac);
		float f3 = (f2 * m_frac);

		float s0, s1, s2, s3;
		float a, b, c, d;
		s0 = twos2float(Common::swap16(m_buffer[(indexRp) & INDEX_MASK])); //previous
		s1 = twos2float(Common::swap16(m_buffer[(indexR) & INDEX_MASK])); // current
		s2 = twos2float(Common::swap16(m_buffer[(indexR2) & INDEX_MASK])); //next
		s3 = twos2float(Common::swap16(m_buffer[(indexR4) & INDEX_MASK])); //nextnext
		a = -s0 + (3 * (s1 - s2)) + s3;
		b = (2 * s0) - (5 * s1) + (4 * s2) - s3;
		c = -s0 + s2;
		d = 2 * s1;
		float sampleL = a * f3;
		sampleL += b * f2;
		sampleL += c * m_frac;
		sampleL += d;
		sampleL /= 2;
		sampleL = sampleL * lvolume;
		sampleL += twos2float(samples[currentSample + 1]);

		rand2 = rand1;
		rand1 = rand();
		temp = sampleL + shape * (errorL1 + errorL1 - errorL2);
		sampleL = temp + doffset + dither * (float) (rand1 - rand2);

		if (sampleL > 1.0) sampleL = 1.0;
		if (sampleL < -1.0) sampleL = -1.0;
		int sampleLi = float2stwos(sampleL);
		samples[currentSample + 1] = sampleLi;

		errorL2 = errorL1;
		errorL1 = temp - sampleL;



		s0 = twos2float(Common::swap16(m_buffer[(indexRp + 1) & INDEX_MASK])); //previous
		s1 = twos2float(Common::swap16(m_buffer[(indexR + 1) & INDEX_MASK])); // current
		s2 = twos2float(Common::swap16(m_buffer[(indexR2 + 1) & INDEX_MASK])); //next
		s3 = twos2float(Common::swap16(m_buffer[(indexR4 + 1) & INDEX_MASK])); //nextnext
		a = -s0 + (3 * (s1 - s2)) + s3;
		b = (2 * s0) - (5 * s1) + (4 * s2) - s3;
		c = -s0 + s2;
		d = 2 * s1;
		float sampleR = a * f3;
		sampleR += b * f2;
		sampleR += c * m_frac;
		sampleR += d;
		sampleR /= 2; // divided by 2
		sampleR = sampleR * rvolume;
		sampleR += twos2float(samples[currentSample]);

		rand4 = rand3;
		rand3 = rand();
		temp = sampleR + shape * (errorR1 + errorR1 - errorR2);
		sampleR = temp + doffset + dither * (float) (rand3 - rand4);

		if (sampleR > 1.0) sampleR = 1.0;
		if (sampleR < -1.0) sampleR = -1.0;
		int sampleRi = float2stwos(sampleR);
		samples[currentSample] = sampleRi;

		errorR2 = errorR1;
		errorR1 = temp - sampleR;

		m_frac += ratio;
		indexR += 2 * (int)m_frac;
		float intpart;
		m_frac = modf(m_frac, &intpart);
	}

	// Padding
	short s[2];
	s[0] = Common::swap16(m_buffer[(indexR - 1) & INDEX_MASK]);
	s[1] = Common::swap16(m_buffer[(indexR - 2) & INDEX_MASK]);
	s[0] = (s[0] * m_RVolume) >> 8;
	s[1] = (s[1] * m_LVolume) >> 8;
	for (; currentSample < numSamples * 2; currentSample += 2)
	{
		int sampleR = s[0] + samples[currentSample];
		MathUtil::Clamp(&sampleR, -CLAMP, CLAMP);
		samples[currentSample] = sampleR;
		int sampleL = s[1] + samples[currentSample + 1];
		MathUtil::Clamp(&sampleL, -CLAMP, CLAMP);
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
	if (m_logAudio)
		g_wave_writer.AddStereoSamples(samples, num_samples);
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
}

void CMixer::PushStreamingSamples(const short *samples, unsigned int num_samples)
{
	m_streaming_mixer.PushSamples(samples, num_samples);
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
