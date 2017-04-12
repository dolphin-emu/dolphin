// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "AudioCommon/Mixer.h"

#include <cmath>
#include <cstring>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Common/Swap.h"
#include "Core/ConfigManager.h"

CMixer::CMixer(unsigned int BackendSampleRate) : m_sampleRate(BackendSampleRate)
{
  INFO_LOG(AUDIO_INTERFACE, "Mixer is initialized");

  m_sound_touch.setChannels(2);
  m_sound_touch.setSampleRate(BackendSampleRate);
  m_sound_touch.setPitch(1.0);
  m_sound_touch.setTempo(1.0);
  m_sound_touch.setSetting(SETTING_USE_QUICKSEEK, 0);
  m_sound_touch.setSetting(SETTING_SEQUENCE_MS, 62);
  m_sound_touch.setSetting(SETTING_SEEKWINDOW_MS, 28);
  m_sound_touch.setSetting(SETTING_OVERLAP_MS, 8);
}

CMixer::~CMixer()
{
}

// Executed from sound stream thread
unsigned int CMixer::MixerFifo::Mix(short* samples, unsigned int numSamples,
                                    bool consider_framelimit)
{
  unsigned int currentSample = 0;

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

  float emulationspeed = SConfig::GetInstance().m_EmulationSpeed;
  float aid_sample_rate = static_cast<float>(m_input_sample_rate);
  if (consider_framelimit && emulationspeed > 0.0f)
  {
    float numLeft = static_cast<float>(((indexW - indexR) & INDEX_MASK) / 2);

    u32 low_waterwark = m_input_sample_rate * SConfig::GetInstance().iTimingVariance / 1000;
    low_waterwark = std::min(low_waterwark, MAX_SAMPLES / 2);

    m_numLeftI = (numLeft + m_numLeftI * (CONTROL_AVG - 1)) / CONTROL_AVG;
    float offset = (m_numLeftI - low_waterwark) * CONTROL_FACTOR;
    if (offset > MAX_FREQ_SHIFT)
      offset = MAX_FREQ_SHIFT;
    if (offset < -MAX_FREQ_SHIFT)
      offset = -MAX_FREQ_SHIFT;

    aid_sample_rate = (aid_sample_rate + offset) * emulationspeed;
  }

  const u32 ratio = (u32)(65536.0f * aid_sample_rate / (float)m_mixer->m_sampleRate);

  s32 lvolume = m_LVolume.load();
  s32 rvolume = m_RVolume.load();

  // TODO: consider a higher-quality resampling algorithm.
  for (; currentSample < numSamples * 2 && ((indexW - indexR) & INDEX_MASK) > 2; currentSample += 2)
  {
    u32 indexR2 = indexR + 2;  // next sample

    s16 l1 = Common::swap16(m_buffer[indexR & INDEX_MASK]);   // current
    s16 l2 = Common::swap16(m_buffer[indexR2 & INDEX_MASK]);  // next
    int sampleL = ((l1 << 16) + (l2 - l1) * (u16)m_frac) >> 16;
    sampleL = (sampleL * lvolume) >> 8;
    sampleL += samples[currentSample + 1];
    samples[currentSample + 1] = MathUtil::Clamp(sampleL, -32767, 32767);

    s16 r1 = Common::swap16(m_buffer[(indexR + 1) & INDEX_MASK]);   // current
    s16 r2 = Common::swap16(m_buffer[(indexR2 + 1) & INDEX_MASK]);  // next
    int sampleR = ((r1 << 16) + (r2 - r1) * (u16)m_frac) >> 16;
    sampleR = (sampleR * rvolume) >> 8;
    sampleR += samples[currentSample];
    samples[currentSample] = MathUtil::Clamp(sampleR, -32767, 32767);

    m_frac += ratio;
    indexR += 2 * (u16)(m_frac >> 16);
    m_frac &= 0xffff;
  }

  // Actual number of samples written to the buffer without padding.
  unsigned int actual_sample_count = currentSample / 2;

  // Padding
  short s[2];
  s[0] = Common::swap16(m_buffer[(indexR - 1) & INDEX_MASK]);
  s[1] = Common::swap16(m_buffer[(indexR - 2) & INDEX_MASK]);
  s[0] = (s[0] * rvolume) >> 8;
  s[1] = (s[1] * lvolume) >> 8;
  for (; currentSample < numSamples * 2; currentSample += 2)
  {
    int sampleR = MathUtil::Clamp(s[0] + samples[currentSample + 0], -32767, 32767);
    int sampleL = MathUtil::Clamp(s[1] + samples[currentSample + 1], -32767, 32767);

    samples[currentSample + 0] = sampleR;
    samples[currentSample + 1] = sampleL;
  }

  // Flush cached variable
  m_indexR.store(indexR);

  return actual_sample_count;
}

