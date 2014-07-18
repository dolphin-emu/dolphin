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
#define CONTROL_FACTOR  0.2f // in freq_shift per fifo size offset
#define CONTROL_AVG     32

class CMixer {

public:
	CMixer(unsigned int BackendSampleRate)
		: m_dma_mixer(this, 32000)
		, m_streaming_mixer(this, 48000)
		, m_sampleRate(BackendSampleRate)
		, m_logAudio(0)
		, m_throttle(false)
		, m_speed(0)
	{
		INFO_LOG(AUDIO_INTERFACE, "Mixer is initialized");
	}

	virtual ~CMixer() {}

	// Called from audio threads
	virtual unsigned int Mix(short* samples, unsigned int numSamples, bool consider_framelimit = true);

	// Called from main thread
	virtual void PushSamples(const short* samples, unsigned int num_samples);
	virtual void PushStreamingSamples(const short* samples, unsigned int num_samples);
	unsigned int GetSampleRate() const { return m_sampleRate; }
	void SetStreamingVolume(unsigned int lvolume, unsigned int rvolume);

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
	class MixerFifo {
	public:
		MixerFifo(CMixer *mixer, unsigned sample_rate)
			: m_mixer(mixer)
			, m_input_sample_rate(sample_rate)
			, m_indexW(0)
			, m_indexR(0)
			, m_LVolume(256)
			, m_RVolume(256)
			, m_numLeftI(0.0f)
			, m_frac(0)
		{
			memset(m_buffer, 0, sizeof(m_buffer));
		}
		void PushSamples(const short* samples, unsigned int num_samples);
		unsigned int Mix(short* samples, unsigned int numSamples, bool consider_framelimit = true);
		void SetVolume(unsigned int lvolume, unsigned int rvolume);
	private:
		CMixer *m_mixer;
		unsigned m_input_sample_rate;
		short m_buffer[MAX_SAMPLES * 2];
		volatile u32 m_indexW;
		volatile u32 m_indexR;
		// Volume ranges from 0-256
		volatile s32 m_LVolume;
		volatile s32 m_RVolume;
		float m_numLeftI;
		u32 m_frac;
	};
	MixerFifo m_dma_mixer;
	MixerFifo m_streaming_mixer;
	unsigned int m_sampleRate;

	WaveFileWriter g_wave_writer;

	bool m_logAudio;

	bool m_throttle;

	std::mutex m_csMixing;

	volatile float m_speed; // Current rate of the emulation (1.0 = 100% speed)
};
