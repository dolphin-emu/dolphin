// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitArm64/Jit.h"

#include <array>

#include "Common/Arm64Emitter.h"
#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "Common/SmallVector.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/PowerPC/Interpreter/ExceptionUtils.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

using namespace Arm64Gen;

FixupBranch JitArm64::JumpIfCRFieldBit(int field, int bit, bool jump_if_set)
{
  ARM64Reg XA = gpr.CR(field);
  ARM64Reg WA = EncodeRegTo32(XA);

  switch (bit)
  {
  case PowerPC::CR_SO_BIT:  // check bit 59 set
    return jump_if_set ? TBNZ(XA, PowerPC::CR_EMU_SO_BIT) : TBZ(XA, PowerPC::CR_EMU_SO_BIT);
  case PowerPC::CR_EQ_BIT:  // check bits 31-0 == 0
    return jump_if_set ? CBZ(WA) : CBNZ(WA);
  case PowerPC::CR_GT_BIT:  // check val > 0
    CMP(XA, 0);
    return B(jump_if_set ? CC_GT : CC_LE);
  case PowerPC::CR_LT_BIT:  // check bit 62 set
    return jump_if_set ? TBNZ(XA, PowerPC::CR_EMU_LT_BIT) : TBZ(XA, PowerPC::CR_EMU_LT_BIT);
  default:
    ASSERT_MSG(DYNA_REC, false, "Invalid CR bit");
    return {};
  }
}

void JitArm64::FixGTBeforeSettingCRFieldBit(Arm64Gen::ARM64Reg reg)
{
  // GT is considered unset if the internal representation is <= 0, or in other words,
  // if the internal representation either has bit 63 set or has all bits set to zero.
  // If all bits are zero and we set some bit that's unrelated to GT, we need to set bit 63 so GT
  // doesn't accidentally become considered set. Gross but necessary; this can break actual games.
  ARM64Reg WA = gpr.GetReg();
  ARM64Reg XA = EncodeRegTo64(WA);
  ORR(XA, reg, LogicalImm(1ULL << 63, GPRSize::B64));
  CMP(reg, ARM64Reg::ZR);
  CSEL(reg, reg, XA, CC_NEQ);
  gpr.Unlock(WA);
}

void JitArm64::UpdateFPExceptionSummary(ARM64Reg fpscr)
{
  ARM64Reg WA = gpr.GetReg();

  // fpscr.VX = (fpscr & FPSCR_VX_ANY) != 0
  MOVI2R(WA, FPSCR_VX_ANY);
  TST(WA, fpscr);
  CSET(WA, CCFlags::CC_NEQ);
  BFI(fpscr, WA, MathUtil::IntLog2(FPSCR_VX), 1);

  // fpscr.FEX = ((fpscr >> 22) & (fpscr & FPSCR_ANY_E)) != 0
  AND(WA, fpscr, LogicalImm(FPSCR_ANY_E, GPRSize::B32));
  TST(WA, fpscr, ArithOption(fpscr, ShiftType::LSR, 22));
  CSET(WA, CCFlags::CC_NEQ);
  BFI(fpscr, WA, MathUtil::IntLog2(FPSCR_FEX), 1);

  gpr.Unlock(WA);
}

void JitArm64::UpdateRoundingMode()
{
  const BitSet32 gprs_to_save = gpr.GetCallerSavedUsed();
  const BitSet32 fprs_to_save = fpr.GetCallerSavedUsed();

  ABI_PushRegisters(gprs_to_save);
  m_float_emit.ABI_PushRegisters(fprs_to_save, ARM64Reg::X8);
  ABI_CallFunction(&PowerPC::RoundingModeUpdated, &m_ppc_state);
  m_float_emit.ABI_PopRegisters(fprs_to_save, ARM64Reg::X8);
  ABI_PopRegisters(gprs_to_save);
}

