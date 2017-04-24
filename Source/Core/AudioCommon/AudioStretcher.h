// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

#include <soundtouch/SoundTouch.h>

namespace AudioCommon
{
class AudioStretcher
{
public:
  AudioStretcher(unsigned int sample_rate);
  void StretchAudio(const short* in, unsigned int num_in, short* out, unsigned int num_out);
  void Clear();

private:
  unsigned int m_sample_rate;
  std::array<short, 2> m_last_stretched_sample = {};
  soundtouch::SoundTouch m_sound_touch;
  double m_stretch_ratio = 1.0;
};

}  // AudioCommon
