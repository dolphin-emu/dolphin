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
#include "Core/Core.h"
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
unsigned int Mixer::MixerFifo::Mix(short* samples, unsigned int numSamples,
                                   bool consider_framelimit, float emulationspeed,
                                   bool should_resync, int timing_variance)
{
  // Cache access in non-volatile variable
  // This is the only function changing the read value, so it's safe to
  // cache it locally although it's written here.
  // The writing pointer will be modified outside, but it will only increase,
  // so we will just ignore new written data while interpolating.
  // Without this cache, the compiler wouldn't be allowed to optimize the
  // interpolation loop.
  u32 indexR = m_indexR.load();
  u32 indexW = m_indexW.load();

  // render numleft sample pairs to samples[]
  // advance indexR with sample position
  // remember fractional offset

  float aid_sample_rate =
      FIXED_SAMPLE_RATE_DIVIDEND / static_cast<float>(m_input_sample_rate_divisor);
  if (consider_framelimit && emulationspeed > 0.0f)
  {
    float numLeft = static_cast<float>(((indexW - indexR) & INDEX_MASK) / 2);

    u32 low_watermark = (FIXED_SAMPLE_RATE_DIVIDEND * timing_variance) /
                        (static_cast<u64>(m_input_sample_rate_divisor) * 1000);
    low_watermark = std::min(low_watermark, MAX_SAMPLES / 2);

    // When the writer exits fast-forward, throw away extra audio beyond low_watermark.
    if (should_resync && numLeft > low_watermark)
    {
      numLeft = m_numLeftI = low_watermark;
      indexR = (indexW - 2 * low_watermark) & INDEX_MASK;
      // Verify that numLeft is correct (using its initial formula).
      ASSERT(numLeft == static_cast<float>(((indexW - indexR) & INDEX_MASK) / 2));
    }

    m_numLeftI = (numLeft + m_numLeftI * (CONTROL_AVG - 1)) / CONTROL_AVG;
    float offset = (m_numLeftI - low_watermark) * CONTROL_FACTOR;
    if (offset > MAX_FREQ_SHIFT)
      offset = MAX_FREQ_SHIFT;
    if (offset < -MAX_FREQ_SHIFT)
      offset = -MAX_FREQ_SHIFT;

    aid_sample_rate = (aid_sample_rate + offset) * emulationspeed;
  }

  const u32 ratio = (u32)(65536.0f * aid_sample_rate / (float)m_mixer->m_sampleRate);

  s32 lvolume = m_LVolume.load();
  s32 rvolume = m_RVolume.load();

  const auto read_buffer = [this](auto index) {
    return m_little_endian ? m_buffer[index] : Common::swap16(m_buffer[index]);
  };

  unsigned int currentSample = 0;
  // TODO: consider a higher-quality resampling algorithm.
  for (; currentSample < numSamples * 2 && ((indexW - indexR) & INDEX_MASK) > 2; currentSample += 2)
  {
    u32 indexR2 = indexR + 2;  // next sample

    s16 l1 = read_buffer(indexR & INDEX_MASK);   // current
    s16 l2 = read_buffer(indexR2 & INDEX_MASK);  // next
    int sampleL = ((l1 << 16) + (l2 - l1) * (u16)m_frac) >> 16;
    sampleL = (sampleL * lvolume) >> 8;
    sampleL += samples[currentSample + 1];
    samples[currentSample + 1] = std::clamp(sampleL, -32767, 32767);

    s16 r1 = read_buffer((indexR + 1) & INDEX_MASK);   // current
    s16 r2 = read_buffer((indexR2 + 1) & INDEX_MASK);  // next
    int sampleR = ((r1 << 16) + (r2 - r1) * (u16)m_frac) >> 16;
    sampleR = (sampleR * rvolume) >> 8;
    sampleR += samples[currentSample];
    samples[currentSample] = std::clamp(sampleR, -32767, 32767);

    m_frac += ratio;
    indexR += 2 * (u16)(m_frac >> 16);
    m_frac &= 0xffff;
  }

  // Actual number of samples written to the buffer without padding.
  unsigned int actual_sample_count = currentSample / 2;

  // Padding
  short s[2];
  s[0] = read_buffer((indexR - 1) & INDEX_MASK);
  s[1] = read_buffer((indexR - 2) & INDEX_MASK);
  s[0] = (s[0] * rvolume) >> 8;
  s[1] = (s[1] * lvolume) >> 8;
  for (; currentSample < numSamples * 2; currentSample += 2)
  {
    int sampleR = std::clamp(s[0] + samples[currentSample + 0], -32767, 32767);
    int sampleL = std::clamp(s[1] + samples[currentSample + 1], -32767, 32767);

    samples[currentSample + 0] = sampleR;
    samples[currentSample + 1] = sampleL;
  }

  // Flush cached variable
  m_indexR.store(indexR);

  return actual_sample_count;
}

