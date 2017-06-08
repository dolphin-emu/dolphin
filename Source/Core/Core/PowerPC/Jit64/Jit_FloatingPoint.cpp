// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <vector>

#include "Common/Assert.h"
#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"
#include "Core/PowerPC/Jit64Common/Jit64PowerPCState.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PowerPC.h"

using namespace Gen;

alignas(16) static const u64 psSignBits[2] = {0x8000000000000000ULL, 0x0000000000000000ULL};
alignas(16) static const u64 psSignBits2[2] = {0x8000000000000000ULL, 0x8000000000000000ULL};
alignas(16) static const u64 psAbsMask[2] = {0x7FFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL};
alignas(16) static const u64 psAbsMask2[2] = {0x7FFFFFFFFFFFFFFFULL, 0x7FFFFFFFFFFFFFFFULL};
alignas(16) static const u64 psGeneratedQNaN[2] = {0x7FF8000000000000ULL, 0x7FF8000000000000ULL};
alignas(16) static const double half_qnan_and_s32_max[2] = {0x7FFFFFFF, -0x80000};

X64Reg Jit64::fp_tri_op(int d, int a, int b, bool reversible, bool single,
                        void (XEmitter::*avxOp)(X64Reg, X64Reg, const OpArg&),
                        void (XEmitter::*sseOp)(X64Reg, const OpArg&), bool packed,
                        bool preserve_inputs, bool roundRHS)
{
  fpr.Lock(d, a, b);
  fpr.BindToRegister(d, d == a || d == b || !single);
  X64Reg dest = preserve_inputs ? XMM1 : fpr.RX(d);
  if (roundRHS)
  {
    if (d == a && !preserve_inputs)
    {
      Force25BitPrecision(XMM0, fpr.R(b), XMM1);
      (this->*sseOp)(fpr.RX(d), R(XMM0));
    }
    else
    {
      Force25BitPrecision(dest, fpr.R(b), XMM0);
      (this->*sseOp)(dest, fpr.R(a));
    }
  }
  else
  {
    avx_op(avxOp, sseOp, dest, fpr.R(a), fpr.R(b), packed, reversible);
  }
  return dest;
}

// We can avoid calculating FPRF if it's not needed; every float operation resets it, so
// if it's going to be clobbered in a future instruction before being read, we can just
// not calculate it.
void Jit64::SetFPRFIfNeeded(X64Reg xmm)
{
  // As far as we know, the games that use this flag only need FPRF for fmul and fmadd, but
  // FPRF is fast enough in JIT that we might as well just enable it for every float instruction
  // if the FPRF flag is set.
  if (SConfig::GetInstance().bFPRF && js.op->wantsFPRF)
    SetFPRF(xmm);
}

