// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Jit64/Jit.h"

#include "Common/BitSet.h"
#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "Common/x64Emitter.h"

#include "Core/CoreTiming.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/PowerPC/Jit64/RegCache/JitRegCache.h"
#include "Core/PowerPC/Jit64Common/Jit64PowerPCState.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

using namespace Gen;

static OpArg CROffset(int field)
{
  return PPCSTATE_CR(field);
}

void Jit64::GetCRFieldBit(int field, int bit, X64Reg out, bool negate)
{
  switch (bit)
  {
  case PowerPC::CR_SO_BIT:  // check bit 59 set
    BT(64, CROffset(field), Imm8(PowerPC::CR_EMU_SO_BIT));
    SETcc(negate ? CC_NC : CC_C, R(out));
    break;

  case PowerPC::CR_EQ_BIT:  // check bits 31-0 == 0
    CMP(32, CROffset(field), Imm8(0));
    SETcc(negate ? CC_NZ : CC_Z, R(out));
    break;

  case PowerPC::CR_GT_BIT:  // check val > 0
    CMP(64, CROffset(field), Imm8(0));
    SETcc(negate ? CC_NG : CC_G, R(out));
    break;

  case PowerPC::CR_LT_BIT:  // check bit 62 set
    BT(64, CROffset(field), Imm8(PowerPC::CR_EMU_LT_BIT));
    SETcc(negate ? CC_NC : CC_C, R(out));
    break;

  default:
    ASSERT_MSG(DYNA_REC, false, "Invalid CR bit");
  }
}

void Jit64::SetCRFieldBit(int field, int bit, X64Reg in)
{
  MOV(64, R(RSCRATCH2), CROffset(field));
  MOVZX(32, 8, in, R(in));

  switch (bit)
  {
  case PowerPC::CR_SO_BIT:  // set bit 59 to input
    FixGTBeforeSettingCRFieldBit(RSCRATCH2);
    BTR(64, R(RSCRATCH2), Imm8(PowerPC::CR_EMU_SO_BIT));
    SHL(64, R(in), Imm8(PowerPC::CR_EMU_SO_BIT));
    OR(64, R(RSCRATCH2), R(in));
    break;

  case PowerPC::CR_EQ_BIT:  // clear low 32 bits, set bit 0 to !input
    FixGTBeforeSettingEQ(RSCRATCH2);
    SHR(64, R(RSCRATCH2), Imm8(32));
    SHL(64, R(RSCRATCH2), Imm8(32));
    XOR(32, R(in), Imm8(1));
    OR(64, R(RSCRATCH2), R(in));
    break;

  case PowerPC::CR_GT_BIT:  // set bit 63 to !input
    BTR(64, R(RSCRATCH2), Imm8(63));
    NOT(32, R(in));
    BTS(64, R(RSCRATCH2), Imm8(32));
    SHL(64, R(in), Imm8(63));
    OR(64, R(RSCRATCH2), R(in));
    break;

  case PowerPC::CR_LT_BIT:  // set bit 62 to input
    FixGTBeforeSettingCRFieldBit(RSCRATCH2);
    BTR(64, R(RSCRATCH2), Imm8(PowerPC::CR_EMU_LT_BIT));
    SHL(64, R(in), Imm8(PowerPC::CR_EMU_LT_BIT));
    OR(64, R(RSCRATCH2), R(in));
    break;
  }

  MOV(64, CROffset(field), R(RSCRATCH2));
}

void Jit64::ClearCRFieldBit(int field, int bit)
{
  switch (bit)
  {
  case PowerPC::CR_SO_BIT:
    BTR(64, CROffset(field), Imm8(PowerPC::CR_EMU_SO_BIT));
    break;

  case PowerPC::CR_EQ_BIT:
    MOV(64, R(RSCRATCH), CROffset(field));
    FixGTBeforeSettingCRFieldBit(RSCRATCH);
    OR(64, R(RSCRATCH), Imm8(1));
    MOV(64, CROffset(field), R(RSCRATCH));
    break;

  case PowerPC::CR_GT_BIT:
    BTS(64, CROffset(field), Imm8(63));
    break;

  case PowerPC::CR_LT_BIT:
    BTR(64, CROffset(field), Imm8(PowerPC::CR_EMU_LT_BIT));
    break;
  }
  // We don't need to set bit 32; the cases where that's needed only come up when setting bits, not
  // clearing.
}

