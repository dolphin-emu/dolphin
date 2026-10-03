// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Jit64/Jit.h"

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"
#include "Core/Debugger/BranchWatch.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/Jit64/RegCache/JitRegCache.h"
#include "Core/PowerPC/Jit64Common/Jit64PowerPCState.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

// The branches are known good, or at least reasonably good.
// No need for a disable-mechanism.

// If defined, clears CR0 at blr and bl-s. If the assumption that
// flags never carry over between functions holds, then the task for
// an optimizer becomes much easier.

// #define ACID_TEST

// Zelda and many more games seem to pass the Acid Test.

using namespace Gen;

void Jit64::sc(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITBranchOff);

  gpr.Flush();
  fpr.Flush();
  MOV(32, PPCSTATE(pc), Imm32(js.compilerPC + 4));
  LOCK();
  OR(32, PPCSTATE(Exceptions), Imm32(EXCEPTION_SYSCALL));
  WriteExceptionExit();
}

void Jit64::rfi(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITBranchOff);

  gpr.Flush();
  fpr.Flush();

  // See Interpreter rfi for details
  const u32 mask = 0x87C0FFFF;
  const u32 clearMSR13 = 0xFFFBFFFF;  // Mask used to clear the bit MSR[13]
  // MSR = ((MSR & ~mask) | (SRR1 & mask)) & clearMSR13;
  AND(32, PPCSTATE(msr), Imm32((~mask) & clearMSR13));
  MOV(32, R(RSCRATCH), PPCSTATE_SRR1);
  AND(32, R(RSCRATCH), Imm32(mask & clearMSR13));
  OR(32, PPCSTATE(msr), R(RSCRATCH));

  // Call MSRUpdated to update feature_flags. Only the bits that come from SRR1
  // are relevant for this, so it's fine to pass in RSCRATCH in place of msr.
  MSRUpdated(R(RSCRATCH), RSCRATCH2);

  // NPC = SRR0;
  MOV(32, R(RSCRATCH), PPCSTATE_SRR0);
  WriteRfiExitDestInRSCRATCH();
}

template <bool condition>
void Jit64::WriteBranchWatch(u32 origin, u32 destination, UGeckoInstruction inst,
                             BitSet32 caller_save)
{
  if (IsBranchWatchEnabled())
  {
    ABI_PushRegistersAndAdjustStack(caller_save, 0);
    MOV(64, R(ABI_PARAM1), ImmPtr(&m_branch_watch));
    MOV(64, R(ABI_PARAM2), Imm64(Core::FakeBranchWatchCollectionKey{origin, destination}));
    MOV(32, R(ABI_PARAM3), Imm32(inst.hex));
    ABI_CallFunction(m_ppc_state.msr.IR ? (condition ? &Core::BranchWatch::HitVirtualTrue_fk :
                                                       &Core::BranchWatch::HitVirtualFalse_fk) :
                                          (condition ? &Core::BranchWatch::HitPhysicalTrue_fk :
                                                       &Core::BranchWatch::HitPhysicalFalse_fk));
    ABI_PopRegistersAndAdjustStack(caller_save, 0);
  }
}

template void Jit64::WriteBranchWatch<true>(u32, u32, UGeckoInstruction, BitSet32);
template void Jit64::WriteBranchWatch<false>(u32, u32, UGeckoInstruction, BitSet32);

void Jit64::WriteBranchWatchDestInRSCRATCH(u32 origin, UGeckoInstruction inst, BitSet32 caller_save)
{
  if (IsBranchWatchEnabled())
  {
    // Assert RSCRATCH won't be clobbered before it is moved from.
    static_assert(ABI_PARAM1 != RSCRATCH);

    ABI_PushRegistersAndAdjustStack(caller_save, 0);
    MOV(64, R(ABI_PARAM1), ImmPtr(&m_branch_watch));
    MOV(32, R(ABI_PARAM3), R(RSCRATCH));
    MOV(32, R(ABI_PARAM2), Imm32(origin));
    MOV(32, R(ABI_PARAM4), Imm32(inst.hex));
    ABI_CallFunction(m_ppc_state.msr.IR ? &Core::BranchWatch::HitVirtualTrue :
                                          &Core::BranchWatch::HitPhysicalTrue);
    ABI_PopRegistersAndAdjustStack(caller_save, 0);
  }
}

