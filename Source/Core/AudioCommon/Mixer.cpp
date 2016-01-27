// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>
#include <sstream>

#include "AudioCommon/AudioCommon.h"
#include "AudioCommon/Mixer.h"
#include "Common/CPUDetect.h"
#include "Common/FileUtil.h"
#include "Common/MathUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/AudioInterface.h"

// UGLINESS
#include "Core/PowerPC/PowerPC.h"

//TODO put into constants header file
#define M_PI	3.14159265358979323846

#define MAX_FREQ_SHIFT  200  // per 32000 Hz
#define CONTROL_FACTOR  0.2f // in freq_shift per fifo size offset
#define CONTROL_AVG     32

CMixer::CMixer(u32 BackendSampleRate)
	: m_sampleRate(BackendSampleRate)
	, m_log_dtk_audio(false)
	, m_log_dsp_audio(false)
	, m_speed(0)
{
	std::string preset = SConfig::GetInstance().sFilterPreset;
	u32 samples_per_crossing, num_crossings;

	//  no use trying to make wiimote speaker high quality
	//  if the desire is there, use SincMixer with FIRFilter(5, 128, 0.125) is good enough
	//  sources say ADPCM is x2 (3000 x 2 = 6000), but setting this input
	//  rate to 6000 doesn't seem to produce the right result

	m_wiimote_speaker_mixer = std::make_unique<LinearMixer>(this, 3000);

	if (preset == FILTER_LINEAR) {
		m_dma_mixer = std::make_unique<LinearMixer>(this, 32000);
		m_streaming_mixer = std::make_unique<LinearMixer>(this, 48000);
	} else {
		if (preset == FILTER_SINC_CUSTOM) {
			num_crossings = SConfig::GetInstance().iFilterTaps;
			samples_per_crossing = SConfig::GetInstance().iFilterResolution;
		}
		else {
			num_crossings = AudioCommon::FILTER_PRESETS.at(preset).first;
			samples_per_crossing = AudioCommon::FILTER_PRESETS.at(preset).second;
		}
		std::shared_ptr<FIRFilter> firfilter(new FIRFilter(num_crossings, samples_per_crossing, 0.45));

		m_dma_mixer = std::make_unique<SincMixer>(this, 32000, firfilter);
		m_streaming_mixer = std::make_unique<SincMixer>(this, 48000, firfilter);
	}

	INFO_LOG(AUDIO_INTERFACE, "Mixer is initialized");
}

void CMixer::LinearMixer::Interpolate(u32 index, float* output_l, float* output_r) {
	float l1 = m_floats[index & INDEX_MASK];
	float r1 = m_floats[(index + 1) & INDEX_MASK];
	float l2 = m_floats[(index + 2) & INDEX_MASK];
	float r2 = m_floats[(index + 3) & INDEX_MASK];

	*output_l = lerp(l1, l2, m_frac);
	*output_r = lerp(r1, r2, m_frac);
}

void CMixer::SincMixer::Interpolate(u32 index, float* output_l, float* output_r) {
	double left_channel = 0.0, right_channel = 0.0;

	float* filter = m_firfilter->m_filter;
	float* deltas = m_firfilter->m_deltas;
	const u32 samples_per_crossing = m_firfilter->m_samples_per_crossing;
	const u32 wing_size = m_firfilter->m_wing_size;

	double left_frac = (m_frac * samples_per_crossing);
	u32 left_index = (u32)left_frac;
	left_frac = left_frac - left_index;

	u32 current_index = index;
	while (left_index < wing_size)
	{
		double impulse = filter[left_index];
		impulse += deltas[left_index] * left_frac;

		left_channel += m_floats[current_index & INDEX_MASK] * impulse;
		right_channel += m_floats[(current_index + 1) & INDEX_MASK] * impulse;

		left_index += samples_per_crossing;
		current_index -= 2;
	}

	double right_frac = (1 - m_frac) * samples_per_crossing;
	u32 right_index = (u32)right_frac;
	right_frac = right_frac - right_index;

	current_index = index + 2;
	while (right_index < wing_size)
	{
		double impulse = filter[right_index];
		impulse += deltas[right_index] * right_frac;

		left_channel += m_floats[current_index & INDEX_MASK] * impulse;
		right_channel += m_floats[(current_index + 1) & INDEX_MASK] * impulse;

		right_index += samples_per_crossing;
		current_index += 2;
	}

	*output_l = (float)left_channel;
	*output_r = (float)right_channel;
}