void Jit64::SetCRFieldBit(int field, int bit)
{
  MOV(64, R(RSCRATCH), CROffset(field));

  switch (bit)
  {
  case PowerPC::CR_SO_BIT:
    FixGTBeforeSettingCRFieldBit(RSCRATCH);
    BTS(64, R(RSCRATCH), Imm8(PowerPC::CR_EMU_SO_BIT));
    break;

  case PowerPC::CR_EQ_BIT:
    FixGTBeforeSettingEQ(RSCRATCH);
    SHR(64, R(RSCRATCH), Imm8(32));
    SHL(64, R(RSCRATCH), Imm8(32));
    break;

  case PowerPC::CR_GT_BIT:
    BTR(64, R(RSCRATCH), Imm8(63));
    BTS(64, R(RSCRATCH), Imm8(32));
    break;

  case PowerPC::CR_LT_BIT:
    FixGTBeforeSettingCRFieldBit(RSCRATCH);
    BTS(64, R(RSCRATCH), Imm8(PowerPC::CR_EMU_LT_BIT));
    break;
  }

  MOV(64, CROffset(field), R(RSCRATCH));
}

void Jit64::FixGTBeforeSettingCRFieldBit(Gen::X64Reg reg)
{
  // GT is considered unset if the internal representation is <= 0, or in other words,
  // if the internal representation either has bit 63 set or has all bits set to zero.
  // If all bits are zero and we set some bit that's unrelated to GT, we need to set bit 63 so GT
  // doesn't accidentally become considered set. Gross but necessary; this can break actual games.
  TEST(64, R(reg), R(reg));
  FixupBranch dont_clear_gt = J_CC(CC_NZ);
  BTS(64, R(reg), Imm8(63));
  SetJumpTarget(dont_clear_gt);
}

void Jit64::FixGTBeforeSettingEQ(Gen::X64Reg reg)
{
  TEST(32, R(reg), R(reg));
  FixupBranch dont_set_bit_32 = J_CC(CC_Z);
  BTS(64, R(reg), Imm8(63));
  SetJumpTarget(dont_set_bit_32);
}

FixupBranch Jit64::JumpIfCRFieldBit(int field, int bit, bool jump_if_set)
{
  switch (bit)
  {
  case PowerPC::CR_SO_BIT:  // check bit 59 set
    BT(64, CROffset(field), Imm8(PowerPC::CR_EMU_SO_BIT));
    return J_CC(jump_if_set ? CC_C : CC_NC, Jump::Near);

  case PowerPC::CR_EQ_BIT:  // check bits 31-0 == 0
    CMP(32, CROffset(field), Imm8(0));
    return J_CC(jump_if_set ? CC_Z : CC_NZ, Jump::Near);

  case PowerPC::CR_GT_BIT:  // check val > 0
    CMP(64, CROffset(field), Imm8(0));
    return J_CC(jump_if_set ? CC_G : CC_LE, Jump::Near);

  case PowerPC::CR_LT_BIT:  // check bit 62 set
    BT(64, CROffset(field), Imm8(PowerPC::CR_EMU_LT_BIT));
    return J_CC(jump_if_set ? CC_C : CC_NC, Jump::Near);

  default:
    ASSERT_MSG(DYNA_REC, false, "Invalid CR bit");
  }

  // Should never happen.
  return FixupBranch();
}

// Could be done with one temp register, but with two temp registers it's faster
void Jit64::UpdateFPExceptionSummary(X64Reg fpscr, X64Reg tmp1, X64Reg tmp2)
{
  // Kill dependency on tmp1 (not required for correctness, since SHL will shift out upper bytes)
  XOR(32, R(tmp1), R(tmp1));

  // fpscr.VX = (fpscr & FPSCR_VX_ANY) != 0
  TEST(32, R(fpscr), Imm32(FPSCR_VX_ANY));
  SETcc(CC_NZ, R(tmp1));
  SHL(32, R(tmp1), Imm8(MathUtil::IntLog2(FPSCR_VX)));
  AND(32, R(fpscr), Imm32(~(FPSCR_VX | FPSCR_FEX)));
  OR(32, R(fpscr), R(tmp1));

  // fpscr.FEX = ((fpscr >> 22) & (fpscr & FPSCR_ANY_E)) != 0
  MOV(32, R(tmp1), R(fpscr));
  MOV(32, R(tmp2), R(fpscr));
  SHR(32, R(tmp1), Imm8(22));
  AND(32, R(tmp2), Imm32(FPSCR_ANY_E));
  TEST(32, R(tmp1), R(tmp2));
  // Unfortunately we eat a partial register stall below - we can't zero any of the registers before
  // the TEST, and we can't use XOR right after the TEST since that would overwrite flags. However,
  // there is no false dependency, since SETcc depends on TEST's flags and TEST depends on tmp1.
  SETcc(CC_NZ, R(tmp1));
  SHL(32, R(tmp1), Imm8(MathUtil::IntLog2(FPSCR_FEX)));
  OR(32, R(fpscr), R(tmp1));
}

