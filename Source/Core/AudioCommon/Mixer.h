// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <cstring>
#include <mutex>

#include "AudioCommon/WaveFile.h"

// 16 bit Stereo
#define MAX_SAMPLES     (1024 * 4) // 128 ms
#define INDEX_MASK      (MAX_SAMPLES * 2 - 1)

#define BESSEL_EPSILON			1e-21

class CMixer
{
public:
	CMixer(u32 BackendSampleRate);
	virtual ~CMixer() {}

	// Called from audio threads
	u32 Mix(float* samples, u32 numSamples, bool consider_framelimit = true);

	// Called from main thread
	virtual void PushSamples(const s16* samples, u32 num_samples);
	virtual void PushStreamingSamples(const s16* samples, u32 num_samples);
	virtual void PushWiimoteSpeakerSamples(const s16* samples, u32 num_samples, u32 sample_rate);
	u32 GetSampleRate() const { return m_sampleRate; }

	void SetDMAInputSampleRate(u32 rate);
	void SetStreamInputSampleRate(u32 rate);
	void SetStreamingVolume(u32 lvolume, u32 rvolume);
	void SetWiimoteSpeakerVolume(u32 lvolume, u32 rvolume);

	void StartLogDTKAudio(const std::string& filename);
	void StopLogDTKAudio();

	void StartLogDSPAudio(const std::string& filename);
	void StopLogDSPAudio();

	float GetCurrentSpeed() const { return m_speed.load(); }
	void UpdateSpeed(float val) { m_speed.store(val); }

	static inline float lerp(float sample1, float sample2, float fraction) {
		return (1.f - fraction) * sample1 + fraction * sample2;
	}

	// converts [-32768, 32767] -> [-1.0, 1.0]
	static inline float Signed16ToFloat(const s16 s)
	{
		return (s > 0) ? (float)(s / (float)0x7fff) : (float)(s / (float)0x8000);
	}

	// converts [-1.0, 1.0] -> [-32768, 32767]
	static inline s16 FloatToSigned16(const float f)
	{
		return (f > 0) ? (s16)(f * 0x7fff) : (s16)(f * 0x8000);
	}

protected:
	class FIRFilter
	{
		void PopulateFilterCoeff();
		void PopulateFilterDeltas();
		void CheckFilterCache();
		bool GetFilterFromFile(const std::string &filename, float* filter);
		bool PutFilterToFile(const std::string &filename, float* filter);
		double ModBessel0th(const double x) const;
	public:
		FIRFilter(u32 nz = 13, u32 nl = 65536, double fc = 0.45, double beta = 7.0)
			: m_num_crossings(nz)
			, m_samples_per_crossing(nl)
			, m_kaiser_beta(beta)
			, m_wing_size(nl * (nz - 1) / 2)
			, m_lowpass_frequency(fc)
		{
			m_filter = (float*)malloc(m_wing_size * sizeof(float));
			m_deltas = (float*)malloc(m_wing_size * sizeof(float));
			CheckFilterCache();
		}
		~FIRFilter()
		{
			free(m_filter);
			free(m_deltas);
			m_filter = m_deltas = nullptr;
		}
		float* m_filter;
		float* m_deltas;
		const u32 m_num_crossings;
		const u32 m_samples_per_crossing;
		const u32 m_wing_size;
	private:
		const double m_kaiser_beta;
		const double m_lowpass_frequency;
	};

	class MixerFifo
	{
	public:
		MixerFifo(CMixer *mixer, u32 sample_rate, std::shared_ptr<FIRFilter> filter)
			: m_mixer(mixer)
			, m_input_sample_rate(sample_rate)
			, m_indexW(0)
			, m_indexR(0)
			, m_floatI(0)
			, m_LVolume(1.f)
			, m_RVolume(1.f)
			, m_numLeftI(0.f)
			, m_frac(0.f)
			, m_firfilter(filter)
			, m_filter_length((filter) ? ((filter->m_num_crossings - 1) / 2) : 1) // one-sided length of filter
		{
		}
		virtual void Interpolate(u32 index, float* output_l, float* output_r) = 0;
		void PushSamples(const s16* samples, u32 num_samples);
		unsigned int Mix(float* samples, u32 numSamples, bool consider_framelimit = true);
		void SetInputSampleRate(u32 rate);
		void SetVolume(u32 lvolume, u32 rvolume);
		bool m_debug;
	protected:
		CMixer* m_mixer;
		u32 m_input_sample_rate;
		std::array<s16, MAX_SAMPLES * 2> m_buffer{};
		std::array<float, MAX_SAMPLES * 2> m_floats{};
		std::atomic<u32> m_indexW;
		std::atomic<u32> m_indexR;
		std::atomic<u32> m_floatI;
		// Volume ranges from 0-256
		std::atomic<float> m_LVolume;
		std::atomic<float> m_RVolume;
		float m_numLeftI;
		float m_frac;
		std::shared_ptr<FIRFilter> m_firfilter;
		u32 m_filter_length;
	};

	class LinearMixer : public MixerFifo
	{
	public:
		LinearMixer(CMixer* mixer, u32 sample_rate)
			: MixerFifo(mixer, sample_rate, nullptr)
		{}
		void Interpolate(u32 index, float* output_l, float* output_r);

	};

	class SincMixer : public MixerFifo
	{
	public:
		SincMixer(CMixer* mixer, u32 sample_rate, std::shared_ptr<FIRFilter> filter)
			: MixerFifo(mixer, sample_rate, filter)
		{}
		void Interpolate(u32 index, float* output_l, float* output_r);

	};

	std::unique_ptr<MixerFifo> m_dma_mixer;
	std::unique_ptr<MixerFifo> m_streaming_mixer;
	std::unique_ptr<MixerFifo> m_wiimote_speaker_mixer;
	u32 m_sampleRate;

	WaveFileWriter m_wave_writer_dtk;
	WaveFileWriter m_wave_writer_dsp;

	bool m_log_dtk_audio;
	bool m_log_dsp_audio;

	std::atomic<float> m_speed; // Current rate of the emulation (1.0 = 100% speed)
};
