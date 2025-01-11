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

  Core::System& system = Core::System::GetInstance();
  TestFres test(system);

  for (const u64 ivalue : double_test_values)
  {
    SCOPED_TRACE(fmt::format("fres input: {:016x}\n", ivalue));

    const double dvalue = std::bit_cast<double>(ivalue);

    const u64 expected = std::bit_cast<u64>(Common::ApproximateReciprocal(dvalue));
    const u64 actual = test.fres(ivalue);

    EXPECT_EQ(expected, actual);

    for (u32 zx = 0; zx < 2; ++zx)
    {
      UReg_FPSCR& fpscr = system.GetPPCState().fpscr;
      fpscr = {};
      fpscr.ZX = zx;

      test.fres(ivalue);

      const bool input_is_zero = (ivalue & (Common::DOUBLE_EXP | Common::DOUBLE_FRAC)) == 0;

      const bool zx_expected = input_is_zero || zx;
      const bool fx_expected = input_is_zero && !zx;

      const u32 fpscr_expected = (zx_expected ? FPSCR_ZX : 0) | (fx_expected ? FPSCR_FX : 0);

      EXPECT_EQ(fpscr_expected, fpscr.Hex);
    }
  }
}
