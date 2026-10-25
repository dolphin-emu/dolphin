// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitPPC64/Jit.h"

#include "Common/CommonTypes.h"
#include "Common/PPC64Emitter.h"
#include "Core/CoreTiming.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PowerPC.h"

using namespace PPC64Gen;

// Branch Watch records branches in the interpreter, so branches fall back to it while it is on.

void JitPPC64::sc(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITBranchOff);

  FlushRegisters(FlushMode::All);
  {
    const auto tmp = gpr.GetScopedReg();
    LWZ(tmp, PPC_REG, PPCSTATE_OFF(Exceptions));
    ORI(tmp, tmp, EXCEPTION_SYSCALL);
    STW(tmp, PPC_REG, PPCSTATE_OFF(Exceptions));
  }
  WriteExceptionExit(js.compilerPC + 4, false, true);
}

void JitPPC64::bx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITBranchOff);
  FALLBACK_IF(IsBranchWatchEnabled());

  if (inst.LK)
  {
    const auto tmp = gpr.GetScopedReg();
    MOVI2R(tmp, js.compilerPC + 4);
    STW(tmp, PPC_REG, PPCSTATE_OFF_SPR(SPR_LR));
  }

  // PPCAnalyzer::Analyze() followed the branch, thus the next instruction of the block is the
  // destination of the branch, no need to do anything, except for a call whose return the block
  // doesn't follow too.
  if (js.op->branchAction == PPCAnalyst::BranchAction::Follow)
  {
    if (inst.LK && !js.op->skipLRStack)
      FakeLKExit(js.compilerPC + 4);
    return;
  }

  FlushRegisters(FlushMode::All);

  if (js.op->branchAction == PPCAnalyst::BranchAction::IdleLoop)
  {
    // make idle loops go faster
    ABI_CallFunction(&CoreTiming::GlobalIdle);
    WriteExceptionExit(js.op->branchTo);
    return;
  }

  WriteExit(js.op->branchTo, inst.LK, js.compilerPC + 4);
}

void JitPPC64::bcx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITBranchOff);
  FALLBACK_IF(IsBranchWatchEnabled());

  // The CR field is bound before the CTR test branches, so that the cache is the same on both
  // paths.
  if ((inst.BO & BO_DONT_CHECK_CONDITION) == 0)
    gpr.CR(inst.BI >> 2);

  FixupBranch ctr_dont_branch;
  if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)  // Decrement and test CTR
  {
    const auto tmp = gpr.GetScopedReg();
    LWZ(tmp, PPC_REG, PPCSTATE_OFF_SPR(SPR_CTR));
    ADDI(tmp, tmp, -1);
    STW(tmp, PPC_REG, PPCSTATE_OFF_SPR(SPR_CTR));
    CMPLWI(CRF::CR0, tmp, 0);
    ctr_dont_branch = BC((inst.BO & BO_BRANCH_IF_CTR_0) ? CCFlags::NE : CCFlags::EQ, CRF::CR0);
  }

  FixupBranch condition_dont_branch;
  if ((inst.BO & BO_DONT_CHECK_CONDITION) == 0)  // Test a CR bit
  {
    condition_dont_branch =
        JumpIfCRFieldBit(inst.BI >> 2, 3 - (inst.BI & 3), !(inst.BO_2 & BO_BRANCH_IF_TRUE));
  }

  if (inst.LK)
  {
    const auto tmp = gpr.GetScopedReg();
    MOVI2R(tmp, js.compilerPC + 4);
    STW(tmp, PPC_REG, PPCSTATE_OFF_SPR(SPR_LR));
  }

  // PPCAnalyzer::Analyze() followed this unconditional branch, thus the next instruction of the
  // block is the destination of the branch, no need to do anything, except for a call whose
  // return the block doesn't follow too.
  if (js.op->branchAction == PPCAnalyst::BranchAction::Follow)
  {
    if (inst.LK && !js.op->skipLRStack)
      FakeLKExit(js.compilerPC + 4);
    return;
  }

  // The code after the branch continues with the guest registers in host registers.
  FlushRegisters(FlushMode::MaintainState);

  if (js.op->branchAction == PPCAnalyst::BranchAction::IdleLoop)
  {
    // make idle loops go faster
    ABI_CallFunction(&CoreTiming::GlobalIdle);
    WriteExceptionExit(js.op->branchTo);
  }
  else
  {
    WriteExit(js.op->branchTo, inst.LK, js.compilerPC + 4);
  }

  if ((inst.BO & BO_DONT_CHECK_CONDITION) == 0)
    SetJumpTarget(condition_dont_branch);
  if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)
    SetJumpTarget(ctr_dont_branch);

  if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
  {
    FlushRegisters(FlushMode::All);
    WriteExit(js.compilerPC + 4);
  }
}

