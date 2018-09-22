// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>
#include <vector>

#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Common/FloatUtils.h"
#include "Common/x64ABI.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64Common/Jit64AsmCommon.h"
#include "Core/PowerPC/Jit64Common/Jit64PowerPCState.h"

#include <gtest/gtest.h>

class TestCommonAsmRoutines : public CommonAsmRoutines
{
public:
  TestCommonAsmRoutines() : CommonAsmRoutines(jit)
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

TEST(Jit64, Frsqrte)
{
  TestCommonAsmRoutines routines;

  const std::vector<u64> special_values{
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
  };

  UReg_FPSCR fpscr;

  for (u64 ivalue : special_values)
  {
    double dvalue = Common::BitCast<double>(ivalue);

    u64 expected = Common::BitCast<u64>(Common::ApproximateReciprocalSquareRoot(dvalue));

    u64 actual = routines.wrapped_frsqrte(ivalue, fpscr);

    printf("%016llx -> %016llx == %016llx\n", ivalue, actual, expected);

    EXPECT_EQ(expected, actual);
  }
}
