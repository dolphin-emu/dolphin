// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Arm64Emitter.h"
#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/PowerPC.h"

using namespace Arm64Gen;

void JitArm64::psq_lXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStorePairedOff);
  FALLBACK_IF(jo.memcheck || !jo.fastmem);

  // The asm routines assume address translation is on.
  FALLBACK_IF(!MSR.DR);

  // X30 is LR
  // X0 contains the scale
  // X1 is the address
  // X2 is a temporary
  // Q0 is the return register
  // Q1 is a temporary
  const s32 offset = inst.SIMM_12;
  const bool indexed = inst.OPCD == 4;
  const bool update = inst.OPCD == 57 || (inst.OPCD == 4 && !!(inst.SUBOP6 & 32));
  const int i = indexed ? inst.Ix : inst.I;
  const int w = indexed ? inst.Wx : inst.W;

  gpr.Lock(ARM64Reg::W0, ARM64Reg::W1, ARM64Reg::W2, ARM64Reg::W30);
  fpr.Lock(ARM64Reg::Q0, ARM64Reg::Q1);

  const ARM64Reg arm_addr = gpr.R(inst.RA);
  constexpr ARM64Reg scale_reg = ARM64Reg::W0;
  constexpr ARM64Reg addr_reg = ARM64Reg::W1;
  constexpr ARM64Reg type_reg = ARM64Reg::W2;
  ARM64Reg VS;

  if (inst.RA || update)  // Always uses the register on update
  {
    if (indexed)
      ADD(addr_reg, arm_addr, gpr.R(inst.RB));
    else if (offset >= 0)
      ADD(addr_reg, arm_addr, offset);
    else
      SUB(addr_reg, arm_addr, std::abs(offset));
  }
  else
  {
    if (indexed)
      MOV(addr_reg, gpr.R(inst.RB));
    else
      MOVI2R(addr_reg, (u32)offset);
  }

  if (update)
  {
    gpr.BindToRegister(inst.RA, false);
    MOV(arm_addr, addr_reg);
  }

  if (js.assumeNoPairedQuantize)
  {
    VS = fpr.RW(inst.RS, RegType::Single);
    if (!w)
    {
      ADD(EncodeRegTo64(addr_reg), EncodeRegTo64(addr_reg), MEM_REG);
      m_float_emit.LD1(32, 1, EncodeRegToDouble(VS), EncodeRegTo64(addr_reg));
    }
    else
    {
      m_float_emit.LDR(32, VS, EncodeRegTo64(addr_reg), MEM_REG);
    }
    m_float_emit.REV32(8, EncodeRegToDouble(VS), EncodeRegToDouble(VS));
  }
  else
  {
    LDR(IndexType::Unsigned, scale_reg, PPC_REG, PPCSTATE_OFF_SPR(SPR_GQR0 + i));
    UBFM(type_reg, scale_reg, 16, 18);   // Type
    UBFM(scale_reg, scale_reg, 24, 29);  // Scale

    MOVP2R(ARM64Reg::X30, w ? single_load_quantized : paired_load_quantized);
    LDR(EncodeRegTo64(type_reg), ARM64Reg::X30, ArithOption(EncodeRegTo64(type_reg), true));
    BLR(EncodeRegTo64(type_reg));

    VS = fpr.RW(inst.RS, RegType::Single);
    m_float_emit.ORR(EncodeRegToDouble(VS), ARM64Reg::D0, ARM64Reg::D0);
  }

  if (w)
  {
    m_float_emit.FMOV(ARM64Reg::S0, 0x70);  // 1.0 as a Single
    m_float_emit.INS(32, VS, 1, ARM64Reg::Q0, 0);
  }

  gpr.Unlock(ARM64Reg::W0, ARM64Reg::W1, ARM64Reg::W2, ARM64Reg::W30);
  fpr.Unlock(ARM64Reg::Q0, ARM64Reg::Q1);
}

