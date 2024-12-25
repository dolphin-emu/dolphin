// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitArm64/Jit.h"

#include "Common/Arm64Emitter.h"
#include "Common/CommonTypes.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/Debugger/BranchWatch.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

using namespace Arm64Gen;

void JitArm64::sc(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITBranchOff);

  gpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);
  fpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);

  {
    auto WA = gpr.GetScopedReg();
    LDR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(Exceptions));
    ORR(WA, WA, LogicalImm(EXCEPTION_SYSCALL, GPRSize::B32));
    STR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(Exceptions));
  }

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
  auto WA = gpr.GetScopedReg();
  {
    auto WB = gpr.GetScopedReg();
    auto WC = gpr.GetScopedReg();

    LDR(IndexType::Unsigned, WC, PPC_REG, PPCSTATE_OFF(msr));

    ANDI2R(WC, WC, (~mask) & clearMSR13, WA);  // rD = Masked MSR

    LDR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF_SPR(SPR_SRR1));  // rB contains SRR1 here

    ANDI2R(WA, WA, mask & clearMSR13, WB);  // rB contains masked SRR1 here
    ORR(WA, WA, WC);                        // rB = Masked MSR OR masked SRR1

    STR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(msr));  // STR rB in to rA
  }

  MSRUpdated(WA);

  LDR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF_SPR(SPR_SRR0));

  WriteExceptionExit(WA);
}

template <bool condition>
void JitArm64::WriteBranchWatch(u32 origin, u32 destination, UGeckoInstruction inst, ARM64Reg reg_a,
                                ARM64Reg reg_b, BitSet32 gpr_caller_save, BitSet32 fpr_caller_save)
{
  const ARM64Reg branch_watch = EncodeRegTo64(reg_a);
  MOVP2R(branch_watch, &m_branch_watch);
  LDRB(IndexType::Unsigned, reg_b, branch_watch, Core::BranchWatch::GetOffsetOfRecordingActive());
  FixupBranch branch_over = CBZ(reg_b);

  FixupBranch branch_in = B();
  SwitchToFarCode();
  SetJumpTarget(branch_in);

  const ARM64Reg float_emit_tmp = EncodeRegTo64(reg_b);
  ABI_PushRegisters(gpr_caller_save);
  m_float_emit.ABI_PushRegisters(fpr_caller_save, float_emit_tmp);
  ABI_CallFunction(m_ppc_state.msr.IR ? (condition ? &Core::BranchWatch::HitVirtualTrue_fk :
                                                     &Core::BranchWatch::HitVirtualFalse_fk) :
                                        (condition ? &Core::BranchWatch::HitPhysicalTrue_fk :
                                                     &Core::BranchWatch::HitPhysicalFalse_fk),
                   branch_watch, Core::FakeBranchWatchCollectionKey{origin, destination}, inst.hex);
  m_float_emit.ABI_PopRegisters(fpr_caller_save, float_emit_tmp);
  ABI_PopRegisters(gpr_caller_save);

  FixupBranch branch_out = B();
  SwitchToNearCode();
  SetJumpTarget(branch_out);
  SetJumpTarget(branch_over);
}

template void JitArm64::WriteBranchWatch<true>(u32, u32, UGeckoInstruction, ARM64Reg, ARM64Reg,
                                               BitSet32, BitSet32);
template void JitArm64::WriteBranchWatch<false>(u32, u32, UGeckoInstruction, ARM64Reg, ARM64Reg,
                                                BitSet32, BitSet32);

void JitArm64::WriteBranchWatchDestInRegister(u32 origin, ARM64Reg destination,
                                              UGeckoInstruction inst, ARM64Reg reg_a,
                                              ARM64Reg reg_b, BitSet32 gpr_caller_save,
                                              BitSet32 fpr_caller_save)
{
  const ARM64Reg branch_watch = EncodeRegTo64(reg_a);
  MOVP2R(branch_watch, &m_branch_watch);
  LDRB(IndexType::Unsigned, reg_b, branch_watch, Core::BranchWatch::GetOffsetOfRecordingActive());
  FixupBranch branch_over = CBZ(reg_b);

  FixupBranch branch_in = B();
  SwitchToFarCode();
  SetJumpTarget(branch_in);

  const ARM64Reg float_emit_tmp = EncodeRegTo64(reg_b);
  ABI_PushRegisters(gpr_caller_save);
  m_float_emit.ABI_PushRegisters(fpr_caller_save, float_emit_tmp);
  ABI_CallFunction(m_ppc_state.msr.IR ? &Core::BranchWatch::HitVirtualTrue :
                                        &Core::BranchWatch::HitPhysicalTrue,
                   branch_watch, origin, destination, inst.hex);
  m_float_emit.ABI_PopRegisters(fpr_caller_save, float_emit_tmp);
  ABI_PopRegisters(gpr_caller_save);

  FixupBranch branch_out = B();
  SwitchToNearCode();
  SetJumpTarget(branch_out);
  SetJumpTarget(branch_over);
}