void JitArm64::mtmsr(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);
  FALLBACK_IF(jo.fp_exceptions);

  const bool imm_value = gpr.IsImm(inst.RS);
  if (imm_value)
    MSRUpdated(gpr.GetImm(inst.RS));

  STR(IndexType::Unsigned, gpr.R(inst.RS), PPC_REG, PPCSTATE_OFF(msr));

  if (!imm_value)
    MSRUpdated(gpr.R(inst.RS));

  gpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);
  fpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);

  WriteExceptionExit(js.compilerPC + 4, true);
}

void JitArm64::mfmsr(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);

  gpr.BindToRegister(inst.RD, false);
  LDR(IndexType::Unsigned, gpr.R(inst.RD), PPC_REG, PPCSTATE_OFF(msr));
}

void JitArm64::mcrf(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);

  if (inst.CRFS != inst.CRFD)
  {
    gpr.BindCRToRegister(inst.CRFD, false);
    MOV(gpr.CR(inst.CRFD), gpr.CR(inst.CRFS));
  }
}

void JitArm64::mcrxr(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);

  gpr.BindCRToRegister(inst.CRFD, false);
  ARM64Reg WA = gpr.GetReg();
  ARM64Reg XA = EncodeRegTo64(WA);
  ARM64Reg XB = gpr.CR(inst.CRFD);
  ARM64Reg WB = EncodeRegTo32(XB);

  // Copy XER[0-3] into CR[inst.CRFD]
  LDRB(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(xer_ca));
  LDRB(IndexType::Unsigned, WB, PPC_REG, PPCSTATE_OFF(xer_so_ov));

  // [0 SO OV CA]
  BFI(WA, WB, 1, 2);
  // [SO OV CA 0] << 3
  LSL(WA, WA, 4);

  MOVP2R(XB, PowerPC::ConditionRegister::s_crTable.data());
  LDR(XB, XB, XA);

  // Clear XER[0-3]
  static_assert(PPCSTATE_OFF(xer_ca) + 1 == PPCSTATE_OFF(xer_so_ov));
  STRH(IndexType::Unsigned, ARM64Reg::WZR, PPC_REG, PPCSTATE_OFF(xer_ca));

  gpr.Unlock(WA);
}

void JitArm64::mfsr(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);

  gpr.BindToRegister(inst.RD, false);
  LDR(IndexType::Unsigned, gpr.R(inst.RD), PPC_REG, PPCSTATE_OFF_SR(inst.SR));
}

void JitArm64::mtsr(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);

  STR(IndexType::Unsigned, gpr.R(inst.RS), PPC_REG, PPCSTATE_OFF_SR(inst.SR));
}

void JitArm64::mfsrin(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);

  u32 b = inst.RB, d = inst.RD;
  gpr.BindToRegister(d, d == b);

  ARM64Reg RB = gpr.R(b);
  ARM64Reg RD = gpr.R(d);
  ARM64Reg index = gpr.GetReg();
  ARM64Reg addr = EncodeRegTo64(RD);

  UBFM(index, RB, 28, 31);
  ADDI2R(addr, PPC_REG, PPCSTATE_OFF_SR(0), addr);
  LDR(RD, addr, ArithOption(EncodeRegTo64(index), true));

  gpr.Unlock(index);
}

void JitArm64::mtsrin(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);

  u32 b = inst.RB, d = inst.RD;
  gpr.BindToRegister(d, d == b);

  ARM64Reg RB = gpr.R(b);
  ARM64Reg RD = gpr.R(d);
  ARM64Reg index = gpr.GetReg();
  ARM64Reg addr = gpr.GetReg();

  UBFM(index, RB, 28, 31);
  ADDI2R(EncodeRegTo64(addr), PPC_REG, PPCSTATE_OFF_SR(0), EncodeRegTo64(addr));
  STR(RD, EncodeRegTo64(addr), ArithOption(EncodeRegTo64(index), true));

  gpr.Unlock(index, addr);
}

