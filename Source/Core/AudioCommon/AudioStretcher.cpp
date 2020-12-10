// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "AudioCommon/AudioStretcher.h"

namespace AudioCommon
{
AudioStretcher::AudioStretcher(u32 sample_rate) : m_sample_rate(sample_rate)
{
  m_sound_touch.setChannels(2);
  m_sound_touch.setSampleRate(m_sample_rate);
  // Some "random" values that seemed good for Dolphin 
  m_sound_touch.setSetting(SETTING_SEQUENCE_MS, 62);
  m_sound_touch.setSetting(SETTING_SEEKWINDOW_MS, 28);
  // Unfortunately the AA filter is only applied on the sample rate transposer, which we don't use,
  // it just adds overhead and loss of quality. AA is not be used by the tempo stretcher
  m_sound_touch.setSetting(SETTING_USE_AA_FILTER, 0);
}

void AudioStretcher::Clear()
{
  m_sound_touch.clear();
  m_last_stretched_sample[0] = 0;
  m_last_stretched_sample[1] = 0;
  m_stretch_ratio = 1.0;
  m_stretch_ratio_sum = 0.0;
  m_stretch_ratio_count = 0;
}

void AudioStretcher::PushSamples(const s16* in, u32 num_in)
{
  uint prev_processed_samples = m_sound_touch.numSamples();
  m_sound_touch.putSamples(in, num_in);
  // Some samples have been processed, reset our tempo average counter
  if (m_sound_touch.numSamples() != prev_processed_samples)
  {
    m_stretch_ratio_sum = 0.0;
    m_stretch_ratio_count = 0;
  }
}

// This won't return any samples a lot of times as they are processed in batches
u32 AudioStretcher::GetStretchedSamples(s16* out, u32 num_out, bool pad)
{
  u32 samples_received = m_sound_touch.receiveSamples(out, num_out);

  if (samples_received != 0)
  {
    m_last_stretched_sample[0] = out[samples_received * 2 - 2];
    m_last_stretched_sample[1] = out[samples_received * 2 - 1];
  }

  if (!pad)
  {
    return samples_received;
  }

  // Perform padding if we've run out of samples
  for (u32 i = samples_received; i < num_out; ++i)
  {
    out[i * 2 + 0] = m_last_stretched_sample[0];
    out[i * 2 + 1] = m_last_stretched_sample[1];
  }

  return num_out;
}

// Call this before ProcessSamples()
void AudioStretcher::SetTempo(double tempo, bool reset)
{
  // SoutchTouch doesn't constantly produce output samples,
  // it does it in batches when it has enough samples, but at that time,
  // it only considers the "last" set tempo, which won't include
  // the oscillations in speed we had from the last calculated batch,
  // so to avoid missing that information, we use the average of all
  // the tempos set between batches productions.
  // Note that the tempo influences the number of samples needed to
  // produce a batch.
  if (reset)
  {
    m_stretch_ratio_sum = 0.0;
    m_stretch_ratio_count = 0;
  }
  m_stretch_ratio_sum += tempo;
  ++m_stretch_ratio_count;
  m_stretch_ratio = m_stretch_ratio_sum / m_stretch_ratio_count;
  m_sound_touch.setTempo(m_stretch_ratio);
}

void AudioStretcher::SetSampleRate(u32 sample_rate)
{
  m_sample_rate = sample_rate;
  m_sound_touch.setSampleRate(m_sample_rate);
}

// Returns the latency of "processed" samples waiting to be read
double AudioStretcher::GetProcessedLatency() const
{
  return m_sound_touch.numSamples() / double(m_sample_rate);
}

// This returns the smallest amount of samples (as time) that will be processed in a batch.
// We can't have a latency lower than this as these samples are suddenly added
double AudioStretcher::GetAcceptableLatency() const
{
  return m_sound_touch.getSetting(SETTING_NOMINAL_OUTPUT_SEQUENCE) / double(m_sample_rate);
}
}  // namespace AudioCommon
