// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <bit>

#include "Common/CommonTypes.h"
#include "Common/FloatUtils.h"
#include "Common/ScopeGuard.h"
#include "Common/x64ABI.h"
#include "Core/Core.h"
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64Common/Jit64AsmCommon.h"
#include "Core/PowerPC/Jit64Common/Jit64PowerPCState.h"
#include "Core/System.h"

#include "../TestValues.h"

#include <gtest/gtest.h>

namespace
{
class TestFres : public CommonAsmRoutines
{
public:
  explicit TestFres(Core::System& system) : CommonAsmRoutines(jit), jit(system)
  {
    using namespace Gen;

    AllocCodeSpace(4096);
    m_const_pool.Init(AllocChildCodeSpace(1024), 1024);

    const auto raw_fres = reinterpret_cast<double (*)(double)>(AlignCode4());
    GenFres();

    wrapped_fres = reinterpret_cast<u64 (*)(u64, const UReg_FPSCR&)>(AlignCode4());
    ABI_PushRegistersAndAdjustStack(ABI_ALL_CALLEE_SAVED, 8, 16);

    // We know that the only part of PowerPCState that the fres routine accesses is the fpscr.
    // We manufacture a PPCSTATE pointer so it reads from our provided fpscr instead.
    LEA(64, RPPCSTATE, MDisp(ABI_PARAM2, -PPCSTATE_OFF(fpscr)));

    // Call
    MOVQ_xmm(XMM0, R(ABI_PARAM1));
    ABI_CallFunction(raw_fres);
    MOVQ_xmm(R(ABI_RETURN), XMM0);

    ABI_PopRegistersAndAdjustStack(ABI_ALL_CALLEE_SAVED, 8, 16);
    RET();
  }

  u64 (*wrapped_fres)(u64, const UReg_FPSCR&);
  Jit64 jit;
};

}  // namespace

TEST(Jit64, Fres)
{
  Core::DeclareAsCPUThread();
  Common::ScopeGuard cpu_thread_guard([] { Core::UndeclareAsCPUThread(); });

  TestFres test(Core::System::GetInstance());

  // FPSCR with NI set
  const UReg_FPSCR fpscr = UReg_FPSCR(0x00000004);

  for (const u64 ivalue : double_test_values)
  {
    const double dvalue = std::bit_cast<double>(ivalue);

    const u64 expected = std::bit_cast<u64>(Common::ApproximateReciprocal(dvalue));
    const u64 actual = test.wrapped_fres(ivalue, fpscr);

    if (expected != actual)
      fmt::print("{:016x} -> {:016x} == {:016x}\n", ivalue, actual, expected);

    EXPECT_EQ(expected, actual);
  }
}
