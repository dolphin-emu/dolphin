// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Jit64/Jit.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include "Common/Assert.h"
#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/SmallVector.h"
#include "Common/x64Emitter.h"
#include "Core/Config/SessionSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/PowerPC/Jit64/RegCache/JitRegCache.h"
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

// We can avoid calculating FPRF if it's not needed; every float operation resets it, so
// if it's going to be clobbered in a future instruction before being read, we can just
// not calculate it.
void Jit64::SetFPRFIfNeeded(const OpArg& input, bool single)
{
  // As far as we know, the games that use this flag only need FPRF for fmul and fmadd, but
  // FPRF is fast enough in JIT that we might as well just enable it for every float instruction
  // if the FPRF flag is set.
  if (!m_fprf || !js.op->wantsFPRF)
    return;

  X64Reg xmm = XMM0;
  if (input.IsSimpleReg())
    xmm = input.GetSimpleReg();
  else
    MOVSD(xmm, input);

  SetFPRF(xmm, single);
}

void Jit64::FinalizeSingleResult(X64Reg output, const OpArg& input, bool packed, bool duplicate)
{
  // Most games don't need these. Zelda requires it though - some platforms get stuck without them.
  if (jo.accurateSinglePrecision)
  {
    if (packed)
    {
      CVTPD2PS(output, input);
      SetFPRFIfNeeded(R(output), true);
      CVTPS2PD(output, R(output));
    }
    else
    {
      CVTSD2SS(output, input);
      SetFPRFIfNeeded(R(output), true);
      CVTSS2SD(output, R(output));
      if (duplicate)
        MOVDDUP(output, R(output));
    }
  }
  else
  {
    if (!input.IsSimpleReg(output))
    {
      if (duplicate)
        MOVDDUP(output, input);
      else
        MOVAPD(output, input);
    }

    SetFPRFIfNeeded(input, false);
  }
}

void Jit64::FinalizeDoubleResult(X64Reg output, const OpArg& input)
{
  if (!input.IsSimpleReg(output))
    MOVSD(output, input);

  SetFPRFIfNeeded(input, false);
}