void JitArm64::bx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITBranchOff);

  Arm64GPRCache::ScopedARM64Reg WA = ARM64Reg::INVALID_REG;
  if (inst.LK)
  {
    WA = gpr.GetScopedReg();
    MOVI2R(WA, js.compilerPC + 4);
    STR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF_SPR(SPR_LR));
  }

  if (!js.isLastInstruction)
  {
    if (IsDebuggingEnabled())
    {
      const auto WB = gpr.GetScopedReg(), WC = gpr.GetScopedReg();
      BitSet32 gpr_caller_save = gpr.GetCallerSavedUsed() & ~BitSet32{DecodeReg(WB), DecodeReg(WC)};
      if (WA != ARM64Reg::INVALID_REG && js.op->skipLRStack)
        gpr_caller_save[DecodeReg(WA)] = false;
      WriteBranchWatch<true>(js.compilerPC, js.op->branchTo, inst, WB, WC, gpr_caller_save,
                             fpr.GetCallerSavedUsed());
    }
    if (inst.LK && !js.op->skipLRStack)
    {
      // We have to fake the stack as the RET instruction was not
      // found in the same block. This is a big overhead, but still
      // better than calling the dispatcher.
      FakeLKExit(js.compilerPC + 4, WA);
    }

    return;
  }

  gpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);
  fpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);

  if (js.op->branchIsIdleLoop)
  {
    if (WA == ARM64Reg::INVALID_REG)
      WA = gpr.GetScopedReg();

    if (IsDebuggingEnabled())
    {
      const auto WB = gpr.GetScopedReg();
      WriteBranchWatch<true>(js.compilerPC, js.op->branchTo, inst, WA, WB, {}, {});
    }

    // make idle loops go faster
    ARM64Reg XA = EncodeRegTo64(WA);

    MOVP2R(XA, &CoreTiming::GlobalIdle);
    BLR(XA);
    WA.Unlock();

    WriteExceptionExit(js.op->branchTo);
    return;
  }

  if (IsDebuggingEnabled())
  {
    const auto WB = gpr.GetScopedReg(), WC = gpr.GetScopedReg();
    const BitSet32 gpr_caller_save =
        WA != ARM64Reg::INVALID_REG ? BitSet32{DecodeReg(WA)} & CALLER_SAVED_GPRS : BitSet32{};
    WriteBranchWatch<true>(js.compilerPC, js.op->branchTo, inst, WB, WC, gpr_caller_save, {});
  }
  WriteExit(js.op->branchTo, inst.LK, js.compilerPC + 4, WA);
}

void JitArm64::bcx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITBranchOff);

  auto WA = gpr.GetScopedReg();
  auto WB = inst.LK || IsDebuggingEnabled() ? gpr.GetScopedReg() :
                                              Arm64GPRCache::ScopedARM64Reg(WA.GetReg());

  {
    auto WC = IsDebuggingEnabled() && inst.LK && !js.op->branchIsIdleLoop ?
                  gpr.GetScopedReg() :
                  Arm64GPRCache::ScopedARM64Reg(ARM64Reg::INVALID_REG);

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

    if (inst.LK)
    {
      MOVI2R(WA, js.compilerPC + 4);
      STR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF_SPR(SPR_LR));
    }

    gpr.Flush(FlushMode::MaintainState, WB);
    fpr.Flush(FlushMode::MaintainState, ARM64Reg::INVALID_REG);

    if (IsDebuggingEnabled())
    {
      ARM64Reg bw_reg_a, bw_reg_b;
      // WC is only allocated when WA is needed for WriteExit and cannot be clobbered.
      if (WC == ARM64Reg::INVALID_REG)
        bw_reg_a = WA, bw_reg_b = WB;
      else
        bw_reg_a = WB, bw_reg_b = WC;
      const BitSet32 gpr_caller_save =
          gpr.GetCallerSavedUsed() & ~BitSet32{DecodeReg(bw_reg_a), DecodeReg(bw_reg_b)};
      WriteBranchWatch<true>(js.compilerPC, js.op->branchTo, inst, bw_reg_a, bw_reg_b,
                             gpr_caller_save, fpr.GetCallerSavedUsed());
    }
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
      WriteExit(js.op->branchTo, inst.LK, js.compilerPC + 4, WA);
    }

    if ((inst.BO & BO_DONT_CHECK_CONDITION) == 0)
      SetJumpTarget(pConditionDontBranch);
    if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)
      SetJumpTarget(pCTRDontBranch);
  }

  if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
  {
    gpr.Flush(FlushMode::All, WA);
    fpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);
    if (IsDebuggingEnabled())
    {
      WriteBranchWatch<false>(js.compilerPC, js.compilerPC + 4, inst, WA, WB, {}, {});
    }
    WriteExit(js.compilerPC + 4);
  }
  else if (IsDebuggingEnabled())
  {
    const BitSet32 gpr_caller_save =
        gpr.GetCallerSavedUsed() & ~BitSet32{DecodeReg(WA), DecodeReg(WB)};
    WriteBranchWatch<false>(js.compilerPC, js.compilerPC + 4, inst, WA, WB, gpr_caller_save,
                            fpr.GetCallerSavedUsed());
  }
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

  Arm64GPRCache::ScopedARM64Reg WB = ARM64Reg::INVALID_REG;
  if (inst.LK_3)
  {
    WB = gpr.GetScopedReg();
    MOVI2R(WB, js.compilerPC + 4);
    STR(IndexType::Unsigned, WB, PPC_REG, PPCSTATE_OFF_SPR(SPR_LR));
  }

  auto WA = gpr.GetScopedReg();

  LDR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF_SPR(SPR_CTR));
  AND(WA, WA, LogicalImm(~0x3, GPRSize::B32));

  if (IsDebuggingEnabled())
  {
    const auto WC = gpr.GetScopedReg(), WD = gpr.GetScopedReg();
    BitSet32 gpr_caller_save = BitSet32{DecodeReg(WA)};
    if (WB != ARM64Reg::INVALID_REG)
      gpr_caller_save[DecodeReg(WB)] = true;
    gpr_caller_save &= CALLER_SAVED_GPRS;
    WriteBranchWatchDestInRegister(js.compilerPC, WA, inst, WC, WD, gpr_caller_save, {});
  }
  WriteExit(WA, inst.LK_3, js.compilerPC + 4, WB);
}

