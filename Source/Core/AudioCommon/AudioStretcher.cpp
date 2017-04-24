// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "AudioCommon/AudioStretcher.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"

namespace AudioCommon
{
AudioStretcher::AudioStretcher(unsigned int sample_rate) : m_sample_rate(sample_rate)
{
  m_sound_touch.setChannels(2);
  m_sound_touch.setSampleRate(sample_rate);
  m_sound_touch.setPitch(1.0);
  m_sound_touch.setTempo(1.0);
  m_sound_touch.setSetting(SETTING_USE_QUICKSEEK, 0);
  m_sound_touch.setSetting(SETTING_SEQUENCE_MS, 62);
  m_sound_touch.setSetting(SETTING_SEEKWINDOW_MS, 28);
  m_sound_touch.setSetting(SETTING_OVERLAP_MS, 8);
}

void AudioStretcher::Clear()
{
  m_sound_touch.clear();
}

void AudioStretcher::ProcessSamples(const short* in, unsigned int num_in, unsigned int num_out)
{
  const double time_delta = static_cast<double>(num_out) / m_sample_rate;  // seconds

  // We were given actual_samples number of samples, and num_samples were requested from us.
  double current_ratio = static_cast<double>(num_in) / static_cast<double>(num_out);

  const double max_latency = SConfig::GetInstance().m_audio_stretch_max_latency;
  const double max_backlog = m_sample_rate * max_latency / 1000.0 / m_stretch_ratio;
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
  const double lpf_gain = 1.0 - std::exp(-time_delta / lpf_time_scale);
  m_stretch_ratio += lpf_gain * (current_ratio - m_stretch_ratio);

  // Place a lower limit of 10% speed.  When a game boots up, there will be
  // many silence samples.  These do not need to be timestretched.
  m_stretch_ratio = std::max(m_stretch_ratio, 0.1);
  m_sound_touch.setTempo(m_stretch_ratio);

  DEBUG_LOG(AUDIO, "Audio stretching: samples:%u/%u ratio:%f backlog:%f gain: %f", num_in, num_out,
            m_stretch_ratio, backlog_fullness, lpf_gain);

  m_sound_touch.putSamples(in, num_in);
}

void AudioStretcher::GetStretchedSamples(short* out, unsigned int num_out)
{
  const size_t samples_received = m_sound_touch.receiveSamples(out, num_out);

  if (samples_received != 0)
  {
    m_last_stretched_sample[0] = out[samples_received * 2 - 2];
    m_last_stretched_sample[1] = out[samples_received * 2 - 1];
  }

  // Perform padding if we've run out of samples.
  for (size_t i = samples_received; i < num_out; i++)
  {
    out[i * 2 + 0] = m_last_stretched_sample[0];
    out[i * 2 + 1] = m_last_stretched_sample[1];
  }
}

}  // namespace AudioCommon
