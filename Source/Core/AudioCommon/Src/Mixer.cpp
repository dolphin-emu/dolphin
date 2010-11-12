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

// Executed from sound stream thread
unsigned int CMixer::Mix(short* samples, unsigned int numSamples)
{
	if (!samples)
		return 0;

	if (g_dspInitialize.pEmulatorState)
	{
		if (*g_dspInitialize.pEmulatorState != 0)
		{
			// Silence
			memset(samples, 0, numSamples * 4);
			return numSamples;
		}
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

		// Do re-sampling if needed
		if (m_sampleRate == 32000)
		{
			for (unsigned int i = 0; i < numLeft * 2; i++)
				samples[i] = Common::swap16(m_buffer[(m_indexR + i) & INDEX_MASK]);
			m_indexR += numLeft * 2;
		}
		else //linear interpolation
		{
			//render numleft sample pairs to samples[]
			//advance m_indexR with sample position
			//remember fractional offset

			static u32 frac = 0;
			const u32 ratio = (u32)( 65536.0f * 32000.0f / (float)m_sampleRate );

			for (u32 i = 0; i < numLeft * 2; i+=2) {
				u32 m_indexR2 = m_indexR + 2; //next sample
				if ((m_indexR2 & INDEX_MASK) == (m_indexW & INDEX_MASK)) //..if it exists
					m_indexR2 = m_indexR;

				
				s16 l1 = Common::swap16(m_buffer[m_indexR & INDEX_MASK]); //current
				s16 l2 = Common::swap16(m_buffer[m_indexR2 & INDEX_MASK]); //next
				int sampleL = (l1 << 16) + (l2 - l1) * (u16)frac  >> 16;	
				samples[i]   = sampleL;			
				
				s16 r1 = Common::swap16(m_buffer[(m_indexR + 1) & INDEX_MASK]); //current
				s16 r2 = Common::swap16(m_buffer[(m_indexR2 + 1) & INDEX_MASK]); //next
				int sampleR = (r1 << 16) + (r2 - r1) * (u16)frac  >> 16;
				samples[i+1] = sampleR;

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
		memset(&samples[numLeft * 2], 0, (numSamples - numLeft) * 4);

	// Add the DSPHLE sound, re-sampling is done inside
	Premix(samples, numSamples);

	// Add the DTK Music
	if (m_EnableDTKMusic)
	{
		// Re-sampling is done inside
		g_dspInitialize.pGetAudioStreaming(samples, numSamples, m_sampleRate);
	}

	Common::AtomicAdd(m_numSamples, -(s32)numLeft);

	return numSamples;
}


void CMixer::PushSamples(short *samples, unsigned int num_samples)
{
	if (m_throttle)
	{
		// The auto throttle function. This loop will put a ceiling on the CPU MHz.
		while (Common::AtomicLoad(m_numSamples) + RESERVED_SAMPLES >= MAX_SAMPLES)
		{
			if (g_dspInitialize.pEmulatorState)
			{
				if (*g_dspInitialize.pEmulatorState != 0) 
					break;
			}
			// Shortcut key for Throttle Skipping
			#ifdef _WIN32
			if (GetAsyncKeyState(VK_TAB)) break;;
			#endif
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

	if (m_sampleRate == 32000)
		Common::AtomicAdd(m_numSamples, num_samples);
	else if (m_sampleRate == 48000)
		Common::AtomicAdd(m_numSamples, num_samples * 1.5);
	else
		PanicAlert("Mixer: Unsupported sample rate.");

	return;
}

unsigned int CMixer::GetNumSamples()
{
	return Common::AtomicLoad(m_numSamples);
}

