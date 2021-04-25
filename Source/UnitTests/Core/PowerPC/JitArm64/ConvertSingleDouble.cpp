// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <functional>
#include <vector>

#include "Common/Arm64Emitter.h"
#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Common/FPURoundMode.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"
#include "Core/PowerPC/JitArm64/Jit.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

namespace
{
using namespace Arm64Gen;

// The ABI situation for returning an std::tuple seems annoying. Let's use this struct instead
template <typename T>
struct Pair
{
  T value1;
  T value2;
};

class TestConversion : private JitArm64
{
public:
  TestConversion()
  {
    AllocCodeSpace(4096);
    AddChildCodeSpace(&farcode, 2048);

    gpr.Init(this);
    fpr.Init(this);

    js.fpr_is_store_safe = BitSet32(0);

    GetAsmRoutines()->cdts = GetCodePtr();
    GenerateConvertDoubleToSingle();
    GetAsmRoutines()->cstd = GetCodePtr();
    GenerateConvertSingleToDouble();

    gpr.Lock(ARM64Reg::W30);
    fpr.Lock(ARM64Reg::Q0, ARM64Reg::Q1);

    convert_single_to_double_lower = Common::BitCast<u64 (*)(u32)>(GetCodePtr());
    m_float_emit.INS(32, ARM64Reg::S0, 0, ARM64Reg::W0);
    ConvertSingleToDoubleLower(0, ARM64Reg::D0, ARM64Reg::S0, ARM64Reg::Q1);
    m_float_emit.UMOV(64, ARM64Reg::X0, ARM64Reg::D0, 0);
    RET();

    convert_single_to_double_pair = Common::BitCast<Pair<u64> (*)(u32, u32)>(GetCodePtr());
    m_float_emit.INS(32, ARM64Reg::D0, 0, ARM64Reg::W0);
    m_float_emit.INS(32, ARM64Reg::D0, 1, ARM64Reg::W1);
    ConvertSingleToDoublePair(0, ARM64Reg::Q0, ARM64Reg::D0, ARM64Reg::Q1);
    m_float_emit.UMOV(64, ARM64Reg::X0, ARM64Reg::Q0, 0);
    m_float_emit.UMOV(64, ARM64Reg::X1, ARM64Reg::Q0, 1);
    RET();

    convert_double_to_single_lower = Common::BitCast<u32 (*)(u64)>(GetCodePtr());
    m_float_emit.INS(64, ARM64Reg::D0, 0, ARM64Reg::X0);
    ConvertDoubleToSingleLower(0, ARM64Reg::S0, ARM64Reg::D0);
    m_float_emit.UMOV(32, ARM64Reg::W0, ARM64Reg::S0, 0);
    RET();

    convert_double_to_single_pair = Common::BitCast<Pair<u32> (*)(u64, u64)>(GetCodePtr());
    m_float_emit.INS(64, ARM64Reg::Q0, 0, ARM64Reg::X0);
    m_float_emit.INS(64, ARM64Reg::Q0, 1, ARM64Reg::X1);
    ConvertDoubleToSinglePair(0, ARM64Reg::D0, ARM64Reg::Q0);
    m_float_emit.UMOV(64, ARM64Reg::X0, ARM64Reg::D0, 0);
    RET();

    gpr.Unlock(ARM64Reg::W30);
    fpr.Unlock(ARM64Reg::Q0, ARM64Reg::Q1);

    FlushIcache();

    // Set the rounding mode to something that's as annoying as possible to handle
    // (flush-to-zero enabled, and rounding not symmetric about the origin)
    FPURoundMode::SetSIMDMode(FPURoundMode::RoundMode::ROUND_UP, true);
  }

  ~TestConversion() override
  {
    FPURoundMode::LoadDefaultSIMDState();

    FreeCodeSpace();
  }

  u64 ConvertSingleToDouble(u32 value) { return convert_single_to_double_lower(value); }

