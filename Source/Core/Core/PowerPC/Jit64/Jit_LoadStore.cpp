// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// TODO(ector): Tons of pshufb optimization of the loads/stores, for SSSE3+, possibly SSE4, only.
// Should give a very noticable speed boost to paired single heavy code.

#include "Core/PowerPC/Jit64/Jit.h"

#include "Common/Assert.h"
#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Common/x64ABI.h"
#include "Common/x64Emitter.h"

#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/HW/CPU.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/Jit64/RegCache/JitRegCache.h"
#include "Core/PowerPC/Jit64Common/Jit64PowerPCState.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"

using namespace Gen;

void Jit64::lXXx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  int a = inst.RA, b = inst.RB, d = inst.RD;

  // Skip disabled JIT instructions
  FALLBACK_IF(SConfig::GetInstance().bJITLoadStorelbzxOff && (inst.OPCD == 31) &&
              (inst.SUBOP10 == 87));
  FALLBACK_IF(SConfig::GetInstance().bJITLoadStorelXzOff &&
              ((inst.OPCD == 34) || (inst.OPCD == 40) || (inst.OPCD == 32)));
  FALLBACK_IF(SConfig::GetInstance().bJITLoadStorelwzOff && (inst.OPCD == 32));

  // Determine memory access size and sign extend
  int accessSize = 0;
  bool signExtend = false;
  bool byte_reversed = false;
  switch (inst.OPCD)
  {
  case 32:  // lwz
  case 33:  // lwzu
    accessSize = 32;
    signExtend = false;
    break;

  case 34:  // lbz
  case 35:  // lbzu
    accessSize = 8;
    signExtend = false;
    break;

  case 40:  // lhz
  case 41:  // lhzu
    accessSize = 16;
    signExtend = false;
    break;

  case 42:  // lha
  case 43:  // lhau
    accessSize = 16;
    signExtend = true;
    break;

  case 31:
    switch (inst.SUBOP10)
    {
    case 534:  // lwbrx
      byte_reversed = true;
    case 23:  // lwzx
    case 55:  // lwzux
      accessSize = 32;
      signExtend = false;
      break;

    case 87:   // lbzx
    case 119:  // lbzux
      accessSize = 8;
      signExtend = false;
      break;
    case 790:  // lhbrx
      byte_reversed = true;
    case 279:  // lhzx
    case 311:  // lhzux
      accessSize = 16;
      signExtend = false;
      break;

    case 343:  // lhax
    case 375:  // lhaux
      accessSize = 16;
      signExtend = true;
      break;

    default:
      PanicAlert("Invalid instruction");
    }
    break;

  default:
    PanicAlert("Invalid instruction");
  }

  // PowerPC has no 8-bit sign extended load, but x86 does, so merge extsb with the load if we find
  // it.
  if (CanMergeNextInstructions(1) && accessSize == 8 && js.op[1].inst.OPCD == 31 &&
      js.op[1].inst.SUBOP10 == 954 && js.op[1].inst.RS == inst.RD && js.op[1].inst.RA == inst.RD &&
      !js.op[1].inst.Rc)
  {
    js.downcountAmount++;
    js.skipInstructions = 1;
    signExtend = true;
  }

  // Determine whether this instruction updates inst.RA
  bool update;
  if (inst.OPCD == 31)
    update = ((inst.SUBOP10 & 0x20) != 0) && (!gpr.IsImm(b) || gpr.Imm32(b) != 0);
  else
    update = ((inst.OPCD & 1) != 0) && inst.SIMM_16 != 0;

  // Determine whether this instruction indexes with inst.RB
  const bool indexed = inst.OPCD == 31;

  bool storeAddress = false;
  s32 loadOffset = 0;

  // Prepare result
  RCX64Reg Rd = jo.memcheck ? gpr.RevertableBind(d, RCMode::Write) : gpr.Bind(d, RCMode::Write);

  // Prepare address operand
  RCOpArg opAddress;
  if (!update && !a)
  {
    if (indexed)
    {
      opAddress = gpr.BindOrImm(b, RCMode::Read);
    }
    else
    {
      opAddress = RCOpArg::Imm32((u32)(s32)inst.SIMM_16);
    }
  }
  else if (update && ((a == 0) || (d == a)))
  {
    PanicAlert("Invalid instruction");
  }
  else
  {
    if (!indexed && gpr.IsImm(a) && !jo.memcheck)
    {
      u32 val = gpr.Imm32(a) + inst.SIMM_16;
      opAddress = RCOpArg::Imm32(val);
      if (update)
        gpr.SetImmediate32(a, val);
    }
    else if (indexed && gpr.IsImm(a) && gpr.IsImm(b) && !jo.memcheck)
    {
      u32 val = gpr.Imm32(a) + gpr.Imm32(b);
      opAddress = RCOpArg::Imm32(val);
      if (update)
        gpr.SetImmediate32(a, val);
    }
    else
    {
      // If we're using reg+reg mode and b is an immediate, pretend we're using constant offset mode
      const bool use_constant_offset = !indexed || gpr.IsImm(b);

      s32 offset = 0;
      if (use_constant_offset)
        offset = indexed ? gpr.SImm32(b) : (s32)inst.SIMM_16;

      RCOpArg Rb = use_constant_offset ? RCOpArg{} : gpr.Use(b, RCMode::Read);

      // Depending on whether we have an immediate and/or update, find the optimum way to calculate
      // the load address.
      if ((update || use_constant_offset) && !jo.memcheck)
      {
        opAddress = gpr.Bind(a, update ? RCMode::ReadWrite : RCMode::Read);
        RegCache::Realize(opAddress, Rb);

        if (!use_constant_offset)
          ADD(32, opAddress, Rb);
        else if (update)
          ADD(32, opAddress, Imm32((u32)offset));
        else
          loadOffset = offset;
      }
      else
      {
        storeAddress = true;
        // In this case we need an extra temporary register.
        opAddress = RCOpArg::R(RSCRATCH2);
        RCOpArg Ra = gpr.Use(a, RCMode::Read);
        RegCache::Realize(opAddress, Ra, Rb);

        if (use_constant_offset)
          MOV_sum(32, RSCRATCH2, Ra, Imm32((u32)offset));
        else
          MOV_sum(32, RSCRATCH2, Ra, Rb);
      }
    }
  }

  RCX64Reg Ra = (update && storeAddress) ? gpr.Bind(a, RCMode::ReadWrite) : RCX64Reg{};
  RegCache::Realize(opAddress, Ra, Rd);

  BitSet32 registersInUse = CallerSavedRegistersInUse();
  // We need to save the (usually scratch) address register for the update.
  if (update && storeAddress)
    registersInUse[RSCRATCH2] = true;

  SafeLoadToReg(Rd, opAddress, accessSize, loadOffset, registersInUse, signExtend);

  if (update && storeAddress)
    MOV(32, Ra, opAddress);

  // TODO: support no-swap in SafeLoadToReg instead
  if (byte_reversed)
    BSWAP(accessSize, Rd);
}

