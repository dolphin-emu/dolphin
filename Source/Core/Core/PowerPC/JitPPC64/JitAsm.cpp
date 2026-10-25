// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitPPC64/Jit.h"

#include <bit>
#include <cstddef>
#include <limits>
#include <utility>

#include "Common/CommonTypes.h"
#include "Common/FloatUtils.h"
#include "Common/JitRegister.h"
#include "Common/MemoryUtil.h"
#include "Common/PPC64Emitter.h"
#include "Core/Config/MainSettings.h"
#include "Core/CoreTiming.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/JitCommon/JitCache.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

using namespace PPC64Gen;

namespace
{
// The stack frame of the JIT, below the minimal ELFv2 frame header. The parameter save area isn't
// needed by the functions the JIT calls, but costs nothing.
constexpr s32 PARAMETER_SAVE_AREA = STACK_FRAME_MIN_SIZE;
static_assert(JitPPC64::SCRATCH_SAVE == PARAMETER_SAVE_AREA + 8 * 8);
constexpr s32 TOC_SAVE = JitPPC64::QUANTIZED_VALUE_SAVE + 8;
// The non-volatile registers of the ELFv2 ABI: r14-r31, f14-f31 and v20-v31.
constexpr u32 FIRST_SAVED_GPR = 14;
constexpr u32 FIRST_SAVED_FPR = 14;
constexpr u32 FIRST_SAVED_VR = 20;
constexpr s32 GPR_SAVE = TOC_SAVE + 8;
constexpr s32 FPR_SAVE = GPR_SAVE + 8 * (32 - FIRST_SAVED_GPR);
constexpr s32 VR_SAVE = (FPR_SAVE + 8 * (32 - FIRST_SAVED_FPR) + 15) & ~15;
constexpr s32 FRAME_SIZE = VR_SAVE + 16 * (32 - FIRST_SAVED_VR);
static_assert(FRAME_SIZE % 16 == 0, "The stack pointer must stay 16-byte aligned");

// CR2-CR4 are non-volatile.
constexpr u32 SAVED_CR_FIELDS = 0x38;
// The CR save word, in the caller's frame.
constexpr s32 CR_SAVE = 8;
}  // namespace

