// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioCommon/Mixer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <Core/Core.h>

#include "AudioCommon/Enums.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "VideoCommon/PerformanceMetrics.h"

static u32 DPL2QualityToFrameBlockSize(AudioCommon::DPL2Quality quality)
{
  switch (quality)
  {
  case AudioCommon::DPL2Quality::Lowest:
    return 512;
  case AudioCommon::DPL2Quality::Low:
    return 1024;
  case AudioCommon::DPL2Quality::Highest:
    return 4096;
  default:
    return 2048;
  }
}

Mixer::Mixer(u32 BackendSampleRate)
    : m_sampleRate(BackendSampleRate), m_stretcher(BackendSampleRate),
      m_surround_decoder(BackendSampleRate,
                         DPL2QualityToFrameBlockSize(Config::Get(Config::MAIN_DPL2_QUALITY)))
{
  m_config_changed_callback_id = Config::AddConfigChangedCallback([this] { RefreshConfig(); });
  RefreshConfig();

  INFO_LOG_FMT(AUDIO_INTERFACE, "Mixer is initialized");
}

Mixer::~Mixer()
{
  Config::RemoveConfigChangedCallback(m_config_changed_callback_id);
}

void Mixer::DoState(PointerWrap& p)
{
  m_dma_mixer.DoState(p);
  m_streaming_mixer.DoState(p);
  m_wiimote_speaker_mixer.DoState(p);
  m_skylander_portal_mixer.DoState(p);
  for (auto& mixer : m_gba_mixers)
    mixer.DoState(p);
}

std::pair<float, float> Mixer::MixerFifo::GetSample(u32 index, float frac, float sinc_ratio)
{
  constexpr float pi = 3.14159265358979323846f;
  const s32 sinc_window_width = m_mixer->m_config_sinc_window_width;

  float l_sample = 0.0f, r_sample = 0.0f;
  for (s32 i = 1 - sinc_window_width; i <= sinc_window_width; ++i)
  {
    const auto [l, r] = m_buffer[(index + i) & INDEX_MASK];

    const float x = pi * (i - frac);
    float weight = 0.53836f + 0.46164f * std::cos(x / sinc_window_width);
    if (std::abs(x) >= 1e-6f)
      weight *= std::sin(x * sinc_ratio) / x;
    else
      weight *= sinc_ratio;

    r_sample += weight * r;
    l_sample += weight * l;
  }

  return std::make_pair(l_sample, r_sample);
}

// Executed from sound stream thread
void Mixer::MixerFifo::Mix(s16* samples, u32 num_samples, double target_speed)
{
  // Cache access in non-volatile variable. No race conditions are here, as indexW only increases
  // (which is fine), and this is the only place indexR is written to.
  const u32 indexW = m_indexW.load();  // Write index in circular buffer
  u32 indexR = m_indexR.load();        // Read index in circular buffer

  const float l_volume = float(m_LVolume.load()) / 256.0f;
  const float r_volume = float(m_RVolume.load()) / 256.0f;

  auto clamp = [](s32 input) -> s16 { return static_cast<s16>(std::clamp(input, -32768, 32767)); };

  const double ratio = (target_speed * FIXED_SAMPLE_RATE_DIVIDEND) /
                       (m_input_sample_rate_divisor * m_mixer->m_sampleRate);

  // These are timing markers. We want to keep the audio buffer around the low watermark.
  const s32 direct_latency = m_mixer->m_config_direct_latency;
  const s32 low_watermark = std::min<s32>(MAX_SAMPLES, direct_latency * FIXED_SAMPLE_RATE_DIVIDEND /
                                                           (m_input_sample_rate_divisor * 1000));

  // Calculate the number of samples remaining in the buffer
  const s32 sinc_window_width = m_mixer->m_config_sinc_window_width;
  s32 remaining_samples = s32((indexW - indexR) & INDEX_MASK) - sinc_window_width - 2;

  // Calculate the speed at which to play the requested samples in order to maintain the correct
  // synchronization with the video stream. Done by slightly adjusting the pitch of the audio.
  const double requested_samples = m_multiplier * num_samples * ratio;
  double multiplier = (remaining_samples - low_watermark) / requested_samples;
  multiplier = 1.0 + CONTROL_EFFORT * (multiplier - 1.0);
  multiplier = std::clamp(multiplier, MIN_PITCH_SHIFT, MAX_PITCH_SHIFT);

  // If the number of samples is above 2x the low watermark, skip ahead to prevent excessive latency
  const double max_latency = 2.0 * low_watermark;
  if (remaining_samples - requested_samples >= max_latency)
  {
    const s32 jump = 1 + static_cast<s32>(remaining_samples - requested_samples - max_latency);
    remaining_samples -= jump;
    indexR += jump;
  }

  // Low-pass filter to smooth out the multiplier (prevents sudden jumps in pitch)
  const double lpf_gain = -std::expm1(-1.0 / (num_samples * SAMPLE_RATE_LPF));
  const float sinc_ratio = 1.0f / std::max(1.0f, float(ratio * std::max(m_multiplier, multiplier)));

  bool reversed = false;
  for (u32 sample = 0; sample < num_samples; ++sample)
  {
    const auto [l, r] = GetSample(indexR, m_frac, sinc_ratio);
    samples[2 * sample + 0] = clamp(samples[2 * sample + 0] + s32(r * r_volume));
    samples[2 * sample + 1] = clamp(samples[2 * sample + 1] + s32(l * l_volume));

    // Smooth out multiplier with low pass filter
    m_multiplier += lpf_gain * (multiplier - m_multiplier);

    // Determine if we need to reverse the audio (i.e. we have run out of samples)
    reversed |= remaining_samples <= 0;
    reversed &= remaining_samples <= low_watermark;

    // Manage fractional index and increment index.
    m_frac += m_multiplier * (reversed ? -ratio : +ratio);
    const s32 inc = static_cast<s32>(m_frac);
    remaining_samples -= inc;
    indexR += inc;
    m_frac -= inc;
  }

  // Flush cached variable
  m_indexR.store(indexR);
}

