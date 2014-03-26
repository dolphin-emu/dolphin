// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "AudioCommon/WaveFile.h"
#include "Common/StdMutex.h"

// 16 bit Stereo
#define MAX_SAMPLES     (1024 * 2) // 64ms
#define INDEX_MASK      (MAX_SAMPLES * 2 - 1)

#define LOW_WATERMARK   1280 // 40 ms
#define MAX_FREQ_SHIFT  200  // per 32000 Hz
#define CONTROL_FACTOR  0.2  // in freq_shift per fifo size offset
#define CONTROL_AVG     32

class CMixer {

public:
	CMixer(unsigned int AISampleRate = 48000, unsigned int DACSampleRate = 48000, unsigned int BackendSampleRate = 32000)
		: m_aiSampleRate(AISampleRate)
		, m_dacSampleRate(DACSampleRate)
		, m_bits(16)
		, m_channels(2)
		, m_logAudio(0)
		, m_indexW(0)
		, m_indexR(0)
		, m_numLeftI(0.0f)
	{
		// AyuanX: The internal (Core & DSP) sample rate is fixed at 32KHz
		// So when AI/DAC sample rate differs than 32KHz, we have to do re-sampling
		m_sampleRate = BackendSampleRate;

		memset(m_buffer, 0, sizeof(m_buffer));

		INFO_LOG(AUDIO_INTERFACE, "Mixer is initialized (AISampleRate:%i, DACSampleRate:%i)", AISampleRate, DACSampleRate);
	}

	virtual ~CMixer() {}

	// Called from audio threads
	virtual unsigned int Mix(short* samples, unsigned int numSamples, bool consider_framelimit = true);

	// Called from main thread
	virtual void PushSamples(const short* samples, unsigned int num_samples);
	unsigned int GetSampleRate() const {return m_sampleRate;}

	void SetThrottle(bool use) { m_throttle = use;}


	virtual void StartLogAudio(const std::string& filename)
	{
		if (! m_logAudio)
		{
			m_logAudio = true;
			g_wave_writer.Start(filename, GetSampleRate());
			g_wave_writer.SetSkipSilence(false);
			NOTICE_LOG(DSPHLE, "Starting Audio logging");
		}
		else
		{
			WARN_LOG(DSPHLE, "Audio logging has already been started");
		}
	}

	virtual void StopLogAudio()
	{
		if (m_logAudio)
		{
			m_logAudio = false;
			g_wave_writer.Stop();
			NOTICE_LOG(DSPHLE, "Stopping Audio logging");
		}
		else
		{
			WARN_LOG(DSPHLE, "Audio logging has already been stopped");
		}
	}

	std::mutex& MixerCritical() { return m_csMixing; }

	float GetCurrentSpeed() const { return m_speed; }
	void UpdateSpeed(volatile float val) { m_speed = val; }

protected:
	unsigned int m_sampleRate;
	unsigned int m_aiSampleRate;
	unsigned int m_dacSampleRate;
	int m_bits;
	int m_channels;

	WaveFileWriter g_wave_writer;

	bool m_logAudio;

	bool m_throttle;

	short m_buffer[MAX_SAMPLES * 2];
	volatile u32 m_indexW;
	volatile u32 m_indexR;

	std::mutex m_csMixing;
	float m_numLeftI;

	volatile float m_speed; // Current rate of the emulation (1.0 = 100% speed)
private:

};