void Jit64::dcbx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  X64Reg addr = RSCRATCH;
  X64Reg value = RSCRATCH2;
  RCOpArg Ra = inst.RA ? gpr.Use(inst.RA, RCMode::Read) : RCOpArg::Imm32(0);
  RCOpArg Rb = gpr.Use(inst.RB, RCMode::Read);
  RCX64Reg tmp = gpr.Scratch();
  RegCache::Realize(Ra, Rb, tmp);

  MOV_sum(32, addr, Ra, Rb);

  // Check whether a JIT cache line needs to be invalidated.
  LEA(32, value, MScaled(addr, SCALE_8, 0));  // addr << 3 (masks the first 3 bits)
  SHR(32, R(value), Imm8(3 + 5 + 5));         // >> 5 for cache line size, >> 5 for width of bitset
  MOV(64, R(tmp), ImmPtr(GetBlockCache()->GetBlockBitSet()));
  MOV(32, R(value), MComplex(tmp, value, SCALE_4, 0));
  SHR(32, R(addr), Imm8(5));
  BT(32, R(value), R(addr));

  FixupBranch c = J_CC(CC_C, true);
  SwitchToFarCode();
  SetJumpTarget(c);
  BitSet32 registersInUse = CallerSavedRegistersInUse();
  ABI_PushRegistersAndAdjustStack(registersInUse, 0);
  MOV(32, R(ABI_PARAM1), R(addr));
  SHL(32, R(ABI_PARAM1), Imm8(5));
  MOV(32, R(ABI_PARAM2), Imm32(32));
  XOR(32, R(ABI_PARAM3), R(ABI_PARAM3));
  ABI_CallFunction(JitInterface::InvalidateICache);
  ABI_PopRegistersAndAdjustStack(registersInUse, 0);
  asm_routines.ResetStack(*this);
  c = J(true);
  SwitchToNearCode();
  SetJumpTarget(c);
}

