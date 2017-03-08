// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/Jit64/Jit.h"
#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Common/x64Emitter.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"

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

  fpr.BindToRegister(d, false);
  MOVAPD(fpr.RX(d), fpr.R(b));
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
  fpr.Lock(a, b, c, d);
  OpArg op_a = fpr.R(a);
  fpr.BindToRegister(d, d == b || d == c);
  X64Reg tmp = XMM1;
  MOVDDUP(tmp, op_a);    // {a.ps0, a.ps0}
  ADDPD(tmp, fpr.R(b));  // {a.ps0 + b.ps0, a.ps0 + b.ps1}
  switch (inst.SUBOP5)
  {
  case 10:  // ps_sum0: {a.ps0 + b.ps1, c.ps1}
    UNPCKHPD(tmp, fpr.R(c));
    break;
  case 11:  // ps_sum1: {c.ps0, a.ps0 + b.ps1}
    if (fpr.R(c).IsSimpleReg())
    {
      if (cpu_info.bSSE4_1)
      {
        BLENDPD(tmp, fpr.R(c), 1);
      }
      else
      {
        MOVAPD(XMM0, fpr.R(c));
        SHUFPD(XMM0, R(tmp), 2);
        tmp = XMM0;
      }
    }
    else
    {
      MOVLPD(tmp, fpr.R(c));
    }
    break;
  default:
    PanicAlert("ps_sum WTF!!!");
  }
  HandleNaNs(inst, fpr.RX(d), tmp, tmp == XMM1 ? XMM0 : XMM1);
  ForceSinglePrecision(fpr.RX(d), fpr.R(d));
  SetFPRFIfNeeded(fpr.RX(d));
  fpr.UnlockAll();
}

void Jit64::ps_muls(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  FALLBACK_IF(inst.Rc);

  int d = inst.FD;
  int a = inst.FA;
  int c = inst.FC;
  bool round_input = !js.op->fprIsSingle[c];
  fpr.Lock(a, c, d);
  switch (inst.SUBOP5)
  {
  case 12:  // ps_muls0
    MOVDDUP(XMM1, fpr.R(c));
    break;
  case 13:  // ps_muls1
    avx_op(&XEmitter::VSHUFPD, &XEmitter::SHUFPD, XMM1, fpr.R(c), fpr.R(c), 3);
    break;
  default:
    PanicAlert("ps_muls WTF!!!");
  }
  if (round_input)
    Force25BitPrecision(XMM1, R(XMM1), XMM0);
  MULPD(XMM1, fpr.R(a));
  fpr.BindToRegister(d, false);
  HandleNaNs(inst, fpr.RX(d), XMM1);
  ForceSinglePrecision(fpr.RX(d), fpr.R(d));
  SetFPRFIfNeeded(fpr.RX(d));
  fpr.UnlockAll();
}

void Jit64::ps_mergeXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITPairedOff);
  FALLBACK_IF(inst.Rc);

  int d = inst.FD;
  int a = inst.FA;
  int b = inst.FB;
  fpr.Lock(a, b, d);
  fpr.BindToRegister(d, d == a || d == b);

  switch (inst.SUBOP10)
  {
  case 528:
    avx_op(&XEmitter::VUNPCKLPD, &XEmitter::UNPCKLPD, fpr.RX(d), fpr.R(a), fpr.R(b));
    break;  // 00
  case 560:
    avx_op(&XEmitter::VSHUFPD, &XEmitter::SHUFPD, fpr.RX(d), fpr.R(a), fpr.R(b), 2);
    break;  // 01
  case 592:
    avx_op(&XEmitter::VSHUFPD, &XEmitter::SHUFPD, fpr.RX(d), fpr.R(a), fpr.R(b), 1);
    break;  // 10
  case 624:
    avx_op(&XEmitter::VUNPCKHPD, &XEmitter::UNPCKHPD, fpr.RX(d), fpr.R(a), fpr.R(b));
    break;  // 11
  default:
    _assert_msg_(DYNA_REC, 0, "ps_merge - invalid op");
  }
  fpr.UnlockAll();
}

void Jit64::ps_rsqrte(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);
  int b = inst.FB;
  int d = inst.FD;

  gpr.FlushLockX(RSCRATCH_EXTRA);
  fpr.Lock(b, d);
  fpr.BindToRegister(b, true, false);
  fpr.BindToRegister(d, false);

  MOVSD(XMM0, fpr.R(b));
  CALL(asm_routines.frsqrte);
  MOVSD(fpr.R(d), XMM0);

  MOVHLPS(XMM0, fpr.RX(b));
  CALL(asm_routines.frsqrte);
  MOVLHPS(fpr.RX(d), XMM0);

  ForceSinglePrecision(fpr.RX(d), fpr.R(d));
  SetFPRFIfNeeded(fpr.RX(d));
  fpr.UnlockAll();
  gpr.UnlockAllX();
}

void Jit64::ps_res(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);
  int b = inst.FB;
  int d = inst.FD;

  gpr.FlushLockX(RSCRATCH_EXTRA);
  fpr.Lock(b, d);
  fpr.BindToRegister(b, true, false);
  fpr.BindToRegister(d, false);

  MOVSD(XMM0, fpr.R(b));
  CALL(asm_routines.fres);
  MOVSD(fpr.R(d), XMM0);

  MOVHLPS(XMM0, fpr.RX(b));
  CALL(asm_routines.fres);
  MOVLHPS(fpr.RX(d), XMM0);

  ForceSinglePrecision(fpr.RX(d), fpr.R(d));
  SetFPRFIfNeeded(fpr.RX(d));
  fpr.UnlockAll();
  gpr.UnlockAllX();
}

void Jit64::ps_cmpXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);

  FloatCompare(inst, !!(inst.SUBOP10 & 64));
}
