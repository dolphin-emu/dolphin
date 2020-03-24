// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>
#include <tuple>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/x64ABI.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64Common/Jit64AsmCommon.h"
#include "Core/PowerPC/Jit64Common/Jit64PowerPCState.h"

#include <gtest/gtest.h>

namespace
{
class TestCommonAsmRoutines : public CommonAsmRoutines
{
public:
  TestCommonAsmRoutines() : CommonAsmRoutines(jit)
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
  TestCommonAsmRoutines routines;

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
    const u32 actual = routines.wrapped_cdts(input);

    printf("%016llx -> %08x == %08x\n", input, actual, expected);

    EXPECT_EQ(expected, actual);
  }
}
