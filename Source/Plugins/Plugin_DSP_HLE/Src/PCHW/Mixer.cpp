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

	static int PV1l=0,PV2l=0,PV3l=0,PV4l=0;
	static int PV1r=0,PV2r=0,PV3r=0,PV4r=0;
	static int acc=0;

#ifdef _WIN32
	if (!GetAsyncKeyState(VK_TAB)) {
	while (queue_size > queue_maxlength / 2) {
		DSound::DSound_UpdateSound();
		Sleep(0);
	}
	} else {
		return;
	}
#else
	while (queue_size > queue_maxlength / 2) {
		sleep(0);
	}
#endif
	//convert into config option?
	const int mode = 2;

	push_sync.Enter();
	while (num_stereo_samples) 
	{
		acc += sample_rate;
 		while (num_stereo_samples && (acc>=48000))
 		{
 			PV4l=PV3l;
 			PV3l=PV2l;
 			PV2l=PV1l;
 			PV1l=*(buffer++); //32bit processing
 			PV4r=PV3r;
 			PV3r=PV2r;
 			PV2r=PV1r;
 			PV1r=*(buffer++); //32bit processing
			num_stereo_samples--;
 			acc-=48000;
 		}

		// defaults to nearest
		s32 DataL = PV1l;
		s32 DataR = PV1r;

 		if (mode == 1) //linear
 		{
			DataL = PV1l + ((PV2l - PV1l)*acc)/48000;
			DataR = PV1r + ((PV2r - PV1r)*acc)/48000;
		}
 		else if (mode == 2) //cubic
 		{
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
		sample_queue.push(DataL);
		sample_queue.push(DataR);
		queue_size += 2;
 	}
	push_sync.Leave();
}
