// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/FPURoundMode.h"

#ifdef _MSC_VER
#include <intrin.h>
#endif

#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

namespace Common::FPU
{
static u64 GetFPCR()
{
#ifdef _MSC_VER
  return _ReadStatusReg(ARM64_FPCR);
#else
  u64 fpcr;
  __asm__ __volatile__("mrs %0, fpcr" : "=r"(fpcr));
  return fpcr;
#endif
}

static void SetFPCR(u64 fpcr)
{
#ifdef _MSC_VER
  _WriteStatusReg(ARM64_FPCR, fpcr);
#else
  __asm__ __volatile__("msr fpcr, %0" : : "ri"(fpcr));
#endif
}

static const u64 default_fpcr = GetFPCR();
static u64 saved_fpcr = default_fpcr;

void SetSIMDMode(RoundMode rounding_mode, bool non_ieee_mode)
{
  // When AH is disabled, FZ controls flush-to-zero for both inputs and outputs. When AH is enabled,
  // FZ controls flush-to-zero for outputs, and FIZ controls flush-to-zero for inputs.
  constexpr u32 FZ = 1 << 24;
  constexpr u32 AH = 1 << 1;
  constexpr u32 FIZ = 1 << 0;
  constexpr u32 flush_to_zero_mask = FZ | AH | FIZ;

  // On CPUs with FEAT_AFP support, setting AH = 1, FZ = 1, FIZ = 0 emulates the GC/Wii CPU's
  // "non-IEEE mode". Unfortunately, FEAT_AFP didn't exist until 2020, so we can't count on setting
  // AH actually doing anything. But flushing both inputs and outputs seems to cause less problems
  // than flushing nothing, so let's just set FZ and AH and roll with whatever behavior we get.
  const u32 flush_to_zero_bits = (non_ieee_mode ? FZ | AH : 0);
  static bool afp_warning_shown = false;
  if (!afp_warning_shown && !cpu_info.bAFP && non_ieee_mode)
  {
    afp_warning_shown = true;
    WARN_LOG_FMT(POWERPC,
                 "Non-IEEE mode was requested, but host CPU is not known to support FEAT_AFP");
  }

  // lookup table for FPSCR.RN-to-FPCR.RMode translation
  constexpr u32 rounding_mode_table[] = {
      (0 << 22),  // nearest
      (3 << 22),  // zero
      (1 << 22),  // +inf
      (2 << 22),  // -inf
  };
  constexpr u32 rounding_mode_mask = 3 << 22;
  const u32 rounding_mode_bits = rounding_mode_table[rounding_mode];

  const u64 base = default_fpcr & ~(flush_to_zero_mask | rounding_mode_mask);
  SetFPCR(base | rounding_mode_bits | flush_to_zero_bits);
}

void SaveSIMDState()
{
  saved_fpcr = GetFPCR();
}

void LoadSIMDState()
{
  SetFPCR(saved_fpcr);
}

void LoadDefaultSIMDState()
{
  SetFPCR(default_fpcr);
}

}  // namespace Common::FPU
