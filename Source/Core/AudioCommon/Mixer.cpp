// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "AudioCommon/Mixer.h"

#include <algorithm>
#include <cstring>

#include "AudioCommon/Dither.h"
#include "AudioCommon/LinearFilter.h"
#include "AudioCommon/WindowedSincFilter.h"
#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Core/ConfigManager.h"

CMixer::CMixer(u32 BackendSampleRate) : m_output_sample_rate(BackendSampleRate)
{
  // These filter parameters give:
  //   -80db stopband attenuation
  //   center of transition band at 0.8 of the min(input samplerate, output samplerate)
  //   transition bandwidth of about 0.5 the min(input samplerate, output samplerate)
  std::shared_ptr<WindowedSincFilter> lowpass_filter =
      std::make_shared<WindowedSincFilter>(21, 512, 0.8f, 8.0f);

  m_linear_filter = std::make_shared<LinearFilter>();

  m_dma_mixer = std::make_unique<MixerFifo>(this, 32000, lowpass_filter);
  m_streaming_mixer = std::make_unique<MixerFifo>(this, 48000, lowpass_filter);

  m_dither = std::make_unique<ShapedDither>();

  INFO_LOG(AUDIO_INTERFACE, "Mixer is initialized");
}

CMixer::~CMixer()
{
}

CMixer::MixerFifo::MixerFifo(CMixer* mixer, unsigned sample_rate,
                             std::shared_ptr<BaseFilter> filter)
    : m_mixer(mixer), m_filter(std::move(filter)), m_input_sample_rate(sample_rate)
{
  m_floats.Resize(MAX_SAMPLES * 2);
  m_shorts.Resize(MAX_SAMPLES * 2);
}

// Executed from sound stream thread
u32 CMixer::MixerFifo::Mix(std::array<float, MAX_SAMPLES * 2>& samples, u32 numSamples,
                           bool consider_framelimit)
{
  u32 currentSample = 0;

  // Cache access in non-volatile variable
  // This is the only function changing the read value, so it's safe to
  // cache it locally although it's written here.
  // The writing pointer will be modified outside, but it will only increase,
  // so we will just ignore new written data while interpolating.
  // Without this cache, the compiler wouldn't be allowed to optimize the
  // interpolation loop.
  size_t indexR = m_shorts.LoadTail();
  size_t indexW = m_shorts.LoadHead();

  u32 low_waterwark = m_input_sample_rate * SConfig::GetInstance().iTimingVariance / 1000;
  low_waterwark = std::min(low_waterwark, MAX_SAMPLES / 2);

  float numLeft = static_cast<float>(((indexW - indexR) & m_shorts.Mask()) / 2);
  m_numLeftI = (numLeft + m_numLeftI * (CONTROL_AVG - 1)) / CONTROL_AVG;
  float offset = (m_numLeftI - low_waterwark) * CONTROL_FACTOR;
  offset = MathUtil::Clamp(offset, static_cast<float>(-MAX_FREQ_SHIFT),
                           static_cast<float>(MAX_FREQ_SHIFT));

  // render numleft sample pairs to samples[]
  // advance indexR with sample position
  // remember fractional offset

  float emulationspeed = SConfig::GetInstance().m_EmulationSpeed;
  float aid_sample_rate = m_input_sample_rate + offset;
  if (consider_framelimit && emulationspeed > 0.0f)
  {
    aid_sample_rate *= emulationspeed;
  }

  const float ratio = aid_sample_rate / static_cast<float>(m_mixer->m_output_sample_rate);

  float lvolume = static_cast<float>(m_LVolume.load() / 256.f);
  float rvolume = static_cast<float>(m_RVolume.load() / 256.f);

  size_t floatI = m_floats.LoadHead();

  // Since we know that samples come in two-channel pairs,
  // might as well unroll a bit
  for (; ((indexW - floatI) & INDEX_MASK) > 0; floatI += 2)
  {
    m_floats[floatI] = MathUtil::SignedShortToFloat(Common::swap16(m_shorts[floatI]));
    m_floats[floatI + 1] = MathUtil::SignedShortToFloat(Common::swap16(m_shorts[floatI + 1]));
  }

  m_floats.StoreHead(floatI);

  const float rho = 1.f / ratio;  // since Interpolate takes out_rate / in_rate

  // Resampling loop
  for (; currentSample < numSamples * 2 && ((indexW - indexR) & m_shorts.Mask()) > 2;
       currentSample += 2)
  {
    float sample_l;
    float sample_r;
    m_filter->ConvolveStereo(m_floats, indexR, &sample_l, &sample_r, m_frac, rho);

    samples[currentSample] += sample_r * rvolume;
    samples[currentSample + 1] += sample_l * lvolume;

    m_frac += ratio;
    indexR += 2 * static_cast<s32>(m_frac);
    m_frac -= static_cast<s32>(m_frac);
  }

  // Padding
  float pad_right = m_floats[indexR - 1] * rvolume;
  float pad_left = m_floats[indexR - 2] * lvolume;

  for (; currentSample < numSamples * 2; currentSample += 2)
  {
    samples[currentSample] += pad_right;
    samples[currentSample + 1] += pad_left;
  }

  // Flush cached variable
  m_shorts.StoreTail(indexR);

  return numSamples;
}

