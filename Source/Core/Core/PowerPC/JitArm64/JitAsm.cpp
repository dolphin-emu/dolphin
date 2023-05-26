// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitArm64/Jit.h"

#include <limits>

#include "Common/Arm64Emitter.h"
#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/FloatUtils.h"
#include "Common/JitRegister.h"
#include "Common/MathUtil.h"

#include "Core/Config/MainSettings.h"
#include "Core/CoreTiming.h"
#include "Core/HW/CPU.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/JitCommon/JitAsmCommon.h"
#include "Core/PowerPC/JitCommon/JitCache.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

using namespace Arm64Gen;

void JitArm64::GenerateAsm()
{
  const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;

  const bool enable_debugging = Config::Get(Config::MAIN_ENABLE_DEBUGGING);

  // This value is all of the callee saved registers that we are required to save.
  // According to the AACPS64 we need to save R19 ~ R30 and Q8 ~ Q15.
  const u32 ALL_CALLEE_SAVED = 0x7FF80000;
  const u32 ALL_CALLEE_SAVED_FPR = 0x0000FF00;
  BitSet32 regs_to_save(ALL_CALLEE_SAVED);
  BitSet32 regs_to_save_fpr(ALL_CALLEE_SAVED_FPR);
  enter_code = GetCodePtr();

  ABI_PushRegisters(regs_to_save);
  m_float_emit.ABI_PushRegisters(regs_to_save_fpr, ARM64Reg::X30);

  MOVP2R(PPC_REG, &m_ppc_state);

  // Store the stack pointer, so we can reset it if the BLR optimization fails.
  ADD(ARM64Reg::X0, ARM64Reg::SP, 0);
  STR(IndexType::Unsigned, ARM64Reg::X0, PPC_REG, PPCSTATE_OFF(stored_stack_pointer));

  // Push {nullptr; -1} as invalid destination on the stack.
  MOVI2R(ARM64Reg::X0, 0xFFFFFFFF);
  STP(IndexType::Pre, ARM64Reg::ZR, ARM64Reg::X0, ARM64Reg::SP, -16);

  // The PC will be loaded into DISPATCHER_PC after the call to CoreTiming::Advance().
  // Advance() does an exception check so we don't know what PC to use until afterwards.
  FixupBranch to_start_of_timing_slice = B();

  // If we align the dispatcher to a page then we can load its location with one ADRP instruction
  // do
  // {
  //   CoreTiming::Advance();  // <-- Checks for exceptions (changes PC)
  //   DISPATCHER_PC = PC;
  //   do
  //   {
  // dispatcher_no_check:
  //     ExecuteBlock(JitBase::Dispatch());
  // dispatcher:
  //   } while (m_ppc_state.downcount > 0);
  // do_timing:
  //   NPC = PC = DISPATCHER_PC;
  // } while (CPU::GetState() == CPU::State::Running);
  AlignCodePage();
  dispatcher = GetCodePtr();
  WARN_LOG_FMT(DYNA_REC, "Dispatcher is {}", fmt::ptr(dispatcher));

  // Downcount Check
  // The result of slice decrementation should be in flags if somebody jumped here
  FixupBranch bail = B(CC_LE);

  dispatcher_no_timing_check = GetCodePtr();

  auto& cpu = m_system.GetCPU();

  FixupBranch debug_exit;
  if (enable_debugging)
  {
    LDR(IndexType::Unsigned, ARM64Reg::W0, ARM64Reg::X0,
        MOVPage2R(ARM64Reg::X0, cpu.GetStatePtr()));
    debug_exit = CBNZ(ARM64Reg::W0);
  }

  dispatcher_no_check = GetCodePtr();

  bool assembly_dispatcher = true;

  auto& memory = m_system.GetMemory();

  if (assembly_dispatcher)
  {
    // set the mem_base based on MSR flags
    LDR(IndexType::Unsigned, ARM64Reg::W28, PPC_REG, PPCSTATE_OFF(msr));
    FixupBranch physmem = TBNZ(ARM64Reg::W28, 31 - 27);
    MOVP2R(MEM_REG,
           jo.fastmem_arena ? memory.GetPhysicalBase() : memory.GetPhysicalPageMappingsBase());
    FixupBranch membaseend = B();
    SetJumpTarget(physmem);
    MOVP2R(MEM_REG,
           jo.fastmem_arena ? memory.GetLogicalBase() : memory.GetLogicalPageMappingsBase());
    SetJumpTarget(membaseend);

    if (GetBlockCache()->GetFastBlockMap())
    {
      // Check if there is a block
      ARM64Reg pc_masked = ARM64Reg::X25;
      ARM64Reg cache_base = ARM64Reg::X27;
      ARM64Reg block = ARM64Reg::X30;
      LSL(pc_masked, DISPATCHER_PC, 1);
      MOVP2R(cache_base, GetBlockCache()->GetFastBlockMap());
      LDR(block, cache_base, pc_masked);
      FixupBranch not_found = CBZ(block);

      // b.msrBits != msr
      ARM64Reg msr = ARM64Reg::W25;
      ARM64Reg msr2 = ARM64Reg::W24;
      LDR(IndexType::Unsigned, msr, PPC_REG, PPCSTATE_OFF(msr));
      AND(msr, msr, LogicalImm(JitBaseBlockCache::JIT_CACHE_MSR_MASK, 32));
      LDR(IndexType::Unsigned, msr2, block, offsetof(JitBlockData, msrBits));
      CMP(msr, msr2);

      FixupBranch msr_missmatch = B(CC_NEQ);

      // return blocks[block_num].normalEntry;
      LDR(IndexType::Unsigned, block, block, offsetof(JitBlockData, normalEntry));
      BR(block);
      SetJumpTarget(not_found);
      SetJumpTarget(msr_missmatch);
    }
    else
    {
      // iCache[(address >> 2) & iCache_Mask];
      ARM64Reg pc_masked = ARM64Reg::W25;
      ARM64Reg cache_base = ARM64Reg::X27;
      ARM64Reg block = ARM64Reg::X30;
      ORR(pc_masked, ARM64Reg::WZR,
          LogicalImm(JitBaseBlockCache::FAST_BLOCK_MAP_FALLBACK_MASK << 3, 32));
      AND(pc_masked, pc_masked, DISPATCHER_PC, ArithOption(DISPATCHER_PC, ShiftType::LSL, 1));
      MOVP2R(cache_base, GetBlockCache()->GetFastBlockMapFallback());
      LDR(block, cache_base, EncodeRegTo64(pc_masked));
      FixupBranch not_found = CBZ(block);

      // b.effectiveAddress != addr || b.msrBits != msr
      ARM64Reg pc_and_msr = ARM64Reg::W25;
      ARM64Reg pc_and_msr2 = ARM64Reg::W24;
      LDR(IndexType::Unsigned, pc_and_msr, block, offsetof(JitBlockData, effectiveAddress));
      CMP(pc_and_msr, DISPATCHER_PC);
      FixupBranch pc_missmatch = B(CC_NEQ);

      LDR(IndexType::Unsigned, pc_and_msr2, PPC_REG, PPCSTATE_OFF(msr));
      AND(pc_and_msr2, pc_and_msr2, LogicalImm(JitBaseBlockCache::JIT_CACHE_MSR_MASK, 32));
      LDR(IndexType::Unsigned, pc_and_msr, block, offsetof(JitBlockData, msrBits));
      CMP(pc_and_msr, pc_and_msr2);

      FixupBranch msr_missmatch = B(CC_NEQ);

      // return blocks[block_num].normalEntry;
      LDR(IndexType::Unsigned, block, block, offsetof(JitBlockData, normalEntry));
      BR(block);
      SetJumpTarget(not_found);
      SetJumpTarget(pc_missmatch);
      SetJumpTarget(msr_missmatch);
    }
  }

  // Call C version of Dispatch().
  STR(IndexType::Unsigned, DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(pc));
  MOVP2R(ARM64Reg::X8, reinterpret_cast<void*>(&JitBase::Dispatch));
  MOVP2R(ARM64Reg::X0, this);
  BLR(ARM64Reg::X8);

  FixupBranch no_block_available = CBZ(ARM64Reg::X0);

  // set the mem_base based on MSR flags and jump to next block.
  LDR(IndexType::Unsigned, ARM64Reg::W28, PPC_REG, PPCSTATE_OFF(msr));
  FixupBranch physmem = TBNZ(ARM64Reg::W28, 31 - 27);
  MOVP2R(MEM_REG,
         jo.fastmem_arena ? memory.GetPhysicalBase() : memory.GetPhysicalPageMappingsBase());
  BR(ARM64Reg::X0);
  SetJumpTarget(physmem);
  MOVP2R(MEM_REG, jo.fastmem_arena ? memory.GetLogicalBase() : memory.GetLogicalPageMappingsBase());
  BR(ARM64Reg::X0);

  // Call JIT
  SetJumpTarget(no_block_available);
  ResetStack();
  MOVP2R(ARM64Reg::X0, this);
  MOV(ARM64Reg::W1, DISPATCHER_PC);
  MOVP2R(ARM64Reg::X8, reinterpret_cast<void*>(&JitTrampoline));
  BLR(ARM64Reg::X8);
  LDR(IndexType::Unsigned, DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(pc));
  B(dispatcher_no_check);

  SetJumpTarget(bail);
  do_timing = GetCodePtr();
  // Write the current PC out to PPCSTATE
  static_assert(PPCSTATE_OFF(pc) <= 252);
  static_assert(PPCSTATE_OFF(pc) + 4 == PPCSTATE_OFF(npc));
  STP(IndexType::Signed, DISPATCHER_PC, DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(pc));

  // Check the state pointer to see if we are exiting
  // Gets checked on at the end of every slice
  LDR(IndexType::Unsigned, ARM64Reg::W0, ARM64Reg::X0, MOVPage2R(ARM64Reg::X0, cpu.GetStatePtr()));
  FixupBranch exit = CBNZ(ARM64Reg::W0);

  SetJumpTarget(to_start_of_timing_slice);
  MOVP2R(ARM64Reg::X8, &CoreTiming::GlobalAdvance);
  BLR(ARM64Reg::X8);

  // Load the PC back into DISPATCHER_PC (the exception handler might have changed it)
  LDR(IndexType::Unsigned, DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(pc));

  // We can safely assume that downcount >= 1
  B(dispatcher_no_check);

  dispatcher_exit = GetCodePtr();
  SetJumpTarget(exit);
  if (enable_debugging)
    SetJumpTarget(debug_exit);

  // Reset the stack pointer, since the BLR optimization may have pushed things onto the stack
  // without popping them.
  LDR(IndexType::Unsigned, ARM64Reg::X0, PPC_REG, PPCSTATE_OFF(stored_stack_pointer));
  ADD(ARM64Reg::SP, ARM64Reg::X0, 0);

  m_float_emit.ABI_PopRegisters(regs_to_save_fpr, ARM64Reg::X30);
  ABI_PopRegisters(regs_to_save);
  RET(ARM64Reg::X30);

  Common::JitRegister::Register(enter_code, GetCodePtr(), "JIT_Dispatcher");

  GenerateCommonAsm();

  FlushIcache();
}

