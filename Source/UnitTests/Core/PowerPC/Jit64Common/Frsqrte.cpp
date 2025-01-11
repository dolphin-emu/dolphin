// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <bit>
#include <cstring>

#include "Common/CommonTypes.h"
#include "Common/FloatUtils.h"
#include "Common/ScopeGuard.h"
#include "Common/x64ABI.h"
#include "Core/Core.h"
#include "Core/PowerPC/Gekko.h"
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

    const auto raw_frsqrte = reinterpret_cast<double (*)(double)>(AlignCode4());
    GenFrsqrte();

    wrapped_frsqrte = reinterpret_cast<u64 (*)(u64, UReg_FPSCR&)>(AlignCode4());
    ABI_PushRegistersAndAdjustStack(ABI_ALL_CALLEE_SAVED, 8, 16);

    // We know the frsqrte implementation only accesses the fpscr. We manufacture a
    // PPCSTATE pointer so we read/write to our provided fpscr argument instead.
    XOR(32, R(RPPCSTATE), R(RPPCSTATE));
    LEA(64, RSCRATCH, PPCSTATE(fpscr));
    SUB(64, R(ABI_PARAM2), R(RSCRATCH));
    MOV(64, R(RPPCSTATE), R(ABI_PARAM2));

    // Call
    MOVQ_xmm(XMM0, R(ABI_PARAM1));
    ABI_CallFunction(raw_frsqrte);
    MOVQ_xmm(R(ABI_RETURN), XMM0);

    ABI_PopRegistersAndAdjustStack(ABI_ALL_CALLEE_SAVED, 8, 16);
    RET();
  }

  u64 (*wrapped_frsqrte)(u64, UReg_FPSCR&);
  Jit64 jit;
};
}  // namespace

TEST(Jit64, Frsqrte)
{
  Core::DeclareAsCPUThread();
  Common::ScopeGuard cpu_thread_guard([] { Core::UndeclareAsCPUThread(); });

  Core::System& system = Core::System::GetInstance();
  TestCommonAsmRoutines routines(system);

  UReg_FPSCR fpscr;

  for (const u64 ivalue : double_test_values)
  {
    SCOPED_TRACE(fmt::format("frsqrte input: {:016x}\n", ivalue));

    double dvalue = std::bit_cast<double>(ivalue);

    u64 expected = std::bit_cast<u64>(Common::ApproximateReciprocalSquareRoot(dvalue));

    u64 actual = routines.wrapped_frsqrte(ivalue, fpscr);

    EXPECT_EQ(expected, actual);

    for (u32 zx = 0; zx < 2; ++zx)
    {
      for (u32 vxsqrt = 0; vxsqrt < 2; ++vxsqrt)
      {
        fpscr = {};
        fpscr.ZX = zx;
        fpscr.VXSQRT = vxsqrt;

        routines.wrapped_frsqrte(ivalue, fpscr);

        const u32 value_class = Common::ClassifyDouble(dvalue);

        const bool input_is_zero =
            value_class == Common::PPC_FPCLASS_NZ || value_class == Common::PPC_FPCLASS_PZ;
        const bool input_is_negative = value_class == Common::PPC_FPCLASS_NN ||
                                       value_class == Common::PPC_FPCLASS_ND ||
                                       value_class == Common::PPC_FPCLASS_NINF;

        const bool zx_expected = input_is_zero || zx;
        const bool vxsqrt_expected = input_is_negative || vxsqrt;
        const bool fx_expected = (input_is_zero && !zx) || (input_is_negative && !vxsqrt);

        const u32 fpscr_expected = (zx_expected ? FPSCR_ZX : 0) |
                                   (vxsqrt_expected ? FPSCR_VXSQRT : 0) |
                                   (fx_expected ? FPSCR_FX : 0);

        EXPECT_EQ(fpscr_expected, fpscr.Hex);
      }
    }
  }
}