// This is for audio stretching, which requires no playback speed shenanigans
u32 Mixer::MixerFifo::MixRaw(s16* samples, u32 num_samples)
{
  // Cache access in non-volatile variable. No race conditions are here, as indexW only increases
  // (which is fine), and this is the only place indexR is written to.
  const u32 indexW = m_indexW.load();  // Write index in circular buffer
  u32 indexR = m_indexR.load();        // Read index in circular buffer

  const float l_volume = float(m_LVolume.load()) / 256.0f;
  const float r_volume = float(m_RVolume.load()) / 256.0f;

  auto clamp = [](s32 input) -> s16 { return static_cast<s16>(std::clamp(input, -32768, 32767)); };

  const double aid_sample_rate = FIXED_SAMPLE_RATE_DIVIDEND / double(m_input_sample_rate_divisor);
  const double ratio = aid_sample_rate / m_mixer->m_sampleRate;
  const float sinc_ratio = 1.0f / std::max(1.0f, static_cast<float>(ratio));

  // Calculate the number of samples remaining in the buffer
  const s32 sinc_window_width = m_mixer->m_config_sinc_window_width;
  s32 remaining_samples = s32((indexW - indexR) & INDEX_MASK) - sinc_window_width - 2;
  u32 sample = 0;

  for (; sample < num_samples && 0 < remaining_samples; ++sample)
  {
    const auto [l, r] = GetSample(indexR, m_frac, sinc_ratio);
    samples[2 * sample + 0] = clamp(samples[2 * sample + 0] + s32(r * r_volume));
    samples[2 * sample + 1] = clamp(samples[2 * sample + 1] + s32(l * l_volume));

    m_frac += ratio;
    const s32 inc = static_cast<s32>(m_frac);
    remaining_samples -= inc;
    indexR += inc;
    m_frac -= inc;
  }

  if (sample != 0)
  {
    for (u32 padding = sample; padding < num_samples; ++padding)
    {
      samples[2 * padding + 0] = samples[2 * sample - 2];
      samples[2 * padding + 1] = samples[2 * sample - 1];
    }
  }

  // Flush cached variable
  m_indexR.store(indexR);

  return sample;
}

