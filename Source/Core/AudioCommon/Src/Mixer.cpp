// Copyright (C) 2003-2009 Dolphin Project.

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


// This queue solution is temporary. I'll implement something more efficient later.
#include <queue> // System

#include "Thread.h" // Common

#include "Mixer.h"
#include "FixedSizeQueue.h"
#include "AudioCommon.h"

void CMixer::Mix(short *samples, int numSamples)
{
	if (! samples) {
		Premix(NULL, 0);
		return;
	}
	// silence
	memset(samples, 0, numSamples * 2 * sizeof(short));

	if (g_dspInitialize.pEmulatorState) {
		if (*g_dspInitialize.pEmulatorState != 0)
			return;
	}

	Premix(samples, numSamples);
	
	push_sync.Enter();
	int count = 0;
	while (m_queueSize > queue_minlength && count < numSamples * 2) {
		int x = samples[count];
		x += sample_queue.front();
		if (x > 32767) x = 32767;
		if (x < -32767) x = -32767;
		samples[count++] = x;
		sample_queue.pop();
		x = samples[count];
		x += sample_queue.front();
		if (x > 32767) x = 32767;
		if (x < -32767) x = -32767;
		samples[count++] = x;
		sample_queue.pop();
		m_queueSize-=2;
	}
	push_sync.Leave();
}


void CMixer::PushSamples(short *samples, int num_stereo_samples, int core_sample_rate)
{
	if (!soundStream)
		return;

	push_sync.Enter();
	if (m_queueSize == 0)
	{
		m_queueSize = queue_minlength;
		for (int i = 0; i < queue_minlength; i++)
			sample_queue.push((s16)0);
	}
	push_sync.Leave();
	static int PV1l=0,PV2l=0,PV3l=0,PV4l=0;
	static int PV1r=0,PV2r=0,PV3r=0,PV4r=0;
	static int acc=0;     
 
#ifdef _WIN32
	if (GetAsyncKeyState(VK_TAB))
		return;
#endif

	// Write Other Audio
	if (!m_throttle)
		return;
	
	
	/* This is only needed for non-AX sound, currently directly
	   streamed and DTK sound. For AX we call SoundStream::Update in
	   AXTask() for example. */
	while (m_queueSize > queue_maxlength / 2) {

		// Urgh.
		if (g_dspInitialize.pEmulatorState) {
			if (*g_dspInitialize.pEmulatorState != 0) 
				return;
		}
		soundStream->Update();
		Common::SleepCurrentThread(0);
	}
	
	push_sync.Enter();
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
	push_sync.Leave();
}