void JitPPC64::GenerateAsm()
{
  const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;

  const bool enable_debugging = Config::IsDebuggingEnabled();
  auto& cpu = m_system.GetCPU();

  enter_code = GetCodePtr();

  // Prologue: save LR, CR and all non-volatile registers, and the TOC pointer for our caller.
  MFLR(GPR::R0);
  STD(GPR::R0, GPR::SP, STACK_FRAME_LR_SAVE);
  MFCR(GPR::R0);
  STW(GPR::R0, GPR::SP, CR_SAVE);
  STDU(GPR::SP, GPR::SP, -FRAME_SIZE);
  STD(GPR::TOC, GPR::SP, TOC_SAVE);
  for (u32 i = FIRST_SAVED_GPR; i < 32; i++)
    STD(static_cast<GPR>(i), GPR::SP, GPR_SAVE + 8 * (i - FIRST_SAVED_GPR));
  for (u32 i = FIRST_SAVED_FPR; i < 32; i++)
    STFD(static_cast<FPR>(i), GPR::SP, FPR_SAVE + 8 * (i - FIRST_SAVED_FPR));
  for (u32 i = FIRST_SAVED_VR; i < 32; i++)
  {
    ADDI(GPR::R12, GPR::SP, VR_SAVE + 16 * (i - FIRST_SAVED_VR));
    STXVD2X(ToVSR(static_cast<VR>(i)), GPR::R0, GPR::R12);
  }

  MOVP2R(PPC_REG, &m_ppc_state);

  // The BLR optimization's frames go below one whose return address never matches, and the stack
  // pointer there is kept, to drop them.
  LI(GPR::R0, -1);
  STDU(GPR::SP, GPR::SP, -BLR_FRAME_SIZE);
  STD(GPR::R0, GPR::SP, BLR_GUEST_RETURN);
  STD(GPR::SP, PPC_REG, PPCSTATE_OFF(stored_stack_pointer));

  // The PC will be loaded into DISPATCHER_PC after the call to CoreTiming::Advance().
  // Advance() does an exception check so we don't know what PC to use until afterwards.
  const FixupBranch to_start_of_timing_slice = B();

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

  // Blocks exit to here with CR0 set from the downcount.
  const FixupBranch bail = BC(CCFlags::LE, CRF::CR0);

  dispatcher_no_timing_check = GetCodePtr();

  FixupBranch debug_exit;
  if (enable_debugging)
  {
    MOVP2R(GPR::R3, cpu.GetStatePtr());
    LWZ(GPR::R3, GPR::R3, 0);
    static_assert(std::to_underlying(CPU::State::Running) == 0);
    CMPWI(CRF::CR1, GPR::R3, 0);
    debug_exit = BC(CCFlags::NE, CRF::CR1);
  }

  dispatcher_no_check = GetCodePtr();

  // C++ code may have changed MSR[DR], and with it the arena.
  LD(MEM_REG, PPC_REG, PPCSTATE_OFF(mem_ptr));

  if (u8** const entry_points = GetBlockCache()->GetEntryPoints())
  {
    // The entry points map is indexed by ((feature_flags << 30) | (pc >> 2)), and holds 8-byte
    // pointers, so the byte offset is (feature_flags << 33) | (pc << 1).
    LWZ(GPR::R3, PPC_REG, PPCSTATE_OFF(feature_flags));
    MOVP2R(GPR::R4, entry_points);
    SLDI(GPR::R5, DISPATCHER_PC, 1);
    SLDI(GPR::R3, GPR::R3, 33);
    OR(GPR::R5, GPR::R5, GPR::R3);
    LDX(GPR::R6, GPR::R4, GPR::R5);
    CMPDI(CRF::CR1, GPR::R6, 0);
    const FixupBranch not_found = BC(CCFlags::EQ, CRF::CR1);
    MTCTR(GPR::R6);
    BCTR();
    SetJumpTarget(not_found);
  }
  else
  {
    // block = fast_block_map_fallback[(pc >> 2) & FAST_BLOCK_MAP_FALLBACK_MASK];
    static_assert(JitBaseBlockCache::FAST_BLOCK_MAP_FALLBACK_MASK == 0xFFFF);
    MOVP2R(GPR::R4, GetBlockCache()->GetFastBlockMapFallback());
    // ((pc >> 2) & 0xFFFF) * 8 == (pc << 1) & 0x7FFF8
    RLWINM(GPR::R5, DISPATCHER_PC, 1, 13, 28);
    LDX(GPR::R6, GPR::R4, GPR::R5);
    CMPDI(CRF::CR1, GPR::R6, 0);
    const FixupBranch not_found = BC(CCFlags::EQ, CRF::CR1);

    // if (block->effectiveAddress == pc && block->feature_flags == feature_flags)
    LWZ(GPR::R7, GPR::R6, offsetof(JitBlockData, effectiveAddress));
    CMPLW(CRF::CR1, GPR::R7, DISPATCHER_PC);
    const FixupBranch pc_mismatch = BC(CCFlags::NE, CRF::CR1);
    LWZ(GPR::R7, GPR::R6, offsetof(JitBlockData, feature_flags));
    LWZ(GPR::R8, PPC_REG, PPCSTATE_OFF(feature_flags));
    CMPLW(CRF::CR1, GPR::R7, GPR::R8);
    const FixupBranch feature_flags_mismatch = BC(CCFlags::NE, CRF::CR1);

    // return block->normalEntry;
    LD(GPR::R7, GPR::R6, offsetof(JitBlockData, normalEntry));
    MTCTR(GPR::R7);
    BCTR();

    SetJumpTarget(not_found);
    SetJumpTarget(pc_mismatch);
    SetJumpTarget(feature_flags_mismatch);
  }

  STW(DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(pc));

  // There is no point in calling the dispatcher in the fast lookup table case, since the assembly
  // dispatcher would already have found a block.
  if (!GetBlockCache()->GetEntryPoints())
  {
    // Call C version of Dispatch().
    ABI_CallFunction(&JitBase::Dispatch, static_cast<JitBase*>(this));
    CMPDI(CRF::CR1, GPR::R3, 0);
    const FixupBranch no_block_available = BC(CCFlags::EQ, CRF::CR1);
    MTCTR(GPR::R3);
    BCTR();
    SetJumpTarget(no_block_available);
  }

  // Call JIT, which may reuse the code that frames return to.
  ResetStack();
  ABI_CallFunction(&JitTrampoline, static_cast<JitBase*>(this), DISPATCHER_PC);
  LWZ(DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(pc));
  B(dispatcher_no_check);

  SetJumpTarget(bail);
  do_timing = GetCodePtr();
  // Write the current PC and the downcount out to PPCSTATE
  STW(DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(pc));
  STW(DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(npc));
  STW(DOWNCOUNT_REG, PPC_REG, PPCSTATE_OFF(downcount));

  // Check the state pointer to see if we are exiting
  // Gets checked on at the end of every slice
  MOVP2R(GPR::R3, cpu.GetStatePtr());
  LWZ(GPR::R3, GPR::R3, 0);
  CMPWI(CRF::CR1, GPR::R3, 0);
  const FixupBranch exit = BC(CCFlags::NE, CRF::CR1);

  // The downcount is in PPCSTATE here, from above or from the caller of the dispatcher, and is
  // loaded after.
  SetJumpTarget(to_start_of_timing_slice);
  PPC64CodeBlock::ABI_CallFunction(&CoreTiming::GlobalAdvance);
  LWA(DOWNCOUNT_REG, PPC_REG, PPCSTATE_OFF(downcount));
  // Advance may clear the code cache, for a change of settings.
  ResetStack();

  // Load the PC back into DISPATCHER_PC (the exception handler might have changed it)
  LWZ(DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(pc));

  // We can safely assume that downcount >= 1
  B(dispatcher_no_check);

  if (enable_debugging)
  {
    SetJumpTarget(debug_exit);
    STW(DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(pc));
    STW(DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(npc));
    STW(DOWNCOUNT_REG, PPC_REG, PPCSTATE_OFF(downcount));
  }

  dispatcher_exit = GetCodePtr();
  SetJumpTarget(exit);

  // Epilogue, without the frames of the BLR optimization.
  LD(GPR::SP, PPC_REG, PPCSTATE_OFF(stored_stack_pointer));
  ADDI(GPR::SP, GPR::SP, BLR_FRAME_SIZE);
  for (u32 i = FIRST_SAVED_VR; i < 32; i++)
  {
    ADDI(GPR::R12, GPR::SP, VR_SAVE + 16 * (i - FIRST_SAVED_VR));
    LXVD2X(ToVSR(static_cast<VR>(i)), GPR::R0, GPR::R12);
  }
  for (u32 i = FIRST_SAVED_FPR; i < 32; i++)
    LFD(static_cast<FPR>(i), GPR::SP, FPR_SAVE + 8 * (i - FIRST_SAVED_FPR));
  for (u32 i = FIRST_SAVED_GPR; i < 32; i++)
    LD(static_cast<GPR>(i), GPR::SP, GPR_SAVE + 8 * (i - FIRST_SAVED_GPR));
  LD(GPR::TOC, GPR::SP, TOC_SAVE);
  ADDI(GPR::SP, GPR::SP, FRAME_SIZE);
  LWZ(GPR::R0, GPR::SP, CR_SAVE);
  MTCRF(SAVED_CR_FIELDS, GPR::R0);
  LD(GPR::R0, GPR::SP, STACK_FRAME_LR_SAVE);
  MTLR(GPR::R0);
  BLR();

  Common::JitRegister::Register(enter_code, GetCodePtr(), "JIT_Dispatcher");

  const u8* routines_start = GetCodePtr();
  GenerateSlowAccessRoutines();
  GenerateExceptionExitRoutine();
  GenerateQuantizedRoutines();
  m_fprf_double_routine = GetCodePtr();
  GenerateFPRFRoutine(false);
  m_fprf_single_routine = GetCodePtr();
  GenerateFPRFRoutine(true);
  m_fres_routine = GetCodePtr();
  GenerateReciprocalEstimateRoutine();
  m_frsqrte_routine = GetCodePtr();
  GenerateReciprocalSquareRootEstimateRoutine();
  Common::JitRegister::Register(routines_start, GetCodePtr(), "JIT_SlowAccess");

  FlushIcache();
}

