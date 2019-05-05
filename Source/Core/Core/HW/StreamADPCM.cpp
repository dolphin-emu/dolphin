// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Adapted from in_cube by hcs & destop

#include <algorithm>

#include "Core/HW/StreamADPCM.h"

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"

namespace StreamADPCM
{
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
  hist = std::clamp((hist + 0x20) >> 6, -0x200000, 0x1fffff);

  s32 cur = (((s16)(bits << 12) >> (q & 0xf)) << 6) + hist;

  hist2 = hist1;
  hist1 = cur;

  cur >>= 6;
  cur = std::clamp(cur, -0x8000, 0x7fff);

  return (s16)cur;
}

void ADPCMDecoder::ResetFilter()
{
  m_histl1 = 0;
  m_histl2 = 0;
  m_histr1 = 0;
  m_histr2 = 0;
}

void ADPCMDecoder::DoState(PointerWrap& p)
{
  p.Do(m_histl1);
  p.Do(m_histl2);
  p.Do(m_histr1);
  p.Do(m_histr2);
}

void ADPCMDecoder::DecodeBlock(s16* pcm, const u8* adpcm)
{
  for (int i = 0; i < SAMPLES_PER_BLOCK; i++)
  {
    pcm[i * 2] = ADPDecodeSample(adpcm[i + (ONE_BLOCK_SIZE - SAMPLES_PER_BLOCK)] & 0xf, adpcm[0],
                                 m_histl1, m_histl2);
    pcm[i * 2 + 1] = ADPDecodeSample(adpcm[i + (ONE_BLOCK_SIZE - SAMPLES_PER_BLOCK)] >> 4, adpcm[1],
                                     m_histr1, m_histr2);
  }
}
}  // namespace StreamADPCM
