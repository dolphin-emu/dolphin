// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <mutex>
#include <string>
#include <vector>

#include "AudioCommon/WaveFile.h"

// Dither define
#define DITHER_NOISE    (rand() / (float) RAND_MAX - 0.5f)

class CMixer
{
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
		m_output_buffer.reserve(MAX_SAMPLES * 2);
	}

	static const u32 MAX_SAMPLES = 2048;
	static const u32 INDEX_MASK = MAX_SAMPLES * 2 - 1;
	static const float LOW_WATERMARK;
	static const float MAX_FREQ_SHIFT;
	static const float CONTROL_FACTOR;
	static const float CONTROL_AVG;

	virtual ~CMixer() {}

	// Called from audio threads
	u32 Mix(s16* samples, u32 numSamples, bool consider_framelimit = true);

	// Called from main thread
	virtual void PushSamples(const s16* samples, u32 num_samples);
	virtual void PushStreamingSamples(const s16* samples, u32 num_samples);
	virtual void PushWiimoteSpeakerSamples(const s16* samples, u32 num_samples, u32 sample_rate);
	u32 GetSampleRate() const { return m_sample_rate; }

	void SetDMAInputSampleRate(u32 rate);
	void SetStreamInputSampleRate(u32 rate);
	void SetStreamingVolume(u32 lvolume, u32 rvolume);
	void SetWiimoteSpeakerVolume(u32 lvolume, u32 rvolume);

	virtual void StartLogDTKAudio(const std::string& filename)
	{
		if (!m_log_dtk_audio)
		{
			m_log_dtk_audio = true;
			g_wave_writer_dtk.Start(filename, 48000);
			g_wave_writer_dtk.SetSkipSilence(false);
			NOTICE_LOG(DSPHLE, "Starting DTK Audio logging");
		}
		else
		{
			WARN_LOG(DSPHLE, "DTK Audio logging has already been started");
		}
	}

	virtual void StopLogDTKAudio()
	{
		if (m_log_dtk_audio)
		{
			m_log_dtk_audio = false;
			g_wave_writer_dtk.Stop();
			NOTICE_LOG(DSPHLE, "Stopping DTK Audio logging");
		}
		else
		{
			WARN_LOG(DSPHLE, "DTK Audio logging has already been stopped");
		}
	}

	virtual void StartLogDSPAudio(const std::string& filename)
	{
		if (!m_log_dsp_audio)
		{
			m_log_dsp_audio = true;
			g_wave_writer_dsp.Start(filename, 32000);
			g_wave_writer_dsp.SetSkipSilence(false);
			NOTICE_LOG(DSPHLE, "Starting DSP Audio logging");
		}
		else
		{
			WARN_LOG(DSPHLE, "DSP Audio logging has already been started");
		}
	}

	virtual void StopLogDSPAudio()
	{
		if (m_log_dsp_audio)
		{
			m_log_dsp_audio = false;
			g_wave_writer_dsp.Stop();
			NOTICE_LOG(DSPHLE, "Stopping DSP Audio logging");
		}
		else
		{
			WARN_LOG(DSPHLE, "DSP Audio logging has already been stopped");
		}
	}

	std::mutex& MixerCritical() { return m_cs_mixing; }

	float GetCurrentSpeed() const { return m_speed; }
	void UpdateSpeed(volatile float val) { m_speed = val; }

protected:
	class MixerFifo
	{
	public:
		MixerFifo(CMixer* mixer, u32 sample_rate)
			: m_mixer(mixer)
			, m_input_sample_rate(sample_rate)
			, m_write_index(0)
			, m_read_index(0)
			, m_lvolume(255)
			, m_rvolume(255)
			, m_num_left_i(0.0f)
			, m_fraction(0.0f)
		{
			srand((u32) time(nullptr));
		}
		virtual void Interpolate(u32 left_input_index, float* left_output, float* right_output) = 0;
		void PushSamples(const s16* samples, u32 num_samples);
		void Mix(std::vector<float>& samples, u32 numSamples, bool consider_framelimit = true);
		void SetInputSampleRate(u32 rate);
		void SetVolume(u32 lvolume, u32 rvolume);
		void GetVolume(u32* lvolume, u32* rvolume) const;

	protected:
		CMixer*  m_mixer;
		u32      m_input_sample_rate;

		std::array<float, MAX_SAMPLES * 2> m_float_buffer;

		volatile u32 m_write_index;
		volatile u32 m_read_index;

		// Volume ranges from 0-255
		volatile u32 m_lvolume;
		volatile u32 m_rvolume;

		float        m_num_left_i;
		float        m_fraction;
	};

	class LinearMixerFifo : public MixerFifo
	{
	public:
		LinearMixerFifo(CMixer* mixer, u32 sample_rate) : MixerFifo(mixer, sample_rate)	{}
		void Interpolate(u32 left_input_index, float* left_output, float* right_output) override;
	};

	class WindowedSincMixerFifo : public MixerFifo
	{
	public:
		WindowedSincMixerFifo(CMixer* mixer, u32 sample_rate) : MixerFifo(mixer, sample_rate) {}
		void Interpolate(u32 left_input_index, float* left_output, float* right_output) override;
	};

	class Resampler
	{
		static const double LOWPASS_ROLLOFF;
		static const double KAISER_BETA;
		static const double BESSEL_EPSILON; // acceptable delta for Kaiser Window calculation

		void PopulateFilterCoeff();
		double ModBessel0th(const double x);
	public:

		static const u32 SAMPLES_PER_CROSSING = 4096;
		static const u32 NUM_CROSSINGS = 35;
		static const u32 WING_SIZE = SAMPLES_PER_CROSSING * (NUM_CROSSINGS - 1) / 2;

		Resampler()
		{
			PopulateFilterCoeff();
		}

		std::array<double, WING_SIZE> m_lowpass_filter;
		std::array<double, WING_SIZE> m_lowpass_delta;
	};

	Resampler m_resampler;

	WindowedSincMixerFifo m_dma_mixer;
	WindowedSincMixerFifo m_streaming_mixer;

	// Linear interpolation seems to be the best for Wiimote 3khz -> 48khz, for now.
	// TODO: figure out why and make it work with the above FIR
	LinearMixerFifo m_wiimote_speaker_mixer;

	u32 m_sample_rate;

	WaveFileWriter g_wave_writer_dtk;
	WaveFileWriter g_wave_writer_dsp;

	bool m_log_dtk_audio;
	bool m_log_dsp_audio;

	std::mutex m_cs_mixing;

	volatile float m_speed; // Current rate of the emulation (1.0 = 100% speed)

private:
	// converts [-32768, 32767] -> [-1.0, 1.0]
	static inline float Signed16ToFloat(const s16 s)
	{
		return (s > 0) ? (float) (s / (float) 0x7fff) : (float) (s / (float) 0x8000);
	}

	// converts [-1.0, 1.0] -> [-32768, 32767]
	static inline s16 FloatToSigned16(const float f)
	{
		return (f > 0) ? (s16) (f * 0x7fff) : (s16) (f * 0x8000);
	}

	void TriangleDither(float* l_sample, float* r_sample);

	std::vector<float> m_output_buffer;
	float m_l_dither_prev;
	float m_r_dither_prev;
};