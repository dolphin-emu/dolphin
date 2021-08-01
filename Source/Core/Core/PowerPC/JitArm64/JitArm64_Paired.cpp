// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Arm64Emitter.h"
#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/PowerPC.h"

using namespace Arm64Gen;

void JitArm64::ps_mergeXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  FALLBACK_IF(inst.Rc);

  const u32 a = inst.FA;
  const u32 b = inst.FB;
  const u32 d = inst.FD;

  const bool singles = fpr.IsSingle(a) && fpr.IsSingle(b);
  const RegType type = singles ? RegType::Single : RegType::Register;
  const u8 size = singles ? 32 : 64;
  const auto reg_encoder = singles ? EncodeRegToDouble : EncodeRegToQuad;

  const ARM64Reg VA = fpr.R(a, type);
  const ARM64Reg VB = fpr.R(b, type);
  const ARM64Reg VD = fpr.RW(d, type);

  switch (inst.SUBOP10)
  {
  case 528:  // 00
    m_float_emit.TRN1(size, VD, VA, VB);
    break;
  case 560:  // 01
    m_float_emit.INS(size, VD, 0, VA, 0);
    m_float_emit.INS(size, VD, 1, VB, 1);
    break;
  case 592:  // 10
    if (d != a && d != b)
    {
      m_float_emit.INS(size, VD, 0, VA, 1);
      m_float_emit.INS(size, VD, 1, VB, 0);
    }
    else
    {
      ARM64Reg V0 = fpr.GetReg();
      m_float_emit.INS(size, V0, 0, VA, 1);
      m_float_emit.INS(size, V0, 1, VB, 0);
      m_float_emit.MOV(reg_encoder(VD), reg_encoder(V0));
      fpr.Unlock(V0);
    }
    break;
  case 624:  // 11
    m_float_emit.TRN2(size, VD, VA, VB);
    break;
  default:
    ASSERT_MSG(DYNA_REC, 0, "ps_merge - invalid op");
    break;
  }

  ASSERT_MSG(DYNA_REC, singles == (fpr.IsSingle(a) && fpr.IsSingle(b)),
             "Register allocation turned singles into doubles in the middle of ps_mergeXX");
}

void JitArm64::ps_mulsX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  FALLBACK_IF(inst.Rc);

  const u32 a = inst.FA;
  const u32 c = inst.FC;
  const u32 d = inst.FD;

  const bool upper = inst.SUBOP5 == 13;

  const bool singles = fpr.IsSingle(a) && fpr.IsSingle(c);
  const bool round_c = !js.op->fprIsSingle[inst.FC];
  const RegType type = singles ? RegType::Single : RegType::Register;
  const u8 size = singles ? 32 : 64;
  const auto reg_encoder = singles ? EncodeRegToDouble : EncodeRegToQuad;

  const ARM64Reg VA = fpr.R(a, type);
  ARM64Reg VC = fpr.R(c, type);
  const ARM64Reg VD = fpr.RW(d, type);

  ARM64Reg V0Q = ARM64Reg::INVALID_REG;

  if (round_c)
  {
    ASSERT_MSG(DYNA_REC, !singles, "Tried to apply 25-bit precision to single");

    V0Q = fpr.GetReg();
    const ARM64Reg V1Q = fpr.GetReg();

    Force25BitPrecision(reg_encoder(V0Q), reg_encoder(VC), reg_encoder(V1Q));
    VC = reg_encoder(V0Q);

    fpr.Unlock(V1Q);
  }

  m_float_emit.FMUL(size, reg_encoder(VD), reg_encoder(VA), reg_encoder(VC), upper ? 1 : 0);

  if (V0Q != ARM64Reg::INVALID_REG)
    fpr.Unlock(V0Q);

  ASSERT_MSG(DYNA_REC, singles == (fpr.IsSingle(a) && fpr.IsSingle(c)),
             "Register allocation turned singles into doubles in the middle of ps_mulsX");

  fpr.FixSinglePrecision(d);

  SetFPRFIfNeeded(true, VD);
}

