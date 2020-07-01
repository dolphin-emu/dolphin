// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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

  u32 a = inst.FA, b = inst.FB, d = inst.FD;

  bool singles = fpr.IsSingle(a) && fpr.IsSingle(b);
  RegType type = singles ? REG_REG_SINGLE : REG_REG;
  u8 size = singles ? 32 : 64;
  ARM64Reg (*reg_encoder)(ARM64Reg) = singles ? EncodeRegToDouble : EncodeRegToQuad;

  ARM64Reg VA = fpr.R(a, type);
  ARM64Reg VB = fpr.R(b, type);
  ARM64Reg VD = fpr.RW(d, type);

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
}

void JitArm64::ps_mulsX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  FALLBACK_IF(inst.Rc);
  FALLBACK_IF(SConfig::GetInstance().bFPRF && js.op->wantsFPRF);

  u32 a = inst.FA, c = inst.FC, d = inst.FD;

  bool upper = inst.SUBOP5 == 13;

  bool singles = fpr.IsSingle(a) && fpr.IsSingle(c);
  RegType type = singles ? REG_REG_SINGLE : REG_REG;
  u8 size = singles ? 32 : 64;
  ARM64Reg (*reg_encoder)(ARM64Reg) = singles ? EncodeRegToDouble : EncodeRegToQuad;

  ARM64Reg VA = fpr.R(a, type);
  ARM64Reg VC = fpr.R(c, type);
  ARM64Reg VD = fpr.RW(d, type);

  m_float_emit.FMUL(size, reg_encoder(VD), reg_encoder(VA), reg_encoder(VC), upper ? 1 : 0);

  fpr.FixSinglePrecision(d);
}

void JitArm64::ps_maddXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  FALLBACK_IF(inst.Rc);
  FALLBACK_IF(SConfig::GetInstance().bFPRF && js.op->wantsFPRF);

  u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;
  u32 op5 = inst.SUBOP5;

  bool singles = fpr.IsSingle(a) && fpr.IsSingle(b) && fpr.IsSingle(c);
  RegType type = singles ? REG_REG_SINGLE : REG_REG;
  u8 size = singles ? 32 : 64;
  ARM64Reg (*reg_encoder)(ARM64Reg) = singles ? EncodeRegToDouble : EncodeRegToQuad;

  ARM64Reg VA = reg_encoder(fpr.R(a, type));
  ARM64Reg VB = reg_encoder(fpr.R(b, type));
  ARM64Reg VC = reg_encoder(fpr.R(c, type));
  ARM64Reg VD = reg_encoder(fpr.RW(d, type));
  ARM64Reg V0Q = INVALID_REG;
  ARM64Reg V0 = INVALID_REG;
  if (d != b && (d == a || d == c))
  {
    V0Q = fpr.GetReg();
    V0 = reg_encoder(V0Q);
  }

  switch (op5)
  {
  case 14:  // ps_madds0
    // d = a * c.ps0 + b
    if (d == b)
    {
      m_float_emit.FMLA(size, VD, VA, VC, 0);
    }
    else if (d != a && d != c)
    {
      m_float_emit.MOV(VD, VB);
      m_float_emit.FMLA(size, VD, VA, VC, 0);
    }
    else
    {
      m_float_emit.MOV(V0, VB);
      m_float_emit.FMLA(size, V0, VA, VC, 0);
      m_float_emit.MOV(VD, V0);
    }
    break;
  case 15:  // ps_madds1
    // d = a * c.ps1 + b
    if (d == b)
    {
      m_float_emit.FMLA(size, VD, VA, VC, 1);
    }
    else if (d != a && d != c)
    {
      m_float_emit.MOV(VD, VB);
      m_float_emit.FMLA(size, VD, VA, VC, 1);
    }
    else
    {
      m_float_emit.MOV(V0, VB);
      m_float_emit.FMLA(size, V0, VA, VC, 1);
      m_float_emit.MOV(VD, V0);
    }
    break;
  case 28:  // ps_msub
    // d = a * c - b
    if (d == b)
    {
      // d = -(-a * c + b)
      // rounding is incorrect if the rounding mode is +/- infinity
      m_float_emit.FMLS(size, VD, VA, VC);
      m_float_emit.FNEG(size, VD, VD);
    }
    else if (d != a && d != c)
    {
      m_float_emit.FNEG(size, VD, VB);
      m_float_emit.FMLA(size, VD, VA, VC);
    }
    else
    {
      m_float_emit.FNEG(size, V0, VB);
      m_float_emit.FMLA(size, V0, VA, VC);
      m_float_emit.MOV(VD, V0);
    }
    break;
  case 29:  // ps_madd
    // d = a * c + b
    if (d == b)
    {
      m_float_emit.FMLA(size, VD, VA, VC);
    }
    else if (d != a && d != c)
    {
      m_float_emit.MOV(VD, VB);
      m_float_emit.FMLA(size, VD, VA, VC);
    }
    else
    {
      m_float_emit.MOV(V0, VB);
      m_float_emit.FMLA(size, V0, VA, VC);
      m_float_emit.MOV(VD, V0);
    }
    break;
  case 30:  // ps_nmsub
    // d = -(a * c - b)
    // =>
    // d = -a * c + b
    // Note: PowerPC rounds before the final negation.
    // We don't handle this at the moment because it's
    // only relevant when rounding to +/- infinity.
    if (d == b)
    {
      m_float_emit.FMLS(size, VD, VA, VC);
    }
    else if (d != a && d != c)
    {
      m_float_emit.MOV(VD, VB);
      m_float_emit.FMLS(size, VD, VA, VC);
    }
    else
    {
      m_float_emit.MOV(V0, VB);
      m_float_emit.FMLS(size, V0, VA, VC);
      m_float_emit.MOV(VD, V0);
    }
    break;
  case 31:  // ps_nmadd
    // d = -(a * c + b)
    if (d == b)
    {
      m_float_emit.FMLA(size, VD, VA, VC);
      m_float_emit.FNEG(size, VD, VD);
    }
    else if (d != a && d != c)
    {
      // d = -a * c - b
      // See rounding note at ps_nmsub.
      m_float_emit.FNEG(size, VD, VB);
      m_float_emit.FMLS(size, VD, VA, VC);
    }
    else
    {
      m_float_emit.MOV(V0, VB);
      m_float_emit.FMLA(size, V0, VA, VC);
      m_float_emit.FNEG(size, VD, V0);
    }
    break;
  default:
    ASSERT_MSG(DYNA_REC, 0, "ps_madd - invalid op");
    break;
  }
  fpr.FixSinglePrecision(d);

  if (V0Q != INVALID_REG)
    fpr.Unlock(V0Q);
}