static void DoICacheReset(PowerPC::PowerPCState& ppc_state, JitInterface& jit_interface)
{
  ppc_state.iCache.Reset(jit_interface);
}

void Jit64::mtspr(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);
  u32 iIndex = (inst.SPRU << 5) | (inst.SPRL & 0x1F);
  int d = inst.RD;

  switch (iIndex)
  {
  case SPR_DMAU:

  case SPR_SPRG0:
  case SPR_SPRG1:
  case SPR_SPRG2:
  case SPR_SPRG3:

  case SPR_SRR0:
  case SPR_SRR1:

  case SPR_LR:
  case SPR_CTR:

  case SPR_GQR0:
  case SPR_GQR0 + 1:
  case SPR_GQR0 + 2:
  case SPR_GQR0 + 3:
  case SPR_GQR0 + 4:
  case SPR_GQR0 + 5:
  case SPR_GQR0 + 6:
  case SPR_GQR0 + 7:
    // These are safe to do the easy way, see the bottom of this function.
    break;

  case SPR_XER:
  {
    RCX64Reg Rd = gpr.Bind(d, RCMode::Read);
    RegCache::Realize(Rd);

    MOV(32, R(RSCRATCH), Rd);
    AND(32, R(RSCRATCH), Imm32(0xff7f));
    MOV(16, PPCSTATE(xer_stringctrl), R(RSCRATCH));

    MOV(32, R(RSCRATCH), Rd);
    SHR(32, R(RSCRATCH), Imm8(XER_CA_SHIFT));
    AND(8, R(RSCRATCH), Imm8(1));
    MOV(8, PPCSTATE(xer_ca), R(RSCRATCH));

    MOV(32, R(RSCRATCH), Rd);
    SHR(32, R(RSCRATCH), Imm8(XER_OV_SHIFT));
    MOV(8, PPCSTATE(xer_so_ov), R(RSCRATCH));

    return;
  }

  case SPR_HID0:
  {
    RCOpArg Rd = gpr.Use(d, RCMode::Read);
    RegCache::Realize(Rd);

    MOV(32, R(RSCRATCH), Rd);
    BTR(32, R(RSCRATCH), Imm8(31 - 20));  // ICFI
    MOV(32, PPCSTATE_SPR(iIndex), R(RSCRATCH));
    FixupBranch dont_reset_icache = J_CC(CC_NC);
    BitSet32 regs = CallerSavedRegistersInUse();
    ABI_PushRegistersAndAdjustStack(regs, 0);
    ABI_CallFunctionPP(DoICacheReset, &m_ppc_state, &m_system.GetJitInterface());
    ABI_PopRegistersAndAdjustStack(regs, 0);
    SetJumpTarget(dont_reset_icache);
    return;
  }

  default:
    FALLBACK_IF(true);
  }

  // OK, this is easy.
  RCOpArg Rd = gpr.BindOrImm(d, RCMode::Read);
  RegCache::Realize(Rd);
  MOV(32, PPCSTATE_SPR(iIndex), Rd);
}

