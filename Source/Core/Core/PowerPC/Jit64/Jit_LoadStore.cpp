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
#include "Core/PowerPC/Jit64/JitRegCache.h"
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

  if (!CPU::IsStepping() && inst.OPCD == 32 && CanMergeNextInstructions(2) &&
      (inst.hex & 0xFFFF0000) == 0x800D0000 &&
      (js.op[1].inst.hex == 0x28000000 ||
       (SConfig::GetInstance().bWii && js.op[1].inst.hex == 0x2C000000)) &&
      js.op[2].inst.hex == 0x4182fff8)
  {
    s32 offset = (s32)(s16)inst.SIMM_16;
    gpr.BindToRegister(a, true, false);
    gpr.BindToRegister(d, false, true);
    SafeLoadToReg(gpr.RX(d), gpr.R(a), accessSize, offset, CallerSavedRegistersInUse(), signExtend);

    // if it's still 0, we can wait until the next event
    TEST(32, gpr.R(d), gpr.R(d));
    FixupBranch noIdle = J_CC(CC_NZ);

    BitSet32 registersInUse = CallerSavedRegistersInUse();
    ABI_PushRegistersAndAdjustStack(registersInUse, 0);

    ABI_CallFunction(CoreTiming::Idle);

    ABI_PopRegistersAndAdjustStack(registersInUse, 0);

    // ! we must continue executing of the loop after exception handling, maybe there is still 0 in
    // r0
    // MOV(32, PPCSTATE(pc), Imm32(js.compilerPC));
    WriteExceptionExit();

    SetJumpTarget(noIdle);

    // js.compilerPC += 8;
    return;
  }

  // Determine whether this instruction updates inst.RA
  bool update;
  if (inst.OPCD == 31)
    update = ((inst.SUBOP10 & 0x20) != 0) && (!gpr.R(b).IsImm() || gpr.R(b).Imm32() != 0);
  else
    update = ((inst.OPCD & 1) != 0) && inst.SIMM_16 != 0;

  bool storeAddress = false;
  s32 loadOffset = 0;

  // Prepare address operand
  OpArg opAddress;
  if (!update && !a)
  {
    if (inst.OPCD == 31)
    {
      if (!gpr.R(b).IsImm())
        gpr.BindToRegister(b, true, false);
      opAddress = gpr.R(b);
    }
    else
    {
      opAddress = Imm32((u32)(s32)inst.SIMM_16);
    }
  }
  else if (update && ((a == 0) || (d == a)))
  {
    PanicAlert("Invalid instruction");
  }
  else
  {
    if ((inst.OPCD != 31) && gpr.R(a).IsImm() && !jo.memcheck)
    {
      u32 val = gpr.R(a).Imm32() + inst.SIMM_16;
      opAddress = Imm32(val);
      if (update)
        gpr.SetImmediate32(a, val);
    }
    else if ((inst.OPCD == 31) && gpr.R(a).IsImm() && gpr.R(b).IsImm() && !jo.memcheck)
    {
      u32 val = gpr.R(a).Imm32() + gpr.R(b).Imm32();
      opAddress = Imm32(val);
      if (update)
        gpr.SetImmediate32(a, val);
    }
    else
    {
      // If we're using reg+reg mode and b is an immediate, pretend we're using constant offset mode
      bool use_constant_offset = inst.OPCD != 31 || gpr.R(b).IsImm();

      s32 offset = 0;
      if (use_constant_offset)
        offset = inst.OPCD == 31 ? gpr.R(b).SImm32() : (s32)inst.SIMM_16;
      // Depending on whether we have an immediate and/or update, find the optimum way to calculate
      // the load address.
      if ((update || use_constant_offset) && !jo.memcheck)
      {
        gpr.BindToRegister(a, true, update);
        opAddress = gpr.R(a);
        if (!use_constant_offset)
          ADD(32, opAddress, gpr.R(b));
        else if (update)
          ADD(32, opAddress, Imm32((u32)offset));
        else
          loadOffset = offset;
      }
      else
      {
        // In this case we need an extra temporary register.
        opAddress = R(RSCRATCH2);
        storeAddress = true;
        if (use_constant_offset)
          MOV_sum(32, RSCRATCH2, gpr.R(a), Imm32((u32)offset));
        else
          MOV_sum(32, RSCRATCH2, gpr.R(a), gpr.R(b));
      }
    }
  }

  gpr.Lock(a, b, d);

  if (update && storeAddress)
    gpr.BindToRegister(a, true, true);

  // A bit of an evil hack here. We need to retain the original value of this register for the
  // exception path, but we'd rather not needlessly pass it around if we don't have to, since
  // the exception path is very rare. So we store the value in the regcache, let the load path
  // clobber it, then restore the value in the exception path.
  // TODO: no other load has to do this at the moment, since no other loads go directly to the
  // target registers, but if that ever changes, we need to do it there too.
  if (jo.memcheck)
  {
    gpr.StoreFromRegister(d);
    js.revertGprLoad = d;
  }
  gpr.BindToRegister(d, false, true);

  BitSet32 registersInUse = CallerSavedRegistersInUse();
  // We need to save the (usually scratch) address register for the update.
  if (update && storeAddress)
    registersInUse[RSCRATCH2] = true;

  SafeLoadToReg(gpr.RX(d), opAddress, accessSize, loadOffset, registersInUse, signExtend);

  if (update && storeAddress)
    MOV(32, gpr.R(a), opAddress);

  // TODO: support no-swap in SafeLoadToReg instead
  if (byte_reversed)
    BSWAP(accessSize, gpr.RX(d));

  gpr.UnlockAll();
  gpr.UnlockAllX();
}