  Pair<u64> ConvertSingleToDouble(u32 value1, u32 value2)
  {
    return convert_single_to_double_pair(value1, value2);
  }

  u32 ConvertDoubleToSingle(u64 value) { return convert_double_to_single_lower(value); }

  Pair<u32> ConvertDoubleToSingle(u64 value1, u64 value2)
  {
    return convert_double_to_single_pair(value1, value2);
  }

private:
  std::function<u64(u32)> convert_single_to_double_lower;
  std::function<Pair<u64>(u32, u32)> convert_single_to_double_pair;
  std::function<u32(u64)> convert_double_to_single_lower;
  std::function<Pair<u32>(u64, u64)> convert_double_to_single_pair;
};

}  // namespace

TEST(JitArm64, ConvertDoubleToSingle)
{
  TestConversion test;

  const std::vector<u64> input_values{
      // Special values
      0x0000'0000'0000'0000,  // positive zero
      0x0000'0000'0000'0001,  // smallest positive denormal
      0x0000'0000'0100'0000,
      0x000F'FFFF'FFFF'FFFF,  // largest positive denormal
      0x0010'0000'0000'0000,  // smallest positive normal
      0x0010'0000'0000'0002,
      0x3FF0'0000'0000'0000,  // 1.0
      0x7FEF'FFFF'FFFF'FFFF,  // largest positive normal
      0x7FF0'0000'0000'0000,  // positive infinity
      0x7FF0'0000'0000'0001,  // first positive SNaN
      0x7FF7'FFFF'FFFF'FFFF,  // last positive SNaN
      0x7FF8'0000'0000'0000,  // first positive QNaN
      0x7FFF'FFFF'FFFF'FFFF,  // last positive QNaN
      0x8000'0000'0000'0000,  // negative zero
      0x8000'0000'0000'0001,  // smallest negative denormal
      0x8000'0000'0100'0000,
      0x800F'FFFF'FFFF'FFFF,  // largest negative denormal
      0x8010'0000'0000'0000,  // smallest negative normal
      0x8010'0000'0000'0002,
      0xBFF0'0000'0000'0000,  // -1.0
      0xFFEF'FFFF'FFFF'FFFF,  // largest negative normal
      0xFFF0'0000'0000'0000,  // negative infinity
      0xFFF0'0000'0000'0001,  // first negative SNaN
      0xFFF7'FFFF'FFFF'FFFF,  // last negative SNaN
      0xFFF8'0000'0000'0000,  // first negative QNaN
      0xFFFF'FFFF'FFFF'FFFF,  // last negative QNaN

      // (exp > 896) Boundary Case
      0x3800'0000'0000'0000,  // 2^(-127) = Denormal in single-prec
      0x3810'0000'0000'0000,  // 2^(-126) = Smallest single-prec normal
      0xB800'0000'0000'0000,  // -2^(-127) = Denormal in single-prec
      0xB810'0000'0000'0000,  // -2^(-126) = Smallest single-prec normal
      0x3800'1234'5678'9ABC, 0x3810'1234'5678'9ABC, 0xB800'1234'5678'9ABC, 0xB810'1234'5678'9ABC,

      // (exp >= 874) Boundary Case
      0x3680'0000'0000'0000,  // 2^(-150) = Unrepresentable in single-prec
      0x36A0'0000'0000'0000,  // 2^(-149) = Smallest single-prec denormal
      0x36B0'0000'0000'0000,  // 2^(-148) = Single-prec denormal
      0xB680'0000'0000'0000,  // -2^(-150) = Unrepresentable in single-prec
      0xB6A0'0000'0000'0000,  // -2^(-149) = Smallest single-prec denormal
      0xB6B0'0000'0000'0000,  // -2^(-148) = Single-prec denormal
      0x3680'1234'5678'9ABC, 0x36A0'1234'5678'9ABC, 0x36B0'1234'5678'9ABC, 0xB680'1234'5678'9ABC,
      0xB6A0'1234'5678'9ABC, 0xB6B0'1234'5678'9ABC,

      // Some typical numbers
      0x3FF8'0000'0000'0000,  // 1.5
      0x408F'4000'0000'0000,  // 1000
      0xC008'0000'0000'0000,  // -3
  };

  for (const u64 input : input_values)
  {
    const u32 expected = ConvertToSingle(input);
    const u32 actual = test.ConvertDoubleToSingle(input);

    if (expected != actual)
      fmt::print("{:016x} -> {:08x} == {:08x}\n", input, actual, expected);

    EXPECT_EQ(expected, actual);
  }

  for (const u64 input1 : input_values)
  {
    for (const u64 input2 : input_values)
    {
      const u32 expected1 = ConvertToSingle(input1);
      const u32 expected2 = ConvertToSingle(input2);
      const auto [actual1, actual2] = test.ConvertDoubleToSingle(input1, input2);

      if (expected1 != actual1 || expected2 != actual2)
      {
        fmt::print("{:016x} -> {:08x} == {:08x},\n", input1, actual1, expected1);
        fmt::print("{:016x} -> {:08x} == {:08x}\n", input2, actual2, expected2);
      }

      EXPECT_EQ(expected1, actual1);
      EXPECT_EQ(expected2, actual2);
    }
  }
}