void Jit64::HandleNaNs(UGeckoInstruction inst, X64Reg xmm_out, X64Reg xmm, X64Reg clobber)
{
  //                      | PowerPC  | x86
  // ---------------------+----------+---------
  // input NaN precedence | 1*3 + 2  | 1*2 + 3
  // generated QNaN       | positive | negative
  //
  // Dragon Ball: Revenge of King Piccolo requires generated NaNs
  // to be positive, so we'll have to handle them manually.

  if (!SConfig::GetInstance().bAccurateNaNs)
  {
    if (xmm_out != xmm)
      MOVAPD(xmm_out, R(xmm));
    return;
  }

  _assert_(xmm != clobber);

  std::vector<u32> inputs;
  u32 a = inst.FA, b = inst.FB, c = inst.FC;
  for (u32 i : {a, b, c})
  {
    if (!js.op->fregsIn[i])
      continue;
    if (std::find(inputs.begin(), inputs.end(), i) == inputs.end())
      inputs.push_back(i);
  }
  if (inst.OPCD != 4)
  {
    // not paired-single
    UCOMISD(xmm, R(xmm));
    FixupBranch handle_nan = J_CC(CC_P, true);
    SwitchToFarCode();
    SetJumpTarget(handle_nan);
    std::vector<FixupBranch> fixups;
    for (u32 x : inputs)
    {
      MOVDDUP(xmm, fpr.R(x));
      UCOMISD(xmm, R(xmm));
      fixups.push_back(J_CC(CC_P));
    }
    MOVDDUP(xmm, MConst(psGeneratedQNaN));
    for (FixupBranch fixup : fixups)
      SetJumpTarget(fixup);
    FixupBranch done = J(true);
    SwitchToNearCode();
    SetJumpTarget(done);
  }
  else
  {
    // paired-single
    std::reverse(inputs.begin(), inputs.end());
    if (cpu_info.bSSE4_1)
    {
      avx_op(&XEmitter::VCMPPD, &XEmitter::CMPPD, clobber, R(xmm), R(xmm), CMP_UNORD);
      PTEST(clobber, R(clobber));
      FixupBranch handle_nan = J_CC(CC_NZ, true);
      SwitchToFarCode();
      SetJumpTarget(handle_nan);
      _assert_msg_(DYNA_REC, clobber == XMM0, "BLENDVPD implicitly uses XMM0");
      BLENDVPD(xmm, MConst(psGeneratedQNaN));
      for (u32 x : inputs)
      {
        avx_op(&XEmitter::VCMPPD, &XEmitter::CMPPD, clobber, fpr.R(x), fpr.R(x), CMP_UNORD);
        BLENDVPD(xmm, fpr.R(x));
      }
      FixupBranch done = J(true);
      SwitchToNearCode();
      SetJumpTarget(done);
    }
    else
    {
      // SSE2 fallback
      X64Reg tmp = fpr.GetFreeXReg();
      fpr.FlushLockX(tmp);
      MOVAPD(clobber, R(xmm));
      CMPPD(clobber, R(clobber), CMP_UNORD);
      MOVMSKPD(RSCRATCH, R(clobber));
      TEST(32, R(RSCRATCH), R(RSCRATCH));
      FixupBranch handle_nan = J_CC(CC_NZ, true);
      SwitchToFarCode();
      SetJumpTarget(handle_nan);
      MOVAPD(tmp, R(clobber));
      ANDNPD(clobber, R(xmm));
      ANDPD(tmp, MConst(psGeneratedQNaN));
      ORPD(tmp, R(clobber));
      MOVAPD(xmm, R(tmp));
      for (u32 x : inputs)
      {
        MOVAPD(clobber, fpr.R(x));
        CMPPD(clobber, R(clobber), CMP_ORD);
        MOVAPD(tmp, R(clobber));
        ANDNPD(clobber, fpr.R(x));
        ANDPD(xmm, R(tmp));
        ORPD(xmm, R(clobber));
      }
      FixupBranch done = J(true);
      SwitchToNearCode();
      SetJumpTarget(done);
      fpr.UnlockX(tmp);
    }
  }
  if (xmm_out != xmm)
    MOVAPD(xmm_out, R(xmm));
}