void Jit64::dcbx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  X64Reg addr = RSCRATCH;
  X64Reg value = RSCRATCH2;
  X64Reg tmp = gpr.GetFreeXReg();
  gpr.FlushLockX(tmp);

  MOV_sum(32, addr, inst.RA ? gpr.R(inst.RA) : Imm32(0), gpr.R(inst.RB));

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

  gpr.UnlockAllX();
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
  if (SConfig::GetInstance().bDCBZOFF)
    return;
  FALLBACK_IF(SConfig::GetInstance().bLowDCBZHack);

  int a = inst.RA;
  int b = inst.RB;

  MOV(32, R(RSCRATCH), gpr.R(b));
  if (a)
    ADD(32, R(RSCRATCH), gpr.R(a));
  AND(32, R(RSCRATCH), Imm32(~31));

  if (UReg_MSR(MSR).DR)
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

  if (UReg_MSR(MSR).DR)
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
    _assert_msg_(DYNA_REC, 0, "stX: Invalid access size.");
    return;
  }

  // If we already know the address of the write
  if (!a || gpr.R(a).IsImm())
  {
    u32 addr = (a ? gpr.R(a).Imm32() : 0) + offset;
    bool exception = WriteToConstAddress(accessSize, gpr.R(s), addr, CallerSavedRegistersInUse());
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
        ADD(32, gpr.R(a), Imm32((u32)offset));
      }
    }
  }
  else
  {
    gpr.Lock(a, s);
    gpr.BindToRegister(a, true, update);
    if (gpr.R(s).IsImm())
    {
      SafeWriteRegToReg(gpr.R(s), gpr.RX(a), accessSize, offset, CallerSavedRegistersInUse(),
                        SAFE_LOADSTORE_CLOBBER_RSCRATCH_INSTEAD_OF_ADDR);
    }
    else
    {
      X64Reg reg_value;
      if (WriteClobbersRegValue(accessSize, /* swap */ true))
      {
        MOV(32, R(RSCRATCH2), gpr.R(s));
        reg_value = RSCRATCH2;
      }
      else
      {
        gpr.BindToRegister(s, true, false);
        reg_value = gpr.RX(s);
      }
      SafeWriteRegToReg(reg_value, gpr.RX(a), accessSize, offset, CallerSavedRegistersInUse(),
                        SAFE_LOADSTORE_CLOBBER_RSCRATCH_INSTEAD_OF_ADDR);
    }

    if (update)
      ADD(32, gpr.R(a), Imm32((u32)offset));
  }
  gpr.UnlockAll();
}

