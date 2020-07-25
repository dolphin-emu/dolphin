// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

#include <SoundTouch.h>

namespace AudioCommon
{
class AudioStretcher
{
public:
  explicit AudioStretcher(u32 sample_rate);
  void PushSamples(const s16* in, u32 num_in);
  void GetStretchedSamples(s16* out, u32 num_out);
  void Clear();
  void SetTempo(double tempo, bool reset = false);
  void SetSampleRate(u32 sample_rate);
  double GetProcessedLatency() const;
  double GetAcceptableLatency() const;

private:
  u32 m_sample_rate;
  std::array<s16, 2> m_last_stretched_sample = {};
  soundtouch::SoundTouch m_sound_touch;
  double m_stretch_ratio = 1.0;
  double m_stretch_ratio_sum = 0.0;
  u32 m_stretch_ratio_count = 0;
};

}  // namespace AudioCommon
