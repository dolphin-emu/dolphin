// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>
#include <tuple>

#include "Common/CommonTypes.h"
#include "Common/x64ABI.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64Common/Jit64AsmCommon.h"
#include "Core/PowerPC/Jit64Common/Jit64PowerPCState.h"
#include "Core/System.h"

#include "../TestValues.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

namespace
{
class TestCommonAsmRoutines : public CommonAsmRoutines
{
public:
  explicit TestCommonAsmRoutines(Core::System& system) : CommonAsmRoutines(jit), jit(system)
  {
    using namespace Gen;

    AllocCodeSpace(4096);
    m_const_pool.Init(AllocChildCodeSpace(1024), 1024);

    const auto raw_cdts = reinterpret_cast<double (*)(double)>(AlignCode4());
    GenConvertDoubleToSingle();

    wrapped_cdts = reinterpret_cast<u32 (*)(u64)>(AlignCode4());
    ABI_PushRegistersAndAdjustStack(ABI_ALL_CALLEE_SAVED, 8, 16);

    // Call
    MOVQ_xmm(XMM0, R(ABI_PARAM1));
    ABI_CallFunction(raw_cdts);
    MOV(32, R(ABI_RETURN), R(RSCRATCH));

    ABI_PopRegistersAndAdjustStack(ABI_ALL_CALLEE_SAVED, 8, 16);
    RET();
  }

  u32 (*wrapped_cdts)(u64);
  Jit64 jit;
};
}  // namespace

TEST(Jit64, ConvertDoubleToSingle)
{
  TestCommonAsmRoutines routines(Core::System::GetInstance());

  for (const u64 input : double_test_values)
  {
    const u32 expected = ConvertToSingle(input);
    const u32 actual = routines.wrapped_cdts(input);

    fmt::print("{:016x} -> {:08x} == {:08x}\n", input, actual, expected);

    EXPECT_EQ(expected, actual);
  }
}