void JitArm64::GenerateCommonAsm()
{
  GetAsmRoutines()->fres = GetCodePtr();
  GenerateFres();
  Common::JitRegister::Register(GetAsmRoutines()->fres, GetCodePtr(), "JIT_fres");

  GetAsmRoutines()->frsqrte = GetCodePtr();
  GenerateFrsqrte();
  Common::JitRegister::Register(GetAsmRoutines()->frsqrte, GetCodePtr(), "JIT_frsqrte");

  GetAsmRoutines()->cdts = GetCodePtr();
  GenerateConvertDoubleToSingle();
  Common::JitRegister::Register(GetAsmRoutines()->cdts, GetCodePtr(), "JIT_cdts");

  GetAsmRoutines()->cstd = GetCodePtr();
  GenerateConvertSingleToDouble();
  Common::JitRegister::Register(GetAsmRoutines()->cstd, GetCodePtr(), "JIT_cstd");

  GetAsmRoutines()->fprf_single = GetCodePtr();
  GenerateFPRF(true);
  GetAsmRoutines()->fprf_double = GetCodePtr();
  GenerateFPRF(false);
  Common::JitRegister::Register(GetAsmRoutines()->fprf_single, GetCodePtr(), "JIT_FPRF");

  GenerateQuantizedLoads();
  GenerateQuantizedStores();
}

