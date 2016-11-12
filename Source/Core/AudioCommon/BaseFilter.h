// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/RingBuffer.h"

class BaseFilter
{
public:
  BaseFilter(u32 taps = 0);
  virtual ~BaseFilter() = default;
  // ratio is out_rate / in_rate
  virtual void ConvolveStereo(const RingBuffer<float>& input, size_t index, float* output_l,
                              float* output_r, float fraction, float ratio) const = 0;
  u32 GetNumTaps() const;

protected:
  const u32 m_num_taps;  // one-sided
};