// Executed from sound stream thread
u32 CMixer::MixerFifo::Mix(float* samples, u32 numSamples, bool consider_framelimit)
{
	u32 currentSample = 0;

	u32 indexR = m_indexR.load();
	u32 indexW = m_indexW.load();

	int low_waterwark = m_input_sample_rate * SConfig::GetInstance().iTimingVariance / 1000;
	low_waterwark = std::min(low_waterwark, MAX_SAMPLES / 2);

	float numLeft = (float)(((indexW - indexR) & INDEX_MASK) / 2);
	m_numLeftI = (numLeft + m_numLeftI*(CONTROL_AVG - 1)) / CONTROL_AVG;
	float offset = (m_numLeftI - low_waterwark) * CONTROL_FACTOR;
	offset = MathUtil::Clamp(offset, (float)-MAX_FREQ_SHIFT, (float)MAX_FREQ_SHIFT);

	float emulationspeed = SConfig::GetInstance().m_EmulationSpeed;
	float aid_sample_rate = m_input_sample_rate + offset;
	if (consider_framelimit && emulationspeed > 0.0f)
	{
		aid_sample_rate = aid_sample_rate * emulationspeed;
	}

	const float ratio = aid_sample_rate / (float)m_mixer->m_sampleRate;

	float lvolume = m_LVolume.load();
	float rvolume = m_RVolume.load();
	u32 floatI = m_floatI.load();
	for (; ((indexW - floatI) & INDEX_MASK) > 0; ++floatI) {
		m_floats[floatI & INDEX_MASK] = Signed16ToFloat(Common::swap16(m_buffer[floatI & INDEX_MASK]));

	}

	m_floatI.store(floatI);

	// TODO: consider a higher-quality resampling algorithm.
	for (; currentSample < numSamples * 2 && ((indexW - indexR) & INDEX_MASK) > (m_filter_length + 5) * 2; currentSample += 2)
	{
		float sample_l, sample_r;
		Interpolate(indexR, &sample_l, &sample_r);

		samples[currentSample + 1] += sample_l * lvolume;
		samples[currentSample] += sample_r * rvolume;

		m_frac += ratio;
		indexR += 2 * (s32)m_frac;
		m_frac = m_frac - (s32)m_frac;

	}


	// Padding
	float s[2];
	s[0] = m_floats[(indexR - 1) & INDEX_MASK] * rvolume;
	s[1] = m_floats[(indexR - 2) & INDEX_MASK] * lvolume;

	for (; currentSample < numSamples * 2; currentSample += 2)
	{
		samples[currentSample] += s[0];
		samples[currentSample + 1] += s[1];
	}

	// Flush cached variable
	m_indexR.store(indexR);

	return numSamples;
}

u32 CMixer::Mix(float* samples, u32 num_samples, bool consider_framelimit)
{
	if (!samples)
		return 0;

	memset(samples, 0, num_samples * 2 * sizeof(float));

	if (PowerPC::GetState() != PowerPC::CPU_RUNNING)
	{
		memset(samples, 0, num_samples * 2 * sizeof(s16));
		return num_samples;
	}

	m_dma_mixer->Mix(samples, num_samples, consider_framelimit);
	m_streaming_mixer->Mix(samples, num_samples, consider_framelimit);
	m_wiimote_speaker_mixer->Mix(samples, num_samples, consider_framelimit);

	// clamp values
	for (u32 i = 0; i < num_samples * 2; i += 2)
	{
		samples[i + 1] = MathUtil::Clamp(samples[i + 1], -1.f, 1.f);
		samples[i] = MathUtil::Clamp(samples[i], -1.f, 1.f);
	}

	return num_samples;
}

