// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/FPURoundMode.h"

#include <bit>

#include "Common/CommonTypes.h"

namespace Common::FPU
{
static u64 GetFPSCR()
{
  double fpscr;
  __asm__ __volatile__("mffs %0" : "=d"(fpscr));
  return std::bit_cast<u64>(fpscr);
}

static void SetFPSCR(u64 fpscr)
{
  __asm__ __volatile__("mtfsf 0xff, %0" : : "d"(std::bit_cast<double>(fpscr)));
}

static const u64 default_fpscr = GetFPSCR();
static u64 saved_fpscr = default_fpscr;

void SetSIMDMode(RoundMode rounding_mode, bool non_ieee_mode)
{
  // FPSCR.RN uses the same encoding on the host as on the GC/Wii CPU.
  constexpr u64 RN = 3;

  // FPSCR.NI is always left clear. It cant emulate the GC/Wii CPUs non-IEEE mode, which flushes
  // denormal outputs (but not inputs) to zero: it has no effect on POWER8, POWER9 or Power10, and
  // the architecture allows it to make results approximate and to vary between executions. So
  // cpu_info.bFlushToZero is false, and outputs are flushed in software instead.
  constexpr u64 NI = 4;

  SetFPSCR((default_fpscr & ~(RN | NI)) | rounding_mode);
}

void SaveSIMDState()
{
  saved_fpscr = GetFPSCR();
}

void LoadSIMDState()
{
  SetFPSCR(saved_fpscr);
}

void LoadDefaultSIMDState()
{
  SetFPSCR(default_fpscr);
}

}  // namespace Common::FPU
