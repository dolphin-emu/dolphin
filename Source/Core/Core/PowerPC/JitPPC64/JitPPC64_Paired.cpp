// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Paired-single instructions. The arithmetic computes both halves at once with vector
// instructions, as the single precision floating-point instructions compute each, unless C has to
// be rounded to 25 bits first: then each half is computed by the scalar code, with half 1 swapped
// into doubleword 0. The vector code leaves a tie of a fused multiply-add in either half to the
// interpreter, which also updates FPSCR's status on that path.

#include "Core/PowerPC/JitPPC64/Jit.h"

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/PPC64Emitter.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PowerPC.h"

using namespace PPC64Gen;

void JitPPC64::ps_arith(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  INTERPRET_IF(inst.Rc);
  FALLBACK_IF(jo.fp_exceptions || (jo.div_by_zero_exceptions && inst.SUBOP5 == 18));

  // ps_muls0, ps_muls1, ps_madds0 and ps_madds1 are ps_mul and ps_madd, with one half of C for
  // both halves.
  u32 op5 = inst.SUBOP5;
  int c_half = -1;
  switch (op5)
  {
  case 12:  // ps_muls0
  case 13:  // ps_muls1
    c_half = op5 - 12;
    op5 = 25;
    break;
  case 14:  // ps_madds0
  case 15:  // ps_madds1
    c_half = op5 - 14;
    op5 = 29;
    break;
  default:
    break;
  }

  const bool use_c = op5 >= 25;
  const bool use_b = op5 != 25;
  const bool round_c = use_c && !IsSingle(inst.FC);

  const VSR a = fpr.R(inst.FA);
  const VSR b = use_b ? fpr.R(inst.FB) : VSR::VS0;
  const VSR c = use_c ? fpr.R(inst.FC) : VSR::VS0;
  const VSR d = fpr.RW(inst.FD);

  // Both halves at once, unless C has to be rounded to 25 bits first, which needs each in a GPR.
  if (!round_c)
  {
    EmitPairedArith(inst, op5, c_half, d, a, b, c);
    return;
  }

  // Half 1 of the operands, in doubleword 0, and C's half 1 for half 0 of ps_muls1 and ps_madds1.
  const auto a1 = fpr.GetScopedReg();
  const auto b1 = fpr.GetScopedReg();
  const auto c1 = fpr.GetScopedReg();
  XXSWAPD(a1, a);
  if (use_b)
    XXSWAPD(b1, b);
  if (use_c && c_half != 0)
    XXSWAPD(c1, c);
  const VSR c_for_half_0 = c_half == 1 ? VSR(c1) : c;
  const VSR c_for_half_1 = c_half == 0 ? c : VSR(c1);

  const auto result0 = fpr.GetScopedReg();
  const auto result1 = fpr.GetScopedReg();
  EmitFloatArith(op5, true, round_c, result0, a, b, c_for_half_0);
  EmitFloatArith(op5, true, round_c, result1, a1, b1, c_for_half_1);
  XXPERMDI(d, result0, result1, 0);
  if (m_fprf && js.op->wantsFPRF)
    SetFPRF(result0, true);
}

void JitPPC64::EmitPairedArith(UGeckoInstruction inst, u32 op5, int c_half, VSR d, VSR a, VSR b,
                               VSR c)
{
  // As EmitFloatArith for each half: the operation in double precision, rounded to single
  // precision, with the fused multiply-adds' ties corrected, and the negation after rounding,
  // except for NaNs.
  const bool fma = op5 >= 28;
  const bool negate = op5 == 30 || op5 == 31;
  const bool subtract_b = op5 == 28 || op5 == 30;
  // Loaded here rather than by ForcePairedSingle, which the tie path skips, so that it is loaded
  // after this on both paths.
  EnsureNILoaded();

  // The interpreter, for a tie, is called after the scratch registers are released.
  FixupBranch tie;
  bool has_tie = false;
  {
    // C's half for both halves, for ps_muls0, ps_muls1, ps_madds0 and ps_madds1.
    PPC64FPRCache::ScopedReg c_halves;
    VSR c_operand = c;
    if (c_half >= 0)
    {
      c_halves = fpr.GetScopedReg();
      XXPERMDI(c_halves, c, c, c_half == 0 ? 0 : 3);
      c_operand = c_halves;
    }

    const auto result = fpr.GetScopedReg();
    switch (op5)
    {
    case 18:
      XVDIVDP(result, a, b);
      break;
    case 20:
      XVSUBDP(result, a, b);
      break;
    case 21:
      XVADDDP(result, a, b);
      break;
    case 25:
      XVMULDP(result, a, c_operand);
      break;
    default:
      XXLMR(result, b);
      subtract_b ? XVMSUBADP(result, a, c_operand) : XVMADDADP(result, a, c_operand);
      break;
    }

    // A tie for rounding to single precision, the low 29 bits being 0x10000000, needs the error of
    // the fused multiply-add, which the interpreter computes.
    if (fma && m_accurate_fmadds)
    {
      if (m_isa_3_0)
      {
        // Shifted to the top, a tie is 1 << 63.
        const auto shifted = fpr.GetScopedReg();
        const auto tie_bits = fpr.GetScopedReg();
        XXSPLTIB(tie_bits, 35);
        VSLD(shifted, result, tie_bits);
        XXSPLTIB(tie_bits, 63);
        {
          const auto ones = fpr.GetScopedReg();
          XXLEQV(ones, ones, ones);
          VSLD(tie_bits, ones, tie_bits);
        }
        VCMPEQUD(shifted, shifted, tie_bits, true);
        // CR6's EQ is set if neither half is a tie.
        tie = BCFar(CCFlags::NE, CRF::CR6);
      }
      else
      {
        // As EmitFloatArith, for each half.
        const auto bits = gpr.GetScopedReg();
        const auto half1 = fpr.GetScopedReg();
        MFVSRD(bits, result);
        CLRLDI(bits, bits, 35);
        XORIS(bits, bits, 0x1000);
        CMPLDI(CRF::CR1, bits, 0);
        XXSWAPD(half1, result);
        MFVSRD(bits, half1);
        CLRLDI(bits, bits, 35);
        XORIS(bits, bits, 0x1000);
        CMPLDI(CRF::CR0, bits, 0);
        CROR(CRBit(CRF::CR1, CCFlags::EQ), CRBit(CRF::CR1, CCFlags::EQ),
             CRBit(CRF::CR0, CCFlags::EQ));
        tie = BCFar(CCFlags::EQ, CRF::CR1);
      }
      has_tie = true;
    }

    ForcePairedSingle(result);
    if (negate)
    {
      const auto not_nan = fpr.GetScopedReg();
      const auto negated = fpr.GetScopedReg();
      XVCMPEQDP(not_nan, result, result);
      XVNEGDP(negated, result);
      XXSEL(result, result, negated, not_nan);
    }
    XXLMR(d, result);
    if (m_fprf && js.op->wantsFPRF)
      SetFPRF(result, true);
  }

  if (has_tie)
  {
    const u8* done = GetCodePtr();
    SwitchToFarCode();
    SetJumpTarget(tie);
    InterpretFromFarCode(inst, done);
    SwitchToNearCode();
  }
}

