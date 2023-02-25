// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

alignas(16) extern const u8 pbswapShuffle1x4[16];
alignas(16) extern const u8 pbswapShuffle2x4[16];
alignas(16) extern const float m_one[4];
alignas(16) extern const float m_quantizeTableS[128];
alignas(16) extern const float m_dequantizeTableS[128];

struct CommonAsmRoutinesBase
{
  const u8* enter_code;

  const u8* dispatcher_mispredicted_blr;
  const u8* dispatcher;
  const u8* dispatcher_no_timing_check;
  const u8* dispatcher_no_check;
  const u8* dispatcher_exit;

  const u8* do_timing;

  const u8* frsqrte;
  const u8* fres;
  const u8* mfcr;
  const u8* cdts;
  const u8* cstd;
  const u8* fprf_single;
  const u8* fprf_double;

  // In: array index: GQR to use.
  // In: ECX: Address to read from.
  // Out: XMM0: Bottom two 32-bit slots hold the read value,
  //            converted to a pair of floats.
  // Trashes: all three RSCRATCH
  // Note: Store PC if this could cause an exception
  const u8** paired_load_quantized;

  // In: array index: GQR to use.
  // In: ECX: Address to read from.
  // Out: XMM0: Bottom 32-bit slot holds the read value.
  // Trashes: all three RSCRATCH
  // Note: Store PC if this could cause an exception
  const u8** single_load_quantized;

  // In: array index: GQR to use.
  // In: ECX: Address to write to.
  // In: XMM0: Bottom two 32-bit slots hold the pair of floats to be written.
  // Out: Nothing.
  // Trashes: all three RSCRATCH
  // Note: Store PC if this could cause an exception
  const u8** paired_store_quantized;

  // In: array index: GQR to use.
  // In: ECX: Address to write to.
  // In: XMM0: Bottom 32-bit slot holds the float to be written.
  // Note: Store PC if this could cause an exception
  const u8** single_store_quantized;
};