void Jit64::mfspr(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);
  u32 iIndex = (inst.SPRU << 5) | (inst.SPRL & 0x1F);
  int d = inst.RD;
  switch (iIndex)
  {
  case SPR_TL:
  case SPR_TU:
  {
    // TODO: we really only need to call GetFakeTimeBase once per JIT block; this matters because
    // typical use of this instruction is to call it three times, e.g. mftbu/mftbl/mftbu/cmpw/bne
    // to deal with possible timer wraparound. This makes the second two (out of three) completely
    // redundant for the JIT.
    // no register choice

    RCX64Reg rdx = gpr.Scratch(RDX);
    RCX64Reg rax = gpr.Scratch(RAX);
    RCX64Reg rcx = gpr.Scratch(RCX);

    auto& core_timing_globals = m_system.GetCoreTiming().GetGlobals();
    MOV(64, rcx, ImmPtr(&core_timing_globals));

    // An inline implementation of CoreTiming::GetFakeTimeBase, since in timer-heavy games the
    // cost of calling out to C for this is actually significant.
    // Scale downcount by the CPU overclocking factor.
    CVTSI2SS(XMM0, PPCSTATE(downcount));
    MULSS(XMM0, MDisp(rcx, offsetof(CoreTiming::Globals, last_OC_factor_inverted)));
    CVTSS2SI(rdx, R(XMM0));  // RDX is downcount scaled by the overclocking factor
    MOV(32, rax, MDisp(rcx, offsetof(CoreTiming::Globals, slice_length)));
    SUB(64, rax, rdx);  // cycles since the last CoreTiming::Advance() event is (slicelength -
                        // Scaled_downcount)
    ADD(64, rax, MDisp(rcx, offsetof(CoreTiming::Globals, global_timer)));
    SUB(64, rax, MDisp(rcx, offsetof(CoreTiming::Globals, fake_TB_start_ticks)));
    // It might seem convenient to correct the timer for the block position here for even more
    // accurate
    // timing, but as of currently, this can break games. If we end up reading a time *after* the
    // time
    // at which an interrupt was supposed to occur, e.g. because we're 100 cycles into a block with
    // only
    // 50 downcount remaining, some games don't function correctly, such as Karaoke Party
    // Revolution,
    // which won't get past the loading screen.
    // if (js.downcountAmount)
    //	ADD(64, rax, Imm32(js.downcountAmount));

    // a / 12 = (a * 0xAAAAAAAAAAAAAAAB) >> 67
    MOV(64, rdx, Imm64(0xAAAAAAAAAAAAAAABULL));
    MUL(64, rdx);
    MOV(64, rax, MDisp(rcx, offsetof(CoreTiming::Globals, fake_TB_start_value)));
    SHR(64, rdx, Imm8(3));
    ADD(64, rax, rdx);
    MOV(64, PPCSTATE_SPR(SPR_TL), rax);

    if (CanMergeNextInstructions(1))
    {
      const UGeckoInstruction& next = js.op[1].inst;
      // Two calls of TU/TL next to each other are extremely common in typical usage, so merge them
      // if we can.
      u32 nextIndex = (next.SPRU << 5) | (next.SPRL & 0x1F);
      // Be careful; the actual opcode is for mftb (371), not mfspr (339)
      int n = next.RD;
      if (next.OPCD == 31 && next.SUBOP10 == 371 && (nextIndex == SPR_TU || nextIndex == SPR_TL) &&
          n != d)
      {
        js.downcountAmount++;
        js.skipInstructions = 1;
        RCX64Reg Rd = gpr.Bind(d, RCMode::Write);
        RCX64Reg Rn = gpr.Bind(n, RCMode::Write);
        RegCache::Realize(Rd, Rn);
        if (iIndex == SPR_TL)
          MOV(32, Rd, rax);
        if (nextIndex == SPR_TL)
          MOV(32, Rn, rax);
        SHR(64, rax, Imm8(32));
        if (iIndex == SPR_TU)
          MOV(32, Rd, rax);
        if (nextIndex == SPR_TU)
          MOV(32, Rn, rax);
        break;
      }
    }
    RCX64Reg Rd = gpr.Bind(d, RCMode::Write);
    RegCache::Realize(Rd);
    if (iIndex == SPR_TU)
      SHR(64, rax, Imm8(32));
    MOV(32, Rd, rax);
    break;
  }
  case SPR_XER:
  {
    RCX64Reg Rd = gpr.Bind(d, RCMode::Write);
    RegCache::Realize(Rd);
    MOVZX(32, 16, Rd, PPCSTATE(xer_stringctrl));
    MOVZX(32, 8, RSCRATCH, PPCSTATE(xer_ca));
    SHL(32, R(RSCRATCH), Imm8(XER_CA_SHIFT));
    OR(32, Rd, R(RSCRATCH));

    MOVZX(32, 8, RSCRATCH, PPCSTATE(xer_so_ov));
    SHL(32, R(RSCRATCH), Imm8(XER_OV_SHIFT));
    OR(32, Rd, R(RSCRATCH));
    break;
  }
  case SPR_WPAR:
  case SPR_DEC:
  case SPR_PMC1:
  case SPR_PMC2:
  case SPR_PMC3:
  case SPR_PMC4:
  case SPR_UPMC1:
  case SPR_UPMC2:
  case SPR_UPMC3:
  case SPR_UPMC4:
  case SPR_IABR:
    FALLBACK_IF(true);
  default:
  {
    RCX64Reg Rd = gpr.Bind(d, RCMode::Write);
    RegCache::Realize(Rd);
    MOV(32, Rd, PPCSTATE_SPR(iIndex));
    break;
  }
  }
}