void JitPPC64::GenerateExceptionExitRoutine()
{
  // The end of WriteExceptionExit(pc, false, true), shared by the conditional exception exits,
  // which are many in far code: memory checks give every load and store one.
  m_exception_exit_routine = GetCodePtr();
  STW(GPR::R3, PPC_REG, PPCSTATE_OFF(pc));
  STW(GPR::R3, PPC_REG, PPCSTATE_OFF(npc));
  // DISPATCHER_PC is non-volatile, so it keeps the cycles across the call.
  MR(DISPATCHER_PC, GPR::R4);
  ABI_CallFunction(&PowerPC::CheckExceptionsFromJIT, &m_system.GetPowerPC());
  // As DoDownCount, which sets CR0 for the dispatcher.
  SUBF(DOWNCOUNT_REG, DISPATCHER_PC, DOWNCOUNT_REG, true);
  LWZ(DISPATCHER_PC, PPC_REG, PPCSTATE_OFF(npc));
  B(dispatcher);
}

void JitPPC64::GenerateFPRFRoutine(bool single)
{
  if (m_isa_3_0)
  {
    GenerateFPRFRoutineISA3_0(single);
    return;
  }

  // Sets FPSCR[FPRF] for the double in R3 as Common::ClassifyDouble does, or, for a value rounded
  // to single precision, as Common::ClassifyFloat does for it: such a value is a single denormal
  // when its exponent is below the single minimum, 2^-126. Uses R0, R3, R4, CR0 and CR1.
  RLDICL(GPR::R4, GPR::R3, 12, 53);  // Exponent
  SLDI(GPR::R0, GPR::R3, 12);        // Fraction, nonzero or not
  SRDI(GPR::R3, GPR::R3, 63);        // Sign
  CMPDI(CRF::CR1, GPR::R0, 0);

  CMPLWI(CRF::CR0, GPR::R4, 0x7FF);
  const FixupBranch special = BC(CCFlags::EQ, CRF::CR0);
  CMPLWI(CRF::CR0, GPR::R4, single ? 1023 - 126 : 1);
  const FixupBranch tiny = BC(CCFlags::LT, CRF::CR0);

  // Normal: PPC_FPCLASS_PN or PPC_FPCLASS_NN.
  static_assert(Common::PPC_FPCLASS_NN == Common::PPC_FPCLASS_PN + 4);
  SLWI(GPR::R4, GPR::R3, 2);
  ADDI(GPR::R4, GPR::R4, Common::PPC_FPCLASS_PN);
  const FixupBranch normal_done = B();

  // Zero or denormal: for a single, any nonzero value here is a denormal.
  SetJumpTarget(tiny);
  if (single)
  {
    OR(GPR::R0, GPR::R0, GPR::R4);
    CMPDI(CRF::CR1, GPR::R0, 0);
  }
  const FixupBranch denormal = BC(CCFlags::NE, CRF::CR1);
  static_assert(Common::PPC_FPCLASS_NZ == Common::PPC_FPCLASS_PZ + 16);
  SLWI(GPR::R4, GPR::R3, 4);
  ADDI(GPR::R4, GPR::R4, Common::PPC_FPCLASS_PZ);
  const FixupBranch zero_done = B();
  SetJumpTarget(denormal);
  static_assert(Common::PPC_FPCLASS_ND == Common::PPC_FPCLASS_PD + 4);
  SLWI(GPR::R4, GPR::R3, 2);
  ADDI(GPR::R4, GPR::R4, Common::PPC_FPCLASS_PD);
  const FixupBranch denormal_done = B();

  // Infinity or NaN.
  SetJumpTarget(special);
  const FixupBranch nan = BC(CCFlags::NE, CRF::CR1);
  static_assert(Common::PPC_FPCLASS_NINF == Common::PPC_FPCLASS_PINF + 4);
  SLWI(GPR::R4, GPR::R3, 2);
  ADDI(GPR::R4, GPR::R4, Common::PPC_FPCLASS_PINF);
  const FixupBranch infinity_done = B();
  SetJumpTarget(nan);
  LI(GPR::R4, Common::PPC_FPCLASS_QNAN);

  SetJumpTarget(normal_done);
  SetJumpTarget(zero_done);
  SetJumpTarget(denormal_done);
  SetJumpTarget(infinity_done);
  // FPRF is FPSCR's bits 12-16.
  LWZ(GPR::R0, PPC_REG, PPCSTATE_OFF(fpscr));
  RLWIMI(GPR::R0, GPR::R4, 12, 15, 19);
  STW(GPR::R0, PPC_REG, PPCSTATE_OFF(fpscr));
  BLR();
}

