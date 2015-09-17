// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
// Modified For Ishiiruka By Tino

#include "AudioCommon/AudioCommon.h"
#include "AudioCommon/Mixer.h"
#include "Common/Atomic.h"
#include "Common/CPUDetect.h"
#include "Common/MathUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/AudioInterface.h"
#include "Core/HW/VideoInterface.h"

// UGLINESS
#include "Core/PowerPC/PowerPC.h"

#if _M_SSE >= 0x301 && !(defined __GNUC__ && !defined __SSSE3__)
#include <tmmintrin.h>
#endif

const float CMixer::LOW_WATERMARK = 1280;
const float CMixer::MAX_FREQ_SHIFT = 200;
const float CMixer::CONTROL_FACTOR = 0.2f;
const float CMixer::CONTROL_AVG = 32;

void CMixer::LinearMixerFifo::Interpolate(u32 left_input_index, float* left_output, float* right_output)
{
	*left_output = (1 - m_fraction) * m_float_buffer[left_input_index & INDEX_MASK]
		 + m_fraction * m_float_buffer[(left_input_index + 2) & INDEX_MASK];
	*right_output = (1 - m_fraction) * m_float_buffer[(left_input_index + 1) & INDEX_MASK]
		 + m_fraction * m_float_buffer[(left_input_index + 3) & INDEX_MASK];
}

static const float _coeffs[] =
{ -0.5f,  1.0f, -0.5f, 0.0f,
   1.5f, -2.5f,  0.0f, 1.0f,
  -1.5f,  2.0f,  0.5f, 0.0f,
   0.5f, -0.5f,  0.0f, 0.0f };

void CMixer::CubicMixerFifo::Interpolate(u32 left_input_index, float* left_output, float* right_output)
{
	const float x2 = m_fraction; // x
	const float x1 = x2*x2;      // x^2
	const float x0 = x1*x2;      // x^3

	float y0 = _coeffs[0] * x0 + _coeffs[1] * x1 + _coeffs[2] * x2 + _coeffs[3];
	float y1 = _coeffs[4] * x0 + _coeffs[5] * x1 + _coeffs[6] * x2 + _coeffs[7];
	float y2 = _coeffs[8] * x0 + _coeffs[9] * x1 + _coeffs[10] * x2 + _coeffs[11];
	float y3 = _coeffs[12] * x0 + _coeffs[13] * x1 + _coeffs[14] * x2 + _coeffs[15];
	
	*left_output = y0 * m_float_buffer[left_input_index & INDEX_MASK]
		+ y1 * m_float_buffer[(left_input_index + 2) & INDEX_MASK]
		+ y2 * m_float_buffer[(left_input_index + 4) & INDEX_MASK]
		+ y3 * m_float_buffer[(left_input_index + 6) & INDEX_MASK];
	*right_output = y0 * m_float_buffer[(left_input_index + 1) & INDEX_MASK]
		+ y1 * m_float_buffer[(left_input_index + 3) & INDEX_MASK]
		+ y2 * m_float_buffer[(left_input_index + 5) & INDEX_MASK]
		+ y3 * m_float_buffer[(left_input_index + 7) & INDEX_MASK];
}

void CMixer::MixerFifo::Mix(float* samples, u32 numSamples, bool consider_framelimit)
{
	u32 current_sample = 0;
	// Cache access in non-volatile variable so interpolation loop can be optimized
	u32 read_index = m_read_index.load();
	const u32 write_index = m_write_index.load();
	// Sync input rate by fifo size
	float num_left = (float)(((write_index - read_index) & INDEX_MASK) / 2);
	m_num_left_i = (num_left + m_num_left_i * (CONTROL_AVG - 1)) / CONTROL_AVG;
	float offset = (m_num_left_i - LOW_WATERMARK) * CONTROL_FACTOR;
	offset = MathUtil::Clamp(offset, -MAX_FREQ_SHIFT, MAX_FREQ_SHIFT);
	// adjust framerate with framelimit

	u32 framelimit = SConfig::GetInstance().m_Framelimit;
	float aid_sample_rate = m_input_sample_rate + offset;
	if (consider_framelimit && framelimit > 1)
	{
		aid_sample_rate = aid_sample_rate * (framelimit - 1) * 5 / VideoInterface::TargetRefreshRate;
	}
	// ratio = 1 / upscale_factor = stepsize for each sample
	// e.g. going from 32khz to 48khz is 1 / (3 / 2) = 2 / 3
	// note because of syncing and framelimit, ratio will rarely be exactly 2 / 3
	float ratio = aid_sample_rate / (float)m_mixer->m_sample_rate;
	float l_volume = (float)m_lvolume.load() / 256.f;
	float r_volume = (float)m_rvolume.load() / 256.f;
	// for each output sample pair (left and right),
	// linear interpolate between current and next sample
	// increment output sample position
	// increment input sample position by ratio, store fraction
	// QUESTION: do we need to check for NUM_CROSSINGS samples before we interpolate?
	// seems to work fine as is
	for (; current_sample < numSamples * 2 && ((write_index - read_index) & INDEX_MASK) > 0; current_sample += 2)
	{
		float l_output, r_output;
		Interpolate(read_index, &l_output, &r_output);
		samples[current_sample + 1] += l_volume * l_output;
		samples[current_sample] += r_volume * r_output;
		m_fraction += ratio;
		read_index += 2 * (s32)m_fraction;
		m_fraction = m_fraction - (s32)m_fraction;
	}
	// pad output if not enough input samples
	float s[2];
	s[0] = m_float_buffer[(read_index - 1) & INDEX_MASK] * r_volume;
	s[1] = m_float_buffer[(read_index - 2) & INDEX_MASK] * l_volume;
	for (; current_sample < numSamples * 2; current_sample += 2)
	{
		samples[current_sample] += s[0];
		samples[current_sample + 1] += s[1];
	}
	// update read index
	m_read_index.store(read_index);
}

