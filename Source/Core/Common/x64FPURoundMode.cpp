// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/FPURoundMode.h"

#include <cfenv>

#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/Intrinsics.h"

namespace Common::FPU
{
// Get the default SSE states here.
static u32 saved_sse_state = _mm_getcsr();
static const u32 default_sse_state = _mm_getcsr();

void SetSIMDMode(RoundMode rounding_mode, bool non_ieee_mode)
{
  // OR-mask for disabling FPU exceptions (bits 7-12 in the MXCSR register)
  const u32 EXCEPTION_MASK = 0x1F80;
  // Flush-To-Zero (non-IEEE mode: denormal outputs are set to +/- 0)
  const u32 FTZ = 0x8000;
  // lookup table for FPSCR.RN-to-MXCSR.RC translation
  static const u32 simd_rounding_table[] = {
      (0 << 13) | EXCEPTION_MASK,  // nearest
      (3 << 13) | EXCEPTION_MASK,  // zero
      (2 << 13) | EXCEPTION_MASK,  // +inf
      (1 << 13) | EXCEPTION_MASK,  // -inf
  };
  u32 csr = simd_rounding_table[rounding_mode];

  if (non_ieee_mode)
  {
    csr |= FTZ;
  }
  _mm_setcsr(csr);
}

void SaveSIMDState()
{
  saved_sse_state = _mm_getcsr();
}
void LoadSIMDState()
{
  _mm_setcsr(saved_sse_state);
}
void LoadDefaultSIMDState()
{
  _mm_setcsr(default_sse_state);
}
}  // namespace Common::FPU
