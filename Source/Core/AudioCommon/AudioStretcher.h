// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include <SoundTouch.h>

#include "Common/CommonTypes.h"

namespace AudioCommon
{
class AudioStretcher
{
public:
  explicit AudioStretcher(u64 sample_rate);
  void ProcessSamples(const s16* in, u32 num_in, u32 num_out);
  void GetStretchedSamples(s16* out, u32 num_out);
  void Clear();

  DT AvailableSamplesTime() const;

private:
  u64 m_sample_rate;
  std::array<s16, 2> m_last_stretched_sample = {};
  soundtouch::SoundTouch m_sound_touch;
  double m_stretch_ratio = 1.0;
};

}  // namespace AudioCommon
