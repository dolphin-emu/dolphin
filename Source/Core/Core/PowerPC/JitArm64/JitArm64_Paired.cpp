// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitArm64/Jit.h"

#include "Common/Arm64Emitter.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/StringUtil.h"

#include "Core/Config/SessionSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
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

  const ARM64Reg VA = reg_encoder(fpr.R(a, type));
  const ARM64Reg VB = reg_encoder(fpr.R(b, type));
  const ARM64Reg VD = reg_encoder(fpr.RW(d, type));

  switch (inst.SUBOP10)
  {
  case 528:  // 00
    m_float_emit.TRN1(size, VD, VA, VB);
    break;
  case 560:  // 01
    if (d != b)
    {
      if (d != a)
        m_float_emit.MOV(VD, VA);
      if (a != b)
        m_float_emit.INS(size, VD, 1, VB, 1);
    }
    else if (d != a)
    {
      m_float_emit.INS(size, VD, 0, VA, 0);
    }
    break;
  case 592:  // 10
    m_float_emit.EXT(VD, VA, VB, size >> 3);
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

void JitArm64::ps_arith(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  FALLBACK_IF(inst.Rc);
  FALLBACK_IF(jo.fp_exceptions);

  const u32 a = inst.FA;
  const u32 b = inst.FB;
  const u32 c = inst.FC;
  const u32 d = inst.FD;
  const u32 op5 = inst.SUBOP5;

  const bool muls = (op5 & ~0x1) == 12;
  const bool madds = (op5 & ~0x1) == 14;
  const bool use_c = op5 == 25 || (op5 & ~0x13) == 12;  // mul, muls, and all kinds of maddXX
  const bool use_b = op5 != 25 && !muls;                // mul and muls don't use B
  const bool duplicated_c = muls || madds;
  const bool fma = use_b && use_c;
  const bool negate_result = (op5 & ~0x1) == 30;
  const bool msub = op5 == 28 || op5 == 30;

  const auto singles_func = [&] {
    return fpr.IsSingle(a) && (!use_b || fpr.IsSingle(b)) && (!use_c || fpr.IsSingle(c));
  };
  const bool singles = singles_func();

  const bool inaccurate_fma = !Config::Get(Config::SESSION_USE_FMA);
  const bool round_c = use_c && !js.op->fprIsSingle[inst.FC];
  const RegType type = singles ? RegType::Single : RegType::Register;
  const u8 size = singles ? 32 : 64;
  const auto reg_encoder = singles ? EncodeRegToDouble : EncodeRegToQuad;

  const ARM64Reg VA = reg_encoder(fpr.R(a, type));
  const ARM64Reg VB = use_b ? reg_encoder(fpr.R(b, type)) : ARM64Reg::INVALID_REG;
  const ARM64Reg VC = use_c ? reg_encoder(fpr.R(c, type)) : ARM64Reg::INVALID_REG;
  const ARM64Reg VD = reg_encoder(fpr.RW(d, type));

  {
    Arm64FPRCache::ScopedARM64Reg V0Q = ARM64Reg::INVALID_REG;
    Arm64FPRCache::ScopedARM64Reg V1Q = ARM64Reg::INVALID_REG;
    Arm64FPRCache::ScopedARM64Reg V2Q = ARM64Reg::INVALID_REG;

    ARM64Reg rounded_c_reg = VC;
    if (round_c)
    {
      ASSERT_MSG(DYNA_REC, !singles, "Tried to apply 25-bit precision to single");

      V0Q = fpr.GetScopedReg();
      rounded_c_reg = reg_encoder(V0Q);
      Force25BitPrecision(rounded_c_reg, VC);
    }

    ARM64Reg inaccurate_fma_reg = VD;
    if (fma && inaccurate_fma && VD == VB)
    {
      if (V0Q == ARM64Reg::INVALID_REG)
        V0Q = fpr.GetScopedReg();
      inaccurate_fma_reg = reg_encoder(V0Q);
    }

    ARM64Reg result_reg = VD;
    const bool need_accurate_fma_reg =
        fma && !inaccurate_fma && (msub || VD != VB) && (VD == VA || VD == rounded_c_reg);
    const bool preserve_d =
        m_accurate_nans && (VD == VA || (use_b && VD == VB) || (use_c && VD == VC));
    if (need_accurate_fma_reg || preserve_d)
    {
      V1Q = fpr.GetScopedReg();
      result_reg = reg_encoder(V1Q);
    }

    if (m_accurate_nans)
    {
      if (V0Q == ARM64Reg::INVALID_REG)
        V0Q = fpr.GetScopedReg();

      if (duplicated_c || VD == result_reg)
        V2Q = fpr.GetScopedReg();
    }

    switch (op5)
    {
    case 12:  // ps_muls0: d = a * c.ps0
      m_float_emit.FMUL(size, result_reg, VA, rounded_c_reg, 0);
      break;
    case 13:  // ps_muls1: d = a * c.ps1
      m_float_emit.FMUL(size, result_reg, VA, rounded_c_reg, 1);
      break;
    case 14:  // ps_madds0: d = a * c.ps0 + b
      if (inaccurate_fma)
      {
        m_float_emit.FMUL(size, inaccurate_fma_reg, VA, rounded_c_reg, 0);
        m_float_emit.FADD(size, result_reg, inaccurate_fma_reg, VB);
      }
      else
      {
        if (result_reg != VB)
          m_float_emit.MOV(result_reg, VB);
        m_float_emit.FMLA(size, result_reg, VA, rounded_c_reg, 0);
      }
      break;
    case 15:  // ps_madds1: d = a * c.ps1 + b
      if (inaccurate_fma)
      {
        m_float_emit.FMUL(size, inaccurate_fma_reg, VA, rounded_c_reg, 1);
        m_float_emit.FADD(size, result_reg, inaccurate_fma_reg, VB);
      }
      else
      {
        if (result_reg != VB)
          m_float_emit.MOV(result_reg, VB);
        m_float_emit.FMLA(size, result_reg, VA, rounded_c_reg, 1);
      }
      break;
    case 18:  // ps_div
      m_float_emit.FDIV(size, result_reg, VA, VB);
      break;
    case 20:  // ps_sub
      m_float_emit.FSUB(size, result_reg, VA, VB);
      break;
    case 21:  // ps_add
      m_float_emit.FADD(size, result_reg, VA, VB);
      break;
    case 25:  // ps_mul
      m_float_emit.FMUL(size, result_reg, VA, rounded_c_reg);
      break;
    case 28:  // ps_msub:  d = a * c - b
    case 30:  // ps_nmsub: d = -(a * c - b)
      if (inaccurate_fma)
      {
        m_float_emit.FMUL(size, inaccurate_fma_reg, VA, rounded_c_reg);
        m_float_emit.FSUB(size, result_reg, inaccurate_fma_reg, VB);
      }
      else
      {
        m_float_emit.FNEG(size, result_reg, VB);
        m_float_emit.FMLA(size, result_reg, VA, rounded_c_reg);
      }
      break;
    case 29:  // ps_madd:  d = a * c + b
    case 31:  // ps_nmadd: d = -(a * c + b)
      if (inaccurate_fma)
      {
        m_float_emit.FMUL(size, inaccurate_fma_reg, VA, rounded_c_reg);
        m_float_emit.FADD(size, result_reg, inaccurate_fma_reg, VB);
      }
      else
      {
        if (result_reg != VB)
          m_float_emit.MOV(result_reg, VB);
        m_float_emit.FMLA(size, result_reg, VA, rounded_c_reg);
      }
      break;
    default:
      ASSERT_MSG(DYNA_REC, 0, "ps_arith - invalid op");
      break;
    }

    FixupBranch nan_fixup;
    if (m_accurate_nans)
    {
      const ARM64Reg nan_temp_reg = singles ? EncodeRegToSingle(V0Q) : EncodeRegToDouble(V0Q);
      const ARM64Reg nan_temp_reg_paired = reg_encoder(V0Q);

      // Check if we need to handle NaNs

      m_float_emit.FMAXP(nan_temp_reg, result_reg);
      m_float_emit.FCMP(nan_temp_reg);
      FixupBranch no_nan = B(CCFlags::CC_VC);
      FixupBranch nan = B();
      SetJumpTarget(no_nan);

      SwitchToFarCode();
      SetJumpTarget(nan);

      // Pick the right NaNs

      const auto check_input = [&](ARM64Reg input) {
        m_float_emit.FCMEQ(size, nan_temp_reg_paired, input, input);
        m_float_emit.BIF(result_reg, input, nan_temp_reg_paired);
      };

      ARM64Reg c_reg_for_nan_purposes = VC;
      if (duplicated_c)
      {
        c_reg_for_nan_purposes = reg_encoder(V2Q);
        m_float_emit.DUP(size, c_reg_for_nan_purposes, VC, op5 & 0x1);
      }

      if (use_c)
        check_input(c_reg_for_nan_purposes);

      if (use_b && (!use_c || VB != c_reg_for_nan_purposes))
        check_input(VB);

      if ((!use_b || VA != VB) && (!use_c || VA != c_reg_for_nan_purposes))
        check_input(VA);

      // Make the NaNs quiet

      const ARM64Reg quiet_nan_reg = VD == result_reg ? reg_encoder(V2Q) : VD;

      m_float_emit.FADD(size, quiet_nan_reg, result_reg, result_reg);
      m_float_emit.FCMEQ(size, nan_temp_reg_paired, result_reg, result_reg);
      if (negate_result)
        m_float_emit.FNEG(size, result_reg, result_reg);
      if (VD == result_reg)
        m_float_emit.BIF(VD, quiet_nan_reg, nan_temp_reg_paired);
      else  // quiet_nan_reg == VD
        m_float_emit.BIT(VD, result_reg, nan_temp_reg_paired);

      nan_fixup = B();

      SwitchToNearCode();
    }

    // PowerPC's nmadd/nmsub perform rounding before the final negation, which is not the case
    // for any of AArch64's FMA instructions, so we negate using a separate instruction.
    if (negate_result)
      m_float_emit.FNEG(size, VD, result_reg);
    else if (result_reg != VD)
      m_float_emit.MOV(VD, result_reg);

    if (m_accurate_nans)
      SetJumpTarget(nan_fixup);
  }

  ASSERT_MSG(DYNA_REC, singles == singles_func(),
             "Register allocation turned singles into doubles in the middle of ps_arith");

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
    const auto V0Q = fpr.GetScopedReg();
    const ARM64Reg V0 = reg_encoder(V0Q);
    m_float_emit.FCMGE(size, V0, VA);
    m_float_emit.BSL(V0, VC, VB);
    m_float_emit.MOV(VD, V0);
  }

  ASSERT_MSG(DYNA_REC, singles == (fpr.IsSingle(a) && fpr.IsSingle(b) && fpr.IsSingle(c)),
             "Register allocation turned singles into doubles in the middle of ps_sel");
}