void Jit64::dcbt(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  // Prefetch. Since we don't emulate the data cache, we don't need to do anything.

  // If a dcbst follows a dcbt, it probably isn't a case of dynamic code
  // modification, so don't bother invalidating the jit block cache.
  // This is important because invalidating the block cache when we don't
  // need to is terrible for performance.
  // (Invalidating the jit block cache on dcbst is a heuristic.)
  if (CanMergeNextInstructions(1) && js.op[1].inst.OPCD == 31 && js.op[1].inst.SUBOP10 == 54 &&
      js.op[1].inst.RA == inst.RA && js.op[1].inst.RB == inst.RB)
  {
    js.skipInstructions = 1;
  }
}

// Zero cache line.
void Jit64::dcbz(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);
  FALLBACK_IF(SConfig::GetInstance().bLowDCBZHack);

  int a = inst.RA;
  int b = inst.RB;

  {
    RCOpArg Ra = a ? gpr.Use(a, RCMode::Read) : RCOpArg::Imm32(0);
    RCOpArg Rb = gpr.Use(b, RCMode::Read);
    RegCache::Realize(Ra, Rb);

    MOV_sum(32, RSCRATCH, Ra, Rb);
    AND(32, R(RSCRATCH), Imm32(~31));
  }

  if (MSR.DR)
  {
    // Perform lookup to see if we can use fast path.
    MOV(64, R(RSCRATCH2), ImmPtr(&PowerPC::dbat_table[0]));
    PUSH(RSCRATCH);
    SHR(32, R(RSCRATCH), Imm8(PowerPC::BAT_INDEX_SHIFT));
    TEST(32, MComplex(RSCRATCH2, RSCRATCH, SCALE_4, 0), Imm32(PowerPC::BAT_PHYSICAL_BIT));
    POP(RSCRATCH);
    FixupBranch slow = J_CC(CC_Z, true);

    // Fast path: compute full address, then zero out 32 bytes of memory.
    XORPS(XMM0, R(XMM0));
    MOVAPS(MComplex(RMEM, RSCRATCH, SCALE_1, 0), XMM0);
    MOVAPS(MComplex(RMEM, RSCRATCH, SCALE_1, 16), XMM0);

    // Slow path: call the general-case code.
    SwitchToFarCode();
    SetJumpTarget(slow);
  }
  MOV(32, PPCSTATE(pc), Imm32(js.compilerPC));
  BitSet32 registersInUse = CallerSavedRegistersInUse();
  ABI_PushRegistersAndAdjustStack(registersInUse, 0);
  ABI_CallFunctionR(PowerPC::ClearCacheLine, RSCRATCH);
  ABI_PopRegistersAndAdjustStack(registersInUse, 0);

  if (MSR.DR)
  {
    FixupBranch end = J(true);
    SwitchToNearCode();
    SetJumpTarget(end);
  }
}

void Jit64::stX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  int s = inst.RS;
  int a = inst.RA;
  s32 offset = (s32)(s16)inst.SIMM_16;
  bool update = (inst.OPCD & 1) && offset;

  if (!a && update)
    PanicAlert("Invalid stX");

  int accessSize;
  switch (inst.OPCD & ~1)
  {
  case 36:  // stw
    accessSize = 32;
    break;
  case 44:  // sth
    accessSize = 16;
    break;
  case 38:  // stb
    accessSize = 8;
    break;
  default:
    ASSERT_MSG(DYNA_REC, 0, "stX: Invalid access size.");
    return;
  }

  // If we already know the address of the write
  if (!a || gpr.IsImm(a))
  {
    const u32 addr = (a ? gpr.Imm32(a) : 0) + offset;
    const bool exception = [&] {
      RCOpArg Rs = gpr.Use(s, RCMode::Read);
      RegCache::Realize(Rs);
      return WriteToConstAddress(accessSize, Rs, addr, CallerSavedRegistersInUse());
    }();
    if (update)
    {
      if (!jo.memcheck || !exception)
      {
        gpr.SetImmediate32(a, addr);
      }
      else
      {
        RCOpArg Ra = gpr.UseNoImm(a, RCMode::ReadWrite);
        RegCache::Realize(Ra);
        MemoryExceptionCheck();
        ADD(32, Ra, Imm32((u32)offset));
      }
    }
  }
  else
  {
    RCX64Reg Ra = gpr.Bind(a, update ? RCMode::ReadWrite : RCMode::Read);
    RCOpArg reg_value;
    if (!gpr.IsImm(s) && WriteClobbersRegValue(accessSize, /* swap */ true))
    {
      RCOpArg Rs = gpr.Use(s, RCMode::Read);
      RegCache::Realize(Rs);
      reg_value = RCOpArg::R(RSCRATCH2);
      MOV(32, reg_value, Rs);
    }
    else
    {
      reg_value = gpr.BindOrImm(s, RCMode::Read);
    }
    RegCache::Realize(Ra, reg_value);
    SafeWriteRegToReg(reg_value, Ra, accessSize, offset, CallerSavedRegistersInUse(),
                      SAFE_LOADSTORE_CLOBBER_RSCRATCH_INSTEAD_OF_ADDR);

    if (update)
      ADD(32, Ra, Imm32((u32)offset));
  }
}