void Jit64::mtmsr(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);
  FALLBACK_IF(jo.fp_exceptions);

  {
    RCOpArg Rs = gpr.BindOrImm(inst.RS, RCMode::Read);
    RegCache::Realize(Rs);
    MOV(32, PPCSTATE(msr), Rs);

    MSRUpdated(Rs, RSCRATCH2);
  }

  gpr.Flush();
  fpr.Flush();

  // If some exceptions are pending and EE are now enabled, force checking
  // external exceptions when going out of mtmsr in order to execute delayed
  // interrupts as soon as possible.
  TEST(32, PPCSTATE(msr), Imm32(0x8000));
  FixupBranch eeDisabled = J_CC(CC_Z, Jump::Near);

  TEST(32, PPCSTATE(Exceptions),
       Imm32(EXCEPTION_EXTERNAL_INT | EXCEPTION_PERFORMANCE_MONITOR | EXCEPTION_DECREMENTER));
  FixupBranch noExceptionsPending = J_CC(CC_Z, Jump::Near);

  // Check if a CP interrupt is waiting and keep the GPU emulation in sync (issue 4336)
  MOV(64, R(RSCRATCH), ImmPtr(&m_system.GetProcessorInterface().m_interrupt_cause));
  TEST(32, MatR(RSCRATCH), Imm32(ProcessorInterface::INT_CAUSE_CP));
  FixupBranch cpInt = J_CC(CC_NZ, Jump::Near);

  MOV(32, PPCSTATE(pc), Imm32(js.compilerPC + 4));
  WriteExternalExceptionExit();

  SetJumpTarget(cpInt);
  SetJumpTarget(noExceptionsPending);
  SetJumpTarget(eeDisabled);

  MOV(32, R(RSCRATCH), Imm32(js.compilerPC + 4));
  WriteExitDestInRSCRATCH();
}

void Jit64::mfmsr(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);
  // Privileged?
  RCX64Reg Rd = gpr.Bind(inst.RD, RCMode::Write);
  RegCache::Realize(Rd);
  MOV(32, Rd, PPCSTATE(msr));
}

void Jit64::mftb(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);
  mfspr(inst);
}

void Jit64::mfcr(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);
  int d = inst.RD;

  RCX64Reg scratch_guard = gpr.Scratch(RSCRATCH_EXTRA);
  CALL(asm_routines.mfcr);

  RCX64Reg Rd = gpr.Bind(d, RCMode::Write);
  RegCache::Realize(Rd);
  MOV(32, Rd, R(RSCRATCH));
}

void Jit64::mtcrf(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);

  // USES_CR
  u32 crm = inst.CRM;
  if (crm != 0)
  {
    if (gpr.IsImm(inst.RS))
    {
      for (int i = 0; i < 8; i++)
      {
        if ((crm & (0x80 >> i)) != 0)
        {
          u8 newcr = (gpr.Imm32(inst.RS) >> (28 - (i * 4))) & 0xF;
          u64 newcrval = PowerPC::ConditionRegister::PPCToInternal(newcr);
          if ((s64)newcrval == (s32)newcrval)
          {
            MOV(64, CROffset(i), Imm32((s32)newcrval));
          }
          else
          {
            MOV(64, R(RSCRATCH), Imm64(newcrval));
            MOV(64, CROffset(i), R(RSCRATCH));
          }
        }
      }
    }
    else
    {
      MOV(64, R(RSCRATCH2), ImmPtr(PowerPC::ConditionRegister::s_crTable.data()));
      RCX64Reg Rs = gpr.Bind(inst.RS, RCMode::Read);
      RegCache::Realize(Rs);
      for (int i = 0; i < 8; i++)
      {
        if ((crm & (0x80 >> i)) != 0)
        {
          MOV(32, R(RSCRATCH), Rs);
          if (i != 7)
            SHR(32, R(RSCRATCH), Imm8(28 - (i * 4)));
          if (i != 0)
            AND(32, R(RSCRATCH), Imm8(0xF));
          MOV(64, R(RSCRATCH), MComplex(RSCRATCH2, RSCRATCH, SCALE_8, 0));
          MOV(64, CROffset(i), R(RSCRATCH));
        }
      }
    }
  }
}