void Jit64::fp_arith(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);

  int a = inst.FA;
  int b = inst.FB;
  int c = inst.FC;
  int d = inst.FD;
  int arg2 = inst.SUBOP5 == 25 ? c : b;

  bool single = inst.OPCD == 4 || inst.OPCD == 59;
  // If both the inputs are known to have identical top and bottom halves, we can skip the MOVDDUP
  // at the end by
  // using packed arithmetic instead.
  bool packed = inst.OPCD == 4 ||
                (inst.OPCD == 59 && js.op->fprIsDuplicated[a] && js.op->fprIsDuplicated[arg2]);
  // Packed divides are slower than scalar divides on basically all x86, so this optimization isn't
  // worth it in that case.
  // Atoms (and a few really old CPUs) are also slower on packed operations than scalar ones.
  if (inst.OPCD == 59 && (inst.SUBOP5 == 18 || cpu_info.bAtom))
    packed = false;

  bool round_input = single && !js.op->fprIsSingle[inst.FC];
  bool preserve_inputs = SConfig::GetInstance().bAccurateNaNs;

  X64Reg dest = INVALID_REG;
  switch (inst.SUBOP5)
  {
  case 18:
    dest = fp_tri_op(d, a, b, false, single, packed ? &XEmitter::VDIVPD : &XEmitter::VDIVSD,
                     packed ? &XEmitter::DIVPD : &XEmitter::DIVSD, packed, preserve_inputs);
    break;
  case 20:
    dest = fp_tri_op(d, a, b, false, single, packed ? &XEmitter::VSUBPD : &XEmitter::VSUBSD,
                     packed ? &XEmitter::SUBPD : &XEmitter::SUBSD, packed, preserve_inputs);
    break;
  case 21:
    dest = fp_tri_op(d, a, b, true, single, packed ? &XEmitter::VADDPD : &XEmitter::VADDSD,
                     packed ? &XEmitter::ADDPD : &XEmitter::ADDSD, packed, preserve_inputs);
    break;
  case 25:
    dest = fp_tri_op(d, a, c, true, single, packed ? &XEmitter::VMULPD : &XEmitter::VMULSD,
                     packed ? &XEmitter::MULPD : &XEmitter::MULSD, packed, preserve_inputs,
                     round_input);
    break;
  default:
    _assert_msg_(DYNA_REC, 0, "fp_arith WTF!!!");
  }
  HandleNaNs(inst, fpr.RX(d), dest);
  if (single)
    ForceSinglePrecision(fpr.RX(d), fpr.R(d), packed, true);
  SetFPRFIfNeeded(fpr.RX(d));
  fpr.UnlockAll();
}

void Jit64::fmaddXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);

  int a = inst.FA;
  int b = inst.FB;
  int c = inst.FC;
  int d = inst.FD;
  bool single = inst.OPCD == 4 || inst.OPCD == 59;
  bool round_input = single && !js.op->fprIsSingle[c];
  bool packed = inst.OPCD == 4 || (!cpu_info.bAtom && single && js.op->fprIsDuplicated[a] &&
                                   js.op->fprIsDuplicated[b] && js.op->fprIsDuplicated[c]);

  fpr.Lock(a, b, c, d);

  switch (inst.SUBOP5)
  {
  case 14:
    MOVDDUP(XMM1, fpr.R(c));
    if (round_input)
      Force25BitPrecision(XMM1, R(XMM1), XMM0);
    break;
  case 15:
    avx_op(&XEmitter::VSHUFPD, &XEmitter::SHUFPD, XMM1, fpr.R(c), fpr.R(c), 3);
    if (round_input)
      Force25BitPrecision(XMM1, R(XMM1), XMM0);
    break;
  default:
    bool special = inst.SUBOP5 == 30 && (!cpu_info.bFMA || Core::WantsDeterminism());
    X64Reg tmp1 = special ? XMM0 : XMM1;
    X64Reg tmp2 = special ? XMM1 : XMM0;
    if (single && round_input)
      Force25BitPrecision(tmp1, fpr.R(c), tmp2);
    else
      MOVAPD(tmp1, fpr.R(c));
    break;
  }

  // While we don't know if any games are actually affected (replays seem to work with all the usual
  // suspects for desyncing), netplay and other applications need absolute perfect determinism, so
  // be extra careful and don't use FMA, even if in theory it might be okay.
  // Note that FMA isn't necessarily less correct (it may actually be closer to correct) compared
  // to what the Gekko does here; in deterministic mode, the important thing is multiple Dolphin
  // instances on different computers giving identical results.
  if (cpu_info.bFMA && !Core::WantsDeterminism())
  {
    // Statistics suggests b is a lot less likely to be unbound in practice, so
    // if we have to pick one of a or b to bind, let's make it b.
    fpr.BindToRegister(b, true, false);
    switch (inst.SUBOP5)
    {
    case 28:  // msub
      if (packed)
        VFMSUB132PD(XMM1, fpr.RX(b), fpr.R(a));
      else
        VFMSUB132SD(XMM1, fpr.RX(b), fpr.R(a));
      break;
    case 14:  // madds0
    case 15:  // madds1
    case 29:  // madd
      if (packed)
        VFMADD132PD(XMM1, fpr.RX(b), fpr.R(a));
      else
        VFMADD132SD(XMM1, fpr.RX(b), fpr.R(a));
      break;
    // PowerPC and x86 define NMADD/NMSUB differently
    // x86: D = -A*C (+/-) B
    // PPC: D = -(A*C (+/-) B)
    // so we have to swap them; the ADD/SUB here isn't a typo.
    case 30:  // nmsub
      if (packed)
        VFNMADD132PD(XMM1, fpr.RX(b), fpr.R(a));
      else
        VFNMADD132SD(XMM1, fpr.RX(b), fpr.R(a));
      break;
    case 31:  // nmadd
      if (packed)
        VFNMSUB132PD(XMM1, fpr.RX(b), fpr.R(a));
      else
        VFNMSUB132SD(XMM1, fpr.RX(b), fpr.R(a));
      break;
    }
  }
  else if (inst.SUBOP5 == 30)  // nmsub
  {
    // We implement nmsub a little differently ((b - a*c) instead of -(a*c - b)), so handle it
    // separately.
    MOVAPD(XMM1, fpr.R(b));
    if (packed)
    {
      MULPD(XMM0, fpr.R(a));
      SUBPD(XMM1, R(XMM0));
    }
    else
    {
      MULSD(XMM0, fpr.R(a));
      SUBSD(XMM1, R(XMM0));
    }
  }
  else
  {
    if (packed)
    {
      MULPD(XMM1, fpr.R(a));
      if (inst.SUBOP5 == 28)  // msub
        SUBPD(XMM1, fpr.R(b));
      else  //(n)madd(s[01])
        ADDPD(XMM1, fpr.R(b));
    }
    else
    {
      MULSD(XMM1, fpr.R(a));
      if (inst.SUBOP5 == 28)
        SUBSD(XMM1, fpr.R(b));
      else
        ADDSD(XMM1, fpr.R(b));
    }
    if (inst.SUBOP5 == 31)  // nmadd
      XORPD(XMM1, MConst(packed ? psSignBits2 : psSignBits));
  }
  fpr.BindToRegister(d, !single);
  if (single)
  {
    HandleNaNs(inst, fpr.RX(d), XMM1);
    ForceSinglePrecision(fpr.RX(d), fpr.R(d), packed, true);
  }
  else
  {
    HandleNaNs(inst, XMM1, XMM1);
    MOVSD(fpr.RX(d), R(XMM1));
  }
  SetFPRFIfNeeded(fpr.RX(d));
  fpr.UnlockAll();
}

