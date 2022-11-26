// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Jit64/Jit.h"

#include <optional>

#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Common/x64Emitter.h"
#include "Core/PowerPC/Jit64/RegCache/JitRegCache.h"
#include "Core/PowerPC/Jit64Common/Jit64Constants.h"

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

  RCOpArg Rb = fpr.Use(b, RCMode::Read);
  RCX64Reg Rd = fpr.Bind(d, RCMode::Write);
  RegCache::Realize(Rb, Rd);
  MOVAPD(Rd, Rb);
}

void Jit64::ps_sum(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  FALLBACK_IF(inst.Rc);
  FALLBACK_IF(jo.fp_exceptions);

  int d = inst.FD;
  int a = inst.FA;
  int b = inst.FB;
  int c = inst.FC;

  RCOpArg Ra = fpr.Use(a, RCMode::Read);
  RCOpArg Rb = fpr.Use(b, RCMode::Read);
  RCOpArg Rc = fpr.Use(c, RCMode::Read);
  RCX64Reg Rd = fpr.Bind(d, RCMode::Write);
  RegCache::Realize(Ra, Rb, Rc, Rd);

  X64Reg tmp = XMM1;
  MOVDDUP(tmp, Ra);  // {a.ps0, a.ps0}
  ADDPD(tmp, Rb);    // {a.ps0 + b.ps0, a.ps0 + b.ps1}
  switch (inst.SUBOP5)
  {
  case 10:  // ps_sum0: {a.ps0 + b.ps1, c.ps1}
    UNPCKHPD(tmp, Rc);
    break;
  case 11:  // ps_sum1: {c.ps0, a.ps0 + b.ps1}
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
    break;
  default:
    PanicAlertFmt("ps_sum WTF!!!");
  }
  // We're intentionally not calling HandleNaNs here.
  // For addition and subtraction specifically, x86's NaN behavior matches PPC's.
  FinalizeSingleResult(Rd, R(tmp));
}

void Jit64::ps_muls(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  FALLBACK_IF(inst.Rc);
  FALLBACK_IF(jo.fp_exceptions);

  int d = inst.FD;
  int a = inst.FA;
  int c = inst.FC;
  bool round_input = !js.op->fprIsSingle[c];

  RCOpArg Ra = fpr.Use(a, RCMode::Read);
  RCOpArg Rc = fpr.Use(c, RCMode::Read);
  RCX64Reg Rd = fpr.Bind(d, RCMode::Write);
  RCX64Reg Rc_duplicated = m_accurate_nans ? fpr.Scratch() : fpr.Scratch(XMM1);
  RegCache::Realize(Ra, Rc, Rd, Rc_duplicated);

  switch (inst.SUBOP5)
  {
  case 12:  // ps_muls0
    MOVDDUP(Rc_duplicated, Rc);
    break;
  case 13:  // ps_muls1
    avx_op(&XEmitter::VSHUFPD, &XEmitter::SHUFPD, Rc_duplicated, Rc, Rc, 3);
    break;
  default:
    PanicAlertFmt("ps_muls WTF!!!");
  }
  if (round_input)
    Force25BitPrecision(XMM1, R(Rc_duplicated), XMM0);
  else if (XMM1 != Rc_duplicated)
    MOVAPD(XMM1, Rc_duplicated);
  MULPD(XMM1, Ra);
  HandleNaNs(inst, XMM1, XMM0, Ra, std::nullopt, Rc_duplicated);
  FinalizeSingleResult(Rd, R(XMM1));
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
    avx_op(&XEmitter::VUNPCKLPD, &XEmitter::UNPCKLPD, Rd, Ra, Rb);
    break;  // 00
  case 560:
    avx_op(&XEmitter::VSHUFPD, &XEmitter::SHUFPD, Rd, Ra, Rb, 2);
    break;  // 01
  case 592:
    avx_op(&XEmitter::VSHUFPD, &XEmitter::SHUFPD, Rd, Ra, Rb, 1);
    break;  // 10
  case 624:
    avx_op(&XEmitter::VUNPCKHPD, &XEmitter::UNPCKHPD, Rd, Ra, Rb);
    break;  // 11
  default:
    ASSERT_MSG(DYNA_REC, 0, "ps_merge - invalid op");
  }
}

void Jit64::ps_rsqrte(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);
  FALLBACK_IF(jo.fp_exceptions || jo.div_by_zero_exceptions);
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

  FinalizeSingleResult(Rd, Rd);
}

void Jit64::ps_res(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);
  FALLBACK_IF(jo.fp_exceptions || jo.div_by_zero_exceptions);
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

  FinalizeSingleResult(Rd, Rd);
}

void Jit64::ps_cmpXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(jo.fp_exceptions);

  FloatCompare(inst, !!(inst.SUBOP10 & 64));
}