unsigned int Mixer::Mix(short* samples, unsigned int num_samples)
{
  if (!samples)
    return 0;

  memset(samples, 0, num_samples * 2 * sizeof(short));

  // TODO: Determine how emulation speed will be used in audio
  // const float emulation_speed = g_perf_metrics.GetSpeed();
  const float emulation_speed = m_config_emulation_speed;
  const int timing_variance = m_config_timing_variance;
  if (m_config_audio_stretch)
  {
    unsigned int available_samples =
        std::min(m_dma_mixer.AvailableSamples(), m_streaming_mixer.AvailableSamples());

    ASSERT_MSG(AUDIO, available_samples <= MAX_SAMPLES,
               "Audio stretching would overflow m_scratch_buffer: min({}, {}) -> {} > {} ({})",
               m_dma_mixer.AvailableSamples(), m_streaming_mixer.AvailableSamples(),
               available_samples, MAX_SAMPLES, num_samples);

    m_scratch_buffer.fill(0);

    m_dma_mixer.Mix(m_scratch_buffer.data(), available_samples, false, emulation_speed, false,
                    timing_variance);
    m_streaming_mixer.Mix(m_scratch_buffer.data(), available_samples, false, emulation_speed, false,
                          timing_variance);
    m_wiimote_speaker_mixer.Mix(m_scratch_buffer.data(), available_samples, false, emulation_speed,
                                false, timing_variance);
    m_skylander_portal_mixer.Mix(m_scratch_buffer.data(), available_samples, false, emulation_speed,
                                 false, timing_variance);
    for (auto& mixer : m_gba_mixers)
    {
      mixer.Mix(m_scratch_buffer.data(), available_samples, false, emulation_speed, false,
                timing_variance);
    }

    if (!m_is_stretching)
    {
      m_stretcher.Clear();
      m_is_stretching = true;
    }
    m_stretcher.ProcessSamples(m_scratch_buffer.data(), available_samples, num_samples);
    m_stretcher.GetStretchedSamples(samples, num_samples);
  }
  else
  {
    const bool is_fast_forwarding = Core::GetIsThrottlerTempDisabled();
    const bool should_resync = m_was_fast_forwarding && !is_fast_forwarding;
    m_was_fast_forwarding = is_fast_forwarding;

    if (should_resync)
    {
      INFO_LOG_FMT(AUDIO, "Resyncing after fast forward");
    }

    m_dma_mixer.Mix(samples, num_samples, true, emulation_speed, should_resync, timing_variance);
    m_streaming_mixer.Mix(samples, num_samples, true, emulation_speed, should_resync,
                          timing_variance);
    m_wiimote_speaker_mixer.Mix(samples, num_samples, true, emulation_speed, should_resync,
                                timing_variance);
    m_skylander_portal_mixer.Mix(samples, num_samples, true, emulation_speed, should_resync,
                                 timing_variance);
    for (auto& mixer : m_gba_mixers)
      mixer.Mix(samples, num_samples, true, emulation_speed, should_resync, timing_variance);
    m_is_stretching = false;
  }

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
  int over_bytes = num_samples * 4 - (MAX_SAMPLES * 2 - (indexW & INDEX_MASK)) * sizeof(short);
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

unsigned int Mixer::MixerFifo::AvailableSamples() const
{
  unsigned int samples_in_fifo = ((m_indexW.load() - m_indexR.load()) & INDEX_MASK) / 2;
  if (samples_in_fifo <= 1)
    return 0;  // Mixer::MixerFifo::Mix always keeps one sample in the buffer.
  return (samples_in_fifo - 1) * static_cast<u64>(m_mixer->m_sampleRate) *
         m_input_sample_rate_divisor / FIXED_SAMPLE_RATE_DIVIDEND;
}