void JitPPC64::ps_sumX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  INTERPRET_IF(inst.Rc);
  FALLBACK_IF(jo.fp_exceptions);

  // ps_sum0: a.ps0 + b.ps1, c.ps1; ps_sum1: c.ps0, a.ps0 + b.ps1. Both halves are rounded to single
  // precision.
  const bool sum1 = inst.SUBOP5 == 11;
  const VSR a = fpr.R(inst.FA);
  const VSR b = fpr.R(inst.FB);
  const VSR c = fpr.R(inst.FC);
  const VSR d = fpr.RW(inst.FD);

  // The sum and C's other half are rounded to single precision together.
  const auto b1 = fpr.GetScopedReg();
  const auto sum = fpr.GetScopedReg();
  XXSWAPD(b1, b);
  XSADDDP(sum, a, b1);
  if (sum1)
    XXPERMDI(sum, c, sum, 0);
  else
    XXPERMDI(sum, sum, c, 1);
  ForcePairedSingle(sum);
  XXLMR(d, sum);
  // Of the sum, as ps_sum0 sets it for ps0 and ps_sum1 for ps1.
  if (m_fprf && js.op->wantsFPRF)
  {
    // ps_sum1's is in doubleword 1: swapped into b1, which is free now.
    if (sum1)
      XXSWAPD(b1, sum);
    SetFPRF(sum1 ? VSR(b1) : VSR(sum), true);
  }
}

void JitPPC64::ps_sel(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  INTERPRET_IF(inst.Rc);

  const VSR a = fpr.R(inst.FA);
  const VSR b = fpr.R(inst.FB);
  const VSR c = fpr.R(inst.FC);
  const VSR d = fpr.RW(inst.FD);

  // Each half is a >= -0.0 ? c : b, which is b if a is a NaN.
  const auto mask = fpr.GetScopedReg();
  XXLXOR(mask, mask, mask);
  XVCMPGEDP(mask, a, mask);
  XXSEL(d, b, c, mask);
}

void JitPPC64::ps_cmpXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  FALLBACK_IF(jo.fp_exceptions);

  // ps_cmpu0 and ps_cmpo0 compare ps0, ps_cmpu1 and ps_cmpo1 ps1.
  const bool half1 = inst.SUBOP10 == 64 || inst.SUBOP10 == 96;
  const VSR a = fpr.R(inst.FA);
  const VSR b = fpr.R(inst.FB);

  if (half1)
  {
    const auto a1 = fpr.GetScopedReg();
    const auto b1 = fpr.GetScopedReg();
    XXSWAPD(a1, a);
    XXSWAPD(b1, b);
    XSCMPUDP(CRF::CR1, a1, b1);
  }
  else
  {
    XSCMPUDP(CRF::CR1, a, b);
  }
  // With FPRF, these set FPSCR[FPCC].
  if (m_fprf)
    SetFPCCFromHostFloatCompare(CRF::CR1);
  SetCRFieldFromHostFloatCompare(inst.CRFD, CRF::CR1);
}

void JitPPC64::ps_res(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  INTERPRET_IF(inst.Rc);
  FALLBACK_IF(jo.fp_exceptions);
  EmitEstimate(inst, false, true);
}

void JitPPC64::ps_rsqrte(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  INTERPRET_IF(inst.Rc);
  FALLBACK_IF(jo.fp_exceptions);
  EmitEstimate(inst, true, true);
}