void Jit64::stXx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  int a = inst.RA, b = inst.RB, s = inst.RS;
  bool update = !!(inst.SUBOP10 & 32);
  bool byte_reverse = !!(inst.SUBOP10 & 512);
  FALLBACK_IF(!a || (update && a == s) || (update && jo.memcheck && a == b));

  gpr.Lock(a, b, s);

  if (update)
    gpr.BindToRegister(a, true, true);

  MOV_sum(32, RSCRATCH2, gpr.R(a), gpr.R(b));

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

  if (gpr.R(s).IsImm())
  {
    BitSet32 registersInUse = CallerSavedRegistersInUse();
    if (update)
      registersInUse[RSCRATCH2] = true;
    SafeWriteRegToReg(gpr.R(s), RSCRATCH2, accessSize, 0, registersInUse,
                      byte_reverse ? SAFE_LOADSTORE_NO_SWAP : 0);
  }
  else
  {
    X64Reg reg_value;
    if (WriteClobbersRegValue(accessSize, /* swap */ !byte_reverse))
    {
      MOV(32, R(RSCRATCH), gpr.R(s));
      reg_value = RSCRATCH;
    }
    else
    {
      gpr.BindToRegister(s, true, false);
      reg_value = gpr.RX(s);
    }
    BitSet32 registersInUse = CallerSavedRegistersInUse();
    if (update)
      registersInUse[RSCRATCH2] = true;
    SafeWriteRegToReg(reg_value, RSCRATCH2, accessSize, 0, registersInUse,
                      byte_reverse ? SAFE_LOADSTORE_NO_SWAP : 0);
  }

  if (update)
    MOV(32, gpr.R(a), R(RSCRATCH2));

  gpr.UnlockAll();
  gpr.UnlockAllX();
}

// A few games use these heavily in video codecs.
void Jit64::lmw(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  // TODO: This doesn't handle rollback on DSI correctly
  MOV(32, R(RSCRATCH2), Imm32((u32)(s32)inst.SIMM_16));
  if (inst.RA)
    ADD(32, R(RSCRATCH2), gpr.R(inst.RA));
  for (int i = inst.RD; i < 32; i++)
  {
    SafeLoadToReg(RSCRATCH, R(RSCRATCH2), 32, (i - inst.RD) * 4,
                  CallerSavedRegistersInUse() | BitSet32{RSCRATCH2}, false);
    gpr.BindToRegister(i, false, true);
    MOV(32, gpr.R(i), R(RSCRATCH));
  }
  gpr.UnlockAllX();
}

void Jit64::stmw(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  // TODO: This doesn't handle rollback on DSI correctly
  for (int i = inst.RD; i < 32; i++)
  {
    if (inst.RA)
      MOV(32, R(RSCRATCH), gpr.R(inst.RA));
    else
      XOR(32, R(RSCRATCH), R(RSCRATCH));
    if (gpr.R(i).IsImm())
    {
      SafeWriteRegToReg(gpr.R(i), RSCRATCH, 32, (i - inst.RD) * 4 + (u32)(s32)inst.SIMM_16,
                        CallerSavedRegistersInUse());
    }
    else
    {
      MOV(32, R(RSCRATCH2), gpr.R(i));
      SafeWriteRegToReg(RSCRATCH2, RSCRATCH, 32, (i - inst.RD) * 4 + (u32)(s32)inst.SIMM_16,
                        CallerSavedRegistersInUse());
    }
  }
  gpr.UnlockAllX();
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