void JitArm64::twx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);

  s32 a = inst.RA;

  ARM64Reg WA = gpr.GetReg();

  if (inst.OPCD == 3)  // twi
  {
    CMPI2R(gpr.R(a), (s32)(s16)inst.SIMM_16, WA);
  }
  else  // tw
  {
    CMP(gpr.R(a), gpr.R(inst.RB));
  }

  constexpr std::array<CCFlags, 5> conditions{{CC_LT, CC_GT, CC_EQ, CC_VC, CC_VS}};
  Common::SmallVector<FixupBranch, conditions.size()> fixups;

  for (size_t i = 0; i < conditions.size(); i++)
  {
    if (inst.TO & (1U << i))
    {
      FixupBranch f = B(conditions[i]);
      fixups.push_back(f);
    }
  }
  FixupBranch dont_trap = B();

  for (const FixupBranch& fixup : fixups)
  {
    SetJumpTarget(fixup);
  }

  FixupBranch far_addr = B();
  SwitchToFarCode();
  SetJumpTarget(far_addr);

  gpr.Flush(FlushMode::MaintainState, WA);
  fpr.Flush(FlushMode::MaintainState, ARM64Reg::INVALID_REG);

  LDR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(Exceptions));
  ORR(WA, WA, LogicalImm(EXCEPTION_PROGRAM, GPRSize::B32));
  STR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(Exceptions));

  MOVI2R(WA, static_cast<u32>(ProgramExceptionCause::Trap));
  STR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF_SPR(SPR_SRR1));

  WriteExceptionExit(js.compilerPC, false, true);

  SwitchToNearCode();

  SetJumpTarget(dont_trap);

  if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
  {
    gpr.Flush(FlushMode::All, WA);
    fpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);
    WriteExit(js.compilerPC + 4);
  }

  gpr.Unlock(WA);
}

void JitArm64::mfspr(UGeckoInstruction inst)
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
    ARM64Reg Wg = gpr.GetReg();
    ARM64Reg Xg = EncodeRegTo64(Wg);

    ARM64Reg Wresult = gpr.GetReg();
    ARM64Reg Xresult = EncodeRegTo64(Wresult);

    ARM64Reg WA = gpr.GetReg();
    ARM64Reg WB = gpr.GetReg();
    ARM64Reg XA = EncodeRegTo64(WA);
    ARM64Reg XB = EncodeRegTo64(WB);

    ARM64Reg VC = fpr.GetReg();
    ARM64Reg VD = fpr.GetReg();
    ARM64Reg SC = EncodeRegToSingle(VC);
    ARM64Reg SD = EncodeRegToSingle(VD);

    // An inline implementation of CoreTiming::GetFakeTimeBase, since in timer-heavy games the
    // cost of calling out to C for this is actually significant.

    auto& core_timing_globals = m_system.GetCoreTiming().GetGlobals();
    MOVP2R(Xg, &core_timing_globals);

    LDR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(downcount));
    m_float_emit.SCVTF(SC, WA);
    m_float_emit.LDR(32, IndexType::Unsigned, SD, Xg,
                     offsetof(CoreTiming::Globals, last_OC_factor_inverted));
    m_float_emit.FMUL(SC, SC, SD);
    m_float_emit.FCVTS(Xresult, SC, RoundingMode::Z);

    LDP(IndexType::Signed, XA, XB, Xg, offsetof(CoreTiming::Globals, global_timer));
    SXTW(XB, WB);
    SUB(Xresult, XB, Xresult);
    ADD(Xresult, Xresult, XA);

    // It might seem convenient to correct the timer for the block position here for even more
    // accurate timing, but as of currently, this can break games. If we end up reading a time
    // *after* the time at which an interrupt was supposed to occur, e.g. because we're 100 cycles
    // into a block with only 50 downcount remaining, some games don't function correctly, such as
    // Karaoke Party Revolution, which won't get past the loading screen.

    LDP(IndexType::Signed, XA, XB, Xg, offsetof(CoreTiming::Globals, fake_TB_start_value));
    SUB(Xresult, Xresult, XB);

    // a / 12 = (a * 0xAAAAAAAAAAAAAAAB) >> 67
    ORR(XB, ARM64Reg::ZR, LogicalImm(0xAAAAAAAAAAAAAAAA, GPRSize::B64));
    ADD(XB, XB, 1);
    UMULH(Xresult, Xresult, XB);

    ADD(Xresult, XA, Xresult, ArithOption(Xresult, ShiftType::LSR, 3));
    STR(IndexType::Unsigned, Xresult, PPC_REG, PPCSTATE_OFF_SPR(SPR_TL));
    static_assert((PPCSTATE_OFF_SPR(SPR_TL) & 0x7) == 0);

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
        gpr.BindToRegister(d, false);
        gpr.BindToRegister(n, false);
        if (iIndex == SPR_TL)
          MOV(gpr.R(d), Wresult);
        else
          LSR(EncodeRegTo64(gpr.R(d)), Xresult, 32);

        if (nextIndex == SPR_TL)
          MOV(gpr.R(n), Wresult);
        else
          LSR(EncodeRegTo64(gpr.R(n)), Xresult, 32);

        gpr.Unlock(Wg, Wresult, WA, WB);
        fpr.Unlock(VC, VD);
        break;
      }
    }
    gpr.BindToRegister(d, false);
    if (iIndex == SPR_TU)
      LSR(EncodeRegTo64(gpr.R(d)), Xresult, 32);
    else
      MOV(gpr.R(d), Wresult);

    gpr.Unlock(Wg, Wresult, WA, WB);
    fpr.Unlock(VC, VD);
  }
  break;
  case SPR_XER:
  {
    gpr.BindToRegister(d, false);
    ARM64Reg RD = gpr.R(d);
    ARM64Reg WA = gpr.GetReg();
    LDRH(IndexType::Unsigned, RD, PPC_REG, PPCSTATE_OFF(xer_stringctrl));
    LDRB(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(xer_ca));
    ORR(RD, RD, WA, ArithOption(WA, ShiftType::LSL, XER_CA_SHIFT));
    LDRB(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(xer_so_ov));
    ORR(RD, RD, WA, ArithOption(WA, ShiftType::LSL, XER_OV_SHIFT));
    gpr.Unlock(WA);
  }
  break;
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
    gpr.BindToRegister(d, false);
    ARM64Reg RD = gpr.R(d);
    LDR(IndexType::Unsigned, RD, PPC_REG, PPCSTATE_OFF_SPR(iIndex));
    break;
  }
}