void Jit64::HandleNaNs(UGeckoInstruction inst, X64Reg xmm, X64Reg clobber, std::optional<OpArg> Ra,
                       std::optional<OpArg> Rb, std::optional<OpArg> Rc)
{
  //                      | PowerPC  | x86
  // ---------------------+----------+---------
  // input NaN precedence | 1*3 + 2  | 1*2 + 3
  // generated QNaN       | positive | negative
  //
  // Dragon Ball: Revenge of King Piccolo requires generated NaNs
  // to be positive, so we'll have to handle them manually.

  if (!m_accurate_nans)
    return;

  if (inst.OPCD != 4)
  {
    // not paired-single

    UCOMISD(xmm, R(xmm));
    FixupBranch handle_nan = J_CC(CC_P, Jump::Near);
    SwitchToFarCode();
    SetJumpTarget(handle_nan);

    // If any inputs are NaNs, pick the first NaN of them
    Common::SmallVector<FixupBranch, 3> fixups;
    const auto check_input = [&](const OpArg& Rx) {
      MOVDDUP(xmm, Rx);
      UCOMISD(xmm, R(xmm));
      fixups.push_back(J_CC(CC_P));
    };
    if (Ra)
      check_input(*Ra);
    if (Rb && Ra != Rb)
      check_input(*Rb);
    if (Rc && Ra != Rc && Rb != Rc)
      check_input(*Rc);

    // Otherwise, pick the PPC default NaN (will be finished below)
    XORPD(xmm, R(xmm));

    // Turn SNaNs into QNaNs (or finish writing the PPC default NaN)
    for (FixupBranch fixup : fixups)
      SetJumpTarget(fixup);
    ORPD(xmm, MConst(psGeneratedQNaN));

    FixupBranch done = J(Jump::Near);
    SwitchToNearCode();
    SetJumpTarget(done);
  }
  else
  {
    // paired-single

    ASSERT(xmm != clobber);

    if (cpu_info.bSSE4_1)
    {
      avx_op(&XEmitter::VCMPPD, &XEmitter::CMPPD, clobber, R(xmm), R(xmm), CMP_UNORD);
      PTEST(clobber, R(clobber));
      FixupBranch handle_nan = J_CC(CC_NZ, Jump::Near);
      SwitchToFarCode();
      SetJumpTarget(handle_nan);

      // Replace NaNs with PPC default NaN
      ASSERT_MSG(DYNA_REC, clobber == XMM0, "BLENDVPD implicitly uses XMM0");
      BLENDVPD(xmm, MConst(psGeneratedQNaN));

      // If any inputs are NaNs, use those instead
      const auto check_input = [&](const OpArg& Rx) {
        avx_op(&XEmitter::VCMPPD, &XEmitter::CMPPD, clobber, Rx, Rx, CMP_UNORD);
        BLENDVPD(xmm, Rx);
      };
      if (Rc)
        check_input(*Rc);
      if (Rb && Rb != Rc)
        check_input(*Rb);
      if (Ra && Ra != Rb && Ra != Rc)
        check_input(*Ra);
    }
    else
    {
      // SSE2 fallback

      RCX64Reg tmp = fpr.Scratch();
      RegCache::Realize(tmp);
      MOVAPD(clobber, R(xmm));
      CMPPD(clobber, R(clobber), CMP_UNORD);
      MOVMSKPD(RSCRATCH, R(clobber));
      TEST(32, R(RSCRATCH), R(RSCRATCH));
      FixupBranch handle_nan = J_CC(CC_NZ, Jump::Near);
      SwitchToFarCode();
      SetJumpTarget(handle_nan);

      // Replace NaNs with PPC default NaN
      MOVAPD(tmp, R(clobber));
      ANDNPD(clobber, R(xmm));
      ANDPD(tmp, MConst(psGeneratedQNaN));
      ORPD(tmp, R(clobber));
      MOVAPD(xmm, tmp);

      // If any inputs are NaNs, use those instead
      const auto check_input = [&](const OpArg& Rx) {
        MOVAPD(clobber, Rx);
        CMPPD(clobber, R(clobber), CMP_ORD);
        MOVAPD(tmp, R(clobber));
        ANDNPD(clobber, Rx);
        ANDPD(xmm, tmp);
        ORPD(xmm, R(clobber));
      };
      if (Rc)
        check_input(*Rc);
      if (Rb && Rb != Rc)
        check_input(*Rb);
      if (Ra && Ra != Rb && Ra != Rc)
        check_input(*Ra);
    }

    // Turn SNaNs into QNaNs
    avx_op(&XEmitter::VCMPPD, &XEmitter::CMPPD, clobber, R(xmm), R(xmm), CMP_UNORD);
    ANDPD(clobber, MConst(psGeneratedQNaN));
    ORPD(xmm, R(clobber));

    FixupBranch done = J(Jump::Near);
    SwitchToNearCode();
    SetJumpTarget(done);
  }
}

