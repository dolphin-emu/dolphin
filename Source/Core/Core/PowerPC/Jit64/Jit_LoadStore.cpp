// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// TODO(ector): Tons of pshufb optimization of the loads/stores, for SSSE3+, possibly SSE4, only.
// Should give a very noticable speed boost to paired single heavy code.

#include "Core/PowerPC/Jit64/Jit.h"
#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Common/x64ABI.h"
#include "Common/x64Emitter.h"
#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/HW/CPU.h"
#include "Core/HW/DSP.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"
#include "Core/PowerPC/JitCommon/Jit_Util.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"

using namespace Gen;

void Jit64::lXXx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  int a = inst.RA, b = inst.RB, d = inst.RD;

  // Determine whether this instruction updates inst.RA
  bool update;
  if (inst.OPCD == 31)
    update = ((inst.SUBOP10 & 0x20) != 0);
  else
    update = ((inst.OPCD & 1) != 0);

  // Skip disabled JIT instructions
  FALLBACK_IF(SConfig::GetInstance().bJITLoadStorelbzxOff && (inst.OPCD == 31) &&
              (inst.SUBOP10 == 87));
  FALLBACK_IF(SConfig::GetInstance().bJITLoadStorelXzOff &&
              ((inst.OPCD == 34) || (inst.OPCD == 40) || (inst.OPCD == 32)));
  FALLBACK_IF(SConfig::GetInstance().bJITLoadStorelwzOff && (inst.OPCD == 32));

  auto ra = a ? regs.gpr.Lock(a) : regs.gpr.Zero();
  auto rb = inst.OPCD == 31 ? regs.gpr.Lock(b) : regs.gpr.Imm32((u32)(s32)(s16)inst.SIMM_16);
  auto rd = regs.gpr.Lock(d);

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
      UnexpectedInstructionForm();
      return;
    }
    break;

  default:
    UnexpectedInstructionForm();
    return;
  }

  // PowerPC has no 8-bit sign extended load, but x86 does, so merge extsb with the load if we find
  // it.
  if (MergeAllowedNextInstructions(1) && accessSize == 8 && js.op[1].inst.OPCD == 31 &&
      js.op[1].inst.SUBOP10 == 954 && js.op[1].inst.RS == inst.RD && js.op[1].inst.RA == inst.RD &&
      !js.op[1].inst.Rc)
  {
    js.downcountAmount++;
    js.skipInstructions = 1;
    signExtend = true;
  }

  auto xd = rd.BindWriteAndReadIf(rd.IsAliasOf(ra, rb));
  SafeLoad(xd, ra, rb, accessSize, signExtend, !byte_reversed, update);

  // TODO(ector): Make it dynamically enable/disable idle skipping where appropriate
  // Will give nice boost to dual core mode
  // (mb2): I agree,
  // IMHO those Idles should always be skipped and replaced by a more controllable "native" Idle
  // methode
  // ... maybe the throttle one already do that :p
  // TODO: We shouldn't use a debug read here.  It should be possible to get
  // the following instructions out of the JIT state.
  if (SConfig::GetInstance().bSkipIdle && CPU::GetState() != CPU::CPU_STEPPING && inst.OPCD == 32 &&
      MergeAllowedNextInstructions(2) && (inst.hex & 0xFFFF0000) == 0x800D0000 &&
      (js.op[1].inst.hex == 0x28000000 ||
       (SConfig::GetInstance().bWii && js.op[1].inst.hex == 0x2C000000)) &&
      js.op[2].inst.hex == 0x4182fff8)
  {
    // if it's still 0, we can wait until the next event
    TEST(32, xd, xd);
    FixupBranch noIdle = J_CC(CC_NZ);

    BitSet32 registersInUse = CallerSavedRegistersInUse();
    ABI_PushRegistersAndAdjustStack(registersInUse, 0);
    ABI_CallFunction((void*)&CoreTiming::Idle);
    ABI_PopRegistersAndAdjustStack(registersInUse, 0);

    // ! we must continue executing of the loop after exception handling, maybe there is still 0 in
    // r0
    // MOV(32, PPCSTATE(pc), Imm32(js.compilerPC));
    WriteExceptionExit();

    SetJumpTarget(noIdle);
  }
}

