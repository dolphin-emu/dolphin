// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/Jit64/Jit.h"
#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"
#include "Core/PowerPC/Jit64Common/Jit64PowerPCState.h"

using namespace Gen;

// TODO: Add peephole optimizations for multiple consecutive lfd/lfs/stfd/stfs since they are so
// common,
// and pshufb could help a lot.

void Jit64::lfXXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreFloatingOff);
  bool indexed = inst.OPCD == 31;
  bool update = indexed ? !!(inst.SUBOP10 & 0x20) : !!(inst.OPCD & 1);
  bool single = indexed ? !(inst.SUBOP10 & 0x40) : !(inst.OPCD & 2);
  update &= indexed || inst.SIMM_16;

  int d = inst.RD;
  int a = inst.RA;
  int b = inst.RB;

  FALLBACK_IF(!indexed && !a);

  gpr.BindToRegister(a, true, update);

  s32 offset = 0;
  OpArg addr = gpr.R(a);
  if (update && jo.memcheck)
  {
    addr = R(RSCRATCH2);
    MOV(32, addr, gpr.R(a));
  }
  if (indexed)
  {
    if (update)
    {
      ADD(32, addr, gpr.R(b));
    }
    else
    {
      addr = R(RSCRATCH2);
      MOV_sum(32, RSCRATCH2, a ? gpr.R(a) : Imm32(0), gpr.R(b));
    }
  }
  else
  {
    if (update)
      ADD(32, addr, Imm32((s32)(s16)inst.SIMM_16));
    else
      offset = (s16)inst.SIMM_16;
  }

  fpr.Lock(d);
  if (jo.memcheck && single)
  {
    fpr.StoreFromRegister(d);
    js.revertFprLoad = d;
  }
  fpr.BindToRegister(d, !single);
  BitSet32 registersInUse = CallerSavedRegistersInUse();
  if (update && jo.memcheck)
    registersInUse[RSCRATCH2] = true;
  SafeLoadToReg(RSCRATCH, addr, single ? 32 : 64, offset, registersInUse, false);

  if (single)
  {
    ConvertSingleToDouble(fpr.RX(d), RSCRATCH, true);
  }
  else
  {
    MOVQ_xmm(XMM0, R(RSCRATCH));
    MOVSD(fpr.RX(d), R(XMM0));
  }
  if (update && jo.memcheck)
    MOV(32, gpr.R(a), addr);
  fpr.UnlockAll();
  gpr.UnlockAll();
}

void Jit64::stfXXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreFloatingOff);
  bool indexed = inst.OPCD == 31;
  bool update = indexed ? !!(inst.SUBOP10 & 0x20) : !!(inst.OPCD & 1);
  bool single = indexed ? !(inst.SUBOP10 & 0x40) : !(inst.OPCD & 2);
  update &= indexed || inst.SIMM_16;

  int s = inst.RS;
  int a = inst.RA;
  int b = inst.RB;
  s32 imm = (s16)inst.SIMM_16;
  int accessSize = single ? 32 : 64;

  FALLBACK_IF(update && jo.memcheck && a == b);

  if (single)
  {
    if (js.op->fprIsStoreSafe[s])
    {
      CVTSD2SS(XMM0, fpr.R(s));
    }
    else
    {
      fpr.BindToRegister(s, true, false);
      ConvertDoubleToSingle(XMM0, fpr.RX(s));
    }
    MOVD_xmm(R(RSCRATCH), XMM0);
  }
  else
  {
    if (fpr.R(s).IsSimpleReg())
      MOVQ_xmm(R(RSCRATCH), fpr.RX(s));
    else
      MOV(64, R(RSCRATCH), fpr.R(s));
  }

  if (!indexed && (!a || gpr.R(a).IsImm()))
  {
    u32 addr = (a ? gpr.R(a).Imm32() : 0) + imm;
    bool exception =
        WriteToConstAddress(accessSize, R(RSCRATCH), addr, CallerSavedRegistersInUse());

    if (update)
    {
      if (!jo.memcheck || !exception)
      {
        gpr.SetImmediate32(a, addr);
      }
      else
      {
        gpr.KillImmediate(a, true, true);
        MemoryExceptionCheck();
        ADD(32, gpr.R(a), Imm32((u32)imm));
      }
    }
    fpr.UnlockAll();
    gpr.UnlockAll();
    return;
  }

  s32 offset = 0;
  if (update)
    gpr.BindToRegister(a, true, true);
  if (indexed)
  {
    MOV_sum(32, RSCRATCH2, a ? gpr.R(a) : Imm32(0), gpr.R(b));
  }
  else
  {
    if (update)
    {
      LEA(32, RSCRATCH2, MDisp(gpr.RX(a), imm));
    }
    else
    {
      offset = imm;
      MOV(32, R(RSCRATCH2), gpr.R(a));
    }
  }

  BitSet32 registersInUse = CallerSavedRegistersInUse();
  // We need to save the (usually scratch) address register for the update.
  if (update)
    registersInUse[RSCRATCH2] = true;

  SafeWriteRegToReg(RSCRATCH, RSCRATCH2, accessSize, offset, registersInUse);

  if (update)
    MOV(32, gpr.R(a), R(RSCRATCH2));

  fpr.UnlockAll();
  gpr.UnlockAll();
  gpr.UnlockAllX();
}

// This one is a little bit weird; it stores the low 32 bits of a double without converting it
void Jit64::stfiwx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreFloatingOff);

  int s = inst.RS;
  int a = inst.RA;
  int b = inst.RB;

  MOV_sum(32, RSCRATCH2, a ? gpr.R(a) : Imm32(0), gpr.R(b));

  if (fpr.R(s).IsSimpleReg())
    MOVD_xmm(R(RSCRATCH), fpr.RX(s));
  else
    MOV(32, R(RSCRATCH), fpr.R(s));
  SafeWriteRegToReg(RSCRATCH, RSCRATCH2, 32, 0, CallerSavedRegistersInUse());
  gpr.UnlockAllX();
}
