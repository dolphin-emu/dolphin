// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// JitPPC64's routines against the C++ they emulate, at each Power ISA level the host implements.

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/FloatUtils.h"
#include "Common/ScopeGuard.h"
#include "Core/Core.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"
#include "Core/PowerPC/JitPPC64/Jit.h"
#include "Core/PowerPC/JitPPC64/JitPPC64Cache.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

#include "../TestValues.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

namespace
{
using namespace PPC64Gen;

class TestRoutines : public JitPPC64
{
public:
  TestRoutines(Core::System& system, bool isa_3_0) : JitPPC64(system)
  {
    m_isa_3_0 = isa_3_0;
    const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;
    AllocCodeSpace(16384);
    auto& ppc_state = system.GetPPCState();

    const u8* raw_fprf_double = GetCodePtr();
    GenerateFPRFRoutine(false);
    const u8* raw_fprf_single = GetCodePtr();
    GenerateFPRFRoutine(true);
    const u8* raw_fres = GetCodePtr();
    GenerateReciprocalEstimateRoutine();
    const u8* raw_frsqrte = GetCodePtr();
    GenerateReciprocalSquareRootEstimateRoutine();

    // Calls a routine with PPC_REG set, the input in R3, and with Power ISA 3.0 in VS0 too, and
    // stores R4 (whether the interpreter is needed) where R4 points.
    const auto wrapper = [&](const u8* routine, bool needs_vs0, bool store_r4) {
      const u8* start = GetCodePtr();
      MFLR(GPR::R0);
      STD(GPR::R0, GPR::SP, STACK_FRAME_LR_SAVE);
      STD(PPC_REG, GPR::SP, -8);
      STD(GPR::R5, GPR::SP, -16);
      MR(GPR::R5, GPR::R4);
      MOVP2R(PPC_REG, &ppc_state);
      if (needs_vs0 && m_isa_3_0)
        MTVSRD(VSR::VS0, GPR::R3);
      BL(routine);
      if (store_r4)
        STD(GPR::R4, GPR::R5, 0);
      LD(GPR::R5, GPR::SP, -16);
      LD(PPC_REG, GPR::SP, -8);
      LD(GPR::R0, GPR::SP, STACK_FRAME_LR_SAVE);
      MTLR(GPR::R0);
      BLR();
      return start;
    };
    m_fprf_double = std::bit_cast<void (*)(u64)>(wrapper(raw_fprf_double, true, false));
    m_fprf_single = std::bit_cast<void (*)(u64)>(wrapper(raw_fprf_single, true, false));
    m_fres = std::bit_cast<u64 (*)(u64, u64*)>(wrapper(raw_fres, true, true));
    m_frsqrte = std::bit_cast<u64 (*)(u64, u64*)>(wrapper(raw_frsqrte, true, true));

    // ConvertDoubleToSingleFTZ of R3.
    m_convert_ftz = std::bit_cast<u64 (*)(u64)>(GetCodePtr());
    ConvertDoubleToSingleFTZ(GPR::R3, GPR::R4, GPR::R5);
    CLRLDI(GPR::R3, GPR::R3, 32);
    BLR();

    EXPECT_FALSE(HasWriteFailed());
    FlushIcacheSection(region, GetWritableCodePtr());
  }

  void (*m_fprf_double)(u64) = nullptr;
  void (*m_fprf_single)(u64) = nullptr;
  u64 (*m_fres)(u64, u64*) = nullptr;
  u64 (*m_frsqrte)(u64, u64*) = nullptr;
  u64 (*m_convert_ftz)(u64) = nullptr;
};

std::vector<bool> ISALevels()
{
  // Power ISA 2.07, and 3.0 if the host implements it.
  std::vector<bool> levels = {false};
  if (cpu_info.bISA_3_0)
    levels.push_back(true);
  return levels;
}

u32 FPRFAfter(PowerPC::PowerPCState& ppc_state, auto f)
{
  ppc_state.fpscr.Hex = 0x12345678;
  f();
  return ppc_state.fpscr.Hex;
}
}  // namespace

