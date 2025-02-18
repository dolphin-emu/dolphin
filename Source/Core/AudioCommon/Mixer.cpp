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
    : m_sampleRate(BackendSampleRate),
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
  const uint32_t half = 0x7fffffff;

  const uint64_t in_sample_rate = FIXED_SAMPLE_RATE_DIVIDEND / m_input_sample_rate_divisor;
  const uint64_t out_sample_rate = m_mixer->m_sampleRate;

  const uint32_t index_jump = (in_sample_rate << GRANULE_BUFFER_BITS) / out_sample_rate;

  s32 lvolume = m_LVolume.load();
  s32 rvolume = m_RVolume.load();

  while (num_samples-- > 0)
  {
    StereoPair sample = m_front[m_current_index] + m_back[m_current_index - half];

    sample.mul(lvolume << 8, rvolume << 8);

    samples[0] = std::clamp(samples[0] + sample.r, -32767, 32767);
    samples[1] = std::clamp(samples[1] + sample.l, -32767, 32767);

    samples += 2;

    m_current_index += index_jump;

    if (m_current_index < half)
    {
      m_front = m_back;
      dequeue(m_back);

      m_current_index += half;
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

  return num_samples;
}

unsigned int Mixer::MixSurround(float* samples, unsigned int num_samples)
{
  if (!num_samples)
    return 0;

  memset(samples, 0, num_samples * SURROUND_CHANNELS * sizeof(float));

  size_t needed_frames = m_surround_decoder.QueryFramesNeededForSurroundOutput(num_samples);

  constexpr size_t max_samples = 0x8000;
  ASSERT_MSG(AUDIO, needed_frames <= max_samples,
             "needed_frames would overflow m_scratch_buffer: {} -> {} > {}", num_samples,
             needed_frames, max_samples);

  std::array<short, max_samples> buffer;
  size_t available_frames = Mix(buffer.data(), static_cast<u32>(needed_frames));
  if (available_frames != needed_frames)
  {
    ERROR_LOG_FMT(AUDIO,
                  "Error decoding surround frames: needed {} frames for {} samples but got {}",
                  needed_frames, num_samples, available_frames);
    return 0;
  }

  m_surround_decoder.PutFrames(buffer.data(), needed_frames);
  m_surround_decoder.ReceiveFrames(samples, num_samples);

  return num_samples;
}

void Mixer::MixerFifo::PushSamples(const short* samples, unsigned int num_samples)
{
  while (num_samples-- > 0)
  {
    short l = m_little_endian ? samples[1] : Common::swap16(samples[1]);
    short r = m_little_endian ? samples[0] : Common::swap16(samples[0]);

    m_buffer[m_buffer_index] = StereoPair(l, r);
    m_buffer_index = (m_buffer_index + 1) & (m_buffer.size() - 1);
    samples += 2;

    bool start = m_buffer_index == 0;
    bool middle = m_buffer_index == m_buffer.size() / 2;
    if (start || middle)
      enqueue(Granule(m_buffer, middle));
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
void Mixer::MixerFifo::enqueue(const Granule& granule)
{
  const size_t head = m_queue_head.load(std::memory_order_relaxed);
  const size_t tail = m_queue_tail.load(std::memory_order_acquire);

  m_queue[head] = granule;

  size_t next_head = (head + 1) % GRANULE_QUEUE_SIZE;
  if (next_head == tail)
    next_head = (head + GRANULE_QUEUE_SIZE / 2) % GRANULE_QUEUE_SIZE;

  m_queue_looping.store(false, std::memory_order_relaxed);
  m_queue_head.store(next_head, std::memory_order_release);
}

void Mixer::MixerFifo::dequeue(Granule& granule)
{
  const size_t tail = m_queue_tail.load(std::memory_order_relaxed);
  const size_t head = m_queue_head.load(std::memory_order_acquire);

  size_t next_tail = (tail + 1) % GRANULE_QUEUE_SIZE;

  if (next_tail == head)
  {
    next_tail = (tail + GRANULE_QUEUE_SIZE / 2) % GRANULE_QUEUE_SIZE;
    m_queue_looping.store(true, std::memory_order_relaxed);
  }

  if (m_queue_looping.load(std::memory_order_relaxed))
    m_queue_fade_index = std::min(m_queue_fade_index + 1, FADE_WINDOW_LENGTH - 1);
  else
    m_queue_fade_index = 0;

  granule = m_queue[tail];
  granule.mul(FADE_WINDOW[m_queue_fade_index]);

  m_queue_tail.store(next_tail, std::memory_order_release);
}

// StereoPair methods (part of MixerFifo)

void Mixer::MixerFifo::StereoPair::mul(int fixed_16_left, int fixed_16_right)
{
  l = short((int(l) * fixed_16_left) >> 16);
  r = short((int(r) * fixed_16_right) >> 16);
}

void Mixer::MixerFifo::StereoPair::mul(int fixed_16)
{
  mul(fixed_16, fixed_16);
}

Mixer::MixerFifo::StereoPair Mixer::MixerFifo::StereoPair::operator+(const StereoPair& other) const
{
  return StereoPair(std::clamp(int(l) + other.l, -0x7fff, 0x7fff),
                    std::clamp(int(r) + other.r, -0x7fff, 0x7fff));
}

Mixer::MixerFifo::StereoPair
Mixer::MixerFifo::StereoPair::interp(const StereoPair& s0, const StereoPair& s1,
                                     const StereoPair& s2, const StereoPair& s3,
                                     const StereoPair& s4, const StereoPair& s5, int t)
{
  const int t0 = 1 << 16;
  const int t1 = t;
  const int t2 = (t1 * t1) >> 16;
  const int t3 = (t1 * t2) >> 16;

  const int c0 = (+0 * t0 + 1 * t1 - 2 * t2 + 1 * t3) / 24;
  const int c1 = (+0 * t0 - 8 * t1 + 15 * t2 - 7 * t3) / 24;
  const int c2 = (+3 * t0 + 0 * t1 - 7 * t2 + 4 * t3) / 6;
  const int c3 = (+0 * t0 + 2 * t1 + 5 * t2 - 4 * t3) / 6;
  const int c4 = (+0 * t0 - 1 * t1 - 6 * t2 + 7 * t3) / 24;
  const int c5 = (+0 * t0 + 0 * t1 + 1 * t2 - 1 * t3) / 24;

  const int l = (c0 * s0.l + c1 * s1.l + c2 * s2.l + c3 * s3.l + c4 * s4.l + c5 * s5.l) >> 15;
  const int r = (c0 * s0.r + c1 * s1.r + c2 * s2.r + c3 * s3.r + c4 * s4.r + c5 * s5.r) >> 15;

  return StereoPair(short(std::clamp<int>(l, -0x7fff, 0x7fff)),
                    short(std::clamp<int>(r, -0x7fff, 0x7fff)));
}

// Implementation of Granule's constructor
Mixer::MixerFifo::Granule::Granule(const GranuleBuffer& input, bool split)
{
  if (split)
  {
    auto buffer_middle = m_buffer.begin() + m_buffer.size() / 2;
    auto input_middle = input.begin() + input.size() / 2;
    std::copy(input.begin(), input_middle, buffer_middle);
    std::copy(input_middle, input.end(), m_buffer.begin());
  }
  else
  {
    std::copy(input.begin(), input.end(), m_buffer.begin());
  }

  for (size_t i = 0; i < m_buffer.size(); ++i)
    m_buffer[i].mul(GRANULE_WINDOW[i]);
}

// Implementation of the operator[]
Mixer::MixerFifo::StereoPair Mixer::MixerFifo::Granule::operator[](uint32_t frac) const
{
  // Calculate index based on fractional position.
  int index = frac >> Mixer::MixerFifo::GRANULE_BUFFER_BITS;

  return StereoPair::interp(
      m_buffer[(index - 2) & (m_buffer.size() - 1)], m_buffer[(index - 1) & (m_buffer.size() - 1)],
      m_buffer[(index + 0) & (m_buffer.size() - 1)], m_buffer[(index + 1) & (m_buffer.size() - 1)],
      m_buffer[(index + 2) & (m_buffer.size() - 1)], m_buffer[(index + 3) & (m_buffer.size() - 1)],
      0xffff & (frac >> (Mixer::MixerFifo::GRANULE_BUFFER_BITS - 16)));
}

// Implementation of mul() with separate left/right multipliers
void Mixer::MixerFifo::Granule::mul(int fixed_16_left, int fixed_16_right)
{
  for (size_t i = 0; i < m_buffer.size(); ++i)
    m_buffer[i].mul(fixed_16_left, fixed_16_right);
}

// Implementation of mul() with a single multiplier
void Mixer::MixerFifo::Granule::mul(int fixed_16)
{
  mul(fixed_16, fixed_16);
}