void JitArm64::ps_sel(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  FALLBACK_IF(inst.Rc);

  u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

  bool singles = fpr.IsSingle(a) && fpr.IsSingle(b) && fpr.IsSingle(c);
  RegType type = singles ? REG_REG_SINGLE : REG_REG;
  u8 size = singles ? 32 : 64;
  ARM64Reg (*reg_encoder)(ARM64Reg) = singles ? EncodeRegToDouble : EncodeRegToQuad;

  ARM64Reg VA = reg_encoder(fpr.R(a, type));
  ARM64Reg VB = reg_encoder(fpr.R(b, type));
  ARM64Reg VC = reg_encoder(fpr.R(c, type));
  ARM64Reg VD = reg_encoder(fpr.RW(d, type));

  if (d != b && d != c)
  {
    m_float_emit.FCMGE(size, VD, VA);
    m_float_emit.BSL(VD, VC, VB);
  }
  else
  {
    ARM64Reg V0Q = fpr.GetReg();
    ARM64Reg V0 = reg_encoder(V0Q);
    m_float_emit.FCMGE(size, V0, VA);
    m_float_emit.BSL(V0, VC, VB);
    m_float_emit.MOV(VD, V0);
    fpr.Unlock(V0Q);
  }
}

void JitArm64::ps_sumX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  FALLBACK_IF(inst.Rc);
  FALLBACK_IF(SConfig::GetInstance().bFPRF && js.op->wantsFPRF);

  u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

  bool upper = inst.SUBOP5 == 11;

  bool singles = fpr.IsSingle(a) && fpr.IsSingle(b) && fpr.IsSingle(c);
  RegType type = singles ? REG_REG_SINGLE : REG_REG;
  u8 size = singles ? 32 : 64;
  ARM64Reg (*reg_encoder)(ARM64Reg) = singles ? EncodeRegToDouble : EncodeRegToQuad;

  ARM64Reg VA = fpr.R(a, type);
  ARM64Reg VB = fpr.R(b, type);
  ARM64Reg VC = fpr.R(c, type);
  ARM64Reg VD = fpr.RW(d, type);
  ARM64Reg V0 = fpr.GetReg();

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

  fpr.FixSinglePrecision(d);

  fpr.Unlock(V0);
}
