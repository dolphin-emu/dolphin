// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioCommon/Mixer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

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

Mixer::Mixer(unsigned int BackendSampleRate)
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

// Executed from sound stream thread
void Mixer::MixerFifo::Mix(short* samples, unsigned int num_samples)
{
  const uint32_t HALF = 0x7fffffff;

  const uint64_t in_sample_rate = FIXED_SAMPLE_RATE_DIVIDEND / m_input_sample_rate_divisor;
  const uint64_t out_sample_rate = m_mixer->m_sampleRate;

  const uint32_t index_jump = (in_sample_rate << GRANULAR_BUFFER_BITS) / out_sample_rate;

  s32 lvolume = m_LVolume.load();
  s32 rvolume = m_RVolume.load();

  while (num_samples-- > 0)
  {
    StereoPair sample = m_front[m_current_index] + m_back[m_current_index - HALF];

    sample.mul(lvolume << 8, rvolume << 8);

    samples[0] = std::clamp(samples[0] + sample.right, -32767, 32767);
    samples[1] = std::clamp(samples[1] + sample.left, -32767, 32767);

    samples += 2;

    m_current_index += index_jump;

    if (m_current_index < HALF)
    {
      m_front = m_back;
      m_back = m_next;

      m_current_index += HALF;
    }
  }
}

unsigned int Mixer::Mix(short* samples, unsigned int num_samples)
{
  if (!samples)
    return 0;

  memset(samples, 0, num_samples * 2 * sizeof(short));

  m_dma_mixer.Mix(samples, num_samples);
  m_streaming_mixer.Mix(samples, num_samples);
  m_wiimote_speaker_mixer.Mix(samples, num_samples);
  m_skylander_portal_mixer.Mix(samples, num_samples);
  for (auto& mixer : m_gba_mixers)
    mixer.Mix(samples, num_samples);
  m_is_stretching = false;

  return num_samples;
}

unsigned int Mixer::MixSurround(float* samples, unsigned int num_samples)
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

void Mixer::MixerFifo::PushSamples(const short* samples, unsigned int num_samples)
{
  while (num_samples-- > 0)
  {
    short left = m_little_endian ? samples[1] : Common::swap16(samples[1]);
    short right = m_little_endian ? samples[0] : Common::swap16(samples[0]);

    m_buffer[m_buffer_index] = StereoPair(left, right);
    m_buffer_index = (m_buffer_index + 1) & (m_buffer.size() - 1);
    samples += 2;

    bool start = m_buffer_index == 0;
    bool middle = m_buffer_index == m_buffer.size() / 2;
    if (start || middle)
      m_next = Granual(m_buffer, middle);
  }
}

void Mixer::PushSamples(const short* samples, unsigned int num_samples)
{
  m_dma_mixer.PushSamples(samples, num_samples);
  if (m_log_dsp_audio)
  {
    int sample_rate_divisor = m_dma_mixer.GetInputSampleRateDivisor();
    auto volume = m_dma_mixer.GetVolume();
    m_wave_writer_dsp.AddStereoSamplesBE(samples, num_samples, sample_rate_divisor, volume.first,
                                         volume.second);
  }
}

void Mixer::PushStreamingSamples(const short* samples, unsigned int num_samples)
{
  m_streaming_mixer.PushSamples(samples, num_samples);
  if (m_log_dtk_audio)
  {
    int sample_rate_divisor = m_streaming_mixer.GetInputSampleRateDivisor();
    auto volume = m_streaming_mixer.GetVolume();
    m_wave_writer_dtk.AddStereoSamplesBE(samples, num_samples, sample_rate_divisor, volume.first,
                                         volume.second);
  }
}

