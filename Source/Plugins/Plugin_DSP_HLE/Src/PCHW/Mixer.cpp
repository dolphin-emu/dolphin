// Copyright (C) 2003-2008 Dolphin Project.

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

#include <queue>
#include "../Config.h"
#include "../Globals.h"
#include "../DSPHandler.h"
#include "Thread.h"
#include "Mixer.h"
#ifdef _WIN32
#include "../PCHW/DSoundStream.h"
#endif

namespace {
Common::CriticalSection push_sync;

// On real hardware, this fifo is much, much smaller. But timing is also tighter than under Windows, so...
int queue_maxlength = 1024 * 8;
std::queue<s16> sample_queue;
}  // namespace

volatile bool mixer_HLEready = false;
volatile int queue_size = 0;

void Mixer(short *buffer, int numSamples, int bits, int rate, int channels)
{
	// silence
	memset(buffer, 0, numSamples * 2 * sizeof(short));

	// first get th DTK Music
	if (g_Config.m_EnableDTKMusic)
	{
		g_dspInitialize.pGetAudioStreaming(buffer, numSamples);
	}

	//if this was called directly from the HLE, and not by timeout
	if (g_Config.m_EnableHLEAudio && (mixer_HLEready || g_Config.m_AntiGap))
	{
		IUCode* pUCode = CDSPHandler::GetInstance().GetUCode();
		if (pUCode != NULL)
			pUCode->MixAdd(buffer, numSamples);
	}

	push_sync.Enter();
	int count = 0;
	while (sample_queue.size() && count < numSamples * 2) {
		int x = buffer[count];
		short sample = sample_queue.front();
		x += sample_queue.front();
		if (x > 32767) x = 32767;
		if (x < -32767) x = -32767;
		buffer[count] = x;
		count++;
		sample_queue.pop();
		queue_size--;
	}
	push_sync.Leave();
}

void Mixer_PushSamples(short *buffer, int num_stereo_samples, int sample_rate) {
//	static FILE *f;
//	if (!f)
//		f = fopen("d:\\hello.raw", "wb");
//	fwrite(buffer, num_stereo_samples * 4, 1, f);
	static int fraction = 0;

	while (queue_size > queue_maxlength / 2) {
#ifdef _WIN32
		DSound::DSound_UpdateSound();
#endif
		Sleep(0);
	}
	push_sync.Enter();
	for (int i = 0; i < num_stereo_samples * 2; i++) {
		while (fraction < 65536) {
			sample_queue.push(buffer[i]);
			queue_size++;
			fraction += (65536ULL * sample_rate) / 48000;
		}
		fraction &= 0xFFFF;
	}
	push_sync.Leave();
}