void Jit64::fsign(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);

  int d = inst.FD;
  int b = inst.FB;
  bool packed = inst.OPCD == 4;

  fpr.Lock(b, d);
  OpArg src = fpr.R(b);
  fpr.BindToRegister(d, false);

  switch (inst.SUBOP10)
  {
  case 40:  // neg
    avx_op(&XEmitter::VXORPD, &XEmitter::XORPD, fpr.RX(d), src,
           MConst(packed ? psSignBits2 : psSignBits), packed);
    break;
  case 136:  // nabs
    avx_op(&XEmitter::VORPD, &XEmitter::ORPD, fpr.RX(d), src,
           MConst(packed ? psSignBits2 : psSignBits), packed);
    break;
  case 264:  // abs
    avx_op(&XEmitter::VANDPD, &XEmitter::ANDPD, fpr.RX(d), src,
           MConst(packed ? psAbsMask2 : psAbsMask), packed);
    break;
  default:
    PanicAlert("fsign bleh");
    break;
  }
  fpr.UnlockAll();
}

void Jit64::fselx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);

  int d = inst.FD;
  int a = inst.FA;
  int b = inst.FB;
  int c = inst.FC;

  bool packed = inst.OPCD == 4;  // ps_sel

  fpr.Lock(a, b, c, d);
  XORPD(XMM0, R(XMM0));
  // This condition is very tricky; there's only one right way to handle both the case of
  // negative/positive zero and NaN properly.
  // (a >= -0.0 ? c : b) transforms into (0 > a ? b : c), hence the NLE.
  if (packed)
    CMPPD(XMM0, fpr.R(a), CMP_NLE);
  else
    CMPSD(XMM0, fpr.R(a), CMP_NLE);

  if (cpu_info.bSSE4_1)
  {
    MOVAPD(XMM1, fpr.R(c));
    BLENDVPD(XMM1, fpr.R(b));
  }
  else
  {
    MOVAPD(XMM1, R(XMM0));
    ANDPD(XMM0, fpr.R(b));
    ANDNPD(XMM1, fpr.R(c));
    ORPD(XMM1, R(XMM0));
  }

  fpr.BindToRegister(d, !packed);
  if (packed)
    MOVAPD(fpr.RX(d), R(XMM1));
  else
    MOVSD(fpr.RX(d), R(XMM1));
  fpr.UnlockAll();
}