void JitPPC64::bcctrx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITBranchOff);
  FALLBACK_IF(IsBranchWatchEnabled());

  // Rare condition seen in (just some versions of?) Nintendo's NES Emulator
  // BO_2 == 001zy -> b if false
  // BO_2 == 011zy -> b if true
  FALLBACK_IF(!(inst.BO_2 & BO_DONT_CHECK_CONDITION));

  // bcctrx doesn't decrement and/or test CTR
  ASSERT_MSG(DYNA_REC, inst.BO_2 & BO_DONT_DECREMENT_FLAG,
             "bcctrx with decrement and test CTR option is invalid!");

  // BO_2 == 1z1zz -> b always

  // NPC = CTR & 0xfffffffc;
  FlushRegisters(FlushMode::All);

  LWZ(DISPATCHER_PC, PPC_REG, PPCSTATE_OFF_SPR(SPR_CTR));
  RLWINM(DISPATCHER_PC, DISPATCHER_PC, 0, 0, 29);
  if (inst.LK_3)
  {
    const auto tmp = gpr.GetScopedReg();
    MOVI2R(tmp, js.compilerPC + 4);
    STW(tmp, PPC_REG, PPCSTATE_OFF_SPR(SPR_LR));
  }

  WriteExit(DISPATCHER_PC, inst.LK_3, js.compilerPC + 4);
}

void JitPPC64::bclrx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITBranchOff);
  FALLBACK_IF(IsBranchWatchEnabled());

  // The CR field is bound before the CTR test branches, so that the cache is the same on both
  // paths.
  if ((inst.BO & BO_DONT_CHECK_CONDITION) == 0)
    gpr.CR(inst.BI >> 2);

  FixupBranch ctr_dont_branch;
  if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)  // Decrement and test CTR
  {
    const auto tmp = gpr.GetScopedReg();
    LWZ(tmp, PPC_REG, PPCSTATE_OFF_SPR(SPR_CTR));
    ADDI(tmp, tmp, -1);
    STW(tmp, PPC_REG, PPCSTATE_OFF_SPR(SPR_CTR));
    CMPLWI(CRF::CR0, tmp, 0);
    ctr_dont_branch = BC((inst.BO & BO_BRANCH_IF_CTR_0) ? CCFlags::NE : CCFlags::EQ, CRF::CR0);
  }

  FixupBranch condition_dont_branch;
  if ((inst.BO & BO_DONT_CHECK_CONDITION) == 0)  // Test a CR bit
  {
    condition_dont_branch =
        JumpIfCRFieldBit(inst.BI >> 2, 3 - (inst.BI & 3), !(inst.BO_2 & BO_BRANCH_IF_TRUE));
  }

  // The code after the branch continues with the guest registers in host registers.
  FlushRegisters(FlushMode::MaintainState);

  // NPC = LR & 0xfffffffc, read before LK writes LR.
  LWZ(DISPATCHER_PC, PPC_REG, PPCSTATE_OFF_SPR(SPR_LR));
  RLWINM(DISPATCHER_PC, DISPATCHER_PC, 0, 0, 29);
  if (inst.LK)
  {
    const auto tmp = gpr.GetScopedReg();
    MOVI2R(tmp, js.compilerPC + 4);
    STW(tmp, PPC_REG, PPCSTATE_OFF_SPR(SPR_LR));
  }

  if (js.op->branchAction == PPCAnalyst::BranchAction::IdleLoop)
  {
    // make idle loops go faster
    ABI_CallFunction(&CoreTiming::GlobalIdle);
    WriteExceptionExit(js.op->branchTo);
  }
  else if (inst.LK)
  {
    WriteExit(DISPATCHER_PC, true, js.compilerPC + 4);
  }
  else
  {
    WriteBLRExit(DISPATCHER_PC);
  }

  if ((inst.BO & BO_DONT_CHECK_CONDITION) == 0)
    SetJumpTarget(condition_dont_branch);
  if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)
    SetJumpTarget(ctr_dont_branch);

  if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
  {
    FlushRegisters(FlushMode::All);
    WriteExit(js.compilerPC + 4);
  }
}

void JitPPC64::rfi(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITBranchOff);

  // As the interpreter, without the privilege check, as the other JITs: restore the bits of MSR
  // that SRR1 saves, clear MSR[13], and return to SRR0, taking a pending exception.
  FlushRegisters(FlushMode::All);
  constexpr u32 mask = 0x87C0FFFF;
  constexpr u32 clear_msr13 = 0xFFFBFFFF;
  {
    const auto msr = gpr.GetScopedReg();
    const auto tmp = gpr.GetScopedReg();
    const auto srr1 = gpr.GetScopedReg();
    LWZ(msr, PPC_REG, PPCSTATE_OFF(msr));
    MOVI2R(tmp, ~mask & clear_msr13);
    AND(msr, msr, tmp);
    LWZ(srr1, PPC_REG, PPCSTATE_OFF_SPR(SPR_SRR1));
    MOVI2R(tmp, mask & clear_msr13);
    AND(srr1, srr1, tmp);
    OR(msr, msr, srr1);
    STW(msr, PPC_REG, PPCSTATE_OFF(msr));
    EmitMSRUpdated(msr);
  }
  EmitPendingPageTableUpdate();

  LWZ(DISPATCHER_PC, PPC_REG, PPCSTATE_OFF_SPR(SPR_SRR0));
  WriteExceptionExit(DISPATCHER_PC);
}