void CMixer::MixerFifo::PushSamples(const s16 *samples, u32 num_samples)
{
	// Cache access in non-volatile variable
	// indexR isn't allowed to cache in the audio throttling loop as it
	// needs to get updates to not deadlock.
	u32 indexW = m_indexW.load();

	// Check if we have enough free space
	// indexW == m_indexR results in empty buffer, so indexR must always be smaller than indexW
	if (num_samples * 2 + ((indexW - m_indexR.load()) & INDEX_MASK) >= MAX_SAMPLES * 2)
		return;

	// AyuanX: Actual re-sampling work has been moved to sound thread
	// to alleviate the workload on main thread
	// and we simply store raw data here to make fast mem copy
	s32 over_bytes = num_samples * 4 - (MAX_SAMPLES * 2 - (indexW & INDEX_MASK)) * sizeof(short);
	if (over_bytes > 0)
	{
		memcpy(&m_buffer[indexW & INDEX_MASK], samples, num_samples * 4 - over_bytes);
		memcpy(&m_buffer[0], samples + (num_samples * 4 - over_bytes) / sizeof(short), over_bytes);
	}
	else
	{
		memcpy(&m_buffer[indexW & INDEX_MASK], samples, num_samples * 4);
	}

	m_indexW.fetch_add(num_samples * 2);
}

void CMixer::PushSamples(const s16 *samples, u32 num_samples)
{
	m_dma_mixer->PushSamples(samples, num_samples);
	if (m_log_dsp_audio)
		m_wave_writer_dsp.AddStereoSamplesBE(samples, num_samples);
}

void CMixer::PushStreamingSamples(const s16 *samples, u32 num_samples)
{
	m_streaming_mixer->PushSamples(samples, num_samples);
	if (m_log_dtk_audio)
		m_wave_writer_dtk.AddStereoSamplesBE(samples, num_samples);
}

void CMixer::PushWiimoteSpeakerSamples(const s16 *samples, u32 num_samples, u32 sample_rate)
{
	s16 samples_stereo[MAX_SAMPLES * 2];

	if (num_samples < MAX_SAMPLES)
	{
		m_wiimote_speaker_mixer->SetInputSampleRate(sample_rate);

		for (u32 i = 0; i < num_samples; ++i)
		{
			samples_stereo[i * 2 + 1] = samples_stereo[i * 2] =
				Common::swap16(samples[i]);
		}

		m_wiimote_speaker_mixer->PushSamples(samples_stereo, num_samples);
	}
}

void CMixer::SetDMAInputSampleRate(u32 rate)
{
	m_dma_mixer->SetInputSampleRate(rate);
}

void CMixer::SetStreamInputSampleRate(u32 rate)
{
	m_streaming_mixer->SetInputSampleRate(rate);
}

void CMixer::SetStreamingVolume(u32 lvolume, u32 rvolume)
{
	m_streaming_mixer->SetVolume(lvolume, rvolume);
}