u32 CMixer::MixerFifo::AvailableSamples()
{
	return ((m_write_index.load() - m_read_index.load()) & INDEX_MASK) * 48000 / (2 * m_input_sample_rate);
}

u32 CMixer::AvailableSamples()
{
	u32 samples = m_dma_mixer.AvailableSamples();
	if (samples == 0)
	{
		samples = m_streaming_mixer.AvailableSamples();
	}
	if (samples == 0)
	{
		samples = m_wiimote_speaker_mixer.AvailableSamples();
	}
	return samples;
}

u32 CMixer::Mix(s16* samples, u32 num_samples, bool consider_framelimit)
{
	if (!samples)
		return 0;
	std::lock_guard<std::mutex> lk(m_cs_mixing);
	if (PowerPC::GetState() != PowerPC::CPU_RUNNING)
	{
		// Silence
		memset(samples, 0, num_samples * 2 * sizeof(s16));
		return num_samples;
	}
	// reset float output buffer
	m_output_buffer.resize(num_samples * 2);
	std::fill_n(m_output_buffer.begin(), num_samples * 2, 0.f);
	m_dma_mixer.Mix(m_output_buffer.data(), num_samples, consider_framelimit);
	m_streaming_mixer.Mix(m_output_buffer.data(), num_samples, consider_framelimit);
	m_wiimote_speaker_mixer.Mix(m_output_buffer.data(), num_samples, consider_framelimit);
	// dither and clamp
	for (u32 i = 0; i < num_samples * 2; i += 2)
	{
		float r_output = m_output_buffer[i] * 38768.0f;
		float l_output = m_output_buffer[i + 1] * 38768.0f;
		TriangleDither(l_output, m_l_dither_prev);
		TriangleDither(r_output, m_r_dither_prev);
		l_output = MathUtil::Clamp(l_output, -32768.f, 32767.f);
		r_output = MathUtil::Clamp(r_output, -32768.f, 32767.f);
		samples[i] = s16(r_output);
		samples[i + 1] = s16(l_output);
	}
	return num_samples;
}

u32 CMixer::Mix(float* samples, u32 num_samples, bool consider_framelimit)
{
	if (!samples)
		return 0;
	std::lock_guard<std::mutex> lk(m_cs_mixing);
	memset(samples, 0, num_samples * 2 * sizeof(float));
	if (PowerPC::GetState() != PowerPC::CPU_RUNNING)
	{
		// Silence		
		return num_samples;
	}
	m_dma_mixer.Mix(samples, num_samples, consider_framelimit);
	m_streaming_mixer.Mix(samples, num_samples, consider_framelimit);
	m_wiimote_speaker_mixer.Mix(samples, num_samples, consider_framelimit);
	return num_samples;
}


