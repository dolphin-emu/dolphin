// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Adapted from in_cube by hcs & destop

#pragma once

#include "Common/CommonTypes.h"

class PointerWrap;

namespace StreamADPCM
{
enum
{
  ONE_BLOCK_SIZE = 32,
  SAMPLES_PER_BLOCK = 28
};

class ADPCMDecoder
{
public:
  void ResetFilter();
  void DoState(PointerWrap& p);
  void DecodeBlock(s16* pcm, const u8* adpcm);

private:
  s32 m_histl1 = 0;
  s32 m_histl2 = 0;
  s32 m_histr1 = 0;
  s32 m_histr2 = 0;
};
}  // namespace StreamADPCM
