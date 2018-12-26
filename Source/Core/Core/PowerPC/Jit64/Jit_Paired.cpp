// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Common/x64Emitter.h"
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/RegCache/JitRegCache.h"

using namespace Gen;

void Jit64::ps_mr(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  FALLBACK_IF(inst.Rc);

  int d = inst.FD;
  int b = inst.FB;
  if (d == b)
    return;

  RCRepr repr = fpr.GetRepr(b);
  RCOpArg Rb = fpr.Use(b, RCMode::Read, repr);
  RCX64Reg Rd = fpr.Bind(d, RCMode::Write);
  RegCache::Realize(Rb, Rd);
  MOVAPD(Rd, Rb);
  Rd.SetRepr(repr);
}

void Jit64::ps_sum(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  FALLBACK_IF(inst.Rc);

  int d = inst.FD;
  int a = inst.FA;
  int b = inst.FB;
  int c = inst.FC;

  ASSERT(inst.SUBOP5 == 10 || inst.SUBOP5 == 11);
  const bool sum0 = inst.SUBOP5 == 10;

  if (fpr.IsSingle(a, b, c))
  {
    RCX64Reg Ra = fpr.Bind(a, RCMode::Read, sum0 ? RCRepr::LowerSingle : RCRepr::PairSingles);
    RCX64Reg Rb = fpr.Bind(b, RCMode::Read, RCRepr::PairSingles);
    RCX64Reg Rc = fpr.Bind(c, RCMode::Read, sum0 ? RCRepr::PairSingles : RCRepr::LowerSingle);
    RCX64Reg Rd = fpr.Bind(d, RCMode::Write);
    RegCache::Realize(Ra, Rb, Rc, Rd);

    X64Reg tmp = XMM1;
    if (sum0)
    {
      // ps_sum0: {a.ps0 + b.ps1, c.ps1}
      if (!fpr.IsDupPhysical(b))
      {
        MOVSHDUP(tmp, Rb);
        ADDPS(tmp, Ra);
      }
      else
      {
        avx_sop(&XEmitter::VADDPS, &XEmitter::ADDPS, tmp, Rb, Ra);
      }
      if (cpu_info.bSSE4_1)
      {
        BLENDPS(tmp, Rc, 2);
      }
      else
      {
        MOVSS(tmp, Rc);
      }
    }
    else
    {
      // ps_sum1: {c.ps0, a.ps0 + b.ps1}
      if (!fpr.IsDupPhysical(a))
      {
        MOVSLDUP(tmp, Ra);
        ADDPS(tmp, Rb);
      }
      else
      {
        avx_sop(&XEmitter::VADDPS, &XEmitter::ADDPS, tmp, Ra, Rb);
      }
      if (cpu_info.bSSE4_1)
      {
        BLENDPS(tmp, Rc, 1);
      }
      else
      {
        MOVAPS(XMM0, Rc);
        MOVSS(XMM0, R(tmp));
        tmp = XMM0;
      }
    }
    RCRepr repr = HandleNaNs(inst, Rd, tmp, RCRepr::PairSingles, tmp == XMM1 ? XMM0 : XMM1);
    Rd.SetRepr(repr);
    SetFPRFIfNeeded(Rd);
  }
  else
  {
    RCOpArg Ra = fpr.Use(a, RCMode::Read, sum0 ? RCRepr::LowerDouble : RCRepr::Canonical);
    RCOpArg Rb = fpr.Use(b, RCMode::Read, RCRepr::Canonical);
    RCOpArg Rc = fpr.Use(c, RCMode::Read, sum0 ? RCRepr::Canonical : RCRepr::LowerDouble);
    RCX64Reg Rd = fpr.Bind(d, RCMode::Write);
    RegCache::Realize(Ra, Rb, Rc, Rd);

    X64Reg tmp = XMM1;
    if (!fpr.IsDupPhysical(a))
    {
      MOVDDUP(tmp, Ra);  // {a.ps0, a.ps0}
      ADDPD(tmp, Rb);    // {a.ps0 + b.ps0, a.ps0 + b.ps1}
    }
    else
    {
      avx_dop(&XEmitter::VADDPD, &XEmitter::ADDPD, tmp, Ra, Rb);
    }
    if (sum0)
    {
      // ps_sum0: {a.ps0 + b.ps1, c.ps1}
      UNPCKHPD(tmp, Rc);
    }
    else
    {
      // ps_sum1: {c.ps0, a.ps0 + b.ps1}
      if (Rc.IsSimpleReg())
      {
        if (cpu_info.bSSE4_1)
        {
          BLENDPD(tmp, Rc, 1);
        }
        else
        {
          MOVAPD(XMM0, Rc);
          SHUFPD(XMM0, R(tmp), 2);
          tmp = XMM0;
        }
      }
      else
      {
        MOVLPD(tmp, Rc);
      }
    }
    RCRepr repr = HandleNaNs(inst, Rd, tmp, RCRepr::Canonical, tmp == XMM1 ? XMM0 : XMM1);
    Rd.SetRepr(repr);
    ForceSinglePrecision(Rd, Rd);
    SetFPRFIfNeeded(Rd);
  }
}