// Input: X1 contains input, and D0 contains result of running the input through AArch64 FRECPE.
// Output in X0 and memory (PPCState). Clobbers X0-X4 and flags.
void JitArm64::GenerateFres()
{
  // The idea behind this implementation: AArch64's frecpe instruction calculates the exponent and
  // sign the same way as PowerPC's fresx does. For the special inputs zero, NaN and infinity,
  // even the mantissa matches. But the mantissa does not match for most other inputs, so in the
  // normal case we calculate the mantissa using the table-based algorithm from the interpreter.

  UBFX(ARM64Reg::X2, ARM64Reg::X1, 52, 11);  // Grab the exponent
  m_float_emit.FMOV(ARM64Reg::X0, ARM64Reg::D0);
  AND(ARM64Reg::X3, ARM64Reg::X1, LogicalImm(Common::DOUBLE_SIGN, 64));
  CMP(ARM64Reg::X2, 895);
  FixupBranch small_exponent = B(CCFlags::CC_LO);

  MOVI2R(ARM64Reg::X4, 1148LL);
  CMP(ARM64Reg::X2, ARM64Reg::X4);
  FixupBranch large_exponent = B(CCFlags::CC_HI);

  UBFX(ARM64Reg::X2, ARM64Reg::X1, 47, 5);  // Grab upper part of mantissa
  MOVP2R(ARM64Reg::X3, &Common::fres_expected);
  ADD(ARM64Reg::X2, ARM64Reg::X3, ARM64Reg::X2, ArithOption(ARM64Reg::X2, ShiftType::LSL, 3));
  LDP(IndexType::Signed, ARM64Reg::W2, ARM64Reg::W3, ARM64Reg::X2, 0);
  UBFX(ARM64Reg::X1, ARM64Reg::X1, 37, 10);  // Grab lower part of mantissa
  MOVI2R(ARM64Reg::W4, 1);
  AND(ARM64Reg::X0, ARM64Reg::X0, LogicalImm(Common::DOUBLE_SIGN | Common::DOUBLE_EXP, 64));
  MADD(ARM64Reg::W1, ARM64Reg::W3, ARM64Reg::W1, ARM64Reg::W4);
  SUB(ARM64Reg::W1, ARM64Reg::W2, ARM64Reg::W1, ArithOption(ARM64Reg::W1, ShiftType::LSR, 1));
  ORR(ARM64Reg::X0, ARM64Reg::X0, ARM64Reg::X1, ArithOption(ARM64Reg::X1, ShiftType::LSL, 29));
  RET();

  SetJumpTarget(small_exponent);
  TST(ARM64Reg::X1, LogicalImm(Common::DOUBLE_EXP | Common::DOUBLE_FRAC, 64));
  FixupBranch zero = B(CCFlags::CC_EQ);
  MOVI2R(ARM64Reg::X4,
         Common::BitCast<u64>(static_cast<double>(std::numeric_limits<float>::max())));
  ORR(ARM64Reg::X0, ARM64Reg::X3, ARM64Reg::X4);
  RET();

  SetJumpTarget(zero);
  LDR(IndexType::Unsigned, ARM64Reg::W4, PPC_REG, PPCSTATE_OFF(fpscr));
  FixupBranch skip_set_zx = TBNZ(ARM64Reg::W4, 26);
  ORRI2R(ARM64Reg::W4, ARM64Reg::W4, FPSCR_FX | FPSCR_ZX, ARM64Reg::W2);
  STR(IndexType::Unsigned, ARM64Reg::W4, PPC_REG, PPCSTATE_OFF(fpscr));
  SetJumpTarget(skip_set_zx);
  RET();

  SetJumpTarget(large_exponent);
  MOVI2R(ARM64Reg::X4, 0x7FF);
  CMP(ARM64Reg::X2, ARM64Reg::X4);
  CSEL(ARM64Reg::X0, ARM64Reg::X0, ARM64Reg::X3, CCFlags::CC_EQ);
  RET();
}