void JitPPC64::GenerateFPRFRoutineISA3_0(bool single)
{
  // As GenerateFPRFRoutine, for the double in VS0, whose class xststdcdp tests, with its sign in
  // LT, which setb turns into -1 (negative) or 0. Uses R0, R4, CR0 and CR1.
  const auto sign_times = [this](u32 shift) {
    SETB(GPR::R4, CRF::CR1);
    RLWINM(GPR::R4, GPR::R4, 0, 31 - shift, 31 - shift);
  };
  // NaNs, infinities, zeroes and denormals.
  XSTSTDCDP(CRF::CR1, VSR::VS0, 0x7F);
  const FixupBranch special = BC(CCFlags::EQ, CRF::CR1);
  FixupBranch tiny;
  if (single)
  {
    // A single denormal: below the single minimum, 2^-126.
    XSXEXPDP(GPR::R4, VSR::VS0);
    CMPLWI(CRF::CR0, GPR::R4, 1023 - 126);
    tiny = BC(CCFlags::LT, CRF::CR0);
  }
  static_assert(Common::PPC_FPCLASS_NN == Common::PPC_FPCLASS_PN + 4);
  sign_times(2);
  ADDI(GPR::R4, GPR::R4, Common::PPC_FPCLASS_PN);
  const FixupBranch normal_done = B();

  SetJumpTarget(special);
  XSTSTDCDP(CRF::CR1, VSR::VS0, 0x40);
  const FixupBranch nan = BC(CCFlags::EQ, CRF::CR1);
  XSTSTDCDP(CRF::CR1, VSR::VS0, 0x30);
  const FixupBranch infinity = BC(CCFlags::EQ, CRF::CR1);
  XSTSTDCDP(CRF::CR1, VSR::VS0, 0x0C);
  const FixupBranch zero = BC(CCFlags::EQ, CRF::CR1);
  // Denormal.
  if (single)
    SetJumpTarget(tiny);
  static_assert(Common::PPC_FPCLASS_ND == Common::PPC_FPCLASS_PD + 4);
  // The single path reaches here from a normal double, whose CR1 also has its sign.
  sign_times(2);
  ADDI(GPR::R4, GPR::R4, Common::PPC_FPCLASS_PD);
  const FixupBranch denormal_done = B();
  SetJumpTarget(zero);
  static_assert(Common::PPC_FPCLASS_NZ == Common::PPC_FPCLASS_PZ + 16);
  sign_times(4);
  ADDI(GPR::R4, GPR::R4, Common::PPC_FPCLASS_PZ);
  const FixupBranch zero_done = B();
  SetJumpTarget(infinity);
  static_assert(Common::PPC_FPCLASS_NINF == Common::PPC_FPCLASS_PINF + 4);
  sign_times(2);
  ADDI(GPR::R4, GPR::R4, Common::PPC_FPCLASS_PINF);
  const FixupBranch infinity_done = B();
  SetJumpTarget(nan);
  LI(GPR::R4, Common::PPC_FPCLASS_QNAN);

  SetJumpTarget(normal_done);
  SetJumpTarget(denormal_done);
  SetJumpTarget(zero_done);
  SetJumpTarget(infinity_done);
  LWZ(GPR::R0, PPC_REG, PPCSTATE_OFF(fpscr));
  RLWIMI(GPR::R0, GPR::R4, 12, 15, 19);
  STW(GPR::R0, PPC_REG, PPCSTATE_OFF(fpscr));
  BLR();
}

