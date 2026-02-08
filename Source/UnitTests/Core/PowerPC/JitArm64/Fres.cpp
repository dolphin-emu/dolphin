// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <bit>
#include <functional>

#include "Common/Arm64Emitter.h"
#include "Common/CommonTypes.h"
#include "Common/ScopeGuard.h"
#include "Core/Core.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

#include "../TestValues.h"

#include <gtest/gtest.h>

namespace
{
using namespace Arm64Gen;

class TestFres : public JitArm64
{
public:
  explicit TestFres(Core::System& system) : JitArm64(system)
  {
    const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;

    AllocCodeSpace(4096);

    const u8* raw_fres = GetCodePtr();
    GenerateFres();

    fres = std::bit_cast<u64 (*)(u64)>(GetCodePtr());
    MOV(ARM64Reg::X15, ARM64Reg::X30);
    MOV(ARM64Reg::X14, PPC_REG);
    MOVP2R(PPC_REG, &system.GetPPCState());
    MOV(ARM64Reg::X1, ARM64Reg::X0);
    m_float_emit.FMOV(ARM64Reg::D0, ARM64Reg::X0);
    m_float_emit.FRECPE(ARM64Reg::D0, ARM64Reg::D0);
    BL(raw_fres);
    MOV(ARM64Reg::X30, ARM64Reg::X15);
    MOV(PPC_REG, ARM64Reg::X14);
    RET();
  }

  std::function<u64(u64)> fres;
};

}  // namespace

TEST(JitArm64, Fres)
{
  Core::DeclareAsCPUThread();
  Common::ScopeGuard cpu_thread_guard([] { Core::UndeclareAsCPUThread(); });

  TestFres test(Core::System::GetInstance());

  for (const u64 ivalue : double_test_values)
  {
    const double dvalue = std::bit_cast<double>(ivalue);

    const u64 expected = std::bit_cast<u64>(Common::ApproximateReciprocal(dvalue));
    const u64 actual = test.fres(ivalue);

    if (expected != actual)
      fmt::print("{:016x} -> {:016x} == {:016x}\n", ivalue, actual, expected);

    EXPECT_EQ(expected, actual);
  }
}