void JitArm64::mftb(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);
  mfspr(inst);
}

void JitArm64::mtspr(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);

  u32 iIndex = (inst.SPRU << 5) | (inst.SPRL & 0x1F);

  switch (iIndex)
  {
  case SPR_DMAU:

  case SPR_SPRG0:
  case SPR_SPRG1:
  case SPR_SPRG2:
  case SPR_SPRG3:

  case SPR_SRR0:
  case SPR_SRR1:
    // These are safe to do the easy way, see the bottom of this function.
    break;

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
    ARM64Reg RD = gpr.R(inst.RD);
    ARM64Reg WA = gpr.GetReg();
    AND(WA, RD, LogicalImm(0xFFFFFF7F, GPRSize::B32));
    STRH(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(xer_stringctrl));
    UBFM(WA, RD, XER_CA_SHIFT, XER_CA_SHIFT + 1);
    STRB(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(xer_ca));
    UBFM(WA, RD, XER_OV_SHIFT, 31);  // Same as WA = RD >> XER_OV_SHIFT
    STRB(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(xer_so_ov));
    gpr.Unlock(WA);
  }
  break;
  default:
    FALLBACK_IF(true);
  }

  // OK, this is easy.
  ARM64Reg RD = gpr.R(inst.RD);
  STR(IndexType::Unsigned, RD, PPC_REG, PPCSTATE_OFF_SPR(iIndex));
}