unsigned int CMixer::Mix(short* samples, unsigned int num_samples)
{
  if (!samples)
    return 0;

  memset(samples, 0, num_samples * 2 * sizeof(short));

  if (SConfig::GetInstance().m_audio_stretch)
  {
    unsigned int available_samples =
        std::min(m_dma_mixer.AvailableSamples(), m_streaming_mixer.AvailableSamples());

    m_stretch_buffer.fill(0);

    m_dma_mixer.Mix(m_stretch_buffer.data(), available_samples, false);
    m_streaming_mixer.Mix(m_stretch_buffer.data(), available_samples, false);
    m_wiimote_speaker_mixer.Mix(m_stretch_buffer.data(), available_samples, false);

    if (!m_is_stretching)
    {
      m_sound_touch.clear();
      m_is_stretching = true;
    }
    StretchAudio(m_stretch_buffer.data(), available_samples, samples, num_samples);
  }
  else
  {
    m_dma_mixer.Mix(samples, num_samples, true);
    m_streaming_mixer.Mix(samples, num_samples, true);
    m_wiimote_speaker_mixer.Mix(samples, num_samples, true);
    m_is_stretching = false;
  }

  return num_samples;
}

void CMixer::StretchAudio(const short* in, unsigned int num_in, short* out, unsigned int num_out)
{
  const double time_delta = static_cast<double>(num_out) / m_sampleRate;  // seconds

  // We were given actual_samples number of samples, and num_samples were requested from us.
  double current_ratio = static_cast<double>(num_in) / static_cast<double>(num_out);

  const double max_latency = SConfig::GetInstance().m_audio_stretch_max_latency;
  const double max_backlog = m_sampleRate * max_latency / 1000.0 / m_stretch_ratio;
  const double backlog_fullness = m_sound_touch.numSamples() / max_backlog;
  if (backlog_fullness > 5.0)
  {
    // Too many samples in backlog: Don't push anymore on
    num_in = 0;
  }

  // We ideally want the backlog to be about 50% full.
  // This gives some headroom both ways to prevent underflow and overflow.
  // We tweak current_ratio to encourage this.
  constexpr double tweak_time_scale = 0.5;  // seconds
  current_ratio *= 1.0 + 2.0 * (backlog_fullness - 0.5) * (time_delta / tweak_time_scale);

  // This low-pass filter smoothes out variance in the calculated stretch ratio.
  // The time-scale determines how responsive this filter is.
  constexpr double lpf_time_scale = 1.0;  // seconds
  const double m_lpf_gain = 1.0 - std::exp(-time_delta / lpf_time_scale);
  m_stretch_ratio += m_lpf_gain * (current_ratio - m_stretch_ratio);

  // Place a lower limit of 10% speed.  When a game boots up, there will be
  // many silence samples.  These do not need to be timestretched.
  m_stretch_ratio = std::max(m_stretch_ratio, 0.1);
  m_sound_touch.setTempo(m_stretch_ratio);

  DEBUG_LOG(AUDIO, "Audio stretching: samples:%u/%u ratio:%f backlog:%f gain: %f", num_in, num_out,
            m_stretch_ratio, backlog_fullness, m_lpf_gain);

  m_sound_touch.putSamples(in, num_in);

  const size_t samples_received = m_sound_touch.receiveSamples(out, num_out);

  if (samples_received != 0)
  {
    m_last_stretched_sample[0] = out[samples_received * 2 - 2];
    m_last_stretched_sample[1] = out[samples_received * 2 - 1];
  }

  // Preform padding if we've run out of samples.
  for (size_t i = samples_received; i < num_out; i++)
  {
    out[i * 2 + 0] = m_last_stretched_sample[0];
    out[i * 2 + 1] = m_last_stretched_sample[1];
  }
}