void Jit64::fp_arith(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);
  FALLBACK_IF(jo.fp_exceptions || (jo.div_by_zero_exceptions && inst.SUBOP5 == 18));

  int a = inst.FA;
  int b = inst.FB;
  int c = inst.FC;
  int d = inst.FD;
  int arg2 = inst.SUBOP5 == 25 ? c : b;

  bool single = inst.OPCD == 4 || inst.OPCD == 59;
  // If both the inputs are known to have identical top and bottom halves, we can skip the MOVDDUP
  // at the end by using packed arithmetic instead.
  bool packed = inst.OPCD == 4 ||
                (inst.OPCD == 59 && js.op->fprIsDuplicated[a] && js.op->fprIsDuplicated[arg2]);
  // Packed divides are slower than scalar divides on basically all x86, so this optimization isn't
  // worth it in that case.
  // Atoms (and a few really old CPUs) are also slower on packed operations than scalar ones.
  if (inst.OPCD == 59 && (inst.SUBOP5 == 18 || cpu_info.bAtom))
    packed = false;

  void (XEmitter::*avxOp)(X64Reg, X64Reg, const OpArg&) = nullptr;
  void (XEmitter::*sseOp)(X64Reg, const OpArg&) = nullptr;
  bool reversible = false;
  bool round_rhs = false;
  bool preserve_inputs = false;
  switch (inst.SUBOP5)
  {
  case 18:
    preserve_inputs = m_accurate_nans;
    avxOp = packed ? &XEmitter::VDIVPD : &XEmitter::VDIVSD;
    sseOp = packed ? &XEmitter::DIVPD : &XEmitter::DIVSD;
    break;
  case 20:
    avxOp = packed ? &XEmitter::VSUBPD : &XEmitter::VSUBSD;
    sseOp = packed ? &XEmitter::SUBPD : &XEmitter::SUBSD;
    break;
  case 21:
    reversible = !m_accurate_nans;
    avxOp = packed ? &XEmitter::VADDPD : &XEmitter::VADDSD;
    sseOp = packed ? &XEmitter::ADDPD : &XEmitter::ADDSD;
    break;
  case 25:
    reversible = true;
    round_rhs = single && !js.op->fprIsSingle[c];
    preserve_inputs = m_accurate_nans;
    avxOp = packed ? &XEmitter::VMULPD : &XEmitter::VMULSD;
    sseOp = packed ? &XEmitter::MULPD : &XEmitter::MULSD;
    break;
  default:
    ASSERT_MSG(DYNA_REC, 0, "fp_arith WTF!!!");
  }

  RCX64Reg Rd = fpr.Bind(d, !single ? RCMode::ReadWrite : RCMode::Write);
  RCOpArg Ra = fpr.Use(a, RCMode::Read);
  RCOpArg Rarg2 = fpr.Use(arg2, RCMode::Read);
  RegCache::Realize(Rd, Ra, Rarg2);

  X64Reg dest = X64Reg(Rd);
  if (preserve_inputs && (a == d || arg2 == d))
    dest = XMM1;
  if (round_rhs)
  {
    if (a == d && !preserve_inputs)
    {
      Force25BitPrecision(XMM0, Rarg2, XMM1);
      (this->*sseOp)(Rd, R(XMM0));
    }
    else
    {
      Force25BitPrecision(dest, Rarg2, XMM0);
      (this->*sseOp)(dest, Ra);
    }
  }
  else
  {
    if (Ra.IsSimpleReg(dest))
    {
      (this->*sseOp)(dest, Rarg2);
    }
    else if (reversible && Rarg2.IsSimpleReg(dest))
    {
      (this->*sseOp)(dest, Ra);
    }
    else if (cpu_info.bAVX && Ra.IsSimpleReg())
    {
      (this->*avxOp)(dest, Ra.GetSimpleReg(), Rarg2);
    }
    else if (cpu_info.bAVX && reversible && Rarg2.IsSimpleReg())
    {
      (this->*avxOp)(dest, Rarg2.GetSimpleReg(), Ra);
    }
    else
    {
      if (Rarg2.IsSimpleReg(dest))
        dest = XMM1;

      if (packed)
        MOVAPD(dest, Ra);
      else
        MOVSD(dest, Ra);
      (this->*sseOp)(dest, a == arg2 ? R(dest) : Rarg2);
    }
  }

  switch (inst.SUBOP5)
  {
  case 18:
    HandleNaNs(inst, dest, XMM0, Ra, Rarg2, std::nullopt);
    break;
  case 25:
    HandleNaNs(inst, dest, XMM0, Ra, std::nullopt, Rarg2);
    break;
  }

  if (single)
    FinalizeSingleResult(Rd, R(dest), packed, true);
  else
    FinalizeDoubleResult(Rd, R(dest));
}