void Jit64::fmrx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);

  int d = inst.FD;
  int b = inst.FB;

  if (d == b)
    return;

  fpr.Lock(b, d);

  if (fpr.IsBound(d))
  {
    // We don't need to load d, but if it is loaded, we need to mark it as dirty.
    fpr.BindToRegister(d);
    // We have to use MOVLPD if b isn't loaded because "MOVSD reg, mem" sets the upper bits (64+)
    // to zero and we don't want that.
    if (!fpr.R(b).IsSimpleReg())
      MOVLPD(fpr.RX(d), fpr.R(b));
    else
      MOVSD(fpr.R(d), fpr.RX(b));
  }
  else
  {
    fpr.BindToRegister(b, true, false);
    MOVSD(fpr.R(d), fpr.RX(b));
  }

  fpr.UnlockAll();
}

void Jit64::FloatCompare(UGeckoInstruction inst, bool upper)
{
  bool fprf = SConfig::GetInstance().bFPRF && js.op->wantsFPRF;
  // bool ordered = !!(inst.SUBOP10 & 32);
  int a = inst.FA;
  int b = inst.FB;
  u32 crf = inst.CRFD;
  int output[4] = {CR_SO, CR_EQ, CR_GT, CR_LT};

  // Merge neighboring fcmp and cror (the primary use of cror).
  UGeckoInstruction next = js.op[1].inst;
  if (analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CROR_MERGE) &&
      CanMergeNextInstructions(1) && next.OPCD == 19 && next.SUBOP10 == 449 &&
      (next.CRBA >> 2) == crf && (next.CRBB >> 2) == crf && (next.CRBD >> 2) == crf)
  {
    js.skipInstructions = 1;
    js.downcountAmount++;
    int dst = 3 - (next.CRBD & 3);
    output[3 - (next.CRBD & 3)] &= ~(1 << dst);
    output[3 - (next.CRBA & 3)] |= 1 << dst;
    output[3 - (next.CRBB & 3)] |= 1 << dst;
  }

  fpr.Lock(a, b);
  fpr.BindToRegister(b, true, false);

  if (fprf)
    AND(32, PPCSTATE(fpscr), Imm32(~FPRF_MASK));

  if (upper)
  {
    fpr.BindToRegister(a, true, false);
    MOVHLPS(XMM0, fpr.RX(a));
    MOVHLPS(XMM1, fpr.RX(b));
    UCOMISD(XMM1, R(XMM0));
  }
  else
  {
    UCOMISD(fpr.RX(b), fpr.R(a));
  }

  FixupBranch pNaN, pLesser, pGreater;
  FixupBranch continue1, continue2, continue3;

  if (a != b)
  {
    // if B > A, goto Lesser's jump target
    pLesser = J_CC(CC_A);
  }

  // if (B != B) or (A != A), goto NaN's jump target
  pNaN = J_CC(CC_P);

  if (a != b)
  {
    // if B < A, goto Greater's jump target
    // JB can't precede the NaN check because it doesn't test ZF
    pGreater = J_CC(CC_B);
  }

  MOV(64, R(RSCRATCH), Imm64(PPCCRToInternal(output[CR_EQ_BIT])));
  if (fprf)
    OR(32, PPCSTATE(fpscr), Imm32(CR_EQ << FPRF_SHIFT));

  continue1 = J();

  SetJumpTarget(pNaN);
  MOV(64, R(RSCRATCH), Imm64(PPCCRToInternal(output[CR_SO_BIT])));
  if (fprf)
    OR(32, PPCSTATE(fpscr), Imm32(CR_SO << FPRF_SHIFT));

  if (a != b)
  {
    continue2 = J();

    SetJumpTarget(pGreater);
    MOV(64, R(RSCRATCH), Imm64(PPCCRToInternal(output[CR_GT_BIT])));
    if (fprf)
      OR(32, PPCSTATE(fpscr), Imm32(CR_GT << FPRF_SHIFT));
    continue3 = J();

    SetJumpTarget(pLesser);
    MOV(64, R(RSCRATCH), Imm64(PPCCRToInternal(output[CR_LT_BIT])));
    if (fprf)
      OR(32, PPCSTATE(fpscr), Imm32(CR_LT << FPRF_SHIFT));
  }

  SetJumpTarget(continue1);
  if (a != b)
  {
    SetJumpTarget(continue2);
    SetJumpTarget(continue3);
  }

  MOV(64, PPCSTATE(cr_val[crf]), R(RSCRATCH));
  fpr.UnlockAll();
}

