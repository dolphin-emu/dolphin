// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "AudioCommon/LinearFilter.h"

LinearFilter::LinearFilter() : BaseFilter(1)
{
}

void LinearFilter::ConvolveStereo(const RingBuffer<float>& input, size_t index, float* output_l,
                                  float* output_r, const float fraction, const float ratio) const
{
  float l1 = input[index - 2];
  float r1 = input[index - 1];
  float l2 = input[index];
  float r2 = input[index + 1];

  *output_l = Lerp(l1, l2, fraction);
  *output_r = Lerp(r1, r2, fraction);
}

float LinearFilter::Lerp(float sample1, float sample2, float fraction)
{
  return (1.f - fraction) * sample1 + fraction * sample2;
}