TEST(JitArm64, ConvertSingleToDouble)
{
  TestConversion test;

  const std::vector<u32> input_values{
      // Special values
      0x0000'0000,  // positive zero
      0x0000'0001,  // smallest positive denormal
      0x0000'1000,
      0x007F'FFFF,  // largest positive denormal
      0x0080'0000,  // smallest positive normal
      0x0080'0002,
      0x3F80'0000,  // 1.0
      0x7F7F'FFFF,  // largest positive normal
      0x7F80'0000,  // positive infinity
      0x7F80'0001,  // first positive SNaN
      0x7FBF'FFFF,  // last positive SNaN
      0x7FC0'0000,  // first positive QNaN
      0x7FFF'FFFF,  // last positive QNaN
      0x8000'0000,  // negative zero
      0x8000'0001,  // smallest negative denormal
      0x8000'1000,
      0x807F'FFFF,  // largest negative denormal
      0x8080'0000,  // smallest negative normal
      0x8080'0002,
      0xBFF0'0000,  // -1.0
      0xFF7F'FFFF,  // largest negative normal
      0xFF80'0000,  // negative infinity
      0xFF80'0001,  // first negative SNaN
      0xFFBF'FFFF,  // last negative SNaN
      0xFFC0'0000,  // first negative QNaN
      0xFFFF'FFFF,  // last negative QNaN

      // Some typical numbers
      0x3FC0'0000,  // 1.5
      0x447A'0000,  // 1000
      0xC040'0000,  // -3
  };

  for (const u32 input : input_values)
  {
    const u64 expected = ConvertToDouble(input);
    const u64 actual = test.ConvertSingleToDouble(input);

    if (expected != actual)
      fmt::print("{:08x} -> {:016x} == {:016x}\n", input, actual, expected);

    EXPECT_EQ(expected, actual);
  }

  for (const u32 input1 : input_values)
  {
    for (const u32 input2 : input_values)
    {
      const u64 expected1 = ConvertToDouble(input1);
      const u64 expected2 = ConvertToDouble(input2);
      const auto [actual1, actual2] = test.ConvertSingleToDouble(input1, input2);

      if (expected1 != actual1 || expected2 != actual2)
      {
        fmt::print("{:08x} -> {:016x} == {:016x},\n", input1, actual1, expected1);
        fmt::print("{:08x} -> {:016x} == {:016x}\n", input2, actual2, expected2);
      }

      EXPECT_EQ(expected1, actual1);
      EXPECT_EQ(expected2, actual2);
    }
  }
}
