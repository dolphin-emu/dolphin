// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/RingBuffer.h"

class BaseFilter
{
public:
  BaseFilter(u32 taps = 0) : m_num_taps(taps) {}
  ~BaseFilter() {}
  // ratio is out_rate / in_rate
  virtual void ConvolveStereo(const RingBuffer<float>& input, u32 index, float* output_l,
                              float* output_r, const float fraction, const float ratio) const = 0;

  const u32 m_num_taps;  // one-sided
};