void JitArm64::crXXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);

  // Special case: crclr
  if (inst.CRBA == inst.CRBB && inst.CRBA == inst.CRBD && inst.SUBOP10 == 193)
  {
    // Clear CR field bit
    int field = inst.CRBD >> 2;
    int bit = 3 - (inst.CRBD & 3);

    gpr.BindCRToRegister(field, true);
    ARM64Reg XA = gpr.CR(field);
    switch (bit)
    {
    case PowerPC::CR_SO_BIT:
      AND(XA, XA, LogicalImm(~(u64(1) << PowerPC::CR_EMU_SO_BIT), GPRSize::B64));
      break;

    case PowerPC::CR_EQ_BIT:
      FixGTBeforeSettingCRFieldBit(XA);
      ORR(XA, XA, LogicalImm(1, GPRSize::B64));
      break;

    case PowerPC::CR_GT_BIT:
      ORR(XA, XA, LogicalImm(u64(1) << 63, GPRSize::B64));
      break;

    case PowerPC::CR_LT_BIT:
      AND(XA, XA, LogicalImm(~(u64(1) << PowerPC::CR_EMU_LT_BIT), GPRSize::B64));
      break;
    }
    return;
  }

  // Special case: crset
  if (inst.CRBA == inst.CRBB && inst.CRBA == inst.CRBD && inst.SUBOP10 == 289)
  {
    // SetCRFieldBit
    int field = inst.CRBD >> 2;
    int bit = 3 - (inst.CRBD & 3);

    gpr.BindCRToRegister(field, true);
    ARM64Reg XA = gpr.CR(field);

    if (bit != PowerPC::CR_GT_BIT)
      FixGTBeforeSettingCRFieldBit(XA);

    switch (bit)
    {
    case PowerPC::CR_SO_BIT:
      ORR(XA, XA, LogicalImm(u64(1) << PowerPC::CR_EMU_SO_BIT, GPRSize::B64));
      break;

    case PowerPC::CR_EQ_BIT:
      AND(XA, XA, LogicalImm(0xFFFF'FFFF'0000'0000, GPRSize::B64));
      break;

    case PowerPC::CR_GT_BIT:
      AND(XA, XA, LogicalImm(~(u64(1) << 63), GPRSize::B64));
      break;

    case PowerPC::CR_LT_BIT:
      ORR(XA, XA, LogicalImm(u64(1) << PowerPC::CR_EMU_LT_BIT, GPRSize::B64));
      break;
    }

    ORR(XA, XA, LogicalImm(u64(1) << 32, GPRSize::B64));
    return;
  }

  ARM64Reg WA = gpr.GetReg();
  ARM64Reg XA = EncodeRegTo64(WA);
  ARM64Reg WB = gpr.GetReg();
  ARM64Reg XB = EncodeRegTo64(WB);

  // creqv or crnand or crnor
  bool negateA = inst.SUBOP10 == 289 || inst.SUBOP10 == 225 || inst.SUBOP10 == 33;
  // crandc or crorc or crnand or crnor
  bool negateB =
      inst.SUBOP10 == 129 || inst.SUBOP10 == 417 || inst.SUBOP10 == 225 || inst.SUBOP10 == 33;

  // GetCRFieldBit
  for (int i = 0; i < 2; i++)
  {
    int field = i ? inst.CRBB >> 2 : inst.CRBA >> 2;
    int bit = i ? 3 - (inst.CRBB & 3) : 3 - (inst.CRBA & 3);
    ARM64Reg out = i ? XB : XA;
    bool negate = i ? negateB : negateA;

    ARM64Reg XC = gpr.CR(field);
    ARM64Reg WC = EncodeRegTo32(XC);
    switch (bit)
    {
    case PowerPC::CR_SO_BIT:  // check bit 59 set
      UBFX(out, XC, PowerPC::CR_EMU_SO_BIT, 1);
      if (negate)
        EOR(out, out, LogicalImm(1, GPRSize::B64));
      break;

    case PowerPC::CR_EQ_BIT:  // check bits 31-0 == 0
      CMP(WC, ARM64Reg::WZR);
      CSET(out, negate ? CC_NEQ : CC_EQ);
      break;

    case PowerPC::CR_GT_BIT:  // check val > 0
      CMP(XC, ARM64Reg::ZR);
      CSET(out, negate ? CC_LE : CC_GT);
      break;

    case PowerPC::CR_LT_BIT:  // check bit 62 set
      UBFX(out, XC, PowerPC::CR_EMU_LT_BIT, 1);
      if (negate)
        EOR(out, out, LogicalImm(1, GPRSize::B64));
      break;

    default:
      ASSERT_MSG(DYNA_REC, false, "Invalid CR bit");
    }
  }

  // Compute combined bit
  switch (inst.SUBOP10)
  {
  case 33:   // crnor: ~(A || B) == (~A && ~B)
  case 129:  // crandc: A && ~B
  case 257:  // crand:  A && B
    AND(XA, XA, XB);
    break;

  case 193:  // crxor: A ^ B
  case 289:  // creqv: ~(A ^ B) = ~A ^ B
    EOR(XA, XA, XB);
    break;

  case 225:  // crnand: ~(A && B) == (~A || ~B)
  case 417:  // crorc: A || ~B
  case 449:  // cror:  A || B
    ORR(XA, XA, XB);
    break;
  }

  // Store result bit in CRBD
  int field = inst.CRBD >> 2;
  int bit = 3 - (inst.CRBD & 3);

  gpr.Unlock(WB);
  WB = ARM64Reg::INVALID_REG;
  gpr.BindCRToRegister(field, true);
  XB = gpr.CR(field);

  if (bit != PowerPC::CR_GT_BIT)
    FixGTBeforeSettingCRFieldBit(XB);

  switch (bit)
  {
  case PowerPC::CR_SO_BIT:  // set bit 59 to input
    BFI(XB, XA, PowerPC::CR_EMU_SO_BIT, 1);
    break;

  case PowerPC::CR_EQ_BIT:  // clear low 32 bits, set bit 0 to !input
    AND(XB, XB, LogicalImm(0xFFFF'FFFF'0000'0000, GPRSize::B64));
    EOR(XA, XA, LogicalImm(1, GPRSize::B64));
    ORR(XB, XB, XA);
    break;

  case PowerPC::CR_GT_BIT:  // set bit 63 to !input
    EOR(XA, XA, LogicalImm(1, GPRSize::B64));
    BFI(XB, XA, 63, 1);
    break;

  case PowerPC::CR_LT_BIT:  // set bit 62 to input
    BFI(XB, XA, PowerPC::CR_EMU_LT_BIT, 1);
    break;
  }

  ORR(XB, XB, LogicalImm(1ULL << 32, GPRSize::B64));

  gpr.Unlock(WA);
}

void JitArm64::mfcr(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);

  gpr.BindToRegister(inst.RD, false);
  ARM64Reg WA = gpr.R(inst.RD);
  ARM64Reg WB = gpr.GetReg();
  ARM64Reg WC = gpr.GetReg();
  ARM64Reg XA = EncodeRegTo64(WA);
  ARM64Reg XB = EncodeRegTo64(WB);
  ARM64Reg XC = EncodeRegTo64(WC);

  for (int i = 0; i < 8; i++)
  {
    ARM64Reg CR = gpr.CR(i);
    ARM64Reg WCR = EncodeRegTo32(CR);

    // SO and LT
    static_assert(PowerPC::CR_SO_BIT == 0);
    static_assert(PowerPC::CR_LT_BIT == 3);
    static_assert(PowerPC::CR_EMU_LT_BIT - PowerPC::CR_EMU_SO_BIT == 3);
    if (i == 0)
    {
      MOVI2R(XB, PowerPC::CR_SO | PowerPC::CR_LT);
      AND(XA, XB, CR, ArithOption(CR, ShiftType::LSR, PowerPC::CR_EMU_SO_BIT));
    }
    else
    {
      AND(XC, XB, CR, ArithOption(CR, ShiftType::LSR, PowerPC::CR_EMU_SO_BIT));
      ORR(XA, XC, XA, ArithOption(XA, ShiftType::LSL, 4));
    }

    // EQ
    ORR(WC, WA, LogicalImm(1 << PowerPC::CR_EQ_BIT, GPRSize::B32));
    CMP(WCR, ARM64Reg::WZR);
    CSEL(WA, WC, WA, CC_EQ);

    // GT
    ORR(WC, WA, LogicalImm(1 << PowerPC::CR_GT_BIT, GPRSize::B32));
    CMP(CR, ARM64Reg::ZR);
    CSEL(WA, WC, WA, CC_GT);

    // To reduce register pressure and to avoid getting a pipeline-unfriendly long run of stores
    // after this instruction, flush registers that would be flushed after this instruction anyway.
    //
    // There's no point in ensuring we flush two registers at the same time, because the offset in
    // ppcState for CRs is too large to be encoded into an STP instruction.
    if (js.op->crDiscardable[i])
      gpr.DiscardCRRegisters(BitSet8{i});
    else if (!js.op->crInUse[i])
      gpr.StoreCRRegisters(BitSet8{i}, WC);
  }

  gpr.Unlock(WB, WC);
}

void JitArm64::mtcrf(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);

  u32 crm = inst.CRM;
  if (crm != 0)
  {
    ARM64Reg RS = gpr.R(inst.RS);
    ARM64Reg WB = gpr.GetReg();
    ARM64Reg XB = EncodeRegTo64(WB);
    MOVP2R(XB, PowerPC::ConditionRegister::s_crTable.data());
    for (int i = 0; i < 8; ++i)
    {
      if ((crm & (0x80 >> i)) != 0)
      {
        gpr.BindCRToRegister(i, false);
        ARM64Reg CR = gpr.CR(i);
        ARM64Reg WCR = EncodeRegTo32(CR);

        if (i != 7)
          LSR(WCR, RS, 28 - i * 4);
        if (i != 0)
        {
          if (i != 7)
            UBFX(WCR, WCR, 0, 4);
          else
            UBFX(WCR, RS, 0, 4);
        }

        LDR(CR, XB, ArithOption(CR, true));
      }
    }
    gpr.Unlock(WB);
  }
}

void JitArm64::mcrfs(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);

  u8 shift = 4 * (7 - inst.CRFS);
  u32 mask = 0xF << shift;
  u32 field = inst.CRFD;

  // Only clear exception bits (but not FEX/VX).
  mask &= FPSCR_FX | FPSCR_ANY_X;

  gpr.BindCRToRegister(field, false);
  ARM64Reg CR = gpr.CR(field);
  ARM64Reg WA = gpr.GetReg();
  ARM64Reg WCR = EncodeRegTo32(CR);
  ARM64Reg XA = EncodeRegTo64(WA);

  LDR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(fpscr));
  UBFX(WCR, WA, shift, 4);

  if (mask != 0)
  {
    const u32 inverted_mask = ~mask;
    AND(WA, WA, LogicalImm(inverted_mask, GPRSize::B32));

    UpdateFPExceptionSummary(WA);
    STR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(fpscr));
  }

  MOVP2R(XA, PowerPC::ConditionRegister::s_crTable.data());
  LDR(CR, XA, ArithOption(CR, true));

  gpr.Unlock(WA);
}