void JitArm64::ps_maddXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  FALLBACK_IF(inst.Rc);

  const u32 a = inst.FA;
  const u32 b = inst.FB;
  const u32 c = inst.FC;
  const u32 d = inst.FD;
  const u32 op5 = inst.SUBOP5;

  const bool singles = fpr.IsSingle(a) && fpr.IsSingle(b) && fpr.IsSingle(c);
  const bool round_c = !js.op->fprIsSingle[inst.FC];
  const RegType type = singles ? RegType::Single : RegType::Register;
  const u8 size = singles ? 32 : 64;
  const auto reg_encoder = singles ? EncodeRegToDouble : EncodeRegToQuad;

  const ARM64Reg VA = reg_encoder(fpr.R(a, type));
  const ARM64Reg VB = reg_encoder(fpr.R(b, type));
  ARM64Reg VC = reg_encoder(fpr.R(c, type));
  const ARM64Reg VD = reg_encoder(fpr.RW(d, type));

  ARM64Reg V0Q = ARM64Reg::INVALID_REG;
  ARM64Reg V0 = ARM64Reg::INVALID_REG;
  ARM64Reg V1Q = ARM64Reg::INVALID_REG;

  const auto allocate_v0_if_needed = [&] {
    if (V0Q == ARM64Reg::INVALID_REG)
    {
      V0Q = fpr.GetReg();
      V0 = reg_encoder(V0Q);
    }
  };

  if (round_c)
  {
    ASSERT_MSG(DYNA_REC, !singles, "Tried to apply 25-bit precision to single");

    allocate_v0_if_needed();
    V1Q = fpr.GetReg();

    Force25BitPrecision(reg_encoder(V1Q), VC, V0);
    VC = reg_encoder(V1Q);
  }

  ARM64Reg result_reg = VD;
  switch (op5)
  {
  case 14:  // ps_madds0: d = a * c.ps0 + b
    if (VD == VB)
    {
      m_float_emit.FMLA(size, VD, VA, VC, 0);
    }
    else if (VD != VA && VD != VC)
    {
      m_float_emit.MOV(VD, VB);
      m_float_emit.FMLA(size, VD, VA, VC, 0);
    }
    else
    {
      allocate_v0_if_needed();
      m_float_emit.MOV(V0, VB);
      m_float_emit.FMLA(size, V0, VA, VC, 0);
      result_reg = V0;
    }
    break;
  case 15:  // ps_madds1: d = a * c.ps1 + b
    if (VD == VB)
    {
      m_float_emit.FMLA(size, VD, VA, VC, 1);
    }
    else if (VD != VA && VD != VC)
    {
      m_float_emit.MOV(VD, VB);
      m_float_emit.FMLA(size, VD, VA, VC, 1);
    }
    else
    {
      allocate_v0_if_needed();
      m_float_emit.MOV(V0, VB);
      m_float_emit.FMLA(size, V0, VA, VC, 1);
      result_reg = V0;
    }
    break;
  case 28:  // ps_msub:  d = a * c - b
  case 30:  // ps_nmsub: d = -(a * c - b)
    if (VD != VA && VD != VC)
    {
      m_float_emit.FNEG(size, VD, VB);
      m_float_emit.FMLA(size, VD, VA, VC);
    }
    else
    {
      allocate_v0_if_needed();
      m_float_emit.FNEG(size, V0, VB);
      m_float_emit.FMLA(size, V0, VA, VC);
      result_reg = V0;
    }
    break;
  case 29:  // ps_madd:  d = a * c + b
  case 31:  // ps_nmadd: d = -(a * c + b)
    if (VD == VB)
    {
      m_float_emit.FMLA(size, VD, VA, VC);
    }
    else if (VD != VA && VD != VC)
    {
      m_float_emit.MOV(VD, VB);
      m_float_emit.FMLA(size, VD, VA, VC);
    }
    else
    {
      allocate_v0_if_needed();
      m_float_emit.MOV(V0, VB);
      m_float_emit.FMLA(size, V0, VA, VC);
      result_reg = V0;
    }
    break;
  default:
    ASSERT_MSG(DYNA_REC, 0, "ps_madd - invalid op");
    break;
  }

  switch (op5)
  {
  case 30:  // ps_nmsub
  case 31:  // ps_nmadd
    // PowerPC's nmadd/nmsub perform rounding before the final negation, which is not the case
    // for any of AArch64's FMA instructions, so we negate using a separate instruction.
    m_float_emit.FNEG(size, VD, result_reg);
    break;
  default:
    if (result_reg != VD)
      m_float_emit.MOV(VD, result_reg);
    break;
  }

  if (V0Q != ARM64Reg::INVALID_REG)
    fpr.Unlock(V0Q);
  if (V1Q != ARM64Reg::INVALID_REG)
    fpr.Unlock(V1Q);

  ASSERT_MSG(DYNA_REC, singles == (fpr.IsSingle(a) && fpr.IsSingle(b) && fpr.IsSingle(c)),
             "Register allocation turned singles into doubles in the middle of ps_maddXX");

  fpr.FixSinglePrecision(d);

  SetFPRFIfNeeded(true, VD);
}