void Jit64::fmaddXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);
  FALLBACK_IF(jo.fp_exceptions);

  // We would like to emulate FMA instructions accurately without rounding error if possible, but
  // unfortunately, emulating FMA in software is just too slow on CPUs that are too old to have FMA
  // instructions, so we have the Config::SESSION_USE_FMA setting to determine whether we should
  // emulate FMA instructions accurately or by a performing a multiply followed by a separate add.
  //
  // Why have a setting instead of just checking cpu_info.bFMA, you might wonder? Because for
  // netplay and TAS, it's important that everyone gets exactly the same results. The setting
  // is not user configurable - Dolphin automatically sets it based on what is supported by the
  // CPUs of everyone in the netplay room (or when not using netplay, simply the system's CPU).
  //
  // There is one circumstance where the software FMA path does get used: when an input recording
  // is created on a CPU that has FMA instructions and then gets played back on a CPU that doesn't.
  // (Or if the user just really wants to override the setting and knows how to do so.)
  const bool use_fma = Config::Get(Config::SESSION_USE_FMA);
  const bool software_fma = use_fma && !cpu_info.bFMA;

  int a = inst.FA;
  int b = inst.FB;
  int c = inst.FC;
  int d = inst.FD;
  bool single = inst.OPCD == 4 || inst.OPCD == 59;
  bool round_input = single && !js.op->fprIsSingle[c];
  bool preserve_inputs = m_accurate_nans;
  bool preserve_d = preserve_inputs && (a == d || b == d || c == d);
  bool packed =
      inst.OPCD == 4 || (!cpu_info.bAtom && !software_fma && single && js.op->fprIsDuplicated[a] &&
                         js.op->fprIsDuplicated[b] && js.op->fprIsDuplicated[c]);

  const bool subtract = inst.SUBOP5 == 28 || inst.SUBOP5 == 30;  // msub, nmsub
  const bool negate = inst.SUBOP5 == 30 || inst.SUBOP5 == 31;    // nmsub, nmadd
  const bool madds0 = inst.SUBOP5 == 14;
  const bool madds1 = inst.SUBOP5 == 15;
  const bool madds_accurate_nans = m_accurate_nans && (madds0 || madds1);

  X64Reg scratch_xmm = XMM0;
  X64Reg result_xmm = XMM1;
  X64Reg Rc_duplicated = XMM2;

  RCOpArg Ra;
  RCOpArg Rb;
  RCOpArg Rc;
  RCX64Reg Rd;
  if (software_fma)
  {
    RCX64Reg xmm2_guard;
    xmm2_guard = fpr.Scratch(XMM2);
    Ra = packed ? fpr.Bind(a, RCMode::Read) : fpr.Use(a, RCMode::Read);
    Rb = packed ? fpr.Bind(b, RCMode::Read) : fpr.Use(b, RCMode::Read);
    Rc = packed ? fpr.Bind(c, RCMode::Read) : fpr.Use(c, RCMode::Read);
    Rd = fpr.Bind(d, single ? RCMode::Write : RCMode::ReadWrite);
    if (preserve_d && packed)
    {
      RCX64Reg result_xmm_guard;
      result_xmm_guard = fpr.Scratch();
      RegCache::Realize(Ra, Rb, Rc, Rd, xmm2_guard, result_xmm_guard);
      result_xmm = Gen::X64Reg(result_xmm_guard);
    }
    else
    {
      RegCache::Realize(Ra, Rb, Rc, Rd, xmm2_guard);
      result_xmm = packed ? Gen::X64Reg(Rd) : XMM0;
    }
  }
  else
  {
    // For use_fma == true:
    //   Statistics suggests b is a lot less likely to be unbound in practice, so
    //   if we have to pick one of a or b to bind, let's make it b.
    Ra = fpr.Use(a, RCMode::Read);
    Rb = use_fma ? fpr.Bind(b, RCMode::Read) : fpr.Use(b, RCMode::Read);
    Rc = fpr.Use(c, RCMode::Read);
    Rd = fpr.Bind(d, single ? RCMode::Write : RCMode::ReadWrite);
    RegCache::Realize(Ra, Rb, Rc, Rd);

    if (madds_accurate_nans)
    {
      RCX64Reg Rc_duplicated_guard;
      Rc_duplicated_guard = fpr.Scratch();
      RegCache::Realize(Rc_duplicated_guard);
      Rc_duplicated = Rc_duplicated_guard;
    }
  }

  if (software_fma)
  {
    for (size_t i = (packed ? 1 : 0); i != std::numeric_limits<size_t>::max(); --i)
    {
      if ((i == 0 || madds0) && !madds1)
      {
        if (round_input)
          Force25BitPrecision(XMM1, Rc, XMM2);
        else
          MOVSD(XMM1, Rc);
      }
      else
      {
        MOVHLPS(XMM1, Rc.GetSimpleReg());
        if (round_input)
          Force25BitPrecision(XMM1, R(XMM1), XMM2);
      }

      // Write the result from the previous loop iteration into result_xmm so we don't lose it.
      // It's important that this is done after reading Rc above, in case we have madds1 and
      // result_xmm == Rd == Rc.
      if (packed && i == 0)
        MOVLHPS(result_xmm, XMM0);

      if (i == 0)
      {
        MOVSD(XMM0, Ra);
        MOVSD(XMM2, Rb);
      }
      else
      {
        MOVHLPS(XMM0, Ra.GetSimpleReg());
        MOVHLPS(XMM2, Rb.GetSimpleReg());
      }

      if (subtract)
        XORPS(XMM2, MConst(psSignBits));

      BitSet32 registers_in_use = CallerSavedRegistersInUse();
      ABI_PushRegistersAndAdjustStack(registers_in_use, 0);
      ABI_CallFunction(static_cast<double (*)(double, double, double)>(&std::fma));
      ABI_PopRegistersAndAdjustStack(registers_in_use, 0);
    }

    if (packed)
      MOVSD(R(result_xmm), XMM0);
    else
      DEBUG_ASSERT(result_xmm == XMM0);

    if (madds_accurate_nans)
    {
      if (madds0)
        MOVDDUP(Rc_duplicated, Rc);
      else
        avx_op(&XEmitter::VSHUFPD, &XEmitter::SHUFPD, Rc_duplicated, Rc, Rc, 3);
    }
  }
  else
  {
    if (madds0)
    {
      MOVDDUP(result_xmm, Rc);
      if (madds_accurate_nans)
        MOVAPD(R(Rc_duplicated), result_xmm);
      if (round_input)
        Force25BitPrecision(result_xmm, R(result_xmm), scratch_xmm);
    }
    else if (madds1)
    {
      avx_op(&XEmitter::VSHUFPD, &XEmitter::SHUFPD, result_xmm, Rc, Rc, 3);
      if (madds_accurate_nans)
        MOVAPD(R(Rc_duplicated), result_xmm);
      if (round_input)
        Force25BitPrecision(result_xmm, R(result_xmm), scratch_xmm);
    }
    else
    {
      if (round_input)
        Force25BitPrecision(result_xmm, Rc, scratch_xmm);
      else
        MOVAPD(result_xmm, Rc);
    }

    if (use_fma)
    {
      if (subtract)
      {
        if (packed)
          VFMSUB132PD(result_xmm, Rb.GetSimpleReg(), Ra);
        else
          VFMSUB132SD(result_xmm, Rb.GetSimpleReg(), Ra);
      }
      else
      {
        if (packed)
          VFMADD132PD(result_xmm, Rb.GetSimpleReg(), Ra);
        else
          VFMADD132SD(result_xmm, Rb.GetSimpleReg(), Ra);
      }
    }
    else
    {
      if (packed)
      {
        MULPD(result_xmm, Ra);
        if (subtract)
          SUBPD(result_xmm, Rb);
        else
          ADDPD(result_xmm, Rb);
      }
      else
      {
        MULSD(result_xmm, Ra);
        if (subtract)
          SUBSD(result_xmm, Rb);
        else
          ADDSD(result_xmm, Rb);
      }
    }
  }

  // Using x64's nmadd/nmsub would require us to swap the sign of the addend
  // (i.e. PPC nmadd maps to x64 nmsub), which can cause problems with signed zeroes.
  // Also, PowerPC's nmadd/nmsub round before the final negation unlike x64's nmadd/nmsub.
  // So, negate using a separate instruction instead of using x64's nmadd/nmsub.
  if (negate)
    XORPD(result_xmm, MConst(packed ? psSignBits2 : psSignBits));

  if (m_accurate_nans && result_xmm == XMM0)
  {
    // HandleNaNs needs to clobber XMM0
    MOVAPD(Rd, R(result_xmm));
    result_xmm = Rd;
    DEBUG_ASSERT(!preserve_d);
  }

  // If packed, the clobber register must be XMM0. If not packed, the clobber register is unused.
  HandleNaNs(inst, result_xmm, XMM0, Ra, Rb, madds_accurate_nans ? R(Rc_duplicated) : Rc);

  if (single)
    FinalizeSingleResult(Rd, R(result_xmm), packed, true);
  else
    FinalizeDoubleResult(Rd, R(result_xmm));
}