u32 CMixer::Mix(s16* samples, u32 num_samples, bool consider_framelimit)
{
  if (!samples)
    return 0;

  memset(samples, 0, num_samples * 2 * sizeof(s16));
  std::fill_n(m_accumulator.begin(), num_samples * 2, 0.f);

  m_dma_mixer->Mix(m_accumulator, num_samples, consider_framelimit);
  m_streaming_mixer->Mix(m_accumulator, num_samples, consider_framelimit);

  for (auto& wiimote_mixer : m_wiimote_speaker_mixers)
  {
    if (wiimote_mixer)
      wiimote_mixer->Mix(m_accumulator, num_samples, consider_framelimit);
  }

  m_dither->ProcessStereo(m_accumulator.data(), samples, num_samples);

  if (m_log_mix_audio)
    m_wave_writer_mix.AddStereoSamples(samples, num_samples, m_output_sample_rate, false);

  return num_samples;
}

void CMixer::MixerFifo::PushSamples(const s16* samples, u32 num_samples)
{
  // Ringbuffer write
  m_shorts.Write(samples, num_samples * 2);
}

void CMixer::PushSamples(const s16* samples, u32 num_samples)
{
  m_dma_mixer->PushSamples(samples, num_samples);
  int sample_rate = m_dma_mixer->GetInputSampleRate();
  if (m_log_dsp_audio)
    m_wave_writer_dsp.AddStereoSamples(samples, num_samples, sample_rate, true);
}

void CMixer::PushStreamingSamples(const s16* samples, u32 num_samples)
{
  m_streaming_mixer->PushSamples(samples, num_samples);
  int sample_rate = m_streaming_mixer->GetInputSampleRate();
  if (m_log_dtk_audio)
    m_wave_writer_dtk.AddStereoSamples(samples, num_samples, sample_rate, true);
}

void CMixer::PushWiimoteSpeakerSamples(u32 wiimote_index, const s16* samples, u32 num_samples,
                                       u32 sample_rate)
{
  if (!m_wiimote_speaker_mixers[wiimote_index])
  {
    m_wiimote_speaker_mixers[wiimote_index] =
        std::make_unique<MixerFifo>(this, 6000, m_linear_filter);
  }

  s16 samples_stereo[MAX_SAMPLES * 2];

  if (num_samples < MAX_SAMPLES)
  {
    m_wiimote_speaker_mixers[wiimote_index]->SetInputSampleRate(sample_rate);

    for (size_t i = 0; i < num_samples; ++i)
    {
      samples_stereo[i * 2] = Common::swap16(samples[i]);
      samples_stereo[i * 2 + 1] = Common::swap16(samples[i]);
    }

    m_wiimote_speaker_mixers[wiimote_index]->PushSamples(samples_stereo, num_samples);
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

void CMixer::SetWiimoteSpeakerVolume(u32 wiimote_index, u32 lvolume, u32 rvolume)
{
  if (m_wiimote_speaker_mixers[wiimote_index])
    m_wiimote_speaker_mixers[wiimote_index]->SetVolume(lvolume, rvolume);
}

void CMixer::StartLogDTKAudio(const std::string& filename)
{
  if (!m_log_dtk_audio)
  {
    m_log_dtk_audio = true;
    m_wave_writer_dtk.Start(filename, m_streaming_mixer->GetInputSampleRate());
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
    m_wave_writer_dsp.Start(filename, m_dma_mixer->GetInputSampleRate());
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

void CMixer::StartLogMixAudio(const std::string& filename)
{
  if (!m_log_mix_audio)
  {
    m_log_mix_audio = true;
    m_wave_writer_mix.Start(filename, m_output_sample_rate);
    m_wave_writer_mix.SetSkipSilence(false);
    NOTICE_LOG(AUDIO, "Starting Mix Audio logging");
  }
  else
  {
    WARN_LOG(AUDIO, "Mix Audio logging has already been started");
  }
}

void CMixer::StopLogMixAudio()
{
  if (m_log_mix_audio)
  {
    m_log_mix_audio = false;
    m_wave_writer_mix.Stop();
    NOTICE_LOG(AUDIO, "Stopping Mix Audio logging");
  }
  else
  {
    WARN_LOG(AUDIO, "Mix Audio logging has already been stopped");
  }
}

void CMixer::MixerFifo::SetInputSampleRate(u32 rate)
{
  m_input_sample_rate = rate;
}

u32 CMixer::MixerFifo::GetInputSampleRate() const
{
  return m_input_sample_rate;
}

void CMixer::MixerFifo::SetVolume(u32 lvolume, u32 rvolume)
{
  m_LVolume.store(lvolume);
  m_RVolume.store(rvolume);
}