void JitArm64::ps_sel(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  FALLBACK_IF(inst.Rc);

  const u32 a = inst.FA;
  const u32 b = inst.FB;
  const u32 c = inst.FC;
  const u32 d = inst.FD;

  const bool singles = fpr.IsSingle(a) && fpr.IsSingle(b) && fpr.IsSingle(c);
  const RegType type = singles ? RegType::Single : RegType::Register;
  const u8 size = singles ? 32 : 64;
  const auto reg_encoder = singles ? EncodeRegToDouble : EncodeRegToQuad;

  const ARM64Reg VA = reg_encoder(fpr.R(a, type));
  const ARM64Reg VB = reg_encoder(fpr.R(b, type));
  const ARM64Reg VC = reg_encoder(fpr.R(c, type));
  const ARM64Reg VD = reg_encoder(fpr.RW(d, type));

  if (d != b && d != c)
  {
    m_float_emit.FCMGE(size, VD, VA);
    m_float_emit.BSL(VD, VC, VB);
  }
  else
  {
    const ARM64Reg V0Q = fpr.GetReg();
    const ARM64Reg V0 = reg_encoder(V0Q);
    m_float_emit.FCMGE(size, V0, VA);
    m_float_emit.BSL(V0, VC, VB);
    m_float_emit.MOV(VD, V0);
    fpr.Unlock(V0Q);
  }

  ASSERT_MSG(DYNA_REC, singles == (fpr.IsSingle(a) && fpr.IsSingle(b) && fpr.IsSingle(c)),
             "Register allocation turned singles into doubles in the middle of ps_sel");
}

void JitArm64::ps_sumX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  FALLBACK_IF(inst.Rc);

  const u32 a = inst.FA;
  const u32 b = inst.FB;
  const u32 c = inst.FC;
  const u32 d = inst.FD;

  const bool upper = inst.SUBOP5 == 11;

  const bool singles = fpr.IsSingle(a) && fpr.IsSingle(b) && fpr.IsSingle(c);
  const RegType type = singles ? RegType::Single : RegType::Register;
  const u8 size = singles ? 32 : 64;
  const auto reg_encoder = singles ? EncodeRegToDouble : EncodeRegToQuad;

  const ARM64Reg VA = fpr.R(a, type);
  const ARM64Reg VB = fpr.R(b, type);
  const ARM64Reg VC = fpr.R(c, type);
  const ARM64Reg VD = fpr.RW(d, type);
  const ARM64Reg V0 = fpr.GetReg();

  m_float_emit.DUP(size, reg_encoder(V0), reg_encoder(upper ? VA : VB), upper ? 0 : 1);
  if (d != c)
  {
    m_float_emit.FADD(size, reg_encoder(VD), reg_encoder(V0), reg_encoder(upper ? VB : VA));
    m_float_emit.INS(size, VD, upper ? 0 : 1, VC, upper ? 0 : 1);
  }
  else
  {
    m_float_emit.FADD(size, reg_encoder(V0), reg_encoder(V0), reg_encoder(upper ? VB : VA));
    m_float_emit.INS(size, VD, upper ? 1 : 0, V0, upper ? 1 : 0);
  }

  fpr.Unlock(V0);

  ASSERT_MSG(DYNA_REC, singles == (fpr.IsSingle(a) && fpr.IsSingle(b) && fpr.IsSingle(c)),
             "Register allocation turned singles into doubles in the middle of ps_sumX");

  fpr.FixSinglePrecision(d);

  SetFPRFIfNeeded(true, VD);
}

