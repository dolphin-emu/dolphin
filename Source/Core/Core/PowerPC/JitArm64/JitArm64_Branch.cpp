// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitArm64/Jit.h"

#include "Common/Arm64Emitter.h"
#include "Common/CommonTypes.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/PowerPC.h"

using namespace Arm64Gen;

void JitArm64::sc(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITBranchOff);

  gpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);
  fpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);

  ARM64Reg WA = gpr.GetReg();

  LDR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(Exceptions));
  ORR(WA, WA, LogicalImm(EXCEPTION_SYSCALL, 32));
  STR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(Exceptions));

  gpr.Unlock(WA);

  WriteExceptionExit(js.compilerPC + 4, false, true);
}

void JitArm64::rfi(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITBranchOff);

  gpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);
  fpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);

  // See Interpreter rfi for details
  const u32 mask = 0x87C0FFFF;
  const u32 clearMSR13 = 0xFFFBFFFF;  // Mask used to clear the bit MSR[13]
  // MSR = ((MSR & ~mask) | (SRR1 & mask)) & clearMSR13;
  // R0 = MSR location
  // R1 = MSR contents
  // R2 = Mask
  // R3 = Mask
  ARM64Reg WA = gpr.GetReg();
  ARM64Reg WB = gpr.GetReg();
  ARM64Reg WC = gpr.GetReg();

  LDR(IndexType::Unsigned, WC, PPC_REG, PPCSTATE_OFF(msr));

  ANDI2R(WC, WC, (~mask) & clearMSR13, WA);  // rD = Masked MSR

  LDR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF_SPR(SPR_SRR1));  // rB contains SRR1 here

  ANDI2R(WA, WA, mask & clearMSR13, WB);  // rB contains masked SRR1 here
  ORR(WA, WA, WC);                        // rB = Masked MSR OR masked SRR1

  STR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(msr));  // STR rB in to rA

  LDR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF_SPR(SPR_SRR0));
  gpr.Unlock(WB, WC);

  WriteExceptionExit(WA);
  gpr.Unlock(WA);
}

void JitArm64::bx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITBranchOff);

  if (inst.LK)
  {
    ARM64Reg WA = gpr.GetReg();
    MOVI2R(WA, js.compilerPC + 4);
    STR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF_SPR(SPR_LR));
    gpr.Unlock(WA);
  }

  if (!js.isLastInstruction)
  {
    if (inst.LK && !js.op->skipLRStack)
    {
      // We have to fake the stack as the RET instruction was not
      // found in the same block. This is a big overhead, but still
      // better than calling the dispatcher.
      FakeLKExit(js.compilerPC + 4);
    }
    return;
  }

  gpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);
  fpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);

  if (js.op->branchIsIdleLoop)
  {
    // make idle loops go faster
    ARM64Reg WA = gpr.GetReg();
    ARM64Reg XA = EncodeRegTo64(WA);

    MOVP2R(XA, &CoreTiming::GlobalIdle);
    BLR(XA);
    gpr.Unlock(WA);

    WriteExceptionExit(js.op->branchTo);
    return;
  }

  WriteExit(js.op->branchTo, inst.LK, js.compilerPC + 4);
}

void JitArm64::bcx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITBranchOff);

  ARM64Reg WA = gpr.GetReg();
  FixupBranch pCTRDontBranch;
  if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)  // Decrement and test CTR
  {
    LDR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF_SPR(SPR_CTR));
    SUBS(WA, WA, 1);
    STR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF_SPR(SPR_CTR));

    if (inst.BO & BO_BRANCH_IF_CTR_0)
      pCTRDontBranch = B(CC_NEQ);
    else
      pCTRDontBranch = B(CC_EQ);
  }

  FixupBranch pConditionDontBranch;

  if ((inst.BO & BO_DONT_CHECK_CONDITION) == 0)  // Test a CR bit
  {
    pConditionDontBranch =
        JumpIfCRFieldBit(inst.BI >> 2, 3 - (inst.BI & 3), !(inst.BO_2 & BO_BRANCH_IF_TRUE));
  }

  FixupBranch far_addr = B();
  SwitchToFarCode();
  SetJumpTarget(far_addr);

  if (inst.LK)
  {
    MOVI2R(WA, js.compilerPC + 4);
    STR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF_SPR(SPR_LR));
  }

  gpr.Flush(FlushMode::MaintainState, WA);
  fpr.Flush(FlushMode::MaintainState, ARM64Reg::INVALID_REG);

  if (js.op->branchIsIdleLoop)
  {
    // make idle loops go faster
    ARM64Reg XA = EncodeRegTo64(WA);

    MOVP2R(XA, &CoreTiming::GlobalIdle);
    BLR(XA);

    WriteExceptionExit(js.op->branchTo);
  }
  else
  {
    WriteExit(js.op->branchTo, inst.LK, js.compilerPC + 4);
  }

  SwitchToNearCode();

  if ((inst.BO & BO_DONT_CHECK_CONDITION) == 0)
    SetJumpTarget(pConditionDontBranch);
  if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)
    SetJumpTarget(pCTRDontBranch);

  if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
  {
    gpr.Flush(FlushMode::All, WA);
    fpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);
    WriteExit(js.compilerPC + 4);
  }

  gpr.Unlock(WA);
}