void Jit64::mcrf(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);

  // USES_CR
  if (inst.CRFS != inst.CRFD)
  {
    MOV(64, R(RSCRATCH), CROffset(inst.CRFS));
    MOV(64, CROffset(inst.CRFD), R(RSCRATCH));
  }
}

void Jit64::mcrxr(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);

  // Copy XER[0-3] into CR[inst.CRFD]
  MOVZX(32, 8, RSCRATCH, PPCSTATE(xer_ca));
  MOVZX(32, 8, RSCRATCH2, PPCSTATE(xer_so_ov));
  // [0 SO OV CA]
  LEA(32, RSCRATCH, MComplex(RSCRATCH, RSCRATCH2, SCALE_2, 0));
  // [SO OV CA 0] << 3
  SHL(32, R(RSCRATCH), Imm8(4));

  MOV(64, R(RSCRATCH2), ImmPtr(PowerPC::ConditionRegister::s_crTable.data()));
  MOV(64, R(RSCRATCH), MRegSum(RSCRATCH, RSCRATCH2));
  MOV(64, CROffset(inst.CRFD), R(RSCRATCH));

  // Clear XER[0-3]
  static_assert(PPCSTATE_OFF(xer_ca) + 1 == PPCSTATE_OFF(xer_so_ov));
  MOV(16, PPCSTATE(xer_ca), Imm16(0));
}

void Jit64::crXXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);
  DEBUG_ASSERT_MSG(DYNA_REC, inst.OPCD == 19, "Invalid crXXX");

  // TODO(merry): Futher optimizations can be performed here. For example,
  // instead of extracting each CR field bit then setting it, the operation
  // could be performed on the internal format directly instead and the
  // relevant bit result can be masked out.

  if (inst.CRBA == inst.CRBB)
  {
    switch (inst.SUBOP10)
    {
    // crclr
    case 129:  // crandc: A && ~B => 0
    case 193:  // crxor:  A ^ B   => 0
      ClearCRFieldBit(inst.CRBD >> 2, 3 - (inst.CRBD & 3));
      return;

    // crset
    case 289:  // creqv: ~(A ^ B) => 1
    case 417:  // crorc: A || ~B  => 1
      SetCRFieldBit(inst.CRBD >> 2, 3 - (inst.CRBD & 3));
      return;

    case 257:  // crand: A && B => A
    case 449:  // cror:  A || B => A
      GetCRFieldBit(inst.CRBA >> 2, 3 - (inst.CRBA & 3), RSCRATCH, false);
      SetCRFieldBit(inst.CRBD >> 2, 3 - (inst.CRBD & 3), RSCRATCH);
      return;

    case 33:   // crnor:  ~(A || B) => ~A
    case 225:  // crnand: ~(A && B) => ~A
      GetCRFieldBit(inst.CRBA >> 2, 3 - (inst.CRBA & 3), RSCRATCH, true);
      SetCRFieldBit(inst.CRBD >> 2, 3 - (inst.CRBD & 3), RSCRATCH);
      return;
    }
  }

  // creqv or crnand or crnor
  bool negateA = inst.SUBOP10 == 289 || inst.SUBOP10 == 225 || inst.SUBOP10 == 33;
  // crandc or crorc or crnand or crnor
  bool negateB =
      inst.SUBOP10 == 129 || inst.SUBOP10 == 417 || inst.SUBOP10 == 225 || inst.SUBOP10 == 33;

  GetCRFieldBit(inst.CRBA >> 2, 3 - (inst.CRBA & 3), RSCRATCH, negateA);
  GetCRFieldBit(inst.CRBB >> 2, 3 - (inst.CRBB & 3), RSCRATCH2, negateB);

  // Compute combined bit
  switch (inst.SUBOP10)
  {
  case 33:   // crnor: ~(A || B) == (~A && ~B)
  case 129:  // crandc: A && ~B
  case 257:  // crand:  A && B
    AND(8, R(RSCRATCH), R(RSCRATCH2));
    break;

  case 193:  // crxor: A ^ B
  case 289:  // creqv: ~(A ^ B) = ~A ^ B
    XOR(8, R(RSCRATCH), R(RSCRATCH2));
    break;

  case 225:  // crnand: ~(A && B) == (~A || ~B)
  case 417:  // crorc: A || ~B
  case 449:  // cror:  A || B
    OR(8, R(RSCRATCH), R(RSCRATCH2));
    break;
  }

  // Store result bit in CRBD
  SetCRFieldBit(inst.CRBD >> 2, 3 - (inst.CRBD & 3), RSCRATCH);
}

