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

#ifndef _MIXER_H_
#define _MIXER_H_

#include "FixedSizeQueue.h"
#include "Thread.h"

// On real hardware, this fifo is much, much smaller. But timing is also
// tighter than under Windows, so...
#define queue_minlength  1024 * 4
#define queue_maxlength  1024 * 28

class CMixer {
	
public:
	// AyuanX: Mixer sample rate is fixed to 32khz for now
	// if any game sets DSP sample rate to 48khz, we are doomed
	// TODO: Fix this somehow!
	CMixer(unsigned int SampleRate = 32000)
		: m_sampleRate(SampleRate)
		, m_bits(16)
		, m_channels(2)
		, m_mode(2)
		, m_HLEready(false)
		, m_queueSize(0)
	{}

	// Called from audio threads
	virtual int Mix(short *sample, int numSamples);
	virtual int GetNumSamples();

	// Called from main thread
	virtual void PushSamples(short* samples, int num_stereo_samples, int core_sample_rate);
	
	virtual void Premix(short *samples, int numSamples) {}

	int GetSampleRate() {return m_sampleRate;}

	void SetThrottle(bool use) { m_throttle = use;}
	void SetDTKMusic(bool use) { m_EnableDTKMusic = use;}

	// TODO: do we need this
	bool IsHLEReady() { return m_HLEready;}
	void SetHLEReady(bool ready) { m_HLEready = ready;}
	// ---------------------

protected:
	int m_sampleRate;
	int m_bits;
	int m_channels;
	
	int m_mode;
	bool m_HLEready;
	int m_queueSize;

	bool m_EnableDTKMusic;
	bool m_throttle;
private:
	Common::CriticalSection push_sync;
	FixedSizeQueue<s16, queue_maxlength> sample_queue;

};

#endif // _MIXER_H_