void JitPPC64::GenerateReciprocalEstimateRoutine()
{
  // As Common::ApproximateReciprocal, for nonzero finite inputs; zeroes, infinities and NaNs raise
  // exceptions or set FPSCR flags, which the interpreter does. Uses R0, R3, R4, R12, CR0 and CR1.
  FixupBranch special, zero;
  if (m_isa_3_0)
  {
    // With the input in VS0 too: NaNs, infinities and zeroes.
    XSTSTDCDP(CRF::CR1, VSR::VS0, 0x7C);
    special = BC(CCFlags::EQ, CRF::CR1);
    RLDICL(GPR::R4, GPR::R3, 12, 53);  // Exponent
  }
  else
  {
    RLDICL(GPR::R4, GPR::R3, 12, 53);  // Exponent
    CMPLWI(CRF::CR1, GPR::R4, 0x7FF);
    special = BC(CCFlags::EQ, CRF::CR1);
    SLDI(GPR::R0, GPR::R3, 1);
    CMPDI(CRF::CR0, GPR::R0, 0);
    zero = BC(CCFlags::EQ, CRF::CR0);
  }
  RLDICR(GPR::R12, GPR::R3, 0, 0);  // Sign
  CMPLWI(CRF::CR1, GPR::R4, 895);
  const FixupBranch small = BC(CCFlags::LT, CRF::CR1);
  CMPLWI(CRF::CR1, GPR::R4, 1149);
  const FixupBranch large = BC(CCFlags::GE, CRF::CR1);

  // (0x7FD << 52) - exponent, and base - (dec * (i % 1024) + 1) / 2 from fres_expected[i / 1024],
  // i being the top 15 bits of the mantissa.
  SUBFIC(GPR::R0, GPR::R4, 0x7FD);
  SLDI(GPR::R0, GPR::R0, 52);
  OR(GPR::R12, GPR::R12, GPR::R0);
  RLDICL(GPR::R4, GPR::R3, 64 - 37, 49);
  MOVP2R(GPR::R3, Common::fres_expected.data());
  static_assert(sizeof(Common::BaseAndDec) == 8);
  RLWINM(GPR::R0, GPR::R4, 32 - 10 + 3, 24, 28);
  ADD(GPR::R3, GPR::R3, GPR::R0);
  LWZ(GPR::R0, GPR::R3, offsetof(Common::BaseAndDec, m_dec));
  LWZ(GPR::R3, GPR::R3, offsetof(Common::BaseAndDec, m_base));
  CLRLDI(GPR::R4, GPR::R4, 64 - 10);
  MULLW(GPR::R4, GPR::R4, GPR::R0);
  ADDI(GPR::R4, GPR::R4, 1);
  SRWI(GPR::R4, GPR::R4, 1);
  SUBF(GPR::R3, GPR::R4, GPR::R3);
  SLDI(GPR::R3, GPR::R3, 29);
  OR(GPR::R3, GPR::R3, GPR::R12);
  LI(GPR::R4, 0);
  BLR();

  // Small inputs give the largest single, and large ones zero, of the input's sign.
  SetJumpTarget(small);
  MOVI2R(GPR::R3, std::bit_cast<u64>(static_cast<double>(std::numeric_limits<float>::max())));
  OR(GPR::R3, GPR::R3, GPR::R12);
  LI(GPR::R4, 0);
  BLR();
  SetJumpTarget(large);
  MR(GPR::R3, GPR::R12);
  LI(GPR::R4, 0);
  BLR();

  SetJumpTarget(special);
  if (!m_isa_3_0)
    SetJumpTarget(zero);
  LI(GPR::R4, 1);
  BLR();
}