void Jit64::bx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITBranchOff);

  // We must always process the following sentence
  // even if the blocks are merged by PPCAnalyst::Flatten().
  if (inst.LK)
    MOV(32, PPCSTATE_LR, Imm32(js.compilerPC + 4));

  // If this is not the last instruction of a block,
  // we will skip the rest process.
  // Because PPCAnalyst::Flatten() merged the blocks.
  if (!js.isLastInstruction)
  {
    WriteBranchWatch<true>(js.compilerPC, js.op->branchTo, inst, CallerSavedRegistersInUse());
    if (inst.LK && !js.op->skipLRStack)
    {
      // We have to fake the stack as the RET instruction was not
      // found in the same block. This is a big overhead, but still
      // better than calling the dispatcher.
      FakeBLCall(js.compilerPC + 4);
    }
    return;
  }

  gpr.Flush();
  fpr.Flush();

  WriteBranchWatch<true>(js.compilerPC, js.op->branchTo, inst, {});
#ifdef ACID_TEST
  if (inst.LK)
    AND(32, PPCSTATE(cr), Imm32(~(0xFF000000)));
#endif
  if (js.op->branchIsIdleLoop)
  {
    WriteIdleExit(js.op->branchTo);
  }
  else
  {
    WriteExit(js.op->branchTo, inst.LK, js.compilerPC + 4);
  }
}

// TODO - optimize to hell and beyond
// TODO - make nice easy to optimize special cases for the most common
// variants of this instruction.
void Jit64::bcx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITBranchOff);

  // USES_CR

  FixupBranch pCTRDontBranch;
  if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)  // Decrement and test CTR
  {
    SUB(32, PPCSTATE_CTR, Imm8(1));
    if (inst.BO & BO_BRANCH_IF_CTR_0)
      pCTRDontBranch = J_CC(CC_NZ, Jump::Near);
    else
      pCTRDontBranch = J_CC(CC_Z, Jump::Near);
  }

  FixupBranch pConditionDontBranch;
  if ((inst.BO & BO_DONT_CHECK_CONDITION) == 0)  // Test a CR bit
  {
    pConditionDontBranch =
        JumpIfCRFieldBit(inst.BI >> 2, 3 - (inst.BI & 3), !(inst.BO_2 & BO_BRANCH_IF_TRUE));
  }

  if (inst.LK)
    MOV(32, PPCSTATE_LR, Imm32(js.compilerPC + 4));

  // If this is not the last instruction of a block
  // and an unconditional branch, we will skip the rest process.
  // Because PPCAnalyst::Flatten() merged the blocks.
  if (!js.isLastInstruction && (inst.BO & BO_DONT_DECREMENT_FLAG) &&
      (inst.BO & BO_DONT_CHECK_CONDITION))
  {
    WriteBranchWatch<true>(js.compilerPC, js.op->branchTo, inst, CallerSavedRegistersInUse());
    if (inst.LK && !js.op->skipLRStack)
    {
      // We have to fake the stack as the RET instruction was not
      // found in the same block. This is a big overhead, but still
      // better than calling the dispatcher.
      FakeBLCall(js.compilerPC + 4);
    }
    return;
  }

  {
    RCForkGuard gpr_guard = gpr.Fork();
    RCForkGuard fpr_guard = fpr.Fork();
    gpr.Flush();
    fpr.Flush();

    WriteBranchWatch<true>(js.compilerPC, js.op->branchTo, inst, {});
    if (js.op->branchIsIdleLoop)
    {
      WriteIdleExit(js.op->branchTo);
    }
    else
    {
      WriteExit(js.op->branchTo, inst.LK, js.compilerPC + 4);
    }
  }

  if ((inst.BO & BO_DONT_CHECK_CONDITION) == 0)
    SetJumpTarget(pConditionDontBranch);
  if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)
    SetJumpTarget(pCTRDontBranch);

  if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
  {
    gpr.Flush();
    fpr.Flush();
    WriteBranchWatch<false>(js.compilerPC, js.compilerPC + 4, inst, {});
    WriteExit(js.compilerPC + 4);
  }
  WriteBranchWatch<false>(js.compilerPC, js.compilerPC + 4, inst, CallerSavedRegistersInUse());
}

