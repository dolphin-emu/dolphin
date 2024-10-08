// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioCommon/AudioStretcher.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "Common/Logging/Log.h"
#include "Core/Config/MainSettings.h"

namespace AudioCommon
{
AudioStretcher::AudioStretcher(u64 sample_rate) : m_sample_rate(sample_rate)
{
  m_sound_touch.setChannels(2);
  m_sound_touch.setSampleRate(sample_rate);
  m_sound_touch.setPitch(1.0);
  m_sound_touch.setTempo(1.0);
}

void AudioStretcher::Clear()
{
  m_sound_touch.clear();
}

void AudioStretcher::ProcessSamples(const s16* in, u32 num_in, u32 num_out)
{
  const double time_delta = static_cast<double>(num_out) / m_sample_rate;
  double current_ratio = static_cast<double>(num_in) / num_out;  // Current sample ratio.

  // Calculate maximum allowable backlog based on configured maximum latency.
  const double max_latency = Config::Get(Config::MAIN_AUDIO_STRETCH_LATENCY) / 1000.0;
  const double max_backlog = m_sample_rate * max_latency;  // Max number of samples in the backlog.
  const double num_samples = m_sound_touch.numSamples();

  // Prevent backlog from growing too large.
  if (num_samples >= 5.0 * max_backlog)
    num_in = 0;

  // Target for backlog to be about 50% full to allow flexibility.
  const double low_watermark = 0.5 * max_backlog;
  const double requested_samples = m_stretch_ratio * num_out;
  const double num_left = num_samples - requested_samples;

  // Adjustment parameters similar to those used in Mixer for pitch adjustment.
  const double lpf_time_scale = 1.0 / 16.0;
  const double control_effort = 1.0 / 16.0;
  current_ratio *= 1.0 + control_effort * (num_left - low_watermark) / requested_samples;
  current_ratio = std::clamp(current_ratio, 0.1, 10.0);

  // Calculate target sample ratio and apply low-pass filter to smooth changes.
  const double lpf_gain = -std::expm1(-time_delta / lpf_time_scale);
  m_stretch_ratio += lpf_gain * (current_ratio - m_stretch_ratio);
  m_sound_touch.setTempo(m_stretch_ratio);

  // Debug log to monitor stretching stats.
  DEBUG_LOG_FMT(AUDIO, "Audio stretching: samples:{}/{} ratio:{} backlog:{} gain:{}", num_in,
                num_out, m_stretch_ratio, num_samples / max_backlog, lpf_gain);

  // Add new samples to the processor for stretching.
  m_sound_touch.putSamples(in, num_in);
}

void AudioStretcher::GetStretchedSamples(s16* out, u32 num_out)
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

DT AudioStretcher::AvailableSamplesTime() const
{
  const u32 backlog = m_sound_touch.numSamples();
  return std::chrono::duration_cast<DT>(DT_s(backlog) / m_sample_rate);
}

}  // namespace AudioCommon