void JitPPC64::GenerateReciprocalSquareRootEstimateRoutine()
{
  // As Common::ApproximateReciprocalSquareRoot, for positive normal inputs; the others raise
  // exceptions, set FPSCR flags or are rare, and the interpreter runs them. Uses R0, R3, R4, R12,
  // CR0 and CR1.
  FixupBranch negative, zero_or_denormal, special;
  if (m_isa_3_0)
  {
    // With the input in VS0 too: anything but a normal, or a negative, in one test.
    XSTSTDCDP(CRF::CR1, VSR::VS0, 0x7F);
    CROR(CRBit(CRF::CR1, CCFlags::EQ), CRBit(CRF::CR1, CCFlags::EQ), CRBit(CRF::CR1, CCFlags::LT));
    special = BC(CCFlags::EQ, CRF::CR1);
    RLDICL(GPR::R4, GPR::R3, 12, 53);  // Exponent
  }
  else
  {
    CMPDI(CRF::CR0, GPR::R3, 0);
    negative = BC(CCFlags::LT, CRF::CR0);
    RLDICL(GPR::R4, GPR::R3, 12, 53);  // Exponent
    CMPLWI(CRF::CR1, GPR::R4, 0);
    zero_or_denormal = BC(CCFlags::EQ, CRF::CR1);
    CMPLWI(CRF::CR1, GPR::R4, 0x7FF);
    special = BC(CCFlags::EQ, CRF::CR1);
  }

  // ((0x3FF << 52) - ((exponent - (0x3FE << 52)) / 2)) & (0x7FF << 52), the division being exact.
  ADDI(GPR::R12, GPR::R4, -0x3FE);
  SLDI(GPR::R12, GPR::R12, 52);
  SRADI(GPR::R12, GPR::R12, 1);
  LIS(GPR::R0, 0x3FF0);
  SLDI(GPR::R0, GPR::R0, 32);
  SUBF(GPR::R12, GPR::R12, GPR::R0);
  RLDICL(GPR::R12, GPR::R12, 0, 1);
  RLDICR(GPR::R12, GPR::R12, 0, 11);

  // base + dec * (i % 2048) from frsqrte_expected[i / 2048], i being the exponent's lowest bit and
  // the top 15 bits of the mantissa.
  RLDICL(GPR::R4, GPR::R3, 64 - 37, 48);
  MOVP2R(GPR::R0, Common::frsqrte_expected.data());
  RLWINM(GPR::R3, GPR::R4, 32 - 11 + 3, 24, 28);
  ADD(GPR::R3, GPR::R3, GPR::R0);
  LWZ(GPR::R0, GPR::R3, offsetof(Common::BaseAndDec, m_dec));
  LWZ(GPR::R3, GPR::R3, offsetof(Common::BaseAndDec, m_base));
  CLRLDI(GPR::R4, GPR::R4, 64 - 11);
  MULLW(GPR::R4, GPR::R4, GPR::R0);
  ADD(GPR::R3, GPR::R3, GPR::R4);
  SLDI(GPR::R3, GPR::R3, 26);
  OR(GPR::R3, GPR::R3, GPR::R12);
  LI(GPR::R4, 0);
  BLR();

  if (!m_isa_3_0)
  {
    SetJumpTarget(negative);
    SetJumpTarget(zero_or_denormal);
  }
  SetJumpTarget(special);
  LI(GPR::R4, 1);
  BLR();
}