void Jit64::mcrfs(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);

  u8 shift = 4 * (7 - inst.CRFS);
  u32 mask = 0xF << shift;

  // Only clear exception bits (but not FEX/VX).
  mask &= FPSCR_FX | FPSCR_ANY_X;

  RCX64Reg scratch_guard;
  X64Reg scratch;
  if (mask != 0)
  {
    scratch_guard = gpr.Scratch();
    RegCache::Realize(scratch_guard);
    scratch = scratch_guard;
  }
  else
  {
    scratch = RSCRATCH;
  }

  if (cpu_info.bBMI1)
  {
    MOV(32, R(RSCRATCH), PPCSTATE(fpscr));
    MOV(32, R(RSCRATCH2), Imm32((4 << 8) | shift));
    BEXTR(32, RSCRATCH2, R(RSCRATCH), RSCRATCH2);
  }
  else
  {
    MOV(32, R(RSCRATCH2), PPCSTATE(fpscr));
    if (mask != 0)
      MOV(32, R(RSCRATCH), R(RSCRATCH2));

    SHR(32, R(RSCRATCH2), Imm8(shift));
    AND(32, R(RSCRATCH2), Imm32(0xF));
  }

  LEA(64, scratch, MConst(PowerPC::ConditionRegister::s_crTable));
  MOV(64, R(scratch), MComplex(scratch, RSCRATCH2, SCALE_8, 0));
  MOV(64, CROffset(inst.CRFD), R(scratch));

  if (mask != 0)
  {
    AND(32, R(RSCRATCH), Imm32(~mask));
    UpdateFPExceptionSummary(RSCRATCH, RSCRATCH2, scratch);
    MOV(32, PPCSTATE(fpscr), R(RSCRATCH));
  }
}

void Jit64::mffsx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);
  FALLBACK_IF(inst.Rc);

  MOV(32, R(RSCRATCH), PPCSTATE(fpscr));

  int d = inst.FD;
  RCX64Reg Rd = fpr.Bind(d, RCMode::Write);
  RegCache::Realize(Rd);
  MOV(64, R(RSCRATCH2), Imm64(0xFFF8000000000000));
  OR(64, R(RSCRATCH), R(RSCRATCH2));
  MOVQ_xmm(XMM0, R(RSCRATCH));
  MOVSD(Rd, R(XMM0));
}

// MXCSR = s_fpscr_to_mxcsr[FPSCR & 7]
static const u32 s_fpscr_to_mxcsr[8] = {
    0x1F80, 0x7F80, 0x5F80, 0x3F80, 0x9F80, 0xFF80, 0xDF80, 0xBF80,
};

// Needs value of FPSCR in RSCRATCH.
void Jit64::UpdateMXCSR()
{
  LEA(64, RSCRATCH2, MConst(s_fpscr_to_mxcsr));
  AND(32, R(RSCRATCH), Imm32(7));
  LDMXCSR(MComplex(RSCRATCH2, RSCRATCH, SCALE_4, 0));
}

void Jit64::mtfsb0x(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);
  FALLBACK_IF(inst.Rc);

  const u32 mask = 0x80000000 >> inst.CRBD;
  const u32 inverted_mask = ~mask;

  if (mask == FPSCR_FEX || mask == FPSCR_VX)
    return;

  if (inst.CRBD < 29 && (mask & (FPSCR_ANY_X | FPSCR_ANY_E)) == 0)
  {
    AND(32, PPCSTATE(fpscr), Imm32(inverted_mask));
  }
  else
  {
    MOV(32, R(RSCRATCH), PPCSTATE(fpscr));
    AND(32, R(RSCRATCH), Imm32(inverted_mask));

    if ((mask & (FPSCR_ANY_X | FPSCR_ANY_E)) != 0)
    {
      RCX64Reg scratch = gpr.Scratch();
      RegCache::Realize(scratch);

      UpdateFPExceptionSummary(RSCRATCH, RSCRATCH2, scratch);
    }

    MOV(32, PPCSTATE(fpscr), R(RSCRATCH));
    if (inst.CRBD >= 29)
      UpdateMXCSR();
  }
}