void Jit64::stXx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  int a = inst.RA, b = inst.RB, s = inst.RS;
  bool update = !!(inst.SUBOP10 & 32);
  bool byte_reverse = !!(inst.SUBOP10 & 512);
  FALLBACK_IF(!a || (update && a == s) || (update && jo.memcheck && a == b));

  int accessSize;
  switch (inst.SUBOP10 & ~32)
  {
  case 151:
  case 662:
    accessSize = 32;
    break;
  case 407:
  case 918:
    accessSize = 16;
    break;
  case 215:
    accessSize = 8;
    break;
  default:
    PanicAlert("stXx: invalid access size");
    accessSize = 0;
    break;
  }

  const bool does_clobber = WriteClobbersRegValue(accessSize, /* swap */ !byte_reverse);

  RCOpArg Ra = update ? gpr.Bind(a, RCMode::ReadWrite) : gpr.Use(a, RCMode::Read);
  RCOpArg Rb = gpr.Use(b, RCMode::Read);
  RCOpArg Rs = does_clobber ? gpr.Use(s, RCMode::Read) : gpr.BindOrImm(s, RCMode::Read);
  RegCache::Realize(Ra, Rb, Rs);

  MOV_sum(32, RSCRATCH2, Ra, Rb);

  if (!Rs.IsImm() && does_clobber)
  {
    MOV(32, R(RSCRATCH), Rs);
    Rs = RCOpArg::R(RSCRATCH);
  }
  BitSet32 registersInUse = CallerSavedRegistersInUse();
  if (update)
    registersInUse[RSCRATCH2] = true;
  SafeWriteRegToReg(Rs, RSCRATCH2, accessSize, 0, registersInUse,
                    byte_reverse ? SAFE_LOADSTORE_NO_SWAP : 0);

  if (update)
    MOV(32, Ra, R(RSCRATCH2));
}

// A few games use these heavily in video codecs.
void Jit64::lmw(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  int a = inst.RA, d = inst.RD;

  // TODO: This doesn't handle rollback on DSI correctly
  {
    RCOpArg Ra = a ? gpr.Use(a, RCMode::Read) : RCOpArg::Imm32(0);
    RegCache::Realize(Ra);
    MOV_sum(32, RSCRATCH2, Ra, Imm32((u32)(s32)inst.SIMM_16));
  }
  for (int i = d; i < 32; i++)
  {
    SafeLoadToReg(RSCRATCH, R(RSCRATCH2), 32, (i - d) * 4,
                  CallerSavedRegistersInUse() | BitSet32{RSCRATCH2}, false);
    RCOpArg Ri = gpr.Bind(i, RCMode::Write);
    RegCache::Realize(Ri);
    MOV(32, Ri, R(RSCRATCH));
  }
}

void Jit64::stmw(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  int a = inst.RA, d = inst.RD;

  // TODO: This doesn't handle rollback on DSI correctly
  for (int i = d; i < 32; i++)
  {
    RCOpArg Ra = a ? gpr.Use(a, RCMode::Read) : RCOpArg::Imm32(0);
    RCOpArg Ri = gpr.Use(i, RCMode::Read);
    RegCache::Realize(Ra, Ri);

    if (Ra.IsZero())
      XOR(32, R(RSCRATCH), R(RSCRATCH));
    else
      MOV(32, R(RSCRATCH), Ra);
    if (!Ri.IsImm())
    {
      MOV(32, R(RSCRATCH2), Ri);
      Ri = RCOpArg::R(RSCRATCH2);
    }
    SafeWriteRegToReg(Ri, RSCRATCH, 32, (i - d) * 4 + (u32)(s32)inst.SIMM_16,
                      CallerSavedRegistersInUse());
  }
}

void Jit64::eieio(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  // optimizeGatherPipe generally postpones FIFO checks to the end of the JIT block,
  // which is generally safe. However postponing FIFO writes across eieio instructions
  // is incorrect (would crash NBA2K11 strap screen if we improve our FIFO detection).
  if (jo.optimizeGatherPipe && js.fifoBytesSinceCheck > 0)
    js.mustCheckFifo = true;
}
