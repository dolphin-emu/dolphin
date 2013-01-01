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

	unsigned int numLeft = Common::AtomicLoad(m_numSamples);
	if (m_AIplaying) {
		if (numLeft < numSamples)//cannot do much about this
			m_AIplaying = false;
		if (numLeft < MAX_SAMPLES/4)//low watermark
			m_AIplaying = false;
	} else {
		if (numLeft > MAX_SAMPLES/2)//high watermark
			m_AIplaying = true;
	}

	if (m_AIplaying) {
		numLeft = (numLeft > numSamples) ? numSamples : numLeft;

		if (AudioInterface::GetAIDSampleRate() == m_sampleRate) // (1:1)
		{
#if _M_SSE >= 0x301
			if (cpu_info.bSSSE3 && !((numLeft * 2) % 8))
			{
				static const __m128i sr_mask =
					_mm_set_epi32(0x0C0D0E0FL, 0x08090A0BL,
						      0x04050607L, 0x00010203L);

				for (unsigned int i = 0; i < numLeft * 2; i += 8)
				{
					_mm_storeu_si128((__m128i *)&samples[i], _mm_shuffle_epi8(_mm_loadu_si128((__m128i *)&m_buffer[(m_indexR + i) & INDEX_MASK]), sr_mask));
				}
			}
			else
#endif
			{
				for (unsigned int i = 0; i < numLeft * 2; i+=2)
				{
					samples[i] = Common::swap16(m_buffer[(m_indexR + i + 1) & INDEX_MASK]);
					samples[i+1] = Common::swap16(m_buffer[(m_indexR + i) & INDEX_MASK]);
				}
			}
			m_indexR += numLeft * 2;
		}
		else //linear interpolation
		{
			//render numleft sample pairs to samples[]
			//advance m_indexR with sample position
			//remember fractional offset

			static u32 frac = 0;
			const u32 ratio = (u32)( 65536.0f * (float)AudioInterface::GetAIDSampleRate() / (float)m_sampleRate );

			for (u32 i = 0; i < numLeft * 2; i+=2) {
				u32 m_indexR2 = m_indexR + 2; //next sample
				if ((m_indexR2 & INDEX_MASK) == (m_indexW & INDEX_MASK)) //..if it exists
					m_indexR2 = m_indexR;

				
				s16 l1 = Common::swap16(m_buffer[m_indexR & INDEX_MASK]); //current
				s16 l2 = Common::swap16(m_buffer[m_indexR2 & INDEX_MASK]); //next
				int sampleL = ((l1 << 16) + (l2 - l1) * (u16)frac)  >> 16;	
				samples[i+1] = sampleL;			
				
				s16 r1 = Common::swap16(m_buffer[(m_indexR + 1) & INDEX_MASK]); //current
				s16 r2 = Common::swap16(m_buffer[(m_indexR2 + 1) & INDEX_MASK]); //next
				int sampleR = ((r1 << 16) + (r2 - r1) * (u16)frac)  >> 16;
				samples[i] = sampleR;

				frac += ratio;
				m_indexR += 2 * (u16)(frac >> 16);
				frac &= 0xffff;
			}
		}



	} else {
		numLeft = 0;
	}

	// Padding
	if (numSamples > numLeft)
	{
		unsigned short s[2];
		s[0] = Common::swap16(m_buffer[(m_indexR - 1) & INDEX_MASK]);
		s[1] = Common::swap16(m_buffer[(m_indexR - 2) & INDEX_MASK]);
		for (unsigned int i = numLeft*2; i < numSamples*2; i+=2)
			*(u32*)(samples+i) = *(u32*)(s);
//		memset(&samples[numLeft * 2], 0, (numSamples - numLeft) * 4);
	}

	//when logging, also throttle HLE audio
	if (m_logAudio) {
		if (m_AIplaying) {
			Premix(samples, numLeft);

			AudioInterface::Callback_GetStreaming(samples, numLeft, m_sampleRate);

			g_wave_writer.AddStereoSamples(samples, numLeft);
		}
	}
	else { 	//or mix as usual
		// Add the DSPHLE sound, re-sampling is done inside
		Premix(samples, numSamples);

		// Add the DTK Music
		// Re-sampling is done inside
		AudioInterface::Callback_GetStreaming(samples, numSamples, m_sampleRate);
	}


	Common::AtomicAdd(m_numSamples, -(s32)numLeft);

	return numSamples;
}


void CMixer::PushSamples(const short *samples, unsigned int num_samples)
{
	if (m_throttle)
	{
		// The auto throttle function. This loop will put a ceiling on the CPU MHz.
		while (num_samples + Common::AtomicLoad(m_numSamples) > MAX_SAMPLES)
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
	if (num_samples + Common::AtomicLoad(m_numSamples) > MAX_SAMPLES)
		return;

	// AyuanX: Actual re-sampling work has been moved to sound thread
	// to alleviate the workload on main thread
	// and we simply store raw data here to make fast mem copy
	int over_bytes = num_samples * 4 - (MAX_SAMPLES * 2 - (m_indexW & INDEX_MASK)) * sizeof(short);
	if (over_bytes > 0)
	{
		memcpy(&m_buffer[m_indexW & INDEX_MASK], samples, num_samples * 4 - over_bytes);
		memcpy(&m_buffer[0], samples + (num_samples * 4 - over_bytes) / sizeof(short), over_bytes);
	}
	else
	{
		memcpy(&m_buffer[m_indexW & INDEX_MASK], samples, num_samples * 4);
	}

	m_indexW += num_samples * 2;

	if (AudioInterface::GetAIDSampleRate() == m_sampleRate)
		Common::AtomicAdd(m_numSamples, num_samples);
	else if ((AudioInterface::GetAIDSampleRate() == 32000) && (m_sampleRate == 48000))
		Common::AtomicAdd(m_numSamples, num_samples * 3 / 2);
	else
		Common::AtomicAdd(m_numSamples, num_samples * 2 / 3);

	return;
}

unsigned int CMixer::GetNumSamples()
{
	return Common::AtomicLoad(m_numSamples);
}