void JitArm64::bcctrx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITBranchOff);

  // Rare condition seen in (just some versions of?) Nintendo's NES Emulator
  // BO_2 == 001zy -> b if false
  // BO_2 == 011zy -> b if true
  FALLBACK_IF(!(inst.BO_2 & BO_DONT_CHECK_CONDITION));

  // bcctrx doesn't decrement and/or test CTR
  ASSERT_MSG(DYNA_REC, inst.BO_2 & BO_DONT_DECREMENT_FLAG,
             "bcctrx with decrement and test CTR option is invalid!");

  // BO_2 == 1z1zz -> b always

  // NPC = CTR & 0xfffffffc;
  gpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);
  fpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);

  if (inst.LK_3)
  {
    ARM64Reg WB = gpr.GetReg();
    MOVI2R(WB, js.compilerPC + 4);
    STR(IndexType::Unsigned, WB, PPC_REG, PPCSTATE_OFF_SPR(SPR_LR));
    gpr.Unlock(WB);
  }

  ARM64Reg WA = gpr.GetReg();

  LDR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF_SPR(SPR_CTR));
  AND(WA, WA, LogicalImm(~0x3, 32));

  WriteExit(WA, inst.LK_3, js.compilerPC + 4);

  gpr.Unlock(WA);
}

void JitArm64::bclrx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITBranchOff);

  bool conditional =
      (inst.BO & BO_DONT_DECREMENT_FLAG) == 0 || (inst.BO & BO_DONT_CHECK_CONDITION) == 0;

  ARM64Reg WA = gpr.GetReg();
  ARM64Reg WB = conditional || inst.LK ? gpr.GetReg() : ARM64Reg::INVALID_REG;

  FixupBranch pCTRDontBranch;
  if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)  // Decrement and test CTR
  {
    LDR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF_SPR(SPR_CTR));
    SUBS(WA, WA, 1);
    STR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF_SPR(SPR_CTR));

    if (inst.BO & BO_BRANCH_IF_CTR_0)
      pCTRDontBranch = B(CC_NEQ);
    else
      pCTRDontBranch = B(CC_EQ);
  }

  FixupBranch pConditionDontBranch;
  if ((inst.BO & BO_DONT_CHECK_CONDITION) == 0)  // Test a CR bit
  {
    pConditionDontBranch =
        JumpIfCRFieldBit(inst.BI >> 2, 3 - (inst.BI & 3), !(inst.BO_2 & BO_BRANCH_IF_TRUE));
  }

  if (conditional)
  {
    FixupBranch far_addr = B();
    SwitchToFarCode();
    SetJumpTarget(far_addr);
  }

  LDR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF_SPR(SPR_LR));
  AND(WA, WA, LogicalImm(~0x3, 32));

  if (inst.LK)
  {
    MOVI2R(WB, js.compilerPC + 4);
    STR(IndexType::Unsigned, WB, PPC_REG, PPCSTATE_OFF_SPR(SPR_LR));
  }

  gpr.Flush(conditional ? FlushMode::MaintainState : FlushMode::All, WB);
  fpr.Flush(conditional ? FlushMode::MaintainState : FlushMode::All, ARM64Reg::INVALID_REG);

  if (js.op->branchIsIdleLoop)
  {
    // make idle loops go faster
    ARM64Reg XA = EncodeRegTo64(WA);

    MOVP2R(XA, &CoreTiming::GlobalIdle);
    BLR(XA);

    WriteExceptionExit(js.op->branchTo);
  }
  else
  {
    WriteBLRExit(WA);
  }

  if (conditional)
    SwitchToNearCode();

  if ((inst.BO & BO_DONT_CHECK_CONDITION) == 0)
    SetJumpTarget(pConditionDontBranch);
  if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)
    SetJumpTarget(pCTRDontBranch);

  if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
  {
    gpr.Flush(FlushMode::All, WA);
    fpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);
    WriteExit(js.compilerPC + 4);
  }

  gpr.Unlock(WA);
  if (WB != ARM64Reg::INVALID_REG)
    gpr.Unlock(WB);
}