void JitArm64::mffsx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);
  FALLBACK_IF(inst.Rc);

  ARM64Reg WA = gpr.GetReg();
  ARM64Reg XA = EncodeRegTo64(WA);

  LDR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(fpscr));

  ARM64Reg VD = fpr.RW(inst.FD, RegType::LowerPair);

  ORR(XA, XA, LogicalImm(0xFFF8'0000'0000'0000, GPRSize::B64));
  m_float_emit.FMOV(EncodeRegToDouble(VD), XA);

  gpr.Unlock(WA);
}

void JitArm64::mtfsb0x(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);
  FALLBACK_IF(inst.Rc);

  const u32 mask = 0x80000000 >> inst.CRBD;
  const u32 inverted_mask = ~mask;

  if (mask == FPSCR_FEX || mask == FPSCR_VX)
    return;

  ARM64Reg WA = gpr.GetReg();

  LDR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(fpscr));

  AND(WA, WA, LogicalImm(inverted_mask, GPRSize::B32));

  if ((mask & (FPSCR_ANY_X | FPSCR_ANY_E)) != 0)
    UpdateFPExceptionSummary(WA);
  STR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(fpscr));

  gpr.Unlock(WA);

  if (inst.CRBD >= 29)
    UpdateRoundingMode();
}

void JitArm64::mtfsb1x(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);
  FALLBACK_IF(inst.Rc);
  FALLBACK_IF(jo.fp_exceptions);

  const u32 mask = 0x80000000 >> inst.CRBD;

  if (mask == FPSCR_FEX || mask == FPSCR_VX)
    return;

  ARM64Reg WA = gpr.GetReg();

  LDR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(fpscr));

  if ((mask & FPSCR_ANY_X) != 0)
  {
    ARM64Reg WB = gpr.GetReg();
    TST(WA, LogicalImm(mask, GPRSize::B32));
    ORR(WB, WA, LogicalImm(1 << 31, GPRSize::B32));
    CSEL(WA, WA, WB, CCFlags::CC_NEQ);
    gpr.Unlock(WB);
  }
  ORR(WA, WA, LogicalImm(mask, GPRSize::B32));

  if ((mask & (FPSCR_ANY_X | FPSCR_ANY_E)) != 0)
    UpdateFPExceptionSummary(WA);
  STR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(fpscr));

  gpr.Unlock(WA);

  if (inst.CRBD >= 29)
    UpdateRoundingMode();
}