u32 Mixer::Mix(s16* samples, u32 num_samples)
{
  if (!samples)
    return 0;

  memset(samples, 0, num_samples * 2 * sizeof(s16));

  if (m_config_audio_stretch)
  {
    // We want to get as many samples out of this as possible.  Usually all mixers will have the
    // same number of samples available, but if not, we want to empty all of them.
    const u32 available_samples =
        std::min({m_dma_mixer.AvailableSamples(), m_streaming_mixer.AvailableSamples()});

    ASSERT_MSG(AUDIO, available_samples <= MAX_SAMPLES,
               "Audio stretching would overflow m_scratch_buffer: min({}, {}) -> {} > {} ({})",
               m_dma_mixer.AvailableSamples(), m_streaming_mixer.AvailableSamples(),
               available_samples, MAX_SAMPLES, num_samples);

    m_scratch_buffer.fill(0);

    m_dma_mixer.MixRaw(m_scratch_buffer.data(), available_samples);
    m_streaming_mixer.MixRaw(m_scratch_buffer.data(), available_samples);
    m_wiimote_speaker_mixer.MixRaw(m_scratch_buffer.data(), available_samples);
    m_skylander_portal_mixer.MixRaw(m_scratch_buffer.data(), available_samples);

    for (auto& mixer : m_gba_mixers)
      mixer.MixRaw(m_scratch_buffer.data(), available_samples);

    if (!m_is_stretching)
    {
      m_stretcher.Clear();
      m_is_stretching = true;
    }

    m_stretcher.ProcessSamples(m_scratch_buffer.data(), available_samples, num_samples);
    m_stretcher.GetStretchedSamples(samples, num_samples);

    g_perf_metrics.CountAudioLatency(m_stretcher.AvailableSamplesTime() +
                                     m_dma_mixer.AvailableSamplesTime());
  }
  else
  {
    float target_speed = m_config_emulation_speed;
    if (target_speed <= 0.0f || Core::GetIsThrottlerTempDisabled())
      target_speed = g_perf_metrics.GetAudioSpeed();

    m_dma_mixer.Mix(samples, num_samples, target_speed);
    m_streaming_mixer.Mix(samples, num_samples, target_speed);
    m_wiimote_speaker_mixer.Mix(samples, num_samples, target_speed);
    m_skylander_portal_mixer.Mix(samples, num_samples, target_speed);
    for (auto& mixer : m_gba_mixers)
      mixer.Mix(samples, num_samples, target_speed);

    g_perf_metrics.CountAudioLatency(m_dma_mixer.AvailableSamplesTime());
    m_is_stretching = false;
  }

  return num_samples;
}

u32 Mixer::MixSurround(float* samples, u32 num_samples)
{
  if (!num_samples)
    return 0;

  memset(samples, 0, num_samples * SURROUND_CHANNELS * sizeof(float));

  size_t needed_frames = m_surround_decoder.QueryFramesNeededForSurroundOutput(num_samples);

  // Mix() may also use m_scratch_buffer internally, but is safe because it alternates reads
  // and writes.
  ASSERT_MSG(AUDIO, needed_frames <= MAX_SAMPLES,
             "needed_frames would overflow m_scratch_buffer: {} -> {} > {}", num_samples,
             needed_frames, MAX_SAMPLES);
  size_t available_frames = Mix(m_scratch_buffer.data(), static_cast<u32>(needed_frames));
  if (available_frames != needed_frames)
  {
    ERROR_LOG_FMT(AUDIO,
                  "Error decoding surround frames: needed {} frames for {} samples but got {}",
                  needed_frames, num_samples, available_frames);
    return 0;
  }

  m_surround_decoder.PutFrames(m_scratch_buffer.data(), needed_frames);
  m_surround_decoder.ReceiveFrames(samples, num_samples);

  return num_samples;
}

void Mixer::MixerFifo::PushSamples(const s16* samples, u32 num_samples)
{
  // Cache access in non-volatile variable
  u32 indexW = m_indexW.load();

  // prevent writing into the buffer if it's full
  const s32 sinc_window_width = m_mixer->m_config_sinc_window_width;
  const s32 writable_samples =
      static_cast<s32>((m_indexR.load() - sinc_window_width - indexW) & INDEX_MASK);
  num_samples = std::min(num_samples, static_cast<u32>(std::max(0, writable_samples)));

  // We do float conversion here to avoid doing it in the mixing loop, which is performance
  // critical. That way we only need to do it once per sample.
  if (m_little_endian)
    for (u32 i = 0; i < num_samples; ++i, ++indexW)
      m_buffer[indexW & INDEX_MASK] = std::make_pair(static_cast<float>(samples[i * 2 + 0]),
                                                     static_cast<float>(samples[i * 2 + 1]));

  else
    for (u32 i = 0; i < num_samples; ++i, ++indexW)
      m_buffer[indexW & INDEX_MASK] =
          std::make_pair(static_cast<float>(static_cast<s16>(Common::swap16(samples[i * 2 + 0]))),
                         static_cast<float>(static_cast<s16>(Common::swap16(samples[i * 2 + 1]))));

  m_indexW.store(indexW);
}

