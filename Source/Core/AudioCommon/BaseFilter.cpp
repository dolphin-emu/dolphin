// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "AudioCommon/BaseFilter.h"

BaseFilter::BaseFilter(u32 taps) : m_num_taps(taps)
{
}

u32 BaseFilter::GetNumTaps() const
{
  return m_num_taps;
}