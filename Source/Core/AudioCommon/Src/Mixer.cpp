// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "AudioCommon.h"
#include "AudioResampler.h"
#include "Mixer.h"
#include "CPUDetect.h"
#include "../../Core/Src/Host.h"

#include "../../Core/Src/HW/AudioInterface.h"

// UGLINESS
#include "../../Core/Src/PowerPC/PowerPC.h"

#if _M_SSE >= 0x301 && !(defined __GNUC__ && !defined __SSSE3__)
#include <tmmintrin.h>
#endif

namespace
{

// The data coming from the AI looks like this: R0R1 L0L1 ...
// R0R1 is the right sample encoded as big-endian, L0L1 same for left.
//
// We need instead: L1L0 R1R0. That means exchanging right and left, and
// swapping each 16 bit sample to make it little endian.
//
// But it turns out this is exactly what swapping a 32 bit integer does,
// so we just Common::swap32 the samples interpreted as an integer array.
void SwapAndExchangeSamples(s16* samples, u32 nSamples)
{
	int* samplesInt = reinterpret_cast<int*>(samples);
	for (u32 i = 0; i < nSamples; ++i)
	{
		samplesInt[i] = Common::swap32(samplesInt[i]);
	}
}

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

	unsigned int numLeft = GetNumSamples();
	if (m_AIplaying)
	{
		if (numLeft < numSamples) //cannot do much about this
			m_AIplaying = false;
		if (numLeft < MAX_SAMPLES / 4) //low watermark
			m_AIplaying = false;
	}
	else
	{
		if (numLeft > MAX_SAMPLES / 2) //high watermark
			m_AIplaying = true;
	}

	if (m_AIplaying) {
		numLeft = (numLeft > numSamples) ? numSamples : numLeft;

		if (AudioInterface::GetAIDSampleRate() == m_sampleRate) // (1:1)
		{
			m_buffer.PopMultiple(samples, numLeft * 2);
			SwapAndExchangeSamples(samples, numLeft);
		}
		else // linear interpolation
		{
			const u32 ratio = (u32)(65536.0f * (float)AudioInterface::GetAIDSampleRate() / (float)m_sampleRate);

			// Compute the number of samples needed to interpolate to the
			// wanted number of output samples.
			u32 neededInput =
				Common::GetCountNeededForResample(numLeft, m_fracPos, ratio,
				                                  Common::RESAMPLING_LINEAR);

			s16* inputSamples = (s16*)alloca(neededInput * 2 * sizeof (s16));
			m_buffer.PopMultiple(inputSamples, neededInput * 2);
			SwapAndExchangeSamples(inputSamples, neededInput);

			// Resample once for left, once for right.
			u32 new_pos;
			new_pos = Common::ResampleAudio([&](u32 i) { return inputSamples[i * 2]; },
			                                samples, numLeft, 2, m_lastSamplesLeft.data(),
			                                m_fracPos, ratio, Common::RESAMPLING_LINEAR,
			                                NULL);
			new_pos = Common::ResampleAudio([&](u32 i) { return inputSamples[i * 2 + 1]; },
			                                samples + 1, numLeft, 2, m_lastSamplesRight.data(),
			                                m_fracPos, ratio, Common::RESAMPLING_LINEAR,
			                                NULL);
			m_fracPos = new_pos & 0xFFFF;
		}
	}
	else
	{
		numLeft = 0;
	}

	// Pad the buffer by repeating the last sample.
	if (numSamples > numLeft)
	{
		s16 lastSampleLeft = 0, lastSampleRight = 0;
		if (numLeft != 0)
		{
			lastSampleLeft = samples[(numLeft - 1) * 2];
			lastSampleRight = samples[(numLeft - 1) * 2 + 1];
		}

		for (u32 i = 0; i < numSamples - numLeft; ++i)
		{
			samples[(numLeft + i) * 2] = lastSampleLeft;
			samples[(numLeft + i) * 2 + 1] = lastSampleRight;
		}
	}

	//when logging, also throttle HLE audio
	if (m_logAudio)
	{
		if (m_AIplaying)
		{
			Premix(samples, numLeft);
			AudioInterface::Callback_GetStreaming(samples, numLeft, m_sampleRate);
			g_wave_writer.AddStereoSamples(samples, numLeft);
		}
	}
	else
	{
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
	if (m_throttle)
	{
		// The auto throttle function. This loop will put a ceiling on the CPU MHz.
		while (num_samples * 2 >= m_buffer.WritableCount())
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
	if (num_samples * 2 >= m_buffer.WritableCount())
		return;

	// Push the swapped data to the ring buffer.
	m_buffer.PushMultiple(samples, num_samples * 2);
}

unsigned int CMixer::GetNumSamples()
{
	// Guess how many samples would be available after interpolation.
	// As interpolation needs at least on sample from the future to
	// linear interpolate between them, one sample less is available.
	// We also can't say the current interpolation state (specially
	// the frac), so to be sure, subtract one again to be sure not
	// to underflow the fifo.
	
	u32 numSamples = m_buffer.ReadableCount() / 2;

	if (AudioInterface::GetAIDSampleRate() == m_sampleRate)
		numSamples = numSamples; // 1:1
	else if (m_sampleRate == 48000 && AudioInterface::GetAIDSampleRate() == 32000)
		numSamples = numSamples * 3 / 2 - 2; // most common case
	else
		numSamples = numSamples * m_sampleRate / AudioInterface::GetAIDSampleRate() - 2;

	return numSamples;
}