void CMixer::MixerFifo::PushSamples(const s16* samples, u32 num_samples)
{
	// Cache access in non-volatile variable
	// indexR isn't allowed to cache in the audio throttling loop as it
	// needs to get updates to not deadlock.
	u32 current_write_index = m_write_index.load();
	// Check if we have enough free space
	// indexW == m_indexR results in empty buffer, so indexR must always be smaller than indexW
	if (num_samples * 2 + ((current_write_index - m_read_index.load()) & INDEX_MASK) >= MAX_SAMPLES * 2)
		return;
	// AyuanX: Actual re-sampling work has been moved to sound thread
	// to alleviate the workload on main thread
	// convert to float while copying to buffer
	for (u32 i = 0; i < num_samples * 2; ++i)
	{
		m_float_buffer[(current_write_index + i) & INDEX_MASK] = Signed16ToFloat(Common::swap16(samples[i]));
	}
	m_write_index.fetch_add(num_samples * 2);
	return;
}

void CMixer::PushSamples(const s16 *samples, u32 num_samples)
{
	m_dma_mixer.PushSamples(samples, num_samples);
	if (m_log_dsp_audio)
		m_wave_writer_dsp.AddStereoSamplesBE(samples, num_samples);
}

void CMixer::PushStreamingSamples(const s16 *samples, u32 num_samples)
{
	m_streaming_mixer.PushSamples(samples, num_samples);
	if (m_log_dtk_audio)
		m_wave_writer_dtk.AddStereoSamplesBE(samples, num_samples);
}

void CMixer::PushWiimoteSpeakerSamples(const s16 *samples, u32 num_samples, u32 sample_rate)
{
	s16 samples_stereo[MAX_SAMPLES * 2];

	if (num_samples < MAX_SAMPLES)
	{
		m_wiimote_speaker_mixer.SetInputSampleRate(sample_rate);

		for (u32 i = 0; i < num_samples; ++i)
		{
			samples_stereo[i * 2] = Common::swap16(samples[i]);
			samples_stereo[i * 2 + 1] = Common::swap16(samples[i]);
		}

		m_wiimote_speaker_mixer.PushSamples(samples_stereo, num_samples);
	}
}

void CMixer::SetDMAInputSampleRate(u32 rate)
{
	m_dma_mixer.SetInputSampleRate(rate);
}

void CMixer::SetStreamInputSampleRate(u32 rate)
{
	m_streaming_mixer.SetInputSampleRate(rate);
}

void CMixer::SetStreamingVolume(u32 lvolume, u32 rvolume)
{
	m_streaming_mixer.SetVolume(lvolume, rvolume);
}

void CMixer::SetWiimoteSpeakerVolume(u32 lvolume, u32 rvolume)
{
	m_wiimote_speaker_mixer.SetVolume(lvolume, rvolume);
}

void CMixer::MixerFifo::SetInputSampleRate(u32 rate)
{
	m_input_sample_rate = rate;
}

void CMixer::MixerFifo::SetVolume(u32 lvolume, u32 rvolume)
{
	m_lvolume.store(lvolume + (lvolume >> 7));
	m_rvolume.store(rvolume + (rvolume >> 7));
}

void CMixer::MixerFifo::GetVolume(u32* lvolume, u32* rvolume) const
{
	*lvolume = m_lvolume.load();
	*rvolume = m_rvolume.load();
}

void CMixer::StartLogDTKAudio(const std::string& filename)
{
	if (!m_log_dtk_audio)
	{
		m_log_dtk_audio = true;
		m_wave_writer_dtk.Start(filename, 48000);
		m_wave_writer_dtk.SetSkipSilence(false);
		NOTICE_LOG(AUDIO, "Starting DTK Audio logging");
	}
	else
	{
		WARN_LOG(AUDIO, "DTK Audio logging has already been started");
	}
}

void CMixer::StopLogDTKAudio()
{
	if (m_log_dtk_audio)
	{
		m_log_dtk_audio = false;
		m_wave_writer_dtk.Stop();
		NOTICE_LOG(AUDIO, "Stopping DTK Audio logging");
	}
	else
	{
		WARN_LOG(AUDIO, "DTK Audio logging has already been stopped");
	}
}

void CMixer::StartLogDSPAudio(const std::string& filename)
{
	if (!m_log_dsp_audio)
	{
		m_log_dsp_audio = true;
		m_wave_writer_dsp.Start(filename, 32000);
		m_wave_writer_dsp.SetSkipSilence(false);
		NOTICE_LOG(AUDIO, "Starting DSP Audio logging");
	}
	else
	{
		WARN_LOG(AUDIO, "DSP Audio logging has already been started");
	}
}

void CMixer::StopLogDSPAudio()
{
	if (m_log_dsp_audio)
	{
		m_log_dsp_audio = false;
		m_wave_writer_dsp.Stop();
		NOTICE_LOG(AUDIO, "Stopping DSP Audio logging");
	}
	else
	{
		WARN_LOG(AUDIO, "DSP Audio logging has already been stopped");
	}
}