void CMixer::SetWiimoteSpeakerVolume(u32 lvolume, u32 rvolume)
{
	m_wiimote_speaker_mixer->SetVolume(lvolume, rvolume);
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

void CMixer::MixerFifo::SetInputSampleRate(u32 rate)
{
	m_input_sample_rate = rate;
}

void CMixer::MixerFifo::SetVolume(u32 lvolume, u32 rvolume)
{
	m_LVolume.store(lvolume / 256.0f);
	m_RVolume.store(rvolume / 256.0f);
}

double CMixer::FIRFilter::ModBessel0th(const double x) const
{
	double sum = 1.0;
	s32 factorial_store = 1;
	double half_x = x / 2.0;
	double previous = 1.0;
	do {
		double temp = half_x / (double)factorial_store;
		temp *= temp;
		previous *= temp;
		sum += previous;
		factorial_store++;
	} while (previous >= BESSEL_EPSILON * sum);
	return sum;
}

void CMixer::FIRFilter::PopulateFilterDeltas()
{
	for (u32 i = 0; i < m_wing_size - 1; ++i)
	{
		m_deltas[i] = m_filter[i + 1] - m_filter[i];
	}
	m_deltas[m_wing_size - 1] = -m_filter[m_wing_size - 1];
}

void CMixer::FIRFilter::PopulateFilterCoeff()
{
	m_filter[0] = (float)(2 * m_lowpass_frequency);
	double inv_I0_beta = 1.0 / ModBessel0th(m_kaiser_beta);
	double inv_size = 1.0 / (m_wing_size - 1);
	for (u32 i = 1; i < m_wing_size; ++i)
	{
		double offset = M_PI * (double)i / (double)m_samples_per_crossing;
		double sinc = sin(offset * 2 * m_lowpass_frequency) / offset;
		double radicand = (double)i * inv_size;
		radicand = 1.0 - radicand * radicand;
		radicand = (radicand < 0.0) ? 0.0 : radicand;
		m_filter[i] = (float)(sinc * ModBessel0th(m_kaiser_beta * sqrt(radicand)) * inv_I0_beta);
	}

	double dc_gain = 0;
	for (u32 i = m_samples_per_crossing; i < m_wing_size; i += m_samples_per_crossing)
	{
		dc_gain += m_filter[i];
	}
	dc_gain *= 2;
	dc_gain += m_filter[0];
	dc_gain = 1.0 / dc_gain;

	for (u32 i = 0; i < m_wing_size; ++i) {
		m_filter[i] = (float)(m_filter[i] * dc_gain);
	}
}

void CMixer::FIRFilter::CheckFilterCache()
{
	const std::string audio_cache_path = File::GetUserPath(D_CACHE_IDX) + "Audio/";

	if (!File::IsDirectory(audio_cache_path))
	{
		File::SetCurrentDir(File::GetUserPath(D_CACHE_IDX));
		if (!File::CreateDir("Audio"))
		{
			WARN_LOG(AUDIO, "failed to create audio cache folder: %s", audio_cache_path.c_str());
		}
		File::SetCurrentDir(File::GetExeDirectory());
	}

	std::stringstream filter_specs;
	filter_specs << m_num_crossings << "-"
		<< m_samples_per_crossing << "-"
		<< m_kaiser_beta << "-"
		<< m_lowpass_frequency;

	std::string filter_filename = audio_cache_path + "filter-" + filter_specs.str() + ".cache";
	std::string deltas_filename = audio_cache_path + "deltas-" + filter_specs.str() + ".cache";

	bool create_filter = true;
	bool create_deltas = true;

	if (GetFilterFromFile(filter_filename, m_filter))
	{
		INFO_LOG(AUDIO_INTERFACE, "filter successfully retrieved from cache: %s", filter_filename.c_str());
		create_filter = false;

		if (GetFilterFromFile(deltas_filename, m_deltas))
		{
			create_deltas = false;
		}
	}
	else {
		INFO_LOG(AUDIO_INTERFACE, "filter not retrieved from cache: %s", filter_filename.c_str());
	}

	if (create_filter)
	{
		PopulateFilterCoeff();

		if (!PutFilterToFile(filter_filename, m_filter))
		{
			WARN_LOG(AUDIO_INTERFACE, "did not successfully store filter to cache: %s", filter_filename.c_str());
		}
	}

	if (create_deltas)
	{
		PopulateFilterDeltas();

		if (!PutFilterToFile(deltas_filename, m_deltas))
		{
			WARN_LOG(AUDIO_INTERFACE, "did not successfully store deltas to cache: %s", deltas_filename.c_str());
		}
	}
}

bool CMixer::FIRFilter::GetFilterFromFile(const std::string &filename, float* filter)
{
	File::IOFile cache(filename, "rb");
	u32 cache_count = (u32)cache.GetSize() / sizeof(float);
	if (cache_count != m_wing_size)
	{
		return false;
	}
	bool success = cache.ReadArray<float>(filter, m_wing_size);
	cache.Close();
	return success;
}

bool CMixer::FIRFilter::PutFilterToFile(const std::string &filename, float* filter)
{
	File::IOFile cache(filename, "wb");
	bool success = cache.WriteArray<float>(filter, m_wing_size);
	success = success && cache.Flush() && cache.Close();
	return success;
}