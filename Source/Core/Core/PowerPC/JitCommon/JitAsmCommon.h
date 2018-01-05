// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

extern const u8 GC_ALIGNED16(pbswapShuffle1x4[16]);
extern const u8 GC_ALIGNED16(pbswapShuffle2x4[16]);
extern const float GC_ALIGNED16(m_one[4]);
extern const float GC_ALIGNED16(m_quantizeTableS[128]);
extern const float GC_ALIGNED16(m_dequantizeTableS[128]);

class CommonAsmRoutinesBase
{
public:
  const u8* fifoDirectWrite8;
  const u8* fifoDirectWrite16;
  const u8* fifoDirectWrite32;
  const u8* fifoDirectWrite64;

  const u8* enterCode;

  const u8* dispatcherMispredictedBLR;
  const u8* dispatcher;
  const u8* dispatcherNoCheck;

  const u8* doTiming;

  const u8* frsqrte;
  const u8* fres;
  const u8* mfcr;

  // In: array index: GQR to use.
  // In: ECX: Address to read from.
  // Out: XMM0: Bottom two 32-bit slots hold the read value,
  //            converted to a pair of floats.
  // Trashes: all three RSCRATCH
  const u8** pairedLoadQuantized;

  // In: array index: GQR to use.
  // In: ECX: Address to read from.
  // Out: XMM0: Bottom 32-bit slot holds the read value.
  // Trashes: all three RSCRATCH
  const u8** singleLoadQuantized;

  // In: array index: GQR to use.
  // In: ECX: Address to write to.
  // In: XMM0: Bottom two 32-bit slots hold the pair of floats to be written.
  // Out: Nothing.
  // Trashes: all three RSCRATCH
  const u8** pairedStoreQuantized;

  // In: array index: GQR to use.
  // In: ECX: Address to write to.
  // In: XMM0: Bottom 32-bit slot holds the float to be written.
  const u8** singleStoreQuantized;
};
