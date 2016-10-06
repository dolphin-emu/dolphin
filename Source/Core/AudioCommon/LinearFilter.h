// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "AudioCommon/BaseFilter.h"

class LinearFilter final : public BaseFilter
{
public:
  LinearFilter() : BaseFilter(1) {}
  ~LinearFilter() {}

  void ConvolveStereo(const RingBuffer<float>& input, u32 index,
                   float* output_l, float* output_r,
                   const float fraction, const float ratio) const
  {
    float l1 = input[index - 2];
    float r1 = input[index - 1];
    float l2 = input[index];
    float r2 = input[index + 1];

    *output_l = lerp(l1, l2, fraction);
    *output_r = lerp(r1, r2, fraction);
  }

private:
  static inline float lerp(float sample1, float sample2, float fraction)
  {
    return (1.f - fraction) * sample1 + fraction * sample2;
  }
};