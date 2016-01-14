// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <cstring>
#include <mutex>

#include "AudioCommon/WaveFile.h"

// 16 bit Stereo
#define MAX_SAMPLES     (1024 * 4) // 128 ms
#define INDEX_MASK      (MAX_SAMPLES * 2 - 1)

#define MAX_FREQ_SHIFT  200  // per 32000 Hz
#define CONTROL_FACTOR  0.2f // in freq_shift per fifo size offset
#define CONTROL_AVG     32

class CMixer final
{
public:
	explicit CMixer(unsigned int BackendSampleRate);
	~CMixer();

	// Called from audio threads
	unsigned int Mix(short* samples, unsigned int numSamples, bool consider_framelimit = true);

	// Called from main thread
	void PushSamples(const short* samples, unsigned int num_samples);
	void PushStreamingSamples(const short* samples, unsigned int num_samples);
	void PushWiimoteSpeakerSamples(const short* samples, unsigned int num_samples, unsigned int sample_rate);
	unsigned int GetSampleRate() const { return m_sampleRate; }

	void SetDMAInputSampleRate(unsigned int rate);
	void SetStreamInputSampleRate(unsigned int rate);
	void SetStreamingVolume(unsigned int lvolume, unsigned int rvolume);
	void SetWiimoteSpeakerVolume(unsigned int lvolume, unsigned int rvolume);

	void StartLogDTKAudio(const std::string& filename);
	void StopLogDTKAudio();

	void StartLogDSPAudio(const std::string& filename);
	void StopLogDSPAudio();

	float GetCurrentSpeed() const { return m_speed.load(); }
	void UpdateSpeed(float val) { m_speed.store(val); }

private:
	class MixerFifo final
	{
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
		void SetInputSampleRate(unsigned int rate);
		void SetVolume(unsigned int lvolume, unsigned int rvolume);
	private:
		CMixer *m_mixer;
		unsigned m_input_sample_rate;
		short m_buffer[MAX_SAMPLES * 2];
		std::atomic<u32> m_indexW;
		std::atomic<u32> m_indexR;
		// Volume ranges from 0-256
		std::atomic<s32> m_LVolume;
		std::atomic<s32> m_RVolume;
		float m_numLeftI;
		u32 m_frac;
	};
	MixerFifo m_dma_mixer;
	MixerFifo m_streaming_mixer;
	MixerFifo m_wiimote_speaker_mixer;
	unsigned int m_sampleRate;

	WaveFileWriter m_wave_writer_dtk;
	WaveFileWriter m_wave_writer_dsp;

	bool m_log_dtk_audio;
	bool m_log_dsp_audio;

	std::atomic<float> m_speed; // Current rate of the emulation (1.0 = 100% speed)
};