void Jit64::bcctrx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITBranchOff);

  // bcctrx doesn't decrement and/or test CTR
  DEBUG_ASSERT_MSG(POWERPC, inst.BO_2 & BO_DONT_DECREMENT_FLAG,
                   "bcctrx with decrement and test CTR option is invalid!");

  if (inst.BO_2 & BO_DONT_CHECK_CONDITION)
  {
    // BO_2 == 1z1zz -> b always

    // NPC = CTR & 0xfffffffc;
    gpr.Flush();
    fpr.Flush();

    MOV(32, R(RSCRATCH), PPCSTATE_CTR);
    if (inst.LK_3)
      MOV(32, PPCSTATE_LR, Imm32(js.compilerPC + 4));  // LR = PC + 4;
    AND(32, R(RSCRATCH), Imm32(0xFFFFFFFC));
    WriteBranchWatchDestInRSCRATCH(js.compilerPC, inst, BitSet32{RSCRATCH});
    WriteExitDestInRSCRATCH(inst.LK_3, js.compilerPC + 4);
  }
  else
  {
    // Rare condition seen in (just some versions of?) Nintendo's NES Emulator

    // BO_2 == 001zy -> b if false
    // BO_2 == 011zy -> b if true

    FixupBranch b =
        JumpIfCRFieldBit(inst.BI >> 2, 3 - (inst.BI & 3), !(inst.BO_2 & BO_BRANCH_IF_TRUE));
    MOV(32, R(RSCRATCH), PPCSTATE_CTR);
    AND(32, R(RSCRATCH), Imm32(0xFFFFFFFC));
    // MOV(32, PPCSTATE(pc), R(RSCRATCH)); => Already done in WriteExitDestInRSCRATCH()
    if (inst.LK_3)
      MOV(32, PPCSTATE_LR, Imm32(js.compilerPC + 4));  // LR = PC + 4;

    {
      RCForkGuard gpr_guard = gpr.Fork();
      RCForkGuard fpr_guard = fpr.Fork();
      gpr.Flush();
      fpr.Flush();
      WriteBranchWatchDestInRSCRATCH(js.compilerPC, inst, BitSet32{RSCRATCH});
      WriteExitDestInRSCRATCH(inst.LK_3, js.compilerPC + 4);
      // Would really like to continue the block here, but it ends. TODO.
    }
    SetJumpTarget(b);

    if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
    {
      gpr.Flush();
      fpr.Flush();
      WriteBranchWatch<false>(js.compilerPC, js.compilerPC + 4, inst, {});
      WriteExit(js.compilerPC + 4);
    }
    WriteBranchWatch<false>(js.compilerPC, js.compilerPC + 4, inst, CallerSavedRegistersInUse());
  }
}

void Jit64::bclrx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITBranchOff);

  FixupBranch pCTRDontBranch;
  if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)  // Decrement and test CTR
  {
    SUB(32, PPCSTATE_CTR, Imm8(1));
    if (inst.BO & BO_BRANCH_IF_CTR_0)
      pCTRDontBranch = J_CC(CC_NZ, Jump::Near);
    else
      pCTRDontBranch = J_CC(CC_Z, Jump::Near);
  }

  FixupBranch pConditionDontBranch;
  if ((inst.BO & BO_DONT_CHECK_CONDITION) == 0)  // Test a CR bit
  {
    pConditionDontBranch =
        JumpIfCRFieldBit(inst.BI >> 2, 3 - (inst.BI & 3), !(inst.BO_2 & BO_BRANCH_IF_TRUE));
  }

// This below line can be used to prove that blr "eats flags" in practice.
// This observation could let us do some useful optimizations.
#ifdef ACID_TEST
  AND(32, PPCSTATE(cr), Imm32(~(0xFF000000)));
#endif

  MOV(32, R(RSCRATCH), PPCSTATE_LR);
  // We don't have to do this because WriteBLRExit handles it for us. Specifically, since we only
  // ever push divisible-by-four instruction addresses onto the stack, if the return address
  // matches, we're already good. If it doesn't match, the mispredicted-BLR code handles the fixup.
  if (!m_enable_blr_optimization)
    AND(32, R(RSCRATCH), Imm32(0xFFFFFFFC));
  if (inst.LK)
    MOV(32, PPCSTATE_LR, Imm32(js.compilerPC + 4));

  {
    RCForkGuard gpr_guard = gpr.Fork();
    RCForkGuard fpr_guard = fpr.Fork();
    gpr.Flush();
    fpr.Flush();

    if (js.op->branchIsIdleLoop)
    {
      WriteBranchWatch<true>(js.compilerPC, js.op->branchTo, inst, {});
      WriteIdleExit(js.op->branchTo);
    }
    else
    {
      WriteBranchWatchDestInRSCRATCH(js.compilerPC, inst, BitSet32{RSCRATCH});
      WriteBLRExit();
    }
  }

  if ((inst.BO & BO_DONT_CHECK_CONDITION) == 0)
    SetJumpTarget(pConditionDontBranch);
  if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)
    SetJumpTarget(pCTRDontBranch);

  if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
  {
    gpr.Flush();
    fpr.Flush();
    WriteBranchWatch<false>(js.compilerPC, js.compilerPC + 4, inst, {});
    WriteExit(js.compilerPC + 4);
  }
  else
  {
    WriteBranchWatch<false>(js.compilerPC, js.compilerPC + 4, inst, CallerSavedRegistersInUse());
  }
}