void JitArm64::ps_res(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  FALLBACK_IF(inst.Rc);

  const u32 b = inst.FB;
  const u32 d = inst.FD;

  gpr.Lock(ARM64Reg::W0, ARM64Reg::W1, ARM64Reg::W2, ARM64Reg::W3, ARM64Reg::W4, ARM64Reg::W30);
  fpr.Lock(ARM64Reg::Q0);

  const ARM64Reg VB = fpr.R(b, RegType::Register);
  const ARM64Reg VD = fpr.RW(d, RegType::Register);

  m_float_emit.FMOV(ARM64Reg::X1, EncodeRegToDouble(VB));
  m_float_emit.FRECPE(64, ARM64Reg::Q0, EncodeRegToQuad(VB));
  BL(GetAsmRoutines()->fres);
  m_float_emit.UMOV(64, ARM64Reg::X1, EncodeRegToQuad(VB), 1);
  m_float_emit.DUP(64, ARM64Reg::Q0, ARM64Reg::Q0, 1);
  m_float_emit.FMOV(EncodeRegToDouble(VD), ARM64Reg::X0);
  BL(GetAsmRoutines()->fres);
  m_float_emit.INS(64, EncodeRegToQuad(VD), 1, ARM64Reg::X0);

  gpr.Unlock(ARM64Reg::W0, ARM64Reg::W1, ARM64Reg::W2, ARM64Reg::W3, ARM64Reg::W4, ARM64Reg::W30);
  fpr.Unlock(ARM64Reg::Q0);

  fpr.FixSinglePrecision(d);

  SetFPRFIfNeeded(true, VD);
}

void JitArm64::ps_rsqrte(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  FALLBACK_IF(inst.Rc);

  const u32 b = inst.FB;
  const u32 d = inst.FD;

  gpr.Lock(ARM64Reg::W0, ARM64Reg::W1, ARM64Reg::W2, ARM64Reg::W3, ARM64Reg::W4, ARM64Reg::W30);
  fpr.Lock(ARM64Reg::Q0);

  const ARM64Reg VB = fpr.R(b, RegType::Register);
  const ARM64Reg VD = fpr.RW(d, RegType::Register);

  m_float_emit.FMOV(ARM64Reg::X1, EncodeRegToDouble(VB));
  m_float_emit.FRSQRTE(64, ARM64Reg::Q0, EncodeRegToQuad(VB));
  BL(GetAsmRoutines()->frsqrte);
  m_float_emit.UMOV(64, ARM64Reg::X1, EncodeRegToQuad(VB), 1);
  m_float_emit.DUP(64, ARM64Reg::Q0, ARM64Reg::Q0, 1);
  m_float_emit.FMOV(EncodeRegToDouble(VD), ARM64Reg::X0);
  BL(GetAsmRoutines()->frsqrte);
  m_float_emit.INS(64, EncodeRegToQuad(VD), 1, ARM64Reg::X0);

  gpr.Unlock(ARM64Reg::W0, ARM64Reg::W1, ARM64Reg::W2, ARM64Reg::W3, ARM64Reg::W4, ARM64Reg::W30);
  fpr.Unlock(ARM64Reg::Q0);

  fpr.FixSinglePrecision(d);

  SetFPRFIfNeeded(true, VD);
}

void JitArm64::ps_cmpXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);

  const bool upper = inst.SUBOP10 & 64;
  FloatCompare(inst, upper);
}