void Jit64::ps_muls(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  FALLBACK_IF(inst.Rc);

  int d = inst.FD;
  int a = inst.FA;
  int c = inst.FC;

  ASSERT(inst.SUBOP5 == 12 || inst.SUBOP5 == 13);
  const bool muls0 = inst.SUBOP5 == 12;

  if (fpr.IsDup(a, c))
  {
    RCRepr in_repr = fpr.IsSingle(a, c) ? RCRepr::DupSingles : RCRepr::Dup;
    RCOpArg Ra = fpr.Use(a, RCMode::Read, in_repr);
    RCOpArg Rc = fpr.Use(c, RCMode::Read, in_repr);
    RCX64Reg Rd = fpr.Bind(d, RCMode::Write);
    RegCache::Realize(Ra, Rc, Rd);

    if (in_repr == RCRepr::DupSingles)
    {
      avx_sop(&XEmitter::VMULSS, &XEmitter::MULSS, XMM1, Ra, Rc);
    }
    else if (fpr.IsRounded(c))
    {
      avx_dop(&XEmitter::VMULSD, &XEmitter::MULSD, XMM1, Ra, Rc);
    }
    else
    {
      MOVAPD(XMM1, Rc);
      Force25BitPrecision(XMM1, R(XMM1), XMM0);
      MULSD(XMM1, Ra);
    }
    RCRepr repr = HandleNaNs(inst, Rd, XMM1, in_repr);
    Rd.SetRepr(repr);
    if (in_repr == RCRepr::Dup)
      ForceSinglePrecision(Rd, Rd);
    SetFPRFIfNeeded(Rd);
  }
  else
  {
    RCRepr in_repr = fpr.IsSingle(a, c) ? RCRepr::PairSingles : RCRepr::Canonical;
    RCOpArg Ra = fpr.Use(a, RCMode::Read, in_repr);
    RCOpArg Rc = fpr.Use(c, RCMode::Read, in_repr);
    RCX64Reg Rd = fpr.Bind(d, RCMode::Write);
    RegCache::Realize(Ra, Rc, Rd);

    if (fpr.IsDupPhysical(c) && in_repr == RCRepr::PairSingles)
    {
      avx_sop(&XEmitter::VMULPS, &XEmitter::MULPS, XMM1, Ra, Rc);
    }
    else if (in_repr == RCRepr::PairSingles)
    {
      if (muls0)
        MOVSLDUP(XMM1, Rc);
      else
        MOVSHDUP(XMM1, Rc);

      MULPS(XMM1, Ra);
    }
    else if (fpr.IsDupPhysical(c) && fpr.IsRounded(c))
    {
      avx_dop(&XEmitter::VMULPD, &XEmitter::MULPD, XMM1, Ra, Rc);
    }
    else
    {
      if (fpr.IsDupPhysical(c))
        MOVAPD(XMM1, Rc);
      else if (muls0)
        MOVDDUP(XMM1, Rc);
      else
        avx_dop(&XEmitter::VSHUFPD, &XEmitter::SHUFPD, XMM1, Rc, Rc, 3);

      if (!fpr.IsRounded(c))
        Force25BitPrecision(XMM1, R(XMM1), XMM0);

      MULPD(XMM1, Ra);
    }

    RCRepr repr = HandleNaNs(inst, Rd, XMM1, in_repr);
    Rd.SetRepr(repr);
    if (in_repr == RCRepr::Canonical)
      ForceSinglePrecision(Rd, Rd);
    SetFPRFIfNeeded(Rd);
  }
}