void Jit64::fsign(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);

  int d = inst.FD;
  int b = inst.FB;
  bool packed = inst.OPCD == 4;

  RCOpArg src = fpr.Use(b, RCMode::Read);
  RCX64Reg Rd = fpr.Bind(d, RCMode::Write);
  RegCache::Realize(src, Rd);

  switch (inst.SUBOP10)
  {
  case 40:  // neg
    avx_op(&XEmitter::VXORPD, &XEmitter::XORPD, Rd, src, MConst(packed ? psSignBits2 : psSignBits),
           packed);
    break;
  case 136:  // nabs
    avx_op(&XEmitter::VORPD, &XEmitter::ORPD, Rd, src, MConst(packed ? psSignBits2 : psSignBits),
           packed);
    break;
  case 264:  // abs
    avx_op(&XEmitter::VANDPD, &XEmitter::ANDPD, Rd, src, MConst(packed ? psAbsMask2 : psAbsMask),
           packed);
    break;
  default:
    PanicAlertFmt("fsign bleh");
    break;
  }
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

  RCOpArg Ra = fpr.Use(a, RCMode::Read);
  RCOpArg Rb = fpr.Use(b, RCMode::Read);
  RCOpArg Rc = fpr.Use(c, RCMode::Read);
  RCX64Reg Rd = fpr.Bind(d, packed ? RCMode::Write : RCMode::ReadWrite);
  RegCache::Realize(Ra, Rb, Rc, Rd);

  XORPD(XMM0, R(XMM0));
  // This condition is very tricky; there's only one right way to handle both the case of
  // negative/positive zero and NaN properly.
  // (a >= -0.0 ? c : b) transforms into (0 > a ? b : c), hence the NLE.
  if (packed)
    CMPPD(XMM0, Ra, CMP_NLE);
  else
    CMPSD(XMM0, Ra, CMP_NLE);

  if (cpu_info.bAVX)
  {
    X64Reg src1 = XMM1;
    if (Rc.IsSimpleReg())
    {
      src1 = Rc.GetSimpleReg();
    }
    else
    {
      MOVAPD(XMM1, Rc);
    }

    if (d == c || packed)
    {
      VBLENDVPD(Rd, src1, Rb, XMM0);
      return;
    }

    VBLENDVPD(XMM1, src1, Rb, XMM0);
  }
  else if (cpu_info.bSSE4_1)
  {
    if (d == c)
    {
      BLENDVPD(Rd, Rb);
      return;
    }

    MOVAPD(XMM1, Rc);
    BLENDVPD(XMM1, Rb);
  }
  else
  {
    MOVAPD(XMM1, R(XMM0));
    ANDPD(XMM0, Rb);
    ANDNPD(XMM1, Rc);
    ORPD(XMM1, R(XMM0));
  }

  if (packed)
    MOVAPD(Rd, R(XMM1));
  else
    MOVSD(Rd, R(XMM1));
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

  RCOpArg Rd = fpr.Use(d, RCMode::Write);
  RegCache::Realize(Rd);
  if (Rd.IsSimpleReg())
  {
    RCOpArg Rb = fpr.Use(b, RCMode::Read);
    RegCache::Realize(Rb);
    // We have to use MOVLPD if b isn't loaded because "MOVSD reg, mem" sets the upper bits (64+)
    // to zero and we don't want that.
    if (!Rb.IsSimpleReg())
      MOVLPD(Rd.GetSimpleReg(), Rb);
    else
      MOVSD(Rd, Rb.GetSimpleReg());
  }
  else
  {
    RCOpArg Rb = fpr.Bind(b, RCMode::Read);
    RegCache::Realize(Rb);
    MOVSD(Rd, Rb.GetSimpleReg());
  }
}

