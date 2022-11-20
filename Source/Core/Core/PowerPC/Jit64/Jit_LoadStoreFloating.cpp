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
  update &= indexed || inst.SIMM_16;

  int d = inst.RD;
  int a = inst.RA;
  int b = inst.RB;
  s32 imm = (s16)inst.SIMM_16;
  int accessSize = single ? 32 : 64;

  FALLBACK_IF(!indexed && !a);

  RCMode Rd_mode = !single ? RCMode::ReadWrite : RCMode::Write;
  RCX64Reg Rd = jo.memcheck && single ? fpr.RevertableBind(d, Rd_mode) : fpr.Bind(d, Rd_mode);
  RegCache::Realize(Rd);

  if (!indexed && (!a || gpr.IsImm(a)))
  {
    u32 addr = (a ? gpr.Imm32(a) : 0) + imm;
    bool exception =
        SafeLoadToRegImmediate(RSCRATCH, addr, accessSize, CallerSavedRegistersInUse(), false);

    if (update)
    {
      if (!jo.memcheck || !exception)
      {
        gpr.SetImmediate32(a, addr);
      }
      else
      {
        RCOpArg Ra = gpr.UseNoImm(a, RCMode::Write);
        RegCache::Realize(Ra);
        MOV(32, Ra, Imm32(addr));
      }
    }
  }
  else
  {
    RCOpArg Ra = update ? gpr.UseNoImm(a, RCMode::ReadWrite) : gpr.Use(a, RCMode::Read);
    RegCache::Realize(Ra);

    if (indexed)
    {
      RCOpArg Rb = gpr.Use(b, RCMode::Read);
      RegCache::Realize(Rb);
      MOV_sum(32, RSCRATCH2, a ? Ra.Location() : Imm32(0), Rb);
    }
    else
    {
      MOV_sum(32, RSCRATCH2, Ra, Imm32(imm));
    }

    BitSet32 registersInUse = CallerSavedRegistersInUse();
    // We need to save the address register for the update.
    if (update)
      registersInUse[RSCRATCH2] = true;

    SafeLoadToReg(RSCRATCH, RSCRATCH2, accessSize, registersInUse, false);

    if (update)
      MOV(32, Ra, R(RSCRATCH2));
  }

  if (single)
  {
    ConvertSingleToDouble(Rd, RSCRATCH, true);
  }
  else
  {
    MOVQ_xmm(XMM0, R(RSCRATCH));
    MOVSD(Rd, R(XMM0));
  }
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
        RCOpArg Ra = gpr.UseNoImm(a, RCMode::Write);
        RegCache::Realize(Ra);
        MemoryExceptionCheck();
        MOV(32, Ra, Imm32(addr));
      }
    }
    return;
  }

  RCOpArg Ra = update ? gpr.UseNoImm(a, RCMode::ReadWrite) : gpr.Use(a, RCMode::Read);
  RegCache::Realize(Ra);
  if (indexed)
  {
    RCOpArg Rb = gpr.Use(b, RCMode::Read);
    RegCache::Realize(Rb);
    MOV_sum(32, RSCRATCH2, a ? Ra.Location() : Imm32(0), Rb);
  }
  else
  {
    MOV_sum(32, RSCRATCH2, Ra, Imm32(imm));
  }

  BitSet32 registersInUse = CallerSavedRegistersInUse();
  // We need to save the address register for the update.
  if (update)
    registersInUse[RSCRATCH2] = true;

  SafeWriteRegToReg(RSCRATCH, RSCRATCH2, accessSize, CallerSavedRegistersInUse());

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
  SafeWriteRegToReg(RSCRATCH, RSCRATCH2, 32, CallerSavedRegistersInUse());
}
