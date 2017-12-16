// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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

void InitFilter();
void DoState(PointerWrap& p);
void DecodeBlock(s16* pcm, const u8* adpcm);
}
