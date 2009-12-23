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
	numLeft = (numLeft > numSamples) ? numSamples : numLeft;

	// Do re-sampling if needed
	if (m_sampleRate == m_dspSampleRate)
	{
		for (unsigned int i = 0; i < numLeft * 2; i++)
			samples[i] = Common::swap16(m_buffer[(m_indexR + i) & INDEX_MASK]);
		m_indexR += numLeft * 2;
	}
	else if (m_sampleRate < m_dspSampleRate) // If down-sampling needed
	{
		_dbg_assert_msg_(DSPHLE, !(numSamples % 2), "Number of Samples: %i must be even!", numSamples);

		short *pDest = samples;
		int last_l, last_r, cur_l, cur_r;

		for (unsigned int i = 0; i < numLeft * 3 / 2; i++)
		{
			cur_l = Common::swap16(m_buffer[(m_indexR + i * 2) & INDEX_MASK]);
			cur_r = Common::swap16(m_buffer[(m_indexR + i * 2 + 1) & INDEX_MASK]);

			if (i % 3)
			{
				*pDest++ = (last_l + cur_r) / 2;
				*pDest++ = (last_r + cur_r) / 2;
			}

			last_l = cur_l;
			last_r = cur_r;
		}

		m_indexR += numLeft * 2 * 3 / 2;
	}
	else if (m_sampleRate > m_dspSampleRate)
	{
		// AyuanX: Up-sampling is not implemented yet
		PanicAlert("Mixer: Up-sampling is not implemented yet!");
/*
	static int PV1l=0,PV2l=0,PV3l=0,PV4l=0;
	static int PV1r=0,PV2r=0,PV3r=0,PV4r=0;
	static int acc=0;

	while (num_stereo_samples) {
		acc += core_sample_rate;
		while (num_stereo_samples && (acc >= 48000)) {
			PV4l=PV3l;
			PV3l=PV2l;
			PV2l=PV1l;
			PV1l=*(samples++); //32bit processing
			PV4r=PV3r;
			PV3r=PV2r;
			PV2r=PV1r;
			PV1r=*(samples++); //32bit processing
			num_stereo_samples--;
			acc-=48000;
		}
		
		// defaults to nearest
		s32 DataL = PV1l;
		s32 DataR = PV1r;
		
		if (m_mode == 1) { //linear
        
			DataL = PV1l + ((PV2l - PV1l)*acc)/48000;
			DataR = PV1r + ((PV2r - PV1r)*acc)/48000;
		}
		else if (m_mode == 2) {//cubic
			s32 a0l = PV1l - PV2l - PV4l + PV3l;
			s32 a0r = PV1r - PV2r - PV4r + PV3r;
			s32 a1l = PV4l - PV3l - a0l;
			s32 a1r = PV4r - PV3r - a0r;
			s32 a2l = PV1l - PV4l;
			s32 a2r = PV1r - PV4r;
			s32 a3l = PV2l;
			s32 a3r = PV2r;
			
			s32 t0l = ((a0l    )*acc)/48000;
			s32 t0r = ((a0r    )*acc)/48000;
			s32 t1l = ((t0l+a1l)*acc)/48000;
			s32 t1r = ((t0r+a1r)*acc)/48000;
			s32 t2l = ((t1l+a2l)*acc)/48000;
			s32 t2r = ((t1r+a2r)*acc)/48000;
			s32 t3l = ((t2l+a3l));
			s32 t3r = ((t2r+a3r));
			
			DataL = t3l;
			DataR = t3r;
		}
		
		int l = DataL, r = DataR;
		if (l < -32767) l = -32767;
		if (r < -32767) r = -32767;
		if (l > 32767) l = 32767;
		if (r > 32767) r = 32767;
		sample_queue.push(l);
		sample_queue.push(r);
		m_queueSize += 2;
	}
*/
	}

	// Padding
	if (numSamples > numLeft)
		memset(&samples[numLeft * 2], 0, (numSamples - numLeft) * 4);

	// Add the HLE sound
	if (m_sampleRate < m_dspSampleRate)
	{
		PanicAlert("Mixer: DSPHLE down-sampling is not implemented yet!\n"
			"Usually no game should require this, please report!");
	}
	else
	{
		Premix(samples, numSamples, m_sampleRate);
	}

	// Add the DTK Music
	if (m_EnableDTKMusic)
	{
		// Re-sampling is done inside
		g_dspInitialize.pGetAudioStreaming(samples, numSamples, m_sampleRate);
	}

	Common::AtomicAdd(m_numSamples, -(int)numLeft);

	return numSamples;
}


void CMixer::PushSamples(short *samples, unsigned int num_samples, unsigned int sample_rate)
{
	// The auto throttle function. This loop will put a ceiling on the CPU MHz.
	if (m_throttle)
	{
		// AyuanX: Remember to reserve "num_samples * 1.5" free sample space at least!
		// Becuse we may do re-sampling later
		while (Common::AtomicLoad(m_numSamples) >= MAX_SAMPLES - RESERVED_SAMPLES)
		{
			if (g_dspInitialize.pEmulatorState)
			{
				if (*g_dspInitialize.pEmulatorState != 0) 
					break;
			}
			soundStream->Update();
			SLEEP(1);
		}
	}

	// Check if we have enough free space
	if (num_samples > MAX_SAMPLES - Common::AtomicLoad(m_numSamples))
		return;

	// AyuanX: Actual re-sampling work has been moved to sound thread
	// to alleviates the workload on main thread
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

	if (m_sampleRate < m_dspSampleRate)
	{
		// This is kind of tricky :P  
		num_samples = num_samples * 2 / 3;
	}
	else if (m_sampleRate > m_dspSampleRate)
	{
		PanicAlert("Mixer: Up-sampling is not implemented yet!");
	}

	Common::AtomicAdd(m_numSamples, num_samples);

	return;
}

unsigned int CMixer::GetNumSamples()
{
	return Common::AtomicLoad(m_numSamples);
}

