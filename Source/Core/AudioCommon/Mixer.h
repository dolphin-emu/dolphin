// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include "AudioCommon/WaveFile.h"

// Produces white noise in the range of [-4.f, 4.f]
// Note: for me this values produces more natural results than master [-0.5, 0.5]
const float rndrcp = 8.f / float(RAND_MAX);
#define DITHER_NOISE ((rand() * rndrcp) - 4.f)

// converts [-32768, 32767] -> [-1.0, 1.0)
constexpr float Signed16ToFloat(const s16 s)
{
	return s * 0.000030517578125f;
}

// we NEED dithering going from float -> 16bit
__forceinline void TriangleDither(float& sample, float& prev_dither)
{
	float dither = DITHER_NOISE;
	sample += dither - prev_dither;
	prev_dither = dither;
}

class CMixer {
public:
	CMixer(u32 BackendSampleRate)
		: m_dma_mixer(this, 32000)
		, m_streaming_mixer(this, 48000)
		, m_wiimote_speaker_mixer(this, 3000)
		, m_sample_rate(BackendSampleRate)
		, m_log_dtk_audio(0)
		, m_log_dsp_audio(0)
		, m_speed(0)
		, m_l_dither_prev(0)
		, m_r_dither_prev(0)
	{
		INFO_LOG(AUDIO_INTERFACE, "Mixer is initialized");
	}

	static constexpr u32 MAX_SAMPLES = 2048;
	static constexpr u32 INDEX_MASK = MAX_SAMPLES * 2 - 1;
	static constexpr float LOW_WATERMARK = 1280.0f;
	static constexpr float MAX_FREQ_SHIFT = 200.0f;
	static constexpr float CONTROL_FACTOR = 0.2f;
	static constexpr float CONTROL_AVG = 32.0f;

	// Called from audio threads
	u32 Mix(s16* samples, u32 numSamples, bool consider_framelimit = true);
	u32 Mix(float* samples, u32 numSamples, bool consider_framelimit = true);
	u32 AvailableSamples();
	// Called from main thread
	void PushSamples(const s16* samples, u32 num_samples);
	void PushStreamingSamples(const s16* samples, u32 num_samples);
	void PushWiimoteSpeakerSamples(const s16* samples, u32 num_samples, u32 sample_rate);
	u32 GetSampleRate() const { return m_sample_rate; }

	void SetDMAInputSampleRate(u32 rate);
	void SetStreamInputSampleRate(u32 rate);
	void SetStreamingVolume(u32 lvolume, u32 rvolume);
	void SetWiimoteSpeakerVolume(u32 lvolume, u32 rvolume);

	void StartLogDTKAudio(const std::string& filename);
	void StopLogDTKAudio();

	void StartLogDSPAudio(const std::string& filename);
	void StopLogDSPAudio();

	std::mutex& MixerCritical() { return m_cs_mixing; }

	float GetCurrentSpeed() const { return m_speed.load(); }
	void UpdateSpeed(float val) { m_speed.store(val); }

protected:
	class MixerFifo
	{
	public:
		MixerFifo(CMixer *mixer, unsigned sample_rate)
			: m_mixer(mixer)
			, m_input_sample_rate(sample_rate)
			, m_write_index(0)
			, m_read_index(0)
			, m_lvolume(255)
			, m_rvolume(255)
			, m_num_left_i(0.0f)
			, m_fraction(0)
		{
			srand((u32)time(nullptr));
			m_float_buffer.fill(0.0f);
		}

		virtual void Interpolate(u32 left_input_index, float* left_output, float* right_output) = 0;
		void PushSamples(const s16* samples, u32 num_samples);
		void Mix(float* samples, u32 numSamples, bool consider_framelimit = true);
		void SetInputSampleRate(u32 rate);
		void SetVolume(u32 lvolume, u32 rvolume);
		void GetVolume(u32* lvolume, u32* rvolume) const;
		u32 AvailableSamples();
	protected:
		CMixer *m_mixer;
		unsigned m_input_sample_rate;

		std::array<float, MAX_SAMPLES * 2> m_float_buffer;

		std::atomic<u32> m_write_index;
		std::atomic<u32> m_read_index;

		// Volume ranges from 0-255
		std::atomic<s32> m_lvolume;
		std::atomic<s32> m_rvolume;

		float m_num_left_i;
		float m_fraction;
	};

	class LinearMixerFifo : public MixerFifo
	{
	public:
		LinearMixerFifo(CMixer* mixer, u32 sample_rate) : MixerFifo(mixer, sample_rate) {}
		void Interpolate(u32 left_input_index, float* left_output, float* right_output) override;
	};

	class CubicMixerFifo : public MixerFifo
	{
	public:
		CubicMixerFifo(CMixer* mixer, u32 sample_rate) : MixerFifo(mixer, sample_rate) {}
		void Interpolate(u32 left_input_index, float* left_output, float* right_output) override;
	};

	CubicMixerFifo m_dma_mixer;
	CubicMixerFifo m_streaming_mixer;

	// Linear interpolation seems to be the best for Wiimote 3khz -> 48khz, for now.
	// TODO: figure out why and make it work with the above FIR
	LinearMixerFifo m_wiimote_speaker_mixer;

	u32 m_sample_rate;

	WaveFileWriter m_wave_writer_dtk;
	WaveFileWriter m_wave_writer_dsp;

	bool m_log_dtk_audio;
	bool m_log_dsp_audio;

	std::mutex m_cs_mixing;

	std::atomic<float> m_speed; // Current rate of the emulation (1.0 = 100% speed)

private:
	std::vector<float> m_output_buffer;
	float m_l_dither_prev;
	float m_r_dither_prev;
};