void Mixer::PushWiimoteSpeakerSamples(const short* samples, unsigned int num_samples,
                                      unsigned int sample_rate_divisor)
{
  // Max 20 bytes/speaker report, may be 4-bit ADPCM so multiply by 2
  static constexpr u32 MAX_SPEAKER_SAMPLES = 20 * 2;
  std::array<short, MAX_SPEAKER_SAMPLES * 2> samples_stereo;

  ASSERT_MSG(AUDIO, num_samples <= MAX_SPEAKER_SAMPLES,
             "num_samples would overflow samples_stereo: {} > {}", num_samples,
             MAX_SPEAKER_SAMPLES);
  if (num_samples <= MAX_SPEAKER_SAMPLES)
  {
    m_wiimote_speaker_mixer.SetInputSampleRateDivisor(sample_rate_divisor);

    for (unsigned int i = 0; i < num_samples; ++i)
    {
      samples_stereo[i * 2] = samples[i];
      samples_stereo[i * 2 + 1] = samples[i];
    }

    m_wiimote_speaker_mixer.PushSamples(samples_stereo.data(), num_samples);
  }
}

void Mixer::PushSkylanderPortalSamples(const u8* samples, unsigned int num_samples)
{
  // Skylander samples are always supplied as 64 bytes, 32 x 16 bit samples
  // The portal speaker is 1 channel, so duplicate and play as stereo audio
  static constexpr u32 MAX_PORTAL_SPEAKER_SAMPLES = 32;
  std::array<short, MAX_PORTAL_SPEAKER_SAMPLES * 2> samples_stereo;

  ASSERT_MSG(AUDIO, num_samples <= MAX_PORTAL_SPEAKER_SAMPLES,
             "num_samples is not less or equal to 32: {} > {}", num_samples,
             MAX_PORTAL_SPEAKER_SAMPLES);

  if (num_samples <= MAX_PORTAL_SPEAKER_SAMPLES)
  {
    for (unsigned int i = 0; i < num_samples; ++i)
    {
      s16 sample = static_cast<u16>(samples[i * 2 + 1]) << 8 | static_cast<u16>(samples[i * 2]);
      samples_stereo[i * 2] = sample;
      samples_stereo[i * 2 + 1] = sample;
    }

    m_skylander_portal_mixer.PushSamples(samples_stereo.data(), num_samples);
  }
}

void Mixer::PushGBASamples(int device_number, const short* samples, unsigned int num_samples)
{
  m_gba_mixers[device_number].PushSamples(samples, num_samples);
}

void Mixer::SetDMAInputSampleRateDivisor(unsigned int rate_divisor)
{
  m_dma_mixer.SetInputSampleRateDivisor(rate_divisor);
}

void Mixer::SetStreamInputSampleRateDivisor(unsigned int rate_divisor)
{
  m_streaming_mixer.SetInputSampleRateDivisor(rate_divisor);
}

void Mixer::SetGBAInputSampleRateDivisors(int device_number, unsigned int rate_divisor)
{
  m_gba_mixers[device_number].SetInputSampleRateDivisor(rate_divisor);
}

void Mixer::SetStreamingVolume(unsigned int lvolume, unsigned int rvolume)
{
  m_streaming_mixer.SetVolume(lvolume, rvolume);
}

void Mixer::SetWiimoteSpeakerVolume(unsigned int lvolume, unsigned int rvolume)
{
  m_wiimote_speaker_mixer.SetVolume(lvolume, rvolume);
}

void Mixer::SetGBAVolume(int device_number, unsigned int lvolume, unsigned int rvolume)
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
  m_config_timing_variance = Config::Get(Config::MAIN_TIMING_VARIANCE);
  m_config_audio_stretch = Config::Get(Config::MAIN_AUDIO_STRETCH);
}

void Mixer::MixerFifo::DoState(PointerWrap& p)
{
  p.Do(m_input_sample_rate_divisor);
  p.Do(m_LVolume);
  p.Do(m_RVolume);
}

void Mixer::MixerFifo::SetInputSampleRateDivisor(unsigned int rate_divisor)
{
  m_input_sample_rate_divisor = rate_divisor;
}

unsigned int Mixer::MixerFifo::GetInputSampleRateDivisor() const
{
  return m_input_sample_rate_divisor;
}

void Mixer::MixerFifo::SetVolume(unsigned int lvolume, unsigned int rvolume)
{
  m_LVolume.store(lvolume + (lvolume >> 7));
  m_RVolume.store(rvolume + (rvolume >> 7));
}

std::pair<s32, s32> Mixer::MixerFifo::GetVolume() const
{
  return std::make_pair(m_LVolume.load(), m_RVolume.load());
}
