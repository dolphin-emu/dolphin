// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Adapted from in_cube by hcs & destop

#include "Core/HW/StreamADPCM.h"

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"

namespace StreamADPCM
{
// STATE_TO_SAVE
static s32 histl1;
static s32 histl2;
static s32 histr1;
static s32 histr2;

static s16 ADPDecodeSample(s32 bits, s32 q, s32& hist1, s32& hist2)
{
  s32 hist = 0;
  switch (q >> 4)
  {
  case 0:
    hist = 0;
    break;
  case 1:
    hist = (hist1 * 0x3c);
    break;
  case 2:
    hist = (hist1 * 0x73) - (hist2 * 0x34);
    break;
  case 3:
    hist = (hist1 * 0x62) - (hist2 * 0x37);
    break;
  }
  hist = MathUtil::Clamp((hist + 0x20) >> 6, -0x200000, 0x1fffff);

  s32 cur = (((s16)(bits << 12) >> (q & 0xf)) << 6) + hist;

  hist2 = hist1;
  hist1 = cur;

  cur >>= 6;
  cur = MathUtil::Clamp(cur, -0x8000, 0x7fff);

  return (s16)cur;
}

void InitFilter()
{
  histl1 = 0;
  histl2 = 0;
  histr1 = 0;
  histr2 = 0;
}

void DoState(PointerWrap& p)
{
  p.Do(histl1);
  p.Do(histl2);
  p.Do(histr1);
  p.Do(histr2);
}

void DecodeBlock(s16* pcm, const u8* adpcm)
{
  for (int i = 0; i < SAMPLES_PER_BLOCK; i++)
  {
    pcm[i * 2] = ADPDecodeSample(adpcm[i + (ONE_BLOCK_SIZE - SAMPLES_PER_BLOCK)] & 0xf, adpcm[0],
                                 histl1, histl2);
    pcm[i * 2 + 1] = ADPDecodeSample(adpcm[i + (ONE_BLOCK_SIZE - SAMPLES_PER_BLOCK)] >> 4, adpcm[1],
                                     histr1, histr2);
  }
}
}
