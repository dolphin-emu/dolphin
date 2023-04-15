// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <functional>

#include "Common/Arm64Emitter.h"
#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Common/FPURoundMode.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/System.h"

#include "../TestValues.h"

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
  explicit TestConversion(Core::System& system) : JitArm64(system)
  {
    const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;

    AllocCodeSpace(4096);
    AddChildCodeSpace(&m_far_code, 2048);

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
    Common::FPU::SetSIMDMode(Common::FPU::RoundMode::ROUND_UP, true);
  }

  ~TestConversion() override
  {
    Common::FPU::LoadDefaultSIMDState();

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
  TestConversion test(Core::System::GetInstance());

  for (const u64 input : double_test_values)
  {
    const u32 expected = ConvertToSingle(input);
    const u32 actual = test.ConvertDoubleToSingle(input);

    if (expected != actual)
      fmt::print("{:016x} -> {:08x} == {:08x}\n", input, actual, expected);

    EXPECT_EQ(expected, actual);
  }

  for (const u64 input1 : double_test_values)
  {
    for (const u64 input2 : double_test_values)
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
  TestConversion test(Core::System::GetInstance());

  for (const u32 input : single_test_values)
  {
    const u64 expected = ConvertToDouble(input);
    const u64 actual = test.ConvertSingleToDouble(input);

    if (expected != actual)
      fmt::print("{:08x} -> {:016x} == {:016x}\n", input, actual, expected);

    EXPECT_EQ(expected, actual);
  }

  for (const u32 input1 : single_test_values)
  {
    for (const u32 input2 : single_test_values)
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