void Mixer::PushSamples(const s16* samples, u32 num_samples)
{
  m_dma_mixer.PushSamples(samples, num_samples);
  if (m_log_dsp_audio)
  {
    u32 sample_rate_divisor = m_dma_mixer.GetInputSampleRateDivisor();
    auto volume = m_dma_mixer.GetVolume();
    m_wave_writer_dsp.AddStereoSamplesBE(samples, num_samples, sample_rate_divisor, volume.first,
                                         volume.second);
  }
}

void Mixer::PushStreamingSamples(const s16* samples, u32 num_samples)
{
  m_streaming_mixer.PushSamples(samples, num_samples);
  if (m_log_dtk_audio)
  {
    u32 sample_rate_divisor = m_streaming_mixer.GetInputSampleRateDivisor();
    auto volume = m_streaming_mixer.GetVolume();
    m_wave_writer_dtk.AddStereoSamplesBE(samples, num_samples, sample_rate_divisor, volume.first,
                                         volume.second);
  }
}

void Mixer::PushWiimoteSpeakerSamples(const s16* samples, u32 num_samples, u32 sample_rate_divisor)
{
  // Max 20 bytes/speaker report, may be 4-bit ADPCM so multiply by 2
  static constexpr u32 MAX_SPEAKER_SAMPLES = 20 * 2;
  std::array<s16, MAX_SPEAKER_SAMPLES * 2> samples_stereo;

  ASSERT_MSG(AUDIO, num_samples <= MAX_SPEAKER_SAMPLES,
             "num_samples would overflow samples_stereo: {} > {}", num_samples,
             MAX_SPEAKER_SAMPLES);
  if (num_samples <= MAX_SPEAKER_SAMPLES)
  {
    m_wiimote_speaker_mixer.SetInputSampleRateDivisor(sample_rate_divisor);

    for (u32 i = 0; i < num_samples; ++i)
    {
      samples_stereo[i * 2 + 0] = samples[i];
      samples_stereo[i * 2 + 1] = samples[i];
    }

    m_wiimote_speaker_mixer.PushSamples(samples_stereo.data(), num_samples);
  }
}

void Mixer::PushSkylanderPortalSamples(const u8* samples, u32 num_samples)
{
  // Skylander samples are always supplied as 64 bytes, 32 x 16 bit samples
  // The portal speaker is 1 channel, so duplicate and play as stereo audio
  static constexpr u32 MAX_PORTAL_SPEAKER_SAMPLES = 32;
  std::array<s16, MAX_PORTAL_SPEAKER_SAMPLES * 2> samples_stereo;

  ASSERT_MSG(AUDIO, num_samples <= MAX_PORTAL_SPEAKER_SAMPLES,
             "num_samples is not less or equal to 32: {} > {}", num_samples,
             MAX_PORTAL_SPEAKER_SAMPLES);

  if (num_samples <= MAX_PORTAL_SPEAKER_SAMPLES)
  {
    for (u32 i = 0; i < num_samples; ++i)
    {
      s16 sample = static_cast<u16>(samples[i * 2 + 1]) << 8 | static_cast<u16>(samples[i * 2]);
      samples_stereo[i * 2 + 0] = sample;
      samples_stereo[i * 2 + 1] = sample;
    }

    m_skylander_portal_mixer.PushSamples(samples_stereo.data(), num_samples);
  }
}

void Mixer::PushGBASamples(int device_number, const s16* samples, u32 num_samples)
{
  m_gba_mixers[device_number].PushSamples(samples, num_samples);
}

void Mixer::SetDMAInputSampleRateDivisor(u32 rate_divisor)
{
  m_dma_mixer.SetInputSampleRateDivisor(rate_divisor);
}

void Mixer::SetStreamInputSampleRateDivisor(u32 rate_divisor)
{
  m_streaming_mixer.SetInputSampleRateDivisor(rate_divisor);
}

void Mixer::SetGBAInputSampleRateDivisors(int device_number, u32 rate_divisor)
{
  m_gba_mixers[device_number].SetInputSampleRateDivisor(rate_divisor);
}

void Mixer::SetStreamingVolume(u32 lvolume, u32 rvolume)
{
  m_streaming_mixer.SetVolume(lvolume, rvolume);
}

void Mixer::SetWiimoteSpeakerVolume(u32 lvolume, u32 rvolume)
{
  m_wiimote_speaker_mixer.SetVolume(lvolume, rvolume);
}

void Mixer::SetGBAVolume(int device_number, u32 lvolume, u32 rvolume)
{
  m_gba_mixers[device_number].SetVolume(lvolume, rvolume);
}