void JitArm64::mtfsfix(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);
  FALLBACK_IF(inst.Rc);
  FALLBACK_IF(jo.fp_exceptions);

  u8 imm = (inst.hex >> (31 - 19)) & 0xF;
  u8 shift = 28 - 4 * inst.CRFD;
  u32 mask = 0xF << shift;

  ARM64Reg WA = gpr.GetReg();

  LDR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(fpscr));

  if (imm == 0xF)
  {
    ORR(WA, WA, LogicalImm(mask, GPRSize::B32));
  }
  else if (imm == 0x0)
  {
    const u32 inverted_mask = ~mask;
    AND(WA, WA, LogicalImm(inverted_mask, GPRSize::B32));
  }
  else
  {
    ARM64Reg WB = gpr.GetReg();
    MOVZ(WB, imm);
    BFI(WA, WB, shift, 4);
    gpr.Unlock(WB);
  }

  if ((mask & (FPSCR_FEX | FPSCR_VX | FPSCR_ANY_X | FPSCR_ANY_E)) != 0)
    UpdateFPExceptionSummary(WA);
  STR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(fpscr));

  gpr.Unlock(WA);

  // Field 7 contains NI and RN.
  if (inst.CRFD == 7)
    UpdateRoundingMode();
}

void JitArm64::mtfsfx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITSystemRegistersOff);
  FALLBACK_IF(inst.Rc);
  FALLBACK_IF(jo.fp_exceptions);

  u32 mask = 0;
  for (int i = 0; i < 8; i++)
  {
    if (inst.FM & (1 << i))
      mask |= 0xFU << (4 * i);
  }

  if (mask == 0xFFFFFFFF)
  {
    ARM64Reg VB = fpr.R(inst.FB, RegType::LowerPair);
    ARM64Reg WA = gpr.GetReg();

    m_float_emit.FMOV(WA, EncodeRegToSingle(VB));

    UpdateFPExceptionSummary(WA);
    STR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(fpscr));

    gpr.Unlock(WA);
  }
  else if (mask != 0)
  {
    ARM64Reg VB = fpr.R(inst.FB, RegType::LowerPair);
    ARM64Reg WA = gpr.GetReg();
    ARM64Reg WB = gpr.GetReg();

    LDR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(fpscr));
    m_float_emit.FMOV(WB, EncodeRegToSingle(VB));

    if (LogicalImm imm = LogicalImm(mask, GPRSize::B32))
    {
      const u32 inverted_mask = ~mask;
      AND(WA, WA, LogicalImm(inverted_mask, GPRSize::B32));
      AND(WB, WB, imm);
    }
    else
    {
      ARM64Reg WC = gpr.GetReg();

      MOVI2R(WC, mask);
      BIC(WA, WA, WC);
      AND(WB, WB, WC);

      gpr.Unlock(WC);
    }
    ORR(WA, WA, WB);

    gpr.Unlock(WB);

    if ((mask & (FPSCR_FEX | FPSCR_VX | FPSCR_ANY_X | FPSCR_ANY_E)) != 0)
      UpdateFPExceptionSummary(WA);
    STR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(fpscr));

    gpr.Unlock(WA);
  }

  if (inst.FM & 1)
    UpdateRoundingMode();
}
