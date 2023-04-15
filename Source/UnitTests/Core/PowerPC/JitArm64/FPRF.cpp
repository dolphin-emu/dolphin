// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <functional>
#include <vector>

#include "Common/Arm64Emitter.h"
#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

#include "../TestValues.h"

#include <gtest/gtest.h>

namespace
{
using namespace Arm64Gen;

class TestFPRF : public JitArm64
{
public:
  explicit TestFPRF(Core::System& system) : JitArm64(system)
  {
    const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;

    AllocCodeSpace(4096);

    const u8* raw_fprf_single = GetCodePtr();
    GenerateFPRF(true);
    const u8* raw_fprf_double = GetCodePtr();
    GenerateFPRF(false);

    auto& ppc_state = system.GetPPCState();

    fprf_single = Common::BitCast<void (*)(u32)>(GetCodePtr());
    MOV(ARM64Reg::X15, ARM64Reg::X30);
    MOV(ARM64Reg::X14, PPC_REG);
    MOVP2R(PPC_REG, &ppc_state);
    BL(raw_fprf_single);
    MOV(ARM64Reg::X30, ARM64Reg::X15);
    MOV(PPC_REG, ARM64Reg::X14);
    RET();

    fprf_double = Common::BitCast<void (*)(u64)>(GetCodePtr());
    MOV(ARM64Reg::X15, ARM64Reg::X30);
    MOV(ARM64Reg::X14, PPC_REG);
    MOVP2R(PPC_REG, &ppc_state);
    BL(raw_fprf_double);
    MOV(ARM64Reg::X30, ARM64Reg::X15);
    MOV(PPC_REG, ARM64Reg::X14);
    RET();
  }

  std::function<void(u32)> fprf_single;
  std::function<void(u64)> fprf_double;
};

}  // namespace

static u32 RunUpdateFPRF(PowerPC::PowerPCState& ppc_state, const std::function<void()>& f)
{
  ppc_state.fpscr.Hex = 0x12345678;
  f();
  return ppc_state.fpscr.Hex;
}

TEST(JitArm64, FPRF)
{
  auto& system = Core::System::GetInstance();
  auto& ppc_state = system.GetPPCState();
  TestFPRF test(system);

  for (const u64 double_input : double_test_values)
  {
    const u32 expected_double = RunUpdateFPRF(
        ppc_state, [&] { ppc_state.UpdateFPRFDouble(Common::BitCast<double>(double_input)); });
    const u32 actual_double = RunUpdateFPRF(ppc_state, [&] { test.fprf_double(double_input); });
    EXPECT_EQ(expected_double, actual_double);

    const u32 single_input = ConvertToSingle(double_input);

    const u32 expected_single = RunUpdateFPRF(
        ppc_state, [&] { ppc_state.UpdateFPRFSingle(Common::BitCast<float>(single_input)); });
    const u32 actual_single = RunUpdateFPRF(ppc_state, [&] { test.fprf_single(single_input); });
    EXPECT_EQ(expected_single, actual_single);
  }
}