void CMixer::MixerFifo::PushSamples(const short* samples, unsigned int num_samples)
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

void CMixer::PushSamples(const short* samples, unsigned int num_samples)
{
  m_dma_mixer.PushSamples(samples, num_samples);
  int sample_rate = m_dma_mixer.GetInputSampleRate();
  if (m_log_dsp_audio)
    m_wave_writer_dsp.AddStereoSamplesBE(samples, num_samples, sample_rate);
}

void CMixer::PushStreamingSamples(const short* samples, unsigned int num_samples)
{
  m_streaming_mixer.PushSamples(samples, num_samples);
  int sample_rate = m_streaming_mixer.GetInputSampleRate();
  if (m_log_dtk_audio)
    m_wave_writer_dtk.AddStereoSamplesBE(samples, num_samples, sample_rate);
}

void CMixer::PushWiimoteSpeakerSamples(const short* samples, unsigned int num_samples,
                                       unsigned int sample_rate)
{
  short samples_stereo[MAX_SAMPLES * 2];

  if (num_samples < MAX_SAMPLES)
  {
    m_wiimote_speaker_mixer.SetInputSampleRate(sample_rate);

    for (unsigned int i = 0; i < num_samples; ++i)
    {
      samples_stereo[i * 2] = Common::swap16(samples[i]);
      samples_stereo[i * 2 + 1] = Common::swap16(samples[i]);
    }

    m_wiimote_speaker_mixer.PushSamples(samples_stereo, num_samples);
  }
}

void CMixer::SetDMAInputSampleRate(unsigned int rate)
{
  m_dma_mixer.SetInputSampleRate(rate);
}

void CMixer::SetStreamInputSampleRate(unsigned int rate)
{
  m_streaming_mixer.SetInputSampleRate(rate);
}

void CMixer::SetStreamingVolume(unsigned int lvolume, unsigned int rvolume)
{
  m_streaming_mixer.SetVolume(lvolume, rvolume);
}

void CMixer::SetWiimoteSpeakerVolume(unsigned int lvolume, unsigned int rvolume)
{
  m_wiimote_speaker_mixer.SetVolume(lvolume, rvolume);
}

void CMixer::StartLogDTKAudio(const std::string& filename)
{
  if (!m_log_dtk_audio)
  {
    bool success = m_wave_writer_dtk.Start(filename, m_streaming_mixer.GetInputSampleRate());
    if (success)
    {
      m_log_dtk_audio = true;
      m_wave_writer_dtk.SetSkipSilence(false);
      NOTICE_LOG(AUDIO, "Starting DTK Audio logging");
    }
    else
    {
      m_wave_writer_dtk.Stop();
      NOTICE_LOG(AUDIO, "Unable to start DTK Audio logging");
    }
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
    bool success = m_wave_writer_dsp.Start(filename, m_dma_mixer.GetInputSampleRate());
    if (success)
    {
      m_log_dsp_audio = true;
      m_wave_writer_dsp.SetSkipSilence(false);
      NOTICE_LOG(AUDIO, "Starting DSP Audio logging");
    }
    else
    {
      m_wave_writer_dsp.Stop();
      NOTICE_LOG(AUDIO, "Unable to start DSP Audio logging");
    }
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

void CMixer::MixerFifo::SetInputSampleRate(unsigned int rate)
{
  m_input_sample_rate = rate;
}

unsigned int CMixer::MixerFifo::GetInputSampleRate() const
{
  return m_input_sample_rate;
}

void CMixer::MixerFifo::SetVolume(unsigned int lvolume, unsigned int rvolume)
{
  m_LVolume.store(lvolume + (lvolume >> 7));
  m_RVolume.store(rvolume + (rvolume >> 7));
}

unsigned int CMixer::MixerFifo::AvailableSamples() const
{
  unsigned int samples_in_fifo = ((m_indexW.load() - m_indexR.load()) & INDEX_MASK) / 2;
  if (samples_in_fifo <= 1)
    return 0;  // CMixer::MixerFifo::Mix always keeps one sample in the buffer.
  return (samples_in_fifo - 1) * m_mixer->m_sampleRate / m_input_sample_rate;
}