void Jit64::mtfsb1x(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);
  FALLBACK_IF(inst.Rc);
  FALLBACK_IF(jo.fp_exceptions);

  const u32 mask = 0x80000000 >> inst.CRBD;

  if (mask == FPSCR_FEX || mask == FPSCR_VX)
    return;

  MOV(32, R(RSCRATCH), PPCSTATE(fpscr));
  if ((mask & FPSCR_ANY_X) != 0)
  {
    BTS(32, R(RSCRATCH), Imm32(31 - inst.CRBD));
    FixupBranch dont_set_fx = J_CC(CC_C);
    OR(32, R(RSCRATCH), Imm32(1u << 31));
    SetJumpTarget(dont_set_fx);
  }
  else
  {
    OR(32, R(RSCRATCH), Imm32(mask));
  }

  if ((mask & (FPSCR_ANY_X | FPSCR_ANY_E)) != 0)
  {
    RCX64Reg scratch = gpr.Scratch();
    RegCache::Realize(scratch);

    UpdateFPExceptionSummary(RSCRATCH, RSCRATCH2, scratch);
  }

  MOV(32, PPCSTATE(fpscr), R(RSCRATCH));
  if (inst.CRBD >= 29)
    UpdateMXCSR();
}

void Jit64::mtfsfix(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);
  FALLBACK_IF(inst.Rc);
  FALLBACK_IF(jo.fp_exceptions);

  u8 imm = (inst.hex >> (31 - 19)) & 0xF;
  u32 mask = 0xF0000000 >> (4 * inst.CRFD);
  u32 or_mask = imm << (28 - 4 * inst.CRFD);
  u32 and_mask = ~mask;

  MOV(32, R(RSCRATCH), PPCSTATE(fpscr));
  AND(32, R(RSCRATCH), Imm32(and_mask));
  OR(32, R(RSCRATCH), Imm32(or_mask));

  if ((mask & (FPSCR_FEX | FPSCR_VX | FPSCR_ANY_X | FPSCR_ANY_E)) != 0)
  {
    RCX64Reg scratch = gpr.Scratch();
    RegCache::Realize(scratch);

    UpdateFPExceptionSummary(RSCRATCH, RSCRATCH2, scratch);
  }

  MOV(32, PPCSTATE(fpscr), R(RSCRATCH));

  // Field 7 contains NI and RN.
  if (inst.CRFD == 7)
    LDMXCSR(MConst(s_fpscr_to_mxcsr, imm & 7));
}

void Jit64::mtfsfx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);
  FALLBACK_IF(inst.Rc);
  FALLBACK_IF(jo.fp_exceptions);

  u32 mask = 0;
  for (int i = 0; i < 8; i++)
  {
    if (inst.FM & (1 << i))
      mask |= 0xF << (4 * i);
  }

  int b = inst.FB;

  RCOpArg Rb = fpr.Use(b, RCMode::Read);
  RegCache::Realize(Rb);

  if (Rb.IsSimpleReg())
    MOVQ_xmm(R(RSCRATCH), Rb.GetSimpleReg());
  else
    MOV(32, R(RSCRATCH), Rb);

  if (mask != 0xFFFFFFFF)
  {
    MOV(32, R(RSCRATCH2), PPCSTATE(fpscr));
    AND(32, R(RSCRATCH), Imm32(mask));
    AND(32, R(RSCRATCH2), Imm32(~mask));
    OR(32, R(RSCRATCH), R(RSCRATCH2));
  }

  if ((mask & (FPSCR_FEX | FPSCR_VX | FPSCR_ANY_X | FPSCR_ANY_E)) != 0)
  {
    RCX64Reg scratch = gpr.Scratch();
    RegCache::Realize(scratch);

    UpdateFPExceptionSummary(RSCRATCH, RSCRATCH2, scratch);
  }

  MOV(32, PPCSTATE(fpscr), R(RSCRATCH));

  if (inst.FM & 1)
    UpdateMXCSR();
}