void Jit64::FloatCompare(UGeckoInstruction inst, bool upper)
{
  bool fprf = m_fprf && js.op->wantsFPRF;
  // bool ordered = !!(inst.SUBOP10 & 32);
  int a = inst.FA;
  int b = inst.FB;
  u32 crf = inst.CRFD;
  int output[4] = {PowerPC::CR_SO, PowerPC::CR_EQ, PowerPC::CR_GT, PowerPC::CR_LT};

  // Merge neighboring fcmp and cror (the primary use of cror).
  UGeckoInstruction next = js.op[1].inst;
  if (analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CROR_MERGE) &&
      CanMergeNextInstructions(1) && next.OPCD == 19 && next.SUBOP10 == 449 &&
      static_cast<u32>(next.CRBA >> 2) == crf && static_cast<u32>(next.CRBB >> 2) == crf &&
      static_cast<u32>(next.CRBD >> 2) == crf)
  {
    js.skipInstructions = 1;
    js.downcountAmount++;
    int dst = 3 - (next.CRBD & 3);
    output[3 - (next.CRBD & 3)] &= ~(1 << dst);
    output[3 - (next.CRBA & 3)] |= 1 << dst;
    output[3 - (next.CRBB & 3)] |= 1 << dst;
  }

  RCOpArg Ra = upper ? fpr.Bind(a, RCMode::Read) : fpr.Use(a, RCMode::Read);
  RCX64Reg Rb = fpr.Bind(b, RCMode::Read);
  RegCache::Realize(Ra, Rb);

  if (fprf)
    AND(32, PPCSTATE(fpscr), Imm32(~FPCC_MASK));

  if (upper)
  {
    MOVHLPS(XMM0, Ra.GetSimpleReg());
    MOVHLPS(XMM1, Rb);
    UCOMISD(XMM1, R(XMM0));
  }
  else
  {
    UCOMISD(Rb, Ra);
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

  MOV(64, R(RSCRATCH),
      Imm64(PowerPC::ConditionRegister::PPCToInternal(output[PowerPC::CR_EQ_BIT])));
  if (fprf)
    OR(32, PPCSTATE(fpscr), Imm32(PowerPC::CR_EQ << FPRF_SHIFT));

  continue1 = J();

  SetJumpTarget(pNaN);
  MOV(64, R(RSCRATCH),
      Imm64(PowerPC::ConditionRegister::PPCToInternal(output[PowerPC::CR_SO_BIT])));
  if (fprf)
    OR(32, PPCSTATE(fpscr), Imm32(PowerPC::CR_SO << FPRF_SHIFT));

  if (a != b)
  {
    continue2 = J();

    SetJumpTarget(pGreater);
    MOV(64, R(RSCRATCH),
        Imm64(PowerPC::ConditionRegister::PPCToInternal(output[PowerPC::CR_GT_BIT])));
    if (fprf)
      OR(32, PPCSTATE(fpscr), Imm32(PowerPC::CR_GT << FPRF_SHIFT));
    continue3 = J();

    SetJumpTarget(pLesser);
    MOV(64, R(RSCRATCH),
        Imm64(PowerPC::ConditionRegister::PPCToInternal(output[PowerPC::CR_LT_BIT])));
    if (fprf)
      OR(32, PPCSTATE(fpscr), Imm32(PowerPC::CR_LT << FPRF_SHIFT));
  }

  SetJumpTarget(continue1);
  if (a != b)
  {
    SetJumpTarget(continue2);
    SetJumpTarget(continue3);
  }

  MOV(64, PPCSTATE_CR(crf), R(RSCRATCH));
}