void Jit64::ps_mergeXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  FALLBACK_IF(inst.Rc);

  int d = inst.FD;
  int a = inst.FA;
  int b = inst.FB;

  RCOpArg Ra = fpr.Use(a, RCMode::Read);
  RCOpArg Rb = fpr.Use(b, RCMode::Read);
  RCX64Reg Rd = fpr.Bind(d, RCMode::Write);
  RegCache::Realize(Ra, Rb, Rd);

  switch (inst.SUBOP10)
  {
  case 528:
    avx_dop(&XEmitter::VUNPCKLPD, &XEmitter::UNPCKLPD, Rd, Ra, Rb);
    break;  // 00
  case 560:
    avx_dop(&XEmitter::VSHUFPD, &XEmitter::SHUFPD, Rd, Ra, Rb, 2);
    break;  // 01
  case 592:
    avx_dop(&XEmitter::VSHUFPD, &XEmitter::SHUFPD, Rd, Ra, Rb, 1);
    break;  // 10
  case 624:
    avx_dop(&XEmitter::VUNPCKHPD, &XEmitter::UNPCKHPD, Rd, Ra, Rb);
    break;  // 11
  default:
    ASSERT_MSG(DYNA_REC, 0, "ps_merge - invalid op");
  }

  Rd.SetRepr(RCRepr::Canonical);
}

void Jit64::ps_rsqrte(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);
  int b = inst.FB;
  int d = inst.FD;

  RCX64Reg scratch_guard = gpr.Scratch(RSCRATCH_EXTRA);
  RCX64Reg Rb = fpr.Bind(b, RCMode::Read);
  RCX64Reg Rd = fpr.Bind(d, RCMode::Write);
  RegCache::Realize(scratch_guard, Rb, Rd);

  MOVSD(XMM0, Rb);
  CALL(asm_routines.frsqrte);
  MOVSD(Rd, XMM0);

  MOVHLPS(XMM0, Rb);
  CALL(asm_routines.frsqrte);
  MOVLHPS(Rd, XMM0);

  Rd.SetRepr(RCRepr::Canonical);
  ForceSinglePrecision(Rd, Rd);
  SetFPRFIfNeeded(Rd);
}

void Jit64::ps_res(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);
  int b = inst.FB;
  int d = inst.FD;

  RCX64Reg scratch_guard = gpr.Scratch(RSCRATCH_EXTRA);
  RCX64Reg Rb = fpr.Bind(b, RCMode::Read);
  RCX64Reg Rd = fpr.Bind(d, RCMode::Write);
  RegCache::Realize(scratch_guard, Rb, Rd);

  MOVSD(XMM0, Rb);
  CALL(asm_routines.fres);
  MOVSD(Rd, XMM0);

  MOVHLPS(XMM0, Rb);
  CALL(asm_routines.fres);
  MOVLHPS(Rd, XMM0);

  Rd.SetRepr(RCRepr::Canonical);
  ForceSinglePrecision(Rd, Rd);
  SetFPRFIfNeeded(Rd);
}

void Jit64::ps_cmpXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);

  FloatCompare(inst, !!(inst.SUBOP10 & 64));
}