void Mixer::StartLogDTKAudio(const std::string& filename)
{
  if (!m_log_dtk_audio)
  {
    bool success = m_wave_writer_dtk.Start(filename, m_streaming_mixer.GetInputSampleRateDivisor());
    if (success)
    {
      m_log_dtk_audio = true;
      m_wave_writer_dtk.SetSkipSilence(false);
      NOTICE_LOG_FMT(AUDIO, "Starting DTK Audio logging");
    }
    else
    {
      m_wave_writer_dtk.Stop();
      NOTICE_LOG_FMT(AUDIO, "Unable to start DTK Audio logging");
    }
  }
  else
  {
    WARN_LOG_FMT(AUDIO, "DTK Audio logging has already been started");
  }
}

void Mixer::StopLogDTKAudio()
{
  if (m_log_dtk_audio)
  {
    m_log_dtk_audio = false;
    m_wave_writer_dtk.Stop();
    NOTICE_LOG_FMT(AUDIO, "Stopping DTK Audio logging");
  }
  else
  {
    WARN_LOG_FMT(AUDIO, "DTK Audio logging has already been stopped");
  }
}

void Mixer::StartLogDSPAudio(const std::string& filename)
{
  if (!m_log_dsp_audio)
  {
    bool success = m_wave_writer_dsp.Start(filename, m_dma_mixer.GetInputSampleRateDivisor());
    if (success)
    {
      m_log_dsp_audio = true;
      m_wave_writer_dsp.SetSkipSilence(false);
      NOTICE_LOG_FMT(AUDIO, "Starting DSP Audio logging");
    }
    else
    {
      m_wave_writer_dsp.Stop();
      NOTICE_LOG_FMT(AUDIO, "Unable to start DSP Audio logging");
    }
  }
  else
  {
    WARN_LOG_FMT(AUDIO, "DSP Audio logging has already been started");
  }
}

void Mixer::StopLogDSPAudio()
{
  if (m_log_dsp_audio)
  {
    m_log_dsp_audio = false;
    m_wave_writer_dsp.Stop();
    NOTICE_LOG_FMT(AUDIO, "Stopping DSP Audio logging");
  }
  else
  {
    WARN_LOG_FMT(AUDIO, "DSP Audio logging has already been stopped");
  }
}

void Mixer::RefreshConfig()
{
  m_config_emulation_speed = Config::Get(Config::MAIN_EMULATION_SPEED);
  m_config_audio_stretch = Config::Get(Config::MAIN_AUDIO_STRETCH);
  m_config_direct_latency = Config::Get(Config::MAIN_AUDIO_DIRECT_LATENCY);
  m_config_sinc_window_width = Config::Get(Config::MAIN_AUDIO_SINC_WINDOW_WIDTH);
}

void Mixer::MixerFifo::DoState(PointerWrap& p)
{
  p.Do(m_input_sample_rate_divisor);
  p.Do(m_LVolume);
  p.Do(m_RVolume);
}

void Mixer::MixerFifo::SetInputSampleRateDivisor(u64 rate_divisor)
{
  m_input_sample_rate_divisor = rate_divisor;
}

u64 Mixer::MixerFifo::GetInputSampleRateDivisor() const
{
  return m_input_sample_rate_divisor;
}

void Mixer::MixerFifo::SetVolume(s32 lvolume, s32 rvolume)
{
  m_LVolume.store(lvolume + (lvolume >> 7));
  m_RVolume.store(rvolume + (rvolume >> 7));
}

std::pair<s32, s32> Mixer::MixerFifo::GetVolume() const
{
  return std::make_pair(m_LVolume.load(), m_RVolume.load());
}

u32 Mixer::MixerFifo::AvailableFIFOSamples() const
{
  const s32 samples_in_fifo = static_cast<s32>((m_indexW.load() - m_indexR.load()) & INDEX_MASK) -
                              m_mixer->m_config_sinc_window_width - 4;

  if (samples_in_fifo <= 0)
    return 0;

  return static_cast<u32>(samples_in_fifo);
}

u32 Mixer::MixerFifo::AvailableSamples() const
{
  const s64 samples_in_fifo = static_cast<s64>(AvailableFIFOSamples());
  return static_cast<u32>(samples_in_fifo * m_mixer->m_sampleRate * m_input_sample_rate_divisor /
                          FIXED_SAMPLE_RATE_DIVIDEND);
}

DT Mixer::MixerFifo::AvailableSamplesTime() const
{
  return std::chrono::duration_cast<DT>(DT_s(AvailableFIFOSamples()) * m_input_sample_rate_divisor /
                                        FIXED_SAMPLE_RATE_DIVIDEND);
}