void JitArm64::bclrx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITBranchOff);

  bool conditional =
      (inst.BO & BO_DONT_DECREMENT_FLAG) == 0 || (inst.BO & BO_DONT_CHECK_CONDITION) == 0;

  auto WA = gpr.GetScopedReg();
  Arm64GPRCache::ScopedARM64Reg WB;
  if (conditional || inst.LK || IsDebuggingEnabled())
  {
    WB = gpr.GetScopedReg();
  }

  {
    Arm64GPRCache::ScopedARM64Reg WC;
    if (IsDebuggingEnabled())
    {
      WC = gpr.GetScopedReg();
    }

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

    LDR(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF_SPR(SPR_LR));
    AND(WA, WA, LogicalImm(~0x3, GPRSize::B32));

    if (inst.LK)
    {
      MOVI2R(WB, js.compilerPC + 4);
      STR(IndexType::Unsigned, WB, PPC_REG, PPCSTATE_OFF_SPR(SPR_LR));
    }

    gpr.Flush(conditional ? FlushMode::MaintainState : FlushMode::All, WB);
    fpr.Flush(conditional ? FlushMode::MaintainState : FlushMode::All, ARM64Reg::INVALID_REG);

    if (IsDebuggingEnabled())
    {
      BitSet32 gpr_caller_save;
      BitSet32 fpr_caller_save;
      if (conditional)
      {
        gpr_caller_save = gpr.GetCallerSavedUsed() & ~BitSet32{DecodeReg(WB), DecodeReg(WC)};
        if (js.op->branchIsIdleLoop)
          gpr_caller_save[DecodeReg(WA)] = false;
        fpr_caller_save = fpr.GetCallerSavedUsed();
      }
      else
      {
        gpr_caller_save =
            js.op->branchIsIdleLoop ? BitSet32{} : BitSet32{DecodeReg(WA)} & CALLER_SAVED_GPRS;
        fpr_caller_save = {};
      }
      WriteBranchWatchDestInRegister(js.compilerPC, WA, inst, WB, WC, gpr_caller_save,
                                     fpr_caller_save);
    }
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

    if ((inst.BO & BO_DONT_CHECK_CONDITION) == 0)
      SetJumpTarget(pConditionDontBranch);
    if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)
      SetJumpTarget(pCTRDontBranch);
  }

  if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
  {
    gpr.Flush(FlushMode::All, WA);
    fpr.Flush(FlushMode::All, ARM64Reg::INVALID_REG);
    if (IsDebuggingEnabled())
    {
      WriteBranchWatch<false>(js.compilerPC, js.compilerPC + 4, inst, WA, WB, {}, {});
    }
    WriteExit(js.compilerPC + 4);
  }
  else if (IsDebuggingEnabled())
  {
    const BitSet32 gpr_caller_save =
        gpr.GetCallerSavedUsed() & ~BitSet32{DecodeReg(WA), DecodeReg(WB)};
    WriteBranchWatch<false>(js.compilerPC, js.compilerPC + 4, inst, WA, WB, gpr_caller_save,
                            fpr.GetCallerSavedUsed());
  }
}
