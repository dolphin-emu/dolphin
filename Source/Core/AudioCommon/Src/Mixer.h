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

// 16 bit Stereo
#define MAX_SAMPLES			(1024 * 8)
#define INDEX_MASK			(MAX_SAMPLES * 2 - 1)
#define RESERVED_SAMPLES	(256)

class CMixer {
	
public:
	CMixer(unsigned int AISampleRate = 48000, unsigned int DACSampleRate = 48000)
		: m_aiSampleRate(AISampleRate)
		, m_dacSampleRate(DACSampleRate)
		, m_bits(16)
		, m_channels(2)
		, m_HLEready(false)
		, m_numSamples(0)
		, m_indexW(0)
		, m_indexR(0)
	{
		// AyuanX: The internal (Core & DSP) sample rate is fixed at 32KHz
		// So when AI/DAC sample rate differs than 32KHz, we have to do re-sampling
		// I perfer speed so let's do down-sampling instead of up-sampling
		// If you like better sound than speed, feel free to implement the up-sampling code
		m_sampleRate = 32000;
		INFO_LOG(AUDIO_INTERFACE, "Mixer is initialized (AISampleRate:%i, DACSampleRate:%i)", AISampleRate, DACSampleRate);
	}

	// Called from audio threads
	virtual unsigned int Mix(short* samples, unsigned int numSamples);
	virtual void Premix(short *samples, unsigned int numSamples) {}
	unsigned int GetNumSamples();

	// Called from main thread
	virtual void PushSamples(short* samples, unsigned int num_samples);
	unsigned int GetSampleRate() {return m_sampleRate;}

	void SetThrottle(bool use) { m_throttle = use;}
	void SetDTKMusic(bool use) { m_EnableDTKMusic = use;}

	// TODO: do we need this
	bool IsHLEReady() { return m_HLEready;}
	void SetHLEReady(bool ready) { m_HLEready = ready;}
	// ---------------------

protected:
	unsigned int m_sampleRate;
	unsigned int m_aiSampleRate;
	unsigned int m_dacSampleRate;
	int m_bits;
	int m_channels;
	
	bool m_HLEready;

	bool m_EnableDTKMusic;
	bool m_throttle;

	short m_buffer[MAX_SAMPLES * 2];
	u32 m_indexW;
	u32 m_indexR;
	volatile u32 m_numSamples;

private:

};

#endif // _MIXER_H_