void JitArm64::ps_sumX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  FALLBACK_IF(inst.Rc);
  FALLBACK_IF(jo.fp_exceptions);

  const u32 a = inst.FA;
  const u32 b = inst.FB;
  const u32 c = inst.FC;
  const u32 d = inst.FD;

  const bool upper = inst.SUBOP5 & 0x1;

  const bool singles = fpr.IsSingle(a) && fpr.IsSingle(b) && fpr.IsSingle(c);
  const RegType type = singles ? RegType::Single : RegType::Register;
  const u8 size = singles ? 32 : 64;
  const auto reg_encoder = singles ? EncodeRegToDouble : EncodeRegToQuad;
  const auto scalar_reg_encoder = singles ? EncodeRegToSingle : EncodeRegToDouble;

  const ARM64Reg VA = fpr.R(a, type);
  const ARM64Reg VB = fpr.R(b, type);
  const ARM64Reg VC = fpr.R(c, type);
  const ARM64Reg VD = fpr.RW(d, type);

  {
    const auto V0 = fpr.GetScopedReg();

    m_float_emit.DUP(size, reg_encoder(V0), reg_encoder(VB), 1);

    if (m_accurate_nans)
    {
      // If the first input is NaN, set the temp register for the second input to 0. This is
      // because:
      //
      // - If the second input is also NaN, setting it to 0 ensures that the first NaN will be
      // picked.
      // - If only the first input is NaN, setting the second input to 0 has no effect on the
      // result.
      //
      // Either way, we can then do an FADD as usual, and the FADD will make the NaN quiet.
      m_float_emit.FCMP(scalar_reg_encoder(VA));
      FixupBranch a_not_nan = B(CCFlags::CC_VC);
      m_float_emit.MOVI(64, scalar_reg_encoder(V0), 0);
      SetJumpTarget(a_not_nan);
    }

    if (upper)
    {
      m_float_emit.FADD(scalar_reg_encoder(V0), scalar_reg_encoder(V0), scalar_reg_encoder(VA));
      m_float_emit.TRN1(size, reg_encoder(VD), reg_encoder(VC), reg_encoder(V0));
    }
    else if (d != c)
    {
      m_float_emit.FADD(scalar_reg_encoder(VD), scalar_reg_encoder(V0), scalar_reg_encoder(VA));
      m_float_emit.INS(size, VD, 1, VC, 1);
    }
    else
    {
      m_float_emit.FADD(scalar_reg_encoder(V0), scalar_reg_encoder(V0), scalar_reg_encoder(VA));
      m_float_emit.INS(size, VD, 0, V0, 0);
    }
  }

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
  FALLBACK_IF(jo.fp_exceptions || jo.div_by_zero_exceptions);

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
  FALLBACK_IF(jo.fp_exceptions || jo.div_by_zero_exceptions);

  const u32 b = inst.FB;
  const u32 d = inst.FD;

  gpr.Lock(ARM64Reg::W0, ARM64Reg::W1, ARM64Reg::W2, ARM64Reg::W3, ARM64Reg::W30);
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

  gpr.Unlock(ARM64Reg::W0, ARM64Reg::W1, ARM64Reg::W2, ARM64Reg::W3, ARM64Reg::W30);
  fpr.Unlock(ARM64Reg::Q0);

  fpr.FixSinglePrecision(d);

  SetFPRFIfNeeded(true, VD);
}

void JitArm64::ps_cmpXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  FALLBACK_IF(jo.fp_exceptions);

  const bool upper = inst.SUBOP10 & 64;
  FloatCompare(inst, upper);
}