void JitArm64::psq_stXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStorePairedOff);
  FALLBACK_IF(jo.memcheck || !jo.fastmem);

  // The asm routines assume address translation is on.
  FALLBACK_IF(!MSR.DR);

  // X30 is LR
  // X0 contains the scale
  // X1 is the address
  // Q0 is the store register

  const s32 offset = inst.SIMM_12;
  const bool indexed = inst.OPCD == 4;
  const bool update = inst.OPCD == 61 || (inst.OPCD == 4 && !!(inst.SUBOP6 & 32));
  const int i = indexed ? inst.Ix : inst.I;
  const int w = indexed ? inst.Wx : inst.W;

  fpr.Lock(ARM64Reg::Q0, ARM64Reg::Q1);

  const bool have_single = fpr.IsSingle(inst.RS);

  ARM64Reg VS = fpr.R(inst.RS, have_single ? RegType::Single : RegType::Register);

  if (js.assumeNoPairedQuantize)
  {
    if (!have_single)
    {
      const ARM64Reg single_reg = fpr.GetReg();

      if (w)
        m_float_emit.FCVT(32, 64, EncodeRegToDouble(single_reg), EncodeRegToDouble(VS));
      else
        m_float_emit.FCVTN(32, EncodeRegToDouble(single_reg), EncodeRegToDouble(VS));

      VS = single_reg;
    }
  }
  else
  {
    if (have_single)
    {
      m_float_emit.ORR(ARM64Reg::D0, VS, VS);
    }
    else
    {
      if (w)
        m_float_emit.FCVT(32, 64, ARM64Reg::D0, VS);
      else
        m_float_emit.FCVTN(32, ARM64Reg::D0, VS);
    }
  }

  gpr.Lock(ARM64Reg::W0, ARM64Reg::W1, ARM64Reg::W2, ARM64Reg::W30);

  const ARM64Reg arm_addr = gpr.R(inst.RA);
  constexpr ARM64Reg scale_reg = ARM64Reg::W0;
  constexpr ARM64Reg addr_reg = ARM64Reg::W1;
  constexpr ARM64Reg type_reg = ARM64Reg::W2;

  BitSet32 gprs_in_use = gpr.GetCallerSavedUsed();
  BitSet32 fprs_in_use = fpr.GetCallerSavedUsed();

  // Wipe the registers we are using as temporaries
  gprs_in_use &= BitSet32(~7);
  fprs_in_use &= BitSet32(~3);

  if (inst.RA || update)  // Always uses the register on update
  {
    if (indexed)
      ADD(addr_reg, arm_addr, gpr.R(inst.RB));
    else if (offset >= 0)
      ADD(addr_reg, arm_addr, offset);
    else
      SUB(addr_reg, arm_addr, std::abs(offset));
  }
  else
  {
    if (indexed)
      MOV(addr_reg, gpr.R(inst.RB));
    else
      MOVI2R(addr_reg, (u32)offset);
  }

  if (update)
  {
    gpr.BindToRegister(inst.RA, false);
    MOV(arm_addr, addr_reg);
  }

  if (js.assumeNoPairedQuantize)
  {
    u32 flags = BackPatchInfo::FLAG_STORE;

    flags |= (w ? BackPatchInfo::FLAG_SIZE_F32 : BackPatchInfo::FLAG_SIZE_F32X2);

    EmitBackpatchRoutine(flags, jo.fastmem, jo.fastmem, VS, EncodeRegTo64(addr_reg), gprs_in_use,
                         fprs_in_use);
  }
  else
  {
    LDR(IndexType::Unsigned, scale_reg, PPC_REG, PPCSTATE_OFF_SPR(SPR_GQR0 + i));
    UBFM(type_reg, scale_reg, 0, 2);    // Type
    UBFM(scale_reg, scale_reg, 8, 13);  // Scale

    // Inline address check
    // FIXME: This doesn't correctly account for the BAT configuration.
    TST(addr_reg, LogicalImm(0x0c000000, 32));
    FixupBranch pass = B(CC_EQ);
    FixupBranch fail = B();

    SwitchToFarCode();
    SetJumpTarget(fail);
    // Slow
    MOVP2R(ARM64Reg::X30, &paired_store_quantized[16 + w * 8]);
    LDR(EncodeRegTo64(type_reg), ARM64Reg::X30, ArithOption(EncodeRegTo64(type_reg), true));

    ABI_PushRegisters(gprs_in_use);
    m_float_emit.ABI_PushRegisters(fprs_in_use, ARM64Reg::X30);
    BLR(EncodeRegTo64(type_reg));
    m_float_emit.ABI_PopRegisters(fprs_in_use, ARM64Reg::X30);
    ABI_PopRegisters(gprs_in_use);
    FixupBranch continue1 = B();
    SwitchToNearCode();
    SetJumpTarget(pass);

    // Fast
    MOVP2R(ARM64Reg::X30, &paired_store_quantized[w * 8]);
    LDR(EncodeRegTo64(type_reg), ARM64Reg::X30, ArithOption(EncodeRegTo64(type_reg), true));
    BLR(EncodeRegTo64(type_reg));

    SetJumpTarget(continue1);
  }

  if (js.assumeNoPairedQuantize && !have_single)
    fpr.Unlock(VS);

  gpr.Unlock(ARM64Reg::W0, ARM64Reg::W1, ARM64Reg::W2, ARM64Reg::W30);
  fpr.Unlock(ARM64Reg::Q0, ARM64Reg::Q1);
}