// Input: X1 contains input, and D0 contains result of running the input through AArch64 FRSQRTE.
// Output in X0 and memory (PPCState). Clobbers X0-X4 and flags.
void JitArm64::GenerateFrsqrte()
{
  // The idea behind this implementation: AArch64's frsqrte instruction calculates the exponent and
  // sign the same way as PowerPC's frsqrtex does. For the special inputs zero, negative, NaN and
  // inf, even the mantissa matches. But the mantissa does not match for most other inputs, so in
  // the normal case we calculate the mantissa using the table-based algorithm from the interpreter.

  m_float_emit.FMOV(ARM64Reg::X0, ARM64Reg::D0);
  TST(ARM64Reg::X1, LogicalImm(Common::DOUBLE_EXP | Common::DOUBLE_FRAC, 64));
  FixupBranch zero = B(CCFlags::CC_EQ);
  AND(ARM64Reg::X2, ARM64Reg::X1, LogicalImm(Common::DOUBLE_EXP, 64));
  MOVI2R(ARM64Reg::X3, Common::DOUBLE_EXP);
  CMP(ARM64Reg::X2, ARM64Reg::X3);
  FixupBranch nan_or_inf = B(CCFlags::CC_EQ);
  FixupBranch negative = TBNZ(ARM64Reg::X1, 63);
  AND(ARM64Reg::X3, ARM64Reg::X1, LogicalImm(Common::DOUBLE_FRAC, 64));
  FixupBranch normal = CBNZ(ARM64Reg::X2);

  // "Normalize" denormal values
  CLZ(ARM64Reg::X3, ARM64Reg::X3);
  SUB(ARM64Reg::X4, ARM64Reg::X3, 11);
  MOVI2R(ARM64Reg::X2, 0x00C0'0000'0000'0000);
  LSLV(ARM64Reg::X4, ARM64Reg::X1, ARM64Reg::X4);
  SUB(ARM64Reg::X2, ARM64Reg::X2, ARM64Reg::X3, ArithOption(ARM64Reg::X3, ShiftType::LSL, 52));
  AND(ARM64Reg::X3, ARM64Reg::X4, LogicalImm(Common::DOUBLE_FRAC - 1, 64));

  SetJumpTarget(normal);
  LSR(ARM64Reg::X2, ARM64Reg::X2, 48);
  AND(ARM64Reg::X2, ARM64Reg::X2, LogicalImm(0x10, 64));
  MOVP2R(ARM64Reg::X1, &Common::frsqrte_expected);
  ORR(ARM64Reg::X2, ARM64Reg::X2, ARM64Reg::X3, ArithOption(ARM64Reg::X3, ShiftType::LSR, 48));
  EOR(ARM64Reg::X2, ARM64Reg::X2, LogicalImm(0x10, 64));
  ADD(ARM64Reg::X2, ARM64Reg::X1, ARM64Reg::X2, ArithOption(ARM64Reg::X2, ShiftType::LSL, 3));
  LDP(IndexType::Signed, ARM64Reg::W1, ARM64Reg::W2, ARM64Reg::X2, 0);
  UBFX(ARM64Reg::X3, ARM64Reg::X3, 37, 11);
  AND(ARM64Reg::X0, ARM64Reg::X0, LogicalImm(Common::DOUBLE_SIGN | Common::DOUBLE_EXP, 64));
  MSUB(ARM64Reg::W3, ARM64Reg::W3, ARM64Reg::W2, ARM64Reg::W1);
  ORR(ARM64Reg::X0, ARM64Reg::X0, ARM64Reg::X3, ArithOption(ARM64Reg::X3, ShiftType::LSL, 26));
  RET();

  SetJumpTarget(zero);
  LDR(IndexType::Unsigned, ARM64Reg::W4, PPC_REG, PPCSTATE_OFF(fpscr));
  FixupBranch skip_set_zx = TBNZ(ARM64Reg::W4, 26);
  ORRI2R(ARM64Reg::W4, ARM64Reg::W4, FPSCR_FX | FPSCR_ZX, ARM64Reg::W2);
  STR(IndexType::Unsigned, ARM64Reg::W4, PPC_REG, PPCSTATE_OFF(fpscr));
  SetJumpTarget(skip_set_zx);
  RET();

  SetJumpTarget(nan_or_inf);
  MOVI2R(ARM64Reg::X3, Common::BitCast<u64>(-std::numeric_limits<double>::infinity()));
  CMP(ARM64Reg::X1, ARM64Reg::X3);
  FixupBranch nan_or_positive_inf = B(CCFlags::CC_NEQ);

  SetJumpTarget(negative);
  LDR(IndexType::Unsigned, ARM64Reg::W4, PPC_REG, PPCSTATE_OFF(fpscr));
  FixupBranch skip_set_vxsqrt = TBNZ(ARM64Reg::W4, 9);
  ORRI2R(ARM64Reg::W4, ARM64Reg::W4, FPSCR_FX | FPSCR_VXSQRT, ARM64Reg::W2);
  STR(IndexType::Unsigned, ARM64Reg::W4, PPC_REG, PPCSTATE_OFF(fpscr));
  SetJumpTarget(skip_set_vxsqrt);
  SetJumpTarget(nan_or_positive_inf);
  RET();
}

// Input in X0, output in W1, clobbers X0-X3 and flags.
void JitArm64::GenerateConvertDoubleToSingle()
{
  UBFX(ARM64Reg::X2, ARM64Reg::X0, 52, 11);
  LSR(ARM64Reg::X1, ARM64Reg::X0, 32);
  SUB(ARM64Reg::W3, ARM64Reg::W2, 874);
  CMP(ARM64Reg::W3, 896 - 874);
  FixupBranch denormal = B(CCFlags::CC_LS);

  BFXIL(ARM64Reg::X1, ARM64Reg::X0, 29, 30);
  RET();

  SetJumpTarget(denormal);
  LSR(ARM64Reg::X3, ARM64Reg::X0, 21);
  MOVZ(ARM64Reg::X0, 905);
  ORR(ARM64Reg::W3, ARM64Reg::W3, LogicalImm(0x80000000, 32));
  SUB(ARM64Reg::W2, ARM64Reg::W0, ARM64Reg::W2);
  LSRV(ARM64Reg::W2, ARM64Reg::W3, ARM64Reg::W2);
  AND(ARM64Reg::X3, ARM64Reg::X1, LogicalImm(0x80000000, 64));
  ORR(ARM64Reg::X1, ARM64Reg::X3, ARM64Reg::X2);
  RET();
}

// Input in W0, output in X1, clobbers X0-X4 and flags.
void JitArm64::GenerateConvertSingleToDouble()
{
  UBFX(ARM64Reg::W1, ARM64Reg::W0, 23, 8);
  FixupBranch normal_or_nan = CBNZ(ARM64Reg::W1);

  AND(ARM64Reg::W1, ARM64Reg::W0, LogicalImm(0x007fffff, 32));
  FixupBranch denormal = CBNZ(ARM64Reg::W1);

  // Zero
  LSL(ARM64Reg::X1, ARM64Reg::X0, 32);
  RET();

  SetJumpTarget(denormal);
  AND(ARM64Reg::W2, ARM64Reg::W0, LogicalImm(0x80000000, 32));
  CLZ(ARM64Reg::X3, ARM64Reg::X1);
  LSL(ARM64Reg::X2, ARM64Reg::X2, 32);
  ORR(ARM64Reg::X4, ARM64Reg::X3, LogicalImm(0xffffffffffffffc0, 64));
  SUB(ARM64Reg::X2, ARM64Reg::X2, ARM64Reg::X3, ArithOption(ARM64Reg::X3, ShiftType::LSL, 52));
  ADD(ARM64Reg::X3, ARM64Reg::X4, 23);
  LSLV(ARM64Reg::X1, ARM64Reg::X1, ARM64Reg::X3);
  BFI(ARM64Reg::X2, ARM64Reg::X1, 30, 22);
  MOVI2R(ARM64Reg::X1, 0x3a90000000000000);
  ADD(ARM64Reg::X1, ARM64Reg::X2, ARM64Reg::X1);
  RET();

  SetJumpTarget(normal_or_nan);
  AND(ARM64Reg::W2, ARM64Reg::W0, LogicalImm(0x40000000, 32));
  CMP(ARM64Reg::W1, 0xff);
  CSET(ARM64Reg::W4, CCFlags::CC_NEQ);
  AND(ARM64Reg::W3, ARM64Reg::W0, LogicalImm(0xc0000000, 32));
  EOR(ARM64Reg::W2, ARM64Reg::W4, ARM64Reg::W2, ArithOption(ARM64Reg::W2, ShiftType::LSR, 30));
  MOVI2R(ARM64Reg::X1, 0x3800000000000000);
  AND(ARM64Reg::W4, ARM64Reg::W0, LogicalImm(0x3fffffff, 32));
  LSL(ARM64Reg::X3, ARM64Reg::X3, 32);
  CMP(ARM64Reg::W2, 0);
  CSEL(ARM64Reg::X1, ARM64Reg::X1, ARM64Reg::ZR, CCFlags::CC_NEQ);
  BFI(ARM64Reg::X3, ARM64Reg::X4, 29, 30);
  ORR(ARM64Reg::X1, ARM64Reg::X3, ARM64Reg::X1);
  RET();
}

// Input in X0. Outputs to memory (PPCState). Clobbers X0-X4 and flags.
void JitArm64::GenerateFPRF(bool single)
{
  const auto reg_encoder = single ? EncodeRegTo32 : EncodeRegTo64;

  const ARM64Reg input_reg = reg_encoder(ARM64Reg::W0);
  const ARM64Reg temp_reg = reg_encoder(ARM64Reg::W1);
  const ARM64Reg exp_reg = reg_encoder(ARM64Reg::W2);

  constexpr ARM64Reg fprf_reg = ARM64Reg::W3;
  constexpr ARM64Reg fpscr_reg = ARM64Reg::W4;

  const int input_size = single ? 32 : 64;
  const u64 input_exp_mask = single ? Common::FLOAT_EXP : Common::DOUBLE_EXP;
  const u64 input_frac_mask = single ? Common::FLOAT_FRAC : Common::DOUBLE_FRAC;
  constexpr u32 output_sign_mask = 0xC;

  // This code is duplicated for the most common cases for performance.
  // For the less common cases, we branch to an existing copy of this code.
  auto emit_write_fprf_and_ret = [&] {
    BFI(fpscr_reg, fprf_reg, FPRF_SHIFT, FPRF_WIDTH);
    STR(IndexType::Unsigned, fpscr_reg, PPC_REG, PPCSTATE_OFF(fpscr));
    RET();
  };

  // First of all, start the load of the old FPSCR value, in case it takes a while
  LDR(IndexType::Unsigned, fpscr_reg, PPC_REG, PPCSTATE_OFF(fpscr));

  // Most branches handle the sign in the same way. Perform that handling before branching
  MOVI2R(ARM64Reg::W3, Common::PPC_FPCLASS_PN);
  MOVI2R(ARM64Reg::W1, Common::PPC_FPCLASS_NN);
  CMP(input_reg, 0);  // Grab sign bit (conveniently the same bit for floats as for integers)
  CSEL(fprf_reg, ARM64Reg::W1, ARM64Reg::W3, CCFlags::CC_LT);

  AND(exp_reg, input_reg, LogicalImm(input_exp_mask, input_size));  // Grab exponent
  FixupBranch zero_or_denormal = CBZ(exp_reg);

  // exp != 0
  MOVI2R(temp_reg, input_exp_mask);
  CMP(exp_reg, temp_reg);
  FixupBranch nan_or_inf = B(CCFlags::CC_EQ);

  // exp != 0 && exp != EXP_MASK
  emit_write_fprf_and_ret();

  // exp == 0
  SetJumpTarget(zero_or_denormal);
  TST(input_reg, LogicalImm(input_frac_mask, input_size));
  FixupBranch denormal = B(CCFlags::CC_NEQ);

  // exp == 0 && frac == 0
  LSR(ARM64Reg::W1, fprf_reg, 3);
  MOVI2R(fprf_reg, Common::PPC_FPCLASS_PZ & ~output_sign_mask);
  BFI(fprf_reg, ARM64Reg::W1, 4, 1);
  const u8* write_fprf_and_ret = GetCodePtr();
  emit_write_fprf_and_ret();

  // exp == 0 && frac != 0
  SetJumpTarget(denormal);
  ORR(fprf_reg, fprf_reg, LogicalImm(Common::PPC_FPCLASS_PD & ~output_sign_mask, 32));
  B(write_fprf_and_ret);

  // exp == EXP_MASK
  SetJumpTarget(nan_or_inf);
  MOVI2R(ARM64Reg::W2, Common::PPC_FPCLASS_QNAN);
  ORR(ARM64Reg::W1, fprf_reg, LogicalImm(Common::PPC_FPCLASS_PINF & ~output_sign_mask, 32));
  TST(input_reg, LogicalImm(input_frac_mask, input_size));
  CSEL(fprf_reg, ARM64Reg::W1, ARM64Reg::W2, CCFlags::CC_EQ);
  B(write_fprf_and_ret);
}

void JitArm64::GenerateQuantizedLoads()
{
  // X0 is the address
  // X1 is the scale
  // X2 is a temporary
  // X3 is a temporary (used in EmitBackpatchRoutine)
  // X30 is LR
  // Q0 is the return
  // Q1 is a temporary
  ARM64Reg addr_reg = ARM64Reg::X0;
  ARM64Reg scale_reg = ARM64Reg::X1;
  BitSet32 gprs_to_push = CALLER_SAVED_GPRS & ~BitSet32{2, 3};
  if (!jo.memcheck)
    gprs_to_push &= ~BitSet32{0};
  BitSet32 fprs_to_push = BitSet32(0xFFFFFFFF) & ~BitSet32{0, 1};
  ARM64FloatEmitter float_emit(this);

  const u8* start = GetCodePtr();
  const u8* loadPairedIllegal = GetCodePtr();
  BRK(100);
  const u8* loadPairedFloatTwo = GetCodePtr();
  {
    constexpr u32 flags = BackPatchInfo::FLAG_LOAD | BackPatchInfo::FLAG_FLOAT |
                          BackPatchInfo::FLAG_PAIR | BackPatchInfo::FLAG_SIZE_32;

    EmitBackpatchRoutine(flags, MemAccessMode::Auto, ARM64Reg::D0, addr_reg,
                         gprs_to_push & ~BitSet32{1}, fprs_to_push, true);

    RET(ARM64Reg::X30);
  }
  const u8* loadPairedU8Two = GetCodePtr();
  {
    constexpr u32 flags = BackPatchInfo::FLAG_LOAD | BackPatchInfo::FLAG_FLOAT |
                          BackPatchInfo::FLAG_PAIR | BackPatchInfo::FLAG_SIZE_8;

    EmitBackpatchRoutine(flags, MemAccessMode::Auto, ARM64Reg::D0, addr_reg, gprs_to_push,
                         fprs_to_push, true);

    float_emit.UXTL(8, ARM64Reg::D0, ARM64Reg::D0);
    float_emit.UXTL(16, ARM64Reg::D0, ARM64Reg::D0);
    float_emit.UCVTF(32, ARM64Reg::D0, ARM64Reg::D0);

    const s32 load_offset = MOVPage2R(ARM64Reg::X2, &m_dequantizeTableS);
    ADD(scale_reg, ARM64Reg::X2, scale_reg, ArithOption(scale_reg, ShiftType::LSL, 3));
    float_emit.LDR(32, IndexType::Unsigned, ARM64Reg::D1, scale_reg, load_offset);
    float_emit.FMUL(32, ARM64Reg::D0, ARM64Reg::D0, ARM64Reg::D1, 0);
    RET(ARM64Reg::X30);
  }
  const u8* loadPairedS8Two = GetCodePtr();
  {
    constexpr u32 flags = BackPatchInfo::FLAG_LOAD | BackPatchInfo::FLAG_FLOAT |
                          BackPatchInfo::FLAG_PAIR | BackPatchInfo::FLAG_SIZE_8;

    EmitBackpatchRoutine(flags, MemAccessMode::Auto, ARM64Reg::D0, addr_reg, gprs_to_push,
                         fprs_to_push, true);

    float_emit.SXTL(8, ARM64Reg::D0, ARM64Reg::D0);
    float_emit.SXTL(16, ARM64Reg::D0, ARM64Reg::D0);
    float_emit.SCVTF(32, ARM64Reg::D0, ARM64Reg::D0);

    const s32 load_offset = MOVPage2R(ARM64Reg::X2, &m_dequantizeTableS);
    ADD(scale_reg, ARM64Reg::X2, scale_reg, ArithOption(scale_reg, ShiftType::LSL, 3));
    float_emit.LDR(32, IndexType::Unsigned, ARM64Reg::D1, scale_reg, load_offset);
    float_emit.FMUL(32, ARM64Reg::D0, ARM64Reg::D0, ARM64Reg::D1, 0);
    RET(ARM64Reg::X30);
  }
  const u8* loadPairedU16Two = GetCodePtr();
  {
    constexpr u32 flags = BackPatchInfo::FLAG_LOAD | BackPatchInfo::FLAG_FLOAT |
                          BackPatchInfo::FLAG_PAIR | BackPatchInfo::FLAG_SIZE_16;

    EmitBackpatchRoutine(flags, MemAccessMode::Auto, ARM64Reg::D0, addr_reg, gprs_to_push,
                         fprs_to_push, true);

    float_emit.UXTL(16, ARM64Reg::D0, ARM64Reg::D0);
    float_emit.UCVTF(32, ARM64Reg::D0, ARM64Reg::D0);

    const s32 load_offset = MOVPage2R(ARM64Reg::X2, &m_dequantizeTableS);
    ADD(scale_reg, ARM64Reg::X2, scale_reg, ArithOption(scale_reg, ShiftType::LSL, 3));
    float_emit.LDR(32, IndexType::Unsigned, ARM64Reg::D1, scale_reg, load_offset);
    float_emit.FMUL(32, ARM64Reg::D0, ARM64Reg::D0, ARM64Reg::D1, 0);
    RET(ARM64Reg::X30);
  }
  const u8* loadPairedS16Two = GetCodePtr();
  {
    constexpr u32 flags = BackPatchInfo::FLAG_LOAD | BackPatchInfo::FLAG_FLOAT |
                          BackPatchInfo::FLAG_PAIR | BackPatchInfo::FLAG_SIZE_16;

    EmitBackpatchRoutine(flags, MemAccessMode::Auto, ARM64Reg::D0, addr_reg, gprs_to_push,
                         fprs_to_push, true);

    float_emit.SXTL(16, ARM64Reg::D0, ARM64Reg::D0);
    float_emit.SCVTF(32, ARM64Reg::D0, ARM64Reg::D0);

    const s32 load_offset = MOVPage2R(ARM64Reg::X2, &m_dequantizeTableS);
    ADD(scale_reg, ARM64Reg::X2, scale_reg, ArithOption(scale_reg, ShiftType::LSL, 3));
    float_emit.LDR(32, IndexType::Unsigned, ARM64Reg::D1, scale_reg, load_offset);
    float_emit.FMUL(32, ARM64Reg::D0, ARM64Reg::D0, ARM64Reg::D1, 0);
    RET(ARM64Reg::X30);
  }

  const u8* loadPairedFloatOne = GetCodePtr();
  {
    constexpr u32 flags =
        BackPatchInfo::FLAG_LOAD | BackPatchInfo::FLAG_FLOAT | BackPatchInfo::FLAG_SIZE_32;

    EmitBackpatchRoutine(flags, MemAccessMode::Auto, ARM64Reg::D0, addr_reg,
                         gprs_to_push & ~BitSet32{1}, fprs_to_push, true);

    RET(ARM64Reg::X30);
  }
  const u8* loadPairedU8One = GetCodePtr();
  {
    constexpr u32 flags =
        BackPatchInfo::FLAG_LOAD | BackPatchInfo::FLAG_FLOAT | BackPatchInfo::FLAG_SIZE_8;

    EmitBackpatchRoutine(flags, MemAccessMode::Auto, ARM64Reg::D0, addr_reg, gprs_to_push,
                         fprs_to_push, true);

    float_emit.UXTL(8, ARM64Reg::D0, ARM64Reg::D0);
    float_emit.UXTL(16, ARM64Reg::D0, ARM64Reg::D0);
    float_emit.UCVTF(32, ARM64Reg::D0, ARM64Reg::D0);

    const s32 load_offset = MOVPage2R(ARM64Reg::X2, &m_dequantizeTableS);
    ADD(scale_reg, ARM64Reg::X2, scale_reg, ArithOption(scale_reg, ShiftType::LSL, 3));
    float_emit.LDR(32, IndexType::Unsigned, ARM64Reg::D1, scale_reg, load_offset);
    float_emit.FMUL(32, ARM64Reg::D0, ARM64Reg::D0, ARM64Reg::D1, 0);
    RET(ARM64Reg::X30);
  }
  const u8* loadPairedS8One = GetCodePtr();
  {
    constexpr u32 flags =
        BackPatchInfo::FLAG_LOAD | BackPatchInfo::FLAG_FLOAT | BackPatchInfo::FLAG_SIZE_8;

    EmitBackpatchRoutine(flags, MemAccessMode::Auto, ARM64Reg::D0, addr_reg, gprs_to_push,
                         fprs_to_push, true);

    float_emit.SXTL(8, ARM64Reg::D0, ARM64Reg::D0);
    float_emit.SXTL(16, ARM64Reg::D0, ARM64Reg::D0);
    float_emit.SCVTF(32, ARM64Reg::D0, ARM64Reg::D0);

    const s32 load_offset = MOVPage2R(ARM64Reg::X2, &m_dequantizeTableS);
    ADD(scale_reg, ARM64Reg::X2, scale_reg, ArithOption(scale_reg, ShiftType::LSL, 3));
    float_emit.LDR(32, IndexType::Unsigned, ARM64Reg::D1, scale_reg, load_offset);
    float_emit.FMUL(32, ARM64Reg::D0, ARM64Reg::D0, ARM64Reg::D1, 0);
    RET(ARM64Reg::X30);
  }
  const u8* loadPairedU16One = GetCodePtr();
  {
    constexpr u32 flags =
        BackPatchInfo::FLAG_LOAD | BackPatchInfo::FLAG_FLOAT | BackPatchInfo::FLAG_SIZE_16;

    EmitBackpatchRoutine(flags, MemAccessMode::Auto, ARM64Reg::D0, addr_reg, gprs_to_push,
                         fprs_to_push, true);

    float_emit.UXTL(16, ARM64Reg::D0, ARM64Reg::D0);
    float_emit.UCVTF(32, ARM64Reg::D0, ARM64Reg::D0);

    const s32 load_offset = MOVPage2R(ARM64Reg::X2, &m_dequantizeTableS);
    ADD(scale_reg, ARM64Reg::X2, scale_reg, ArithOption(scale_reg, ShiftType::LSL, 3));
    float_emit.LDR(32, IndexType::Unsigned, ARM64Reg::D1, scale_reg, load_offset);
    float_emit.FMUL(32, ARM64Reg::D0, ARM64Reg::D0, ARM64Reg::D1, 0);
    RET(ARM64Reg::X30);
  }
  const u8* loadPairedS16One = GetCodePtr();
  {
    constexpr u32 flags =
        BackPatchInfo::FLAG_LOAD | BackPatchInfo::FLAG_FLOAT | BackPatchInfo::FLAG_SIZE_16;

    EmitBackpatchRoutine(flags, MemAccessMode::Auto, ARM64Reg::D0, addr_reg, gprs_to_push,
                         fprs_to_push, true);

    float_emit.SXTL(16, ARM64Reg::D0, ARM64Reg::D0);
    float_emit.SCVTF(32, ARM64Reg::D0, ARM64Reg::D0);

    const s32 load_offset = MOVPage2R(ARM64Reg::X2, &m_dequantizeTableS);
    ADD(scale_reg, ARM64Reg::X2, scale_reg, ArithOption(scale_reg, ShiftType::LSL, 3));
    float_emit.LDR(32, IndexType::Unsigned, ARM64Reg::D1, scale_reg, load_offset);
    float_emit.FMUL(32, ARM64Reg::D0, ARM64Reg::D0, ARM64Reg::D1, 0);
    RET(ARM64Reg::X30);
  }

  Common::JitRegister::Register(start, GetCodePtr(), "JIT_QuantizedLoad");

  paired_load_quantized = reinterpret_cast<const u8**>(AlignCode16());
  ReserveCodeSpace(8 * sizeof(u8*));

  paired_load_quantized[0] = loadPairedFloatTwo;
  paired_load_quantized[1] = loadPairedIllegal;
  paired_load_quantized[2] = loadPairedIllegal;
  paired_load_quantized[3] = loadPairedIllegal;
  paired_load_quantized[4] = loadPairedU8Two;
  paired_load_quantized[5] = loadPairedU16Two;
  paired_load_quantized[6] = loadPairedS8Two;
  paired_load_quantized[7] = loadPairedS16Two;

  single_load_quantized = reinterpret_cast<const u8**>(AlignCode16());
  ReserveCodeSpace(8 * sizeof(u8*));

  single_load_quantized[0] = loadPairedFloatOne;
  single_load_quantized[1] = loadPairedIllegal;
  single_load_quantized[2] = loadPairedIllegal;
  single_load_quantized[3] = loadPairedIllegal;
  single_load_quantized[4] = loadPairedU8One;
  single_load_quantized[5] = loadPairedU16One;
  single_load_quantized[6] = loadPairedS8One;
  single_load_quantized[7] = loadPairedS16One;
}

void JitArm64::GenerateQuantizedStores()
{
  // X0 is the scale
  // X1 is the address
  // X2 is a temporary
  // X3 is a temporary if jo.fastmem_arena is false (used in EmitBackpatchRoutine)
  // X30 is LR
  // Q0 is the register
  // Q1 is a temporary
  ARM64Reg scale_reg = ARM64Reg::X0;
  ARM64Reg addr_reg = ARM64Reg::X1;
  BitSet32 gprs_to_push = CALLER_SAVED_GPRS & ~BitSet32{0, 2};
  if (!jo.memcheck)
    gprs_to_push &= ~BitSet32{1};
  if (!jo.fastmem_arena)
    gprs_to_push &= ~BitSet32{3};
  BitSet32 fprs_to_push = BitSet32(0xFFFFFFFF) & ~BitSet32{0, 1};
  ARM64FloatEmitter float_emit(this);

  const u8* start = GetCodePtr();
  const u8* storePairedIllegal = GetCodePtr();
  BRK(0x101);
  const u8* storePairedFloat = GetCodePtr();
  {
    constexpr u32 flags = BackPatchInfo::FLAG_STORE | BackPatchInfo::FLAG_FLOAT |
                          BackPatchInfo::FLAG_PAIR | BackPatchInfo::FLAG_SIZE_32;

    EmitBackpatchRoutine(flags, MemAccessMode::Auto, ARM64Reg::D0, addr_reg, gprs_to_push,
                         fprs_to_push, true);

    RET(ARM64Reg::X30);
  }
  const u8* storePairedU8 = GetCodePtr();
  {
    const s32 load_offset = MOVPage2R(ARM64Reg::X2, &m_quantizeTableS);
    ADD(scale_reg, ARM64Reg::X2, scale_reg, ArithOption(scale_reg, ShiftType::LSL, 3));
    float_emit.LDR(32, IndexType::Unsigned, ARM64Reg::D1, scale_reg, load_offset);
    float_emit.FMUL(32, ARM64Reg::D0, ARM64Reg::D0, ARM64Reg::D1, 0);

    float_emit.FCVTZU(32, ARM64Reg::D0, ARM64Reg::D0);
    float_emit.UQXTN(16, ARM64Reg::D0, ARM64Reg::D0);
    float_emit.UQXTN(8, ARM64Reg::D0, ARM64Reg::D0);

    constexpr u32 flags = BackPatchInfo::FLAG_STORE | BackPatchInfo::FLAG_FLOAT |
                          BackPatchInfo::FLAG_PAIR | BackPatchInfo::FLAG_SIZE_8;

    EmitBackpatchRoutine(flags, MemAccessMode::Auto, ARM64Reg::D0, addr_reg, gprs_to_push,
                         fprs_to_push, true);

    RET(ARM64Reg::X30);
  }
  const u8* storePairedS8 = GetCodePtr();
  {
    const s32 load_offset = MOVPage2R(ARM64Reg::X2, &m_quantizeTableS);
    ADD(scale_reg, ARM64Reg::X2, scale_reg, ArithOption(scale_reg, ShiftType::LSL, 3));
    float_emit.LDR(32, IndexType::Unsigned, ARM64Reg::D1, scale_reg, load_offset);
    float_emit.FMUL(32, ARM64Reg::D0, ARM64Reg::D0, ARM64Reg::D1, 0);

    float_emit.FCVTZS(32, ARM64Reg::D0, ARM64Reg::D0);
    float_emit.SQXTN(16, ARM64Reg::D0, ARM64Reg::D0);
    float_emit.SQXTN(8, ARM64Reg::D0, ARM64Reg::D0);

    constexpr u32 flags = BackPatchInfo::FLAG_STORE | BackPatchInfo::FLAG_FLOAT |
                          BackPatchInfo::FLAG_PAIR | BackPatchInfo::FLAG_SIZE_8;

    EmitBackpatchRoutine(flags, MemAccessMode::Auto, ARM64Reg::D0, addr_reg, gprs_to_push,
                         fprs_to_push, true);

    RET(ARM64Reg::X30);
  }
  const u8* storePairedU16 = GetCodePtr();
  {
    const s32 load_offset = MOVPage2R(ARM64Reg::X2, &m_quantizeTableS);
    ADD(scale_reg, ARM64Reg::X2, scale_reg, ArithOption(scale_reg, ShiftType::LSL, 3));
    float_emit.LDR(32, IndexType::Unsigned, ARM64Reg::D1, scale_reg, load_offset);
    float_emit.FMUL(32, ARM64Reg::D0, ARM64Reg::D0, ARM64Reg::D1, 0);

    float_emit.FCVTZU(32, ARM64Reg::D0, ARM64Reg::D0);
    float_emit.UQXTN(16, ARM64Reg::D0, ARM64Reg::D0);

    constexpr u32 flags = BackPatchInfo::FLAG_STORE | BackPatchInfo::FLAG_FLOAT |
                          BackPatchInfo::FLAG_PAIR | BackPatchInfo::FLAG_SIZE_16;

    EmitBackpatchRoutine(flags, MemAccessMode::Auto, ARM64Reg::D0, addr_reg, gprs_to_push,
                         fprs_to_push, true);

    RET(ARM64Reg::X30);
  }
  const u8* storePairedS16 = GetCodePtr();  // Used by Viewtiful Joe's intro movie
  {
    const s32 load_offset = MOVPage2R(ARM64Reg::X2, &m_quantizeTableS);
    ADD(scale_reg, ARM64Reg::X2, scale_reg, ArithOption(scale_reg, ShiftType::LSL, 3));
    float_emit.LDR(32, IndexType::Unsigned, ARM64Reg::D1, scale_reg, load_offset);
    float_emit.FMUL(32, ARM64Reg::D0, ARM64Reg::D0, ARM64Reg::D1, 0);

    float_emit.FCVTZS(32, ARM64Reg::D0, ARM64Reg::D0);
    float_emit.SQXTN(16, ARM64Reg::D0, ARM64Reg::D0);

    constexpr u32 flags = BackPatchInfo::FLAG_STORE | BackPatchInfo::FLAG_FLOAT |
                          BackPatchInfo::FLAG_PAIR | BackPatchInfo::FLAG_SIZE_16;

    EmitBackpatchRoutine(flags, MemAccessMode::Auto, ARM64Reg::D0, addr_reg, gprs_to_push,
                         fprs_to_push, true);

    RET(ARM64Reg::X30);
  }

  const u8* storeSingleFloat = GetCodePtr();
  {
    constexpr u32 flags =
        BackPatchInfo::FLAG_STORE | BackPatchInfo::FLAG_FLOAT | BackPatchInfo::FLAG_SIZE_32;

    EmitBackpatchRoutine(flags, MemAccessMode::Auto, ARM64Reg::D0, addr_reg, gprs_to_push,
                         fprs_to_push, true);

    RET(ARM64Reg::X30);
  }
  const u8* storeSingleU8 = GetCodePtr();  // Used by MKWii
  {
    const s32 load_offset = MOVPage2R(ARM64Reg::X2, &m_quantizeTableS);
    ADD(scale_reg, ARM64Reg::X2, scale_reg, ArithOption(scale_reg, ShiftType::LSL, 3));
    float_emit.LDR(32, IndexType::Unsigned, ARM64Reg::D1, scale_reg, load_offset);
    float_emit.FMUL(32, ARM64Reg::D0, ARM64Reg::D0, ARM64Reg::D1);

    float_emit.FCVTZU(32, ARM64Reg::D0, ARM64Reg::D0);
    float_emit.UQXTN(16, ARM64Reg::D0, ARM64Reg::D0);
    float_emit.UQXTN(8, ARM64Reg::D0, ARM64Reg::D0);

    constexpr u32 flags =
        BackPatchInfo::FLAG_STORE | BackPatchInfo::FLAG_FLOAT | BackPatchInfo::FLAG_SIZE_8;

    EmitBackpatchRoutine(flags, MemAccessMode::Auto, ARM64Reg::D0, addr_reg, gprs_to_push,
                         fprs_to_push, true);

    RET(ARM64Reg::X30);
  }
  const u8* storeSingleS8 = GetCodePtr();
  {
    const s32 load_offset = MOVPage2R(ARM64Reg::X2, &m_quantizeTableS);
    ADD(scale_reg, ARM64Reg::X2, scale_reg, ArithOption(scale_reg, ShiftType::LSL, 3));
    float_emit.LDR(32, IndexType::Unsigned, ARM64Reg::D1, scale_reg, load_offset);
    float_emit.FMUL(32, ARM64Reg::D0, ARM64Reg::D0, ARM64Reg::D1);

    float_emit.FCVTZS(32, ARM64Reg::D0, ARM64Reg::D0);
    float_emit.SQXTN(16, ARM64Reg::D0, ARM64Reg::D0);
    float_emit.SQXTN(8, ARM64Reg::D0, ARM64Reg::D0);

    constexpr u32 flags =
        BackPatchInfo::FLAG_STORE | BackPatchInfo::FLAG_FLOAT | BackPatchInfo::FLAG_SIZE_8;

    EmitBackpatchRoutine(flags, MemAccessMode::Auto, ARM64Reg::D0, addr_reg, gprs_to_push,
                         fprs_to_push, true);

    RET(ARM64Reg::X30);
  }
  const u8* storeSingleU16 = GetCodePtr();  // Used by MKWii
  {
    const s32 load_offset = MOVPage2R(ARM64Reg::X2, &m_quantizeTableS);
    ADD(scale_reg, ARM64Reg::X2, scale_reg, ArithOption(scale_reg, ShiftType::LSL, 3));
    float_emit.LDR(32, IndexType::Unsigned, ARM64Reg::D1, scale_reg, load_offset);
    float_emit.FMUL(32, ARM64Reg::D0, ARM64Reg::D0, ARM64Reg::D1);

    float_emit.FCVTZU(32, ARM64Reg::D0, ARM64Reg::D0);
    float_emit.UQXTN(16, ARM64Reg::D0, ARM64Reg::D0);

    constexpr u32 flags =
        BackPatchInfo::FLAG_STORE | BackPatchInfo::FLAG_FLOAT | BackPatchInfo::FLAG_SIZE_16;

    EmitBackpatchRoutine(flags, MemAccessMode::Auto, ARM64Reg::D0, addr_reg, gprs_to_push,
                         fprs_to_push, true);

    RET(ARM64Reg::X30);
  }
  const u8* storeSingleS16 = GetCodePtr();
  {
    const s32 load_offset = MOVPage2R(ARM64Reg::X2, &m_quantizeTableS);
    ADD(scale_reg, ARM64Reg::X2, scale_reg, ArithOption(scale_reg, ShiftType::LSL, 3));
    float_emit.LDR(32, IndexType::Unsigned, ARM64Reg::D1, scale_reg, load_offset);
    float_emit.FMUL(32, ARM64Reg::D0, ARM64Reg::D0, ARM64Reg::D1);

    float_emit.FCVTZS(32, ARM64Reg::D0, ARM64Reg::D0);
    float_emit.SQXTN(16, ARM64Reg::D0, ARM64Reg::D0);

    constexpr u32 flags =
        BackPatchInfo::FLAG_STORE | BackPatchInfo::FLAG_FLOAT | BackPatchInfo::FLAG_SIZE_16;

    EmitBackpatchRoutine(flags, MemAccessMode::Auto, ARM64Reg::D0, addr_reg, gprs_to_push,
                         fprs_to_push, true);

    RET(ARM64Reg::X30);
  }

  Common::JitRegister::Register(start, GetCodePtr(), "JIT_QuantizedStore");

  paired_store_quantized = reinterpret_cast<const u8**>(AlignCode16());
  ReserveCodeSpace(8 * sizeof(u8*));

  paired_store_quantized[0] = storePairedFloat;
  paired_store_quantized[1] = storePairedIllegal;
  paired_store_quantized[2] = storePairedIllegal;
  paired_store_quantized[3] = storePairedIllegal;
  paired_store_quantized[4] = storePairedU8;
  paired_store_quantized[5] = storePairedU16;
  paired_store_quantized[6] = storePairedS8;
  paired_store_quantized[7] = storePairedS16;

  single_store_quantized = reinterpret_cast<const u8**>(AlignCode16());
  ReserveCodeSpace(8 * sizeof(u8*));

  single_store_quantized[0] = storeSingleFloat;
  single_store_quantized[1] = storePairedIllegal;
  single_store_quantized[2] = storePairedIllegal;
  single_store_quantized[3] = storePairedIllegal;
  single_store_quantized[4] = storeSingleU8;
  single_store_quantized[5] = storeSingleU16;
  single_store_quantized[6] = storeSingleS8;
  single_store_quantized[7] = storeSingleS16;
}