void Jit64::fcmpX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);

  FloatCompare(inst);
}

void Jit64::fctiwx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);

  int d = inst.RD;
  int b = inst.RB;
  fpr.Lock(d, b);
  fpr.BindToRegister(d);

  // Intel uses 0x80000000 as a generic error code while PowerPC uses clamping:
  //
  // input       | output fctiw | output CVTPD2DQ
  // ------------+--------------+----------------
  // > +2^31 - 1 | 0x7fffffff   | 0x80000000
  // < -2^31     | 0x80000000   | 0x80000000
  // any NaN     | 0x80000000   | 0x80000000
  //
  // The upper 32 bits of the result are set to 0xfff80000,
  // except for -0.0 where they are set to 0xfff80001 (TODO).

  MOVAPD(XMM0, MConst(half_qnan_and_s32_max));
  MINSD(XMM0, fpr.R(b));
  switch (inst.SUBOP10)
  {
  // fctiwx
  case 14:
    CVTPD2DQ(XMM0, R(XMM0));
    break;

  // fctiwzx
  case 15:
    CVTTPD2DQ(XMM0, R(XMM0));
    break;
  }
  // d[64+] must not be modified
  MOVSD(fpr.R(d), XMM0);
  fpr.UnlockAll();
}

void Jit64::frspx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);
  int b = inst.FB;
  int d = inst.FD;
  bool packed = js.op->fprIsDuplicated[b] && !cpu_info.bAtom;

  fpr.Lock(b, d);
  OpArg src = fpr.R(b);
  fpr.BindToRegister(d, false);
  ForceSinglePrecision(fpr.RX(d), src, packed, true);
  SetFPRFIfNeeded(fpr.RX(d));
  fpr.UnlockAll();
}

void Jit64::frsqrtex(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);
  int b = inst.FB;
  int d = inst.FD;

  gpr.FlushLockX(RSCRATCH_EXTRA);
  fpr.Lock(b, d);
  fpr.BindToRegister(d);
  MOVAPD(XMM0, fpr.R(b));
  CALL(asm_routines.frsqrte);
  MOVSD(fpr.R(d), XMM0);
  SetFPRFIfNeeded(fpr.RX(d));
  fpr.UnlockAll();
  gpr.UnlockAllX();
}

void Jit64::fresx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);
  int b = inst.FB;
  int d = inst.FD;

  gpr.FlushLockX(RSCRATCH_EXTRA);
  fpr.Lock(b, d);
  MOVAPD(XMM0, fpr.R(b));
  fpr.BindToRegister(d, false);
  CALL(asm_routines.fres);
  MOVDDUP(fpr.RX(d), R(XMM0));
  SetFPRFIfNeeded(fpr.RX(d));
  fpr.UnlockAll();
  gpr.UnlockAllX();
}
