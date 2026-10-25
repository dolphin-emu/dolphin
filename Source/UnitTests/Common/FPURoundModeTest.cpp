// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <bit>

#include <gtest/gtest.h>

#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/FPURoundMode.h"

namespace
{
// Volatile, so the compiler can neither fold the arithmetic nor move it across SetSIMDMode.
volatile double one = 1.0;
volatile double tiny = 0x1p-60;
volatile double smallest_normal = 0x1p-1022;
volatile double half = 0.5;

class FPURoundModeTest : public testing::Test
{
protected:
  void SetUp() override
  {
#ifdef _M_GENERIC
    GTEST_SKIP() << "Generic builds don't control the host FPU.";
#endif
  }

  void TearDown() override { Common::FPU::LoadDefaultSIMDState(); }
};
}  // namespace

TEST_F(FPURoundModeTest, RoundingMode)
{
  struct Case
  {
    Common::FPU::RoundMode mode;
    double plus;   // 1 + 2^-60
    double minus;  // -1 - 2^-60
  };
  constexpr Case cases[] = {
      {Common::FPU::ROUND_NEAR, 1.0, -1.0},
      {Common::FPU::ROUND_CHOP, 1.0, -1.0},
      {Common::FPU::ROUND_UP, 1.0 + 0x1p-52, -1.0},
      {Common::FPU::ROUND_DOWN, 1.0, -1.0 - 0x1p-52},
  };

  for (const Case& c : cases)
  {
    Common::FPU::SetSIMDMode(c.mode, false);
    EXPECT_EQ(c.plus, one + tiny) << "mode " << c.mode;
    EXPECT_EQ(c.minus, -one - tiny) << "mode " << c.mode;
  }
}

TEST_F(FPURoundModeTest, NonIEEEMode)
{
  // The GC/Wii CPU's non-IEEE mode flushes denormal outputs to zero. Only hosts that report
  // bFlushToZero do that in hardware; on other hosts the interpreter flushes in software, so the
  // host FPU must keep producing denormals.
  Common::FPU::SetSIMDMode(Common::FPU::ROUND_NEAR, true);
  const u64 result = std::bit_cast<u64>(smallest_normal * half);
  if (cpu_info.bFlushToZero)
    EXPECT_EQ(0u, result);
  else
    EXPECT_EQ(0x0008000000000000u, result);

  Common::FPU::SetSIMDMode(Common::FPU::ROUND_NEAR, false);
  EXPECT_EQ(0x0008000000000000u, std::bit_cast<u64>(smallest_normal * half));
}