TEST(JitPPC64, FPRF)
{
  Core::DeclareAsCPUThread();
  Common::ScopeGuard cpu_thread_guard([] { Core::UndeclareAsCPUThread(); });
  auto& system = Core::System::GetInstance();
  auto& ppc_state = system.GetPPCState();

  for (const bool isa_3_0 : ISALevels())
  {
    TestRoutines test(system, isa_3_0);
    for (const u64 input : double_test_values)
    {
      EXPECT_EQ(
          FPRFAfter(ppc_state, [&] { ppc_state.UpdateFPRFDouble(std::bit_cast<double>(input)); }),
          FPRFAfter(ppc_state, [&] { test.m_fprf_double(input); }))
          << fmt::format("double {:016x}, ISA 3.0: {}", input, isa_3_0);

      // The single routine gets the value rounded to single precision, as a double.
      const float single = std::bit_cast<float>(ConvertToSingle(input));
      const u64 rounded = std::bit_cast<u64>(static_cast<double>(single));
      if (std::isnan(single))
        continue;
      EXPECT_EQ(FPRFAfter(ppc_state, [&] { ppc_state.UpdateFPRFSingle(single); }),
                FPRFAfter(ppc_state, [&] { test.m_fprf_single(rounded); }))
          << fmt::format("single {:016x}, ISA 3.0: {}", rounded, isa_3_0);
    }
  }
}

TEST(JitPPC64, Estimates)
{
  Core::DeclareAsCPUThread();
  Common::ScopeGuard cpu_thread_guard([] { Core::UndeclareAsCPUThread(); });
  auto& system = Core::System::GetInstance();

  for (const bool isa_3_0 : ISALevels())
  {
    TestRoutines test(system, isa_3_0);
    for (const u64 input : double_test_values)
    {
      const double value = std::bit_cast<double>(input);
      const auto label = fmt::format("{:016x}, ISA 3.0: {}", input, isa_3_0);

      // fres leaves zeroes, infinities and NaNs to the interpreter, which sets FPSCR's flags.
      u64 interpret = ~u64{0};
      u64 result = test.m_fres(input, &interpret);
      const bool fres_special = value == 0 || std::isinf(value) || std::isnan(value);
      EXPECT_EQ(fres_special, interpret != 0) << "fres " << label;
      if (!fres_special)
      {
        EXPECT_EQ(std::bit_cast<u64>(Common::ApproximateReciprocal(value)), result)
            << "fres " << label;
      }

      // frsqrte only computes positive normals.
      interpret = ~u64{0};
      result = test.m_frsqrte(input, &interpret);
      const bool frsqrte_special = !(value > 0) || !std::isnormal(value);
      EXPECT_EQ(frsqrte_special, interpret != 0) << "frsqrte " << label;
      if (!frsqrte_special)
      {
        EXPECT_EQ(std::bit_cast<u64>(Common::ApproximateReciprocalSquareRoot(value)), result)
            << "frsqrte " << label;
      }
    }
  }
}

TEST(JitPPC64, ConvertDoubleToSingleFTZ)
{
  auto& system = Core::System::GetInstance();
  TestRoutines test(system, false);
  for (const u64 input : double_test_values)
  {
    EXPECT_EQ(ConvertToSingleFTZ(input), test.m_convert_ftz(input))
        << fmt::format("{:016x}", input);
  }
}

TEST(JitPPC64, LinkRange)
{
  // The branch to the block is the second instruction of a link, so a target exactly 32 MiB
  // before the link itself is 4 bytes out of its range.
  // Only addresses are compared.
  constexpr uintptr_t link_address = 0x7FFF00000000;
  const u8* link = reinterpret_cast<const u8*>(link_address);
  constexpr s64 range = s64{1} << 25;
  const auto at = [](s64 offset) { return reinterpret_cast<const u8*>(link_address + offset); };
  EXPECT_TRUE(JitPPC64BlockCache::CanBranchToBlock(link, at(4 - range)));
  EXPECT_FALSE(JitPPC64BlockCache::CanBranchToBlock(link, at(-range)));
  EXPECT_TRUE(JitPPC64BlockCache::CanBranchToBlock(link, at(4 + range - 4)));
  EXPECT_FALSE(JitPPC64BlockCache::CanBranchToBlock(link, at(4 + range)));
  EXPECT_TRUE(JitPPC64BlockCache::CanBranchToBlock(link, at(4)));
}
