// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "AudioCommon/BaseFilter.h"

class LinearFilter final : public BaseFilter
{
public:
  LinearFilter();
  ~LinearFilter() = default;
  void ConvolveStereo(const RingBuffer<float>& input, size_t index, float* output_l,
                      float* output_r, float fraction, float ratio) const override;

private:
  static float Lerp(float sample1, float sample2, float fraction);
};