void Jit64::dcbx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  auto addr = regs.gpr.Borrow();
  {
    auto value = regs.gpr.Borrow();
    auto tmp = regs.gpr.Borrow();

    auto ra = inst.RA ? regs.gpr.Lock(inst.RA) : regs.gpr.Zero();
    auto rb = regs.gpr.Lock(inst.RB);

    MOV_sum(32, addr, ra, rb);

    // Check whether a JIT cache line needs to be invalidated.
    LEA(32, value, MScaled(addr, SCALE_8, 0));  // addr << 3 (masks the first 3 bits)
    SHR(32, R(value), Imm8(3 + 5 + 5));  // >> 5 for cache line size, >> 5 for width of bitset
    MOV(64, R(tmp), ImmPtr(jit->GetBlockCache()->GetBlockBitSet()));
    MOV(32, R(value), MComplex(tmp, value, SCALE_4, 0));
    SHR(32, R(addr), Imm8(5));
    BT(32, R(value), R(addr));
  }

  FixupBranch c = J_CC(CC_C, true);
  SwitchToFarCode();
  SetJumpTarget(c);
  BitSet32 registersInUse = CallerSavedRegistersInUse();
  ABI_PushRegistersAndAdjustStack(registersInUse, 0);
  SHL(32, R(addr), Imm8(5));
  ABI_CallFunctionR((void*)JitInterface::InvalidateICacheLine, addr);
  ABI_PopRegistersAndAdjustStack(registersInUse, 0);
  asm_routines.ResetStack(*this);
  c = J(true);
  SwitchToNearCode();
  SetJumpTarget(c);

  // dcbi
  if (inst.SUBOP10 == 470)
  {
    // Flush DSP DMA if DMAState bit is set
    TEST(16, M(&DSP::g_dspState), Imm16(1 << 9));
    c = J_CC(CC_NZ, true);
    SwitchToFarCode();
    SetJumpTarget(c);
    ABI_PushRegistersAndAdjustStack(registersInUse, 0);
    SHL(32, R(addr), Imm8(5));
    ABI_CallFunctionR((void*)DSP::FlushInstantDMA, addr);
    ABI_PopRegistersAndAdjustStack(registersInUse, 0);
    c = J(true);
    SwitchToNearCode();
    SetJumpTarget(c);
  }
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
  if (MergeAllowedNextInstructions(1) && js.op[1].inst.OPCD == 31 && js.op[1].inst.SUBOP10 == 54 &&
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

  int a = inst.RA;
  int b = inst.RB;

  auto ra = a ? regs.gpr.Lock(a) : regs.gpr.Zero();
  auto rb = regs.gpr.Lock(b);
  auto scratch = regs.gpr.Borrow();

  u32 mem_mask = Memory::ADDR_MASK_HW_ACCESS;

  // The following masks the region used by the GC/Wii virtual memory lib
  mem_mask |= Memory::ADDR_MASK_MEM1;

  MOV_sum(32, scratch, ra, rb);
  AND(32, scratch, Imm32(~31));
  TEST(32, scratch, Imm32(mem_mask));
  FixupBranch slow = J_CC(CC_NZ, true);

  // Should this code ever run? I can't find any games that use DCBZ on non-physical addresses, but
  // supposedly there are, at least for some MMU titles. Let's be careful and support it to be sure.
  SwitchToFarCode();
  SetJumpTarget(slow);
  MOV(32, M(&PC), Imm32(jit->js.compilerPC));
  BitSet32 registersInUse = CallerSavedRegistersInUse();
  ABI_PushRegistersAndAdjustStack(registersInUse, 0);
  ABI_CallFunctionR((void*)&PowerPC::ClearCacheLine, scratch);
  ABI_PopRegistersAndAdjustStack(registersInUse, 0);
  FixupBranch exit = J(true);
  SwitchToNearCode();

  // Mask out the address so we don't write to MEM1 out of bounds
  // FIXME: Work out why the AGP disc writes out of bounds
  if (!SConfig::GetInstance().bWii)
    AND(32, scratch, Imm32(Memory::RAM_MASK));
  PXOR(XMM0, R(XMM0));
  MOVAPS(MComplex(RMEM, scratch, SCALE_1, 0), XMM0);
  MOVAPS(MComplex(RMEM, scratch, SCALE_1, 16), XMM0);
  SetJumpTarget(exit);
}

void Jit64::stX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  int s = inst.RS;
  int a = inst.RA;
  bool update = (inst.OPCD & 1);

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
    UnexpectedInstructionForm();
    return;
  }

  auto rs = regs.gpr.Lock(s);
  auto ra = a ? regs.gpr.Lock(a) : regs.gpr.Zero();
  auto ofs = regs.gpr.Imm32((u32)(s32)(s16)inst.SIMM_16);
  SafeWrite(rs, ra, ofs, accessSize, true, update);
}

void Jit64::stXx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);
  int a = inst.RA, b = inst.RB, s = inst.RS;
  bool update = !!(inst.SUBOP10 & 32);
  bool byte_reverse = !!(inst.SUBOP10 & 512);

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
    UnexpectedInstructionForm();
    return;
  }

  auto ra = a ? regs.gpr.Lock(a) : regs.gpr.Zero();
  auto rb = regs.gpr.Lock(b);
  auto rs = regs.gpr.Lock(s);
  SafeWrite(rs, ra, rb, accessSize, !byte_reverse, update);
}

// A few games use these heavily in video codecs.
void Jit64::lmw(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  auto ra = inst.RA ? regs.gpr.Lock(inst.RA) : regs.gpr.Zero();

  // (this needs some thoughts on perf)

  // TODO: This doesn't handle rollback on DSI correctly
  for (int i = inst.RD; i < 32; i++)
  {
    if (i == inst.RA)
      continue;
    auto reg = regs.gpr.Lock(i).Bind(BindMode::Write);
    u32 offset = (i - inst.RD) * 4 + (u32)(s32)inst.SIMM_16;
    auto ofs = regs.gpr.Imm32(offset);

    SafeLoad(reg, ra, ofs, 32, false, true, false);
  }

  if (inst.RA >= inst.RD)
  {
    auto xa = ra.Bind(BindMode::ReadWrite);
    u32 offset = (inst.RA - inst.RD) * 4 + (u32)(s32)inst.SIMM_16;
    auto ofs = regs.gpr.Imm32(offset);
    SafeLoad(xa, xa, ofs, 32, false, true, false);
  }
}

void Jit64::stmw(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  auto ra = inst.RA ? regs.gpr.Lock(inst.RA) : regs.gpr.Zero();

  // TODO: This doesn't handle rollback on DSI correctly
  for (int i = inst.RD; i < 32; i++)
  {
    auto reg = regs.gpr.Lock(i);
    u32 offset = (i - inst.RD) * 4 + (u32)(s32)inst.SIMM_16;
    auto ofs = regs.gpr.Imm32(offset);

    SafeWrite(reg, ra, ofs, 32, true, false);
  }
}