void Jit64::fcmpX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(jo.fp_exceptions);

  FloatCompare(inst);
}

void Jit64::fctiwx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);
  FALLBACK_IF(jo.fp_exceptions);

  int d = inst.RD;
  int b = inst.RB;

  RCOpArg Rb = fpr.Use(b, RCMode::Read);
  RCX64Reg Rd = fpr.Bind(d, RCMode::Write);
  RegCache::Realize(Rb, Rd);

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
  MINSD(XMM0, Rb);
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
  MOVSD(Rd, XMM0);
}

void Jit64::frspx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);
  FALLBACK_IF(jo.fp_exceptions);
  int b = inst.FB;
  int d = inst.FD;
  bool packed = js.op->fprIsDuplicated[b] && !cpu_info.bAtom;

  RCOpArg Rb = fpr.Bind(b, RCMode::Read);
  RCX64Reg Rd = fpr.Bind(d, RCMode::Write);
  RegCache::Realize(Rb, Rd);

  FinalizeSingleResult(Rd, Rb, packed, true);
}

void Jit64::frsqrtex(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);
  FALLBACK_IF(jo.fp_exceptions || jo.div_by_zero_exceptions);
  int b = inst.FB;
  int d = inst.FD;

  RCX64Reg scratch_guard = gpr.Scratch(RSCRATCH_EXTRA);
  RCOpArg Rb = fpr.Use(b, RCMode::Read);
  RCX64Reg Rd = fpr.Bind(d, RCMode::Write);
  RegCache::Realize(scratch_guard, Rb, Rd);

  MOVAPD(XMM0, Rb);
  CALL(asm_routines.frsqrte);
  FinalizeDoubleResult(Rd, R(XMM0));
}

void Jit64::fresx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);
  FALLBACK_IF(jo.fp_exceptions || jo.div_by_zero_exceptions);
  int b = inst.FB;
  int d = inst.FD;

  RCX64Reg scratch_guard = gpr.Scratch(RSCRATCH_EXTRA);
  RCOpArg Rb = fpr.Use(b, RCMode::Read);
  RCX64Reg Rd = fpr.Bind(d, RCMode::Write);
  RegCache::Realize(scratch_guard, Rb, Rd);

  MOVAPD(XMM0, Rb);
  CALL(asm_routines.fres);
  MOVDDUP(Rd, R(XMM0));
  SetFPRFIfNeeded(R(XMM0), true);
}
