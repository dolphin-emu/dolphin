// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Jit64/Jit.h"

#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"
#include "Core/PowerPC/Jit64/RegCache/JitRegCache.h"
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
  update &= indexed || (inst.SIMM_16 != 0);

  int d = inst.RD;
  int a = inst.RA;
  int b = inst.RB;

  FALLBACK_IF(!indexed && !a);

  s32 offset = 0;
  RCOpArg addr = gpr.Bind(a, update ? RCMode::ReadWrite : RCMode::Read);
  RegCache::Realize(addr);

  if (update && jo.memcheck)
  {
    MOV(32, R(RSCRATCH2), addr);
    addr = RCOpArg::R(RSCRATCH2);
  }
  if (indexed)
  {
    RCOpArg Rb = gpr.Use(b, RCMode::Read);
    RegCache::Realize(Rb);
    if (update)
    {
      ADD(32, addr, Rb);
    }
    else
    {
      MOV_sum(32, RSCRATCH2, a ? addr.Location() : Imm32(0), Rb);
      addr = RCOpArg::R(RSCRATCH2);
    }
  }
  else
  {
    if (update)
      ADD(32, addr, Imm32((s32)(s16)inst.SIMM_16));
    else
      offset = (s16)inst.SIMM_16;
  }

  RCMode Rd_mode = !single ? RCMode::ReadWrite : RCMode::Write;
  RCX64Reg Rd = jo.memcheck && single ? fpr.RevertableBind(d, Rd_mode) : fpr.Bind(d, Rd_mode);
  RegCache::Realize(Rd);
  BitSet32 registersInUse = CallerSavedRegistersInUse();
  if (update && jo.memcheck)
    registersInUse[RSCRATCH2] = true;
  SafeLoadToReg(RSCRATCH, addr, single ? 32 : 64, offset, registersInUse, false);

  if (single)
  {
    ConvertSingleToDouble(Rd, RSCRATCH, true);
  }
  else
  {
    MOVQ_xmm(XMM0, R(RSCRATCH));
    MOVSD(Rd, R(XMM0));
  }
  if (update && jo.memcheck)
  {
    RCX64Reg Ra = gpr.Bind(a, RCMode::Write);
    RegCache::Realize(Ra);
    MOV(32, Ra, addr);
  }
}

void Jit64::stfXXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreFloatingOff);
  bool indexed = inst.OPCD == 31;
  bool update = indexed ? !!(inst.SUBOP10 & 0x20) : !!(inst.OPCD & 1);
  bool single = indexed ? !(inst.SUBOP10 & 0x40) : !(inst.OPCD & 2);
  update &= indexed || (inst.SIMM_16 != 0);

  int s = inst.RS;
  int a = inst.RA;
  int b = inst.RB;
  s32 imm = (s16)inst.SIMM_16;
  int accessSize = single ? 32 : 64;

  FALLBACK_IF(update && jo.memcheck && indexed && a == b);

  if (single)
  {
    if (js.fpr_is_store_safe[s] && js.op->fprIsSingle[s])
    {
      RCOpArg Rs = fpr.Use(s, RCMode::Read);
      RegCache::Realize(Rs);
      CVTSD2SS(XMM0, Rs);
      MOVD_xmm(R(RSCRATCH), XMM0);
    }
    else
    {
      RCX64Reg Rs = fpr.Bind(s, RCMode::Read);
      RegCache::Realize(Rs);
      MOVAPD(XMM0, Rs);
      CALL(asm_routines.cdts);
    }
  }
  else
  {
    RCOpArg Rs = fpr.Use(s, RCMode::Read);
    RegCache::Realize(Rs);
    if (Rs.IsSimpleReg())
      MOVQ_xmm(R(RSCRATCH), Rs.GetSimpleReg());
    else
      MOV(64, R(RSCRATCH), Rs);
  }

  if (!indexed && (!a || gpr.IsImm(a)))
  {
    u32 addr = (a ? gpr.Imm32(a) : 0) + imm;
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
        RCOpArg Ra = gpr.RevertableBind(a, RCMode::Write);
        RegCache::Realize(Ra);
        MemoryExceptionCheck();
        MOV(32, Ra, Imm32(addr));
      }
    }
    return;
  }

  s32 offset = 0;
  RCOpArg Ra = update ? gpr.Bind(a, RCMode::ReadWrite) : gpr.Use(a, RCMode::Read);
  RegCache::Realize(Ra);
  if (indexed)
  {
    RCOpArg Rb = gpr.Use(b, RCMode::Read);
    RegCache::Realize(Rb);
    MOV_sum(32, RSCRATCH2, a ? Ra.Location() : Imm32(0), Rb);
  }
  else
  {
    if (update)
    {
      MOV_sum(32, RSCRATCH2, Ra, Imm32(imm));
    }
    else
    {
      offset = imm;
      MOV(32, R(RSCRATCH2), Ra);
    }
  }

  BitSet32 registersInUse = CallerSavedRegistersInUse();
  // We need to save the (usually scratch) address register for the update.
  if (update)
    registersInUse[RSCRATCH2] = true;

  SafeWriteRegToReg(RSCRATCH, RSCRATCH2, accessSize, offset, registersInUse);

  if (update)
    MOV(32, Ra, R(RSCRATCH2));
}

// This one is a little bit weird; it stores the low 32 bits of a double without converting it
void Jit64::stfiwx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreFloatingOff);

  int s = inst.RS;
  int a = inst.RA;
  int b = inst.RB;

  RCOpArg Ra = a ? gpr.Use(a, RCMode::Read) : RCOpArg::Imm32(0);
  RCOpArg Rb = gpr.Use(b, RCMode::Read);
  RCOpArg Rs = fpr.Use(s, RCMode::Read);
  RegCache::Realize(Ra, Rb, Rs);

  MOV_sum(32, RSCRATCH2, Ra, Rb);

  if (Rs.IsSimpleReg())
    MOVD_xmm(R(RSCRATCH), Rs.GetSimpleReg());
  else
    MOV(32, R(RSCRATCH), Rs);
  SafeWriteRegToReg(RSCRATCH, RSCRATCH2, 32, 0, CallerSavedRegistersInUse());
}
