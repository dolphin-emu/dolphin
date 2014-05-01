// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "AudioCommon/AudioCommon.h"
#include "AudioCommon/Mixer.h"
#include "Common/Atomic.h"
#include "Common/CPUDetect.h"
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
unsigned int CMixer::Mix(short* samples, unsigned int numSamples, bool consider_framelimit)
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

	float numLeft = ((indexW - indexR) & INDEX_MASK) / 2;
	m_numLeftI = (numLeft + m_numLeftI*(CONTROL_AVG-1)) / CONTROL_AVG;
	float offset = (m_numLeftI - LOW_WATERMARK) * CONTROL_FACTOR;
	if (offset > MAX_FREQ_SHIFT) offset = MAX_FREQ_SHIFT;
	if (offset < -MAX_FREQ_SHIFT) offset = -MAX_FREQ_SHIFT;

	//render numleft sample pairs to samples[]
	//advance indexR with sample position
	//remember fractional offset

	u32 framelimit = SConfig::GetInstance().m_Framelimit;
	float aid_sample_rate = AudioInterface::GetAIDSampleRate() + offset;
	if (consider_framelimit && framelimit > 2)
	{
		aid_sample_rate = aid_sample_rate * (framelimit - 1) * 5 / VideoInterface::TargetRefreshRate;
	}

	static u32 frac = 0;
	const u32 ratio = (u32)( 65536.0f * aid_sample_rate / (float)m_sampleRate );

	if (ratio > 0x10000)
		ERROR_LOG(AUDIO, "ratio out of range");

	for (; currentSample < numSamples*2 && ((indexW-indexR) & INDEX_MASK) > 2; currentSample+=2) {
		u32 indexR2 = indexR + 2; //next sample

		s16 l1 = Common::swap16(m_buffer[indexR & INDEX_MASK]); //current
		s16 l2 = Common::swap16(m_buffer[indexR2 & INDEX_MASK]); //next
		int sampleL = ((l1 << 16) + (l2 - l1) * (u16)frac)  >> 16;
		samples[currentSample+1] = sampleL;

		s16 r1 = Common::swap16(m_buffer[(indexR + 1) & INDEX_MASK]); //current
		s16 r2 = Common::swap16(m_buffer[(indexR2 + 1) & INDEX_MASK]); //next
		int sampleR = ((r1 << 16) + (r2 - r1) * (u16)frac)  >> 16;
		samples[currentSample] = sampleR;

		frac += ratio;
		indexR += 2 * (u16)(frac >> 16);
		frac &= 0xffff;
	}

	// Padding
	unsigned short s[2];
	s[0] = Common::swap16(m_buffer[(indexR - 1) & INDEX_MASK]);
	s[1] = Common::swap16(m_buffer[(indexR - 2) & INDEX_MASK]);
	for (; currentSample < numSamples*2; currentSample+=2)
	{
		samples[currentSample] = s[0];
		samples[currentSample+1] = s[1];
	}

	// Flush cached variable
	Common::AtomicStore(m_indexR, indexR);

	// Add the DTK Music
	// Re-sampling is done inside
	AudioInterface::Callback_GetStreaming(samples, numSamples, m_sampleRate);
	if (m_logAudio)
		g_wave_writer.AddStereoSamples(samples, numSamples);

	return numSamples;
}


void CMixer::PushSamples(const short *samples, unsigned int num_samples)
{
	// Cache access in non-volatile variable
	// indexR isn't allowed to cache in the audio throttling loop as it
	// needs to get updates to not deadlock.
	u32 indexW = Common::AtomicLoad(m_indexW);

	if (m_throttle)
	{
		// The auto throttle function. This loop will put a ceiling on the CPU MHz.
		while (num_samples * 2 + ((indexW - Common::AtomicLoad(m_indexR)) & INDEX_MASK) >= MAX_SAMPLES * 2)
		{
			if (*PowerPC::GetStatePtr() != PowerPC::CPU_RUNNING || soundStream->IsMuted())
				break;
			// Shortcut key for Throttle Skipping
			if (Core::GetIsFramelimiterTempDisabled())
				break;
			SLEEP(1);
			soundStream->Update();
		}
	}

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

