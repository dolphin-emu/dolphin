// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitArm64/Jit.h"

#include <bit>

#include "Common/Arm64Emitter.h"
#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/ScopeGuard.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/Debugger/BranchWatch.h"
#include "Core/HW/DSP.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"
#include "Core/PowerPC/JitArm64/Jit_Util.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

using namespace Arm64Gen;

void JitArm64::SafeLoadToReg(UGeckoInstruction inst, u32 dest, s32 addr, s32 offsetReg, u32 flags,
                             s32 offset, bool update)
{
  // We want to make sure to not get LR as a temp register
  gpr.Lock(ARM64Reg::W1, ARM64Reg::W30);
  if (jo.memcheck || !jo.fastmem)
    gpr.Lock(ARM64Reg::W0);

  gpr.BindToRegister(dest, dest == (u32)addr || dest == (u32)offsetReg, false);
  ARM64Reg dest_reg = gpr.R(dest);
  ARM64Reg up_reg = ARM64Reg::INVALID_REG;
  ARM64Reg off_reg = ARM64Reg::INVALID_REG;

  if (addr != -1 && !gpr.IsImm(addr))
    up_reg = gpr.R(addr);

  if (offsetReg != -1 && !gpr.IsImm(offsetReg))
    off_reg = gpr.R(offsetReg);

  ARM64Reg addr_reg = ARM64Reg::W1;
  u32 imm_addr = 0;
  bool is_immediate = false;

  if (offsetReg == -1)
  {
    if (addr != -1)
    {
      if (gpr.IsImm(addr))
      {
        is_immediate = true;
        imm_addr = gpr.GetImm(addr) + offset;
      }
      else
      {
        ADDI2R(addr_reg, up_reg, offset, addr_reg);
      }
    }
    else
    {
      is_immediate = true;
      imm_addr = offset;
    }
  }
  else
  {
    if (addr != -1)
    {
      if (gpr.IsImm(addr) && gpr.IsImm(offsetReg))
      {
        is_immediate = true;
        imm_addr = gpr.GetImm(addr) + gpr.GetImm(offsetReg);
      }
      else if (gpr.IsImm(addr) && !gpr.IsImm(offsetReg))
      {
        u32 reg_offset = gpr.GetImm(addr);
        ADDI2R(addr_reg, off_reg, reg_offset, addr_reg);
      }
      else if (!gpr.IsImm(addr) && gpr.IsImm(offsetReg))
      {
        u32 reg_offset = gpr.GetImm(offsetReg);
        ADDI2R(addr_reg, up_reg, reg_offset, addr_reg);
      }
      else
      {
        ADD(addr_reg, up_reg, off_reg);
      }
    }
    else
    {
      if (gpr.IsImm(offsetReg))
      {
        is_immediate = true;
        imm_addr = gpr.GetImm(offsetReg);
      }
      else
      {
        MOV(addr_reg, off_reg);
      }
    }
  }

  ARM64Reg XA = EncodeRegTo64(addr_reg);

  bool addr_reg_set = !is_immediate;
  const auto set_addr_reg_if_needed = [&] {
    if (!addr_reg_set)
      MOVI2R(XA, imm_addr);
  };

  const bool early_update = !jo.memcheck && dest != static_cast<u32>(addr);
  if (update && early_update)
  {
    gpr.BindToRegister(addr, false);
    set_addr_reg_if_needed();
    MOV(gpr.R(addr), addr_reg);
  }

  BitSet32 regs_in_use = gpr.GetCallerSavedUsed();
  BitSet32 fprs_in_use = fpr.GetCallerSavedUsed();
  if (!update || early_update)
    regs_in_use[DecodeReg(ARM64Reg::W1)] = 0;
  if (jo.memcheck || !jo.fastmem)
    regs_in_use[DecodeReg(ARM64Reg::W0)] = 0;
  if (!jo.memcheck)
    regs_in_use[DecodeReg(dest_reg)] = 0;

  u32 access_size = BackPatchInfo::GetFlagSize(flags);
  u32 mmio_address = 0;
  if (is_immediate)
    mmio_address = m_mmu.IsOptimizableMMIOAccess(imm_addr, access_size);

  if (is_immediate && m_mmu.IsOptimizableRAMAddress(imm_addr, access_size, inst))
  {
    set_addr_reg_if_needed();
    EmitBackpatchRoutine(inst, flags, MemAccessMode::AlwaysFastAccess, dest_reg, XA, regs_in_use,
                         fprs_in_use);
  }
  else if (mmio_address)
  {
    regs_in_use[DecodeReg(ARM64Reg::W1)] = 0;
    regs_in_use[DecodeReg(ARM64Reg::W30)] = 0;
    regs_in_use[DecodeReg(dest_reg)] = 0;
    MMIOLoadToReg(m_system, m_system.GetMemory().GetMMIOMapping(), this, &m_float_emit, regs_in_use,
                  fprs_in_use, dest_reg, mmio_address, flags);
    addr_reg_set = false;
  }
  else
  {
    set_addr_reg_if_needed();
    EmitBackpatchRoutine(inst, flags, MemAccessMode::Auto, dest_reg, XA, regs_in_use, fprs_in_use);
  }

  gpr.BindToRegister(dest, false, true);
  ASSERT(dest_reg == gpr.R(dest));

  if (update && !early_update)
  {
    gpr.BindToRegister(addr, false);
    set_addr_reg_if_needed();
    MOV(gpr.R(addr), addr_reg);
  }

  gpr.Unlock(ARM64Reg::W1, ARM64Reg::W30);
  if (jo.memcheck || !jo.fastmem)
    gpr.Unlock(ARM64Reg::W0);
}

void JitArm64::SafeStoreFromReg(UGeckoInstruction inst, s32 dest, u32 value, s32 regOffset,
                                u32 flags, s32 offset, bool update)
{
  // We want to make sure to not get LR as a temp register
  gpr.Lock(ARM64Reg::W1, ARM64Reg::W2, ARM64Reg::W30);
  if (!jo.fastmem)
    gpr.Lock(ARM64Reg::W0);

  ARM64Reg RS = gpr.R(value);

  ARM64Reg reg_dest = ARM64Reg::INVALID_REG;
  ARM64Reg reg_off = ARM64Reg::INVALID_REG;

  if (regOffset != -1 && !gpr.IsImm(regOffset))
    reg_off = gpr.R(regOffset);
  if (dest != -1 && !gpr.IsImm(dest))
    reg_dest = gpr.R(dest);

  ARM64Reg addr_reg = ARM64Reg::W2;

  u32 imm_addr = 0;
  bool is_immediate = false;

  if (regOffset == -1)
  {
    if (dest != -1)
    {
      if (gpr.IsImm(dest))
      {
        is_immediate = true;
        imm_addr = gpr.GetImm(dest) + offset;
      }
      else
      {
        ADDI2R(addr_reg, reg_dest, offset, addr_reg);
      }
    }
    else
    {
      is_immediate = true;
      imm_addr = offset;
    }
  }
  else
  {
    if (dest != -1)
    {
      if (gpr.IsImm(dest) && gpr.IsImm(regOffset))
      {
        is_immediate = true;
        imm_addr = gpr.GetImm(dest) + gpr.GetImm(regOffset);
      }
      else if (gpr.IsImm(dest) && !gpr.IsImm(regOffset))
      {
        u32 reg_offset = gpr.GetImm(dest);
        ADDI2R(addr_reg, reg_off, reg_offset, addr_reg);
      }
      else if (!gpr.IsImm(dest) && gpr.IsImm(regOffset))
      {
        u32 reg_offset = gpr.GetImm(regOffset);
        ADDI2R(addr_reg, reg_dest, reg_offset, addr_reg);
      }
      else
      {
        ADD(addr_reg, reg_dest, reg_off);
      }
    }
    else
    {
      if (gpr.IsImm(regOffset))
      {
        is_immediate = true;
        imm_addr = gpr.GetImm(regOffset);
      }
      else
      {
        MOV(addr_reg, reg_off);
      }
    }
  }

  ARM64Reg XA = EncodeRegTo64(addr_reg);

  bool addr_reg_set = !is_immediate;
  const auto set_addr_reg_if_needed = [&] {
    if (!addr_reg_set)
      MOVI2R(XA, imm_addr);
  };

  const bool early_update = !jo.memcheck && value != static_cast<u32>(dest);
  if (update && early_update)
  {
    gpr.BindToRegister(dest, false);
    set_addr_reg_if_needed();
    MOV(gpr.R(dest), addr_reg);
  }

  BitSet32 regs_in_use = gpr.GetCallerSavedUsed();
  BitSet32 fprs_in_use = fpr.GetCallerSavedUsed();
  regs_in_use[DecodeReg(ARM64Reg::W1)] = 0;
  if (!update || early_update)
    regs_in_use[DecodeReg(ARM64Reg::W2)] = 0;
  if (!jo.fastmem)
    regs_in_use[DecodeReg(ARM64Reg::W0)] = 0;

  u32 access_size = BackPatchInfo::GetFlagSize(flags);
  u32 mmio_address = 0;
  if (is_immediate)
    mmio_address = m_mmu.IsOptimizableMMIOAccess(imm_addr, access_size);

  if (is_immediate && jo.optimizeGatherPipe && m_mmu.IsOptimizableGatherPipeWrite(imm_addr))
  {
    int accessSize;
    if (flags & BackPatchInfo::FLAG_SIZE_32)
      accessSize = 32;
    else if (flags & BackPatchInfo::FLAG_SIZE_16)
      accessSize = 16;
    else
      accessSize = 8;

    LDR(IndexType::Unsigned, ARM64Reg::X2, PPC_REG, PPCSTATE_OFF(gather_pipe_ptr));

    ARM64Reg temp = ARM64Reg::W1;
    temp = ByteswapBeforeStore(this, &m_float_emit, temp, RS, flags, true);

    if (accessSize == 32)
      STR(IndexType::Post, temp, ARM64Reg::X2, 4);
    else if (accessSize == 16)
      STRH(IndexType::Post, temp, ARM64Reg::X2, 2);
    else
      STRB(IndexType::Post, temp, ARM64Reg::X2, 1);

    STR(IndexType::Unsigned, ARM64Reg::X2, PPC_REG, PPCSTATE_OFF(gather_pipe_ptr));

    js.fifoBytesSinceCheck += accessSize >> 3;
  }
  else if (is_immediate && m_mmu.IsOptimizableRAMAddress(imm_addr, access_size, inst))
  {
    set_addr_reg_if_needed();
    EmitBackpatchRoutine(inst, flags, MemAccessMode::AlwaysFastAccess, RS, XA, regs_in_use,
                         fprs_in_use);
  }
  else if (mmio_address)
  {
    regs_in_use[DecodeReg(ARM64Reg::W1)] = 0;
    regs_in_use[DecodeReg(ARM64Reg::W2)] = 0;
    regs_in_use[DecodeReg(ARM64Reg::W30)] = 0;
    regs_in_use[DecodeReg(RS)] = 0;
    MMIOWriteRegToAddr(m_system, m_system.GetMemory().GetMMIOMapping(), this, &m_float_emit,
                       regs_in_use, fprs_in_use, RS, mmio_address, flags);
    addr_reg_set = false;
  }
  else
  {
    set_addr_reg_if_needed();
    EmitBackpatchRoutine(inst, flags, MemAccessMode::Auto, RS, XA, regs_in_use, fprs_in_use);
  }

  if (update && !early_update)
  {
    gpr.BindToRegister(dest, false);
    set_addr_reg_if_needed();
    MOV(gpr.R(dest), addr_reg);
  }

  gpr.Unlock(ARM64Reg::W1, ARM64Reg::W2, ARM64Reg::W30);
  if (!jo.fastmem)
    gpr.Unlock(ARM64Reg::W0);
}

FixupBranch JitArm64::BATAddressLookup(ARM64Reg addr_out, ARM64Reg addr_in, ARM64Reg tmp,
                                       const void* bat_table)
{
  tmp = EncodeRegTo64(tmp);

  MOVP2R(tmp, bat_table);
  LSR(addr_out, addr_in, PowerPC::BAT_INDEX_SHIFT);
  LDR(addr_out, tmp, ArithOption(addr_out, true));
  FixupBranch pass = TBNZ(addr_out, MathUtil::IntLog2(PowerPC::BAT_MAPPED_BIT));
  FixupBranch fail = B();
  SetJumpTarget(pass);
  return fail;
}

FixupBranch JitArm64::CheckIfSafeAddress(Arm64Gen::ARM64Reg addr, Arm64Gen::ARM64Reg tmp1,
                                         Arm64Gen::ARM64Reg tmp2)
{
  tmp2 = EncodeRegTo64(tmp2);

  MOVP2R(tmp2, m_mmu.GetDBATTable().data());
  LSR(tmp1, addr, PowerPC::BAT_INDEX_SHIFT);
  LDR(tmp1, tmp2, ArithOption(tmp1, true));
  FixupBranch pass = TBNZ(tmp1, MathUtil::IntLog2(PowerPC::BAT_PHYSICAL_BIT));
  FixupBranch fail = B();
  SetJumpTarget(pass);
  return fail;
}

void JitArm64::lXX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  u32 a = inst.RA, b = inst.RB, d = inst.RD;
  s32 offset = inst.SIMM_16;
  s32 offsetReg = -1;
  u32 flags = BackPatchInfo::FLAG_LOAD;
  bool update = false;

  switch (inst.OPCD)
  {
  case 31:
    offsetReg = b;
    switch (inst.SUBOP10)
    {
    case 55:  // lwzux
      update = true;
      [[fallthrough]];
    case 23:  // lwzx
      flags |= BackPatchInfo::FLAG_SIZE_32;
      break;
    case 119:  // lbzux
      update = true;
      [[fallthrough]];
    case 87:  // lbzx
      flags |= BackPatchInfo::FLAG_SIZE_8;
      break;
    case 311:  // lhzux
      update = true;
      [[fallthrough]];
    case 279:  // lhzx
      flags |= BackPatchInfo::FLAG_SIZE_16;
      break;
    case 375:  // lhaux
      update = true;
      [[fallthrough]];
    case 343:  // lhax
      flags |= BackPatchInfo::FLAG_EXTEND | BackPatchInfo::FLAG_SIZE_16;
      break;
    case 534:  // lwbrx
      flags |= BackPatchInfo::FLAG_REVERSE | BackPatchInfo::FLAG_SIZE_32;
      break;
    case 790:  // lhbrx
      flags |= BackPatchInfo::FLAG_REVERSE | BackPatchInfo::FLAG_SIZE_16;
      break;
    }
    break;
  case 33:  // lwzu
    update = true;
    [[fallthrough]];
  case 32:  // lwz
    flags |= BackPatchInfo::FLAG_SIZE_32;
    break;
  case 35:  // lbzu
    update = true;
    [[fallthrough]];
  case 34:  // lbz
    flags |= BackPatchInfo::FLAG_SIZE_8;
    break;
  case 41:  // lhzu
    update = true;
    [[fallthrough]];
  case 40:  // lhz
    flags |= BackPatchInfo::FLAG_SIZE_16;
    break;
  case 43:  // lhau
    update = true;
    [[fallthrough]];
  case 42:  // lha
    flags |= BackPatchInfo::FLAG_EXTEND | BackPatchInfo::FLAG_SIZE_16;
    break;
  }

  SafeLoadToReg(inst, d, update ? a : (a ? a : -1), offsetReg, flags, offset, update);
}

void JitArm64::stX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  u32 a = inst.RA, b = inst.RB, s = inst.RS;
  s32 offset = inst.SIMM_16;
  s32 regOffset = -1;
  u32 flags = BackPatchInfo::FLAG_STORE;
  bool update = false;
  switch (inst.OPCD)
  {
  case 31:
    regOffset = b;
    switch (inst.SUBOP10)
    {
    case 183:  // stwux
      update = true;
      [[fallthrough]];
    case 151:  // stwx
      flags |= BackPatchInfo::FLAG_SIZE_32;
      break;
    case 247:  // stbux
      update = true;
      [[fallthrough]];
    case 215:  // stbx
      flags |= BackPatchInfo::FLAG_SIZE_8;
      break;
    case 439:  // sthux
      update = true;
      [[fallthrough]];
    case 407:  // sthx
      flags |= BackPatchInfo::FLAG_SIZE_16;
      break;
    case 662:  // stwbrx
      flags |= BackPatchInfo::FLAG_REVERSE | BackPatchInfo::FLAG_SIZE_32;
      break;
    case 918:  // sthbrx
      flags |= BackPatchInfo::FLAG_REVERSE | BackPatchInfo::FLAG_SIZE_16;
      break;
    }
    break;
  case 37:  // stwu
    update = true;
    [[fallthrough]];
  case 36:  // stw
    flags |= BackPatchInfo::FLAG_SIZE_32;
    break;
  case 39:  // stbu
    update = true;
    [[fallthrough]];
  case 38:  // stb
    flags |= BackPatchInfo::FLAG_SIZE_8;
    break;
  case 45:  // sthu
    update = true;
    [[fallthrough]];
  case 44:  // sth
    flags |= BackPatchInfo::FLAG_SIZE_16;
    break;
  }

  SafeStoreFromReg(inst, update ? a : (a ? a : -1), s, regOffset, flags, offset, update);
}

void JitArm64::lmw(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  u32 a = inst.RA, d = inst.RD;
  s32 offset = inst.SIMM_16;

  gpr.Lock(ARM64Reg::W1, ARM64Reg::W30);
  if (jo.memcheck || !jo.fastmem)
    gpr.Lock(ARM64Reg::W0);

  // MMU games make use of a >= d despite this being invalid according to the PEM.
  // If a >= d occurs, we must make sure to not re-read rA after starting doing the loads.
  ARM64Reg addr_reg = ARM64Reg::W1;
  bool a_is_addr_base_reg = false;
  if (!a)
    MOVI2R(addr_reg, offset);
  else if (gpr.IsImm(a))
    MOVI2R(addr_reg, gpr.GetImm(a) + offset);
  else if (a < d && offset + (31 - d) * 4 < 0x1000)
    a_is_addr_base_reg = true;
  else
    ADDI2R(addr_reg, gpr.R(a), offset, addr_reg);

  ARM64Reg addr_base_reg = a_is_addr_base_reg ? ARM64Reg::INVALID_REG : gpr.GetReg();
  if (!a_is_addr_base_reg)
    MOV(addr_base_reg, addr_reg);

  BitSet32 gprs_to_discard{};
  if (!jo.memcheck)
  {
    gprs_to_discard = js.op->gprDiscardable;
    if (gprs_to_discard[a])
    {
      if (a_is_addr_base_reg)
        gprs_to_discard[a] = false;
      else if (a < d)
        gpr.DiscardRegisters(BitSet32{int(a)});
    }
  }

  BitSet32 gprs_to_flush = ~js.op->gprInUse & BitSet32(0xFFFFFFFFU << d);
  if (!js.op->gprInUse[a])
  {
    if (!a_is_addr_base_reg)
    {
      gprs_to_flush[a] = true;
    }
    else
    {
      gprs_to_flush[a] = false;

      if (a + 1 == d && (std::countr_one((~js.op->gprInUse).m_val >> a) & 1) == 0)
      {
        // In this situation, we can save one store instruction by flushing GPR d together with GPR
        // a, but we shouldn't flush GPR a until the end of the PPC instruction. Therefore, let's
        // also wait with flushing GPR d until the end of the PPC instruction.
        gprs_to_flush[d] = false;
      }
    }
  }

  // TODO: This doesn't handle rollback on DSI correctly
  constexpr u32 flags = BackPatchInfo::FLAG_LOAD | BackPatchInfo::FLAG_SIZE_32;
  for (u32 i = d; i < 32; i++)
  {
    gpr.BindToRegister(i, false, false);
    ARM64Reg dest_reg = gpr.R(i);

    if (a_is_addr_base_reg)
      ADDI2R(addr_reg, gpr.R(a), offset + (i - d) * 4);
    else if (i != d)
      ADDI2R(addr_reg, addr_base_reg, (i - d) * 4);

    BitSet32 regs_in_use = gpr.GetCallerSavedUsed();
    BitSet32 fprs_in_use = fpr.GetCallerSavedUsed();
    regs_in_use[DecodeReg(addr_reg)] = 0;
    if (jo.memcheck || !jo.fastmem)
      regs_in_use[DecodeReg(ARM64Reg::W0)] = 0;
    if (!jo.memcheck)
      regs_in_use[DecodeReg(dest_reg)] = 0;

    EmitBackpatchRoutine(inst, flags, MemAccessMode::Auto, dest_reg, EncodeRegTo64(addr_reg),
                         regs_in_use, fprs_in_use);

    gpr.BindToRegister(i, false, true);
    ASSERT(dest_reg == gpr.R(i));

    // To reduce register pressure and to avoid getting a pipeline-unfriendly long run of stores
    // after this instruction, flush registers that would be flushed after this instruction anyway.
    //
    // We try to store two registers at a time when possible to let the register cache use STP.
    if (gprs_to_discard[i])
    {
      gpr.DiscardRegisters(BitSet32{int(i)});
    }
    else if (gprs_to_flush[i])
    {
      BitSet32 gprs_to_flush_this_time{};
      if (i != 0 && gprs_to_flush[i - 1])
        gprs_to_flush_this_time = BitSet32{int(i - 1), int(i)};
      else if (i == 31 || !gprs_to_flush[i + 1])
        gprs_to_flush_this_time = BitSet32{int(i)};
      else
        continue;

      gpr.StoreRegisters(gprs_to_flush_this_time);
      gprs_to_flush &= ~gprs_to_flush_this_time;
    }
  }

  gpr.Unlock(ARM64Reg::W1, ARM64Reg::W30);
  if (jo.memcheck || !jo.fastmem)
    gpr.Unlock(ARM64Reg::W0);
  if (!a_is_addr_base_reg)
    gpr.Unlock(addr_base_reg);
}

void JitArm64::stmw(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  u32 a = inst.RA, s = inst.RS;
  s32 offset = inst.SIMM_16;

  gpr.Lock(ARM64Reg::W1, ARM64Reg::W2, ARM64Reg::W30);
  if (!jo.fastmem)
    gpr.Lock(ARM64Reg::W0);

  ARM64Reg addr_reg = ARM64Reg::W2;
  bool a_is_addr_base_reg = false;
  if (!a)
    MOVI2R(addr_reg, offset);
  else if (gpr.IsImm(a))
    MOVI2R(addr_reg, gpr.GetImm(a) + offset);
  else if (offset + (31 - s) * 4 < 0x1000)
    a_is_addr_base_reg = true;
  else
    ADDI2R(addr_reg, gpr.R(a), offset, addr_reg);

  ARM64Reg addr_base_reg = a_is_addr_base_reg ? ARM64Reg::INVALID_REG : gpr.GetReg();
  if (!a_is_addr_base_reg)
    MOV(addr_base_reg, addr_reg);

  BitSet32 gprs_to_discard{};
  if (!jo.memcheck)
  {
    gprs_to_discard = js.op->gprDiscardable;
    if (gprs_to_discard[a])
    {
      if (a_is_addr_base_reg)
        gprs_to_discard[a] = false;
      else if (a < s)
        gpr.DiscardRegisters(BitSet32{int(a)});
    }
  }

  const BitSet32 dirty_gprs_to_flush_unmasked = ~js.op->gprInUse & gpr.GetDirtyGPRs();
  BitSet32 dirty_gprs_to_flush = dirty_gprs_to_flush_unmasked & BitSet32(0xFFFFFFFFU << s);
  if (dirty_gprs_to_flush_unmasked[a])
  {
    if (!a_is_addr_base_reg)
    {
      dirty_gprs_to_flush[a] = true;
    }
    else
    {
      dirty_gprs_to_flush[a] = false;

      if (a + 1 == s && (std::countr_one((~js.op->gprInUse).m_val >> a) & 1) == 0)
      {
        // In this situation, we can save one store instruction by flushing GPR s together with GPR
        // a, but we shouldn't flush GPR a until the end of the PPC instruction. Therefore, let's
        // also wait with flushing GPR s until the end of the PPC instruction.
        dirty_gprs_to_flush[s] = false;
      }
    }
  }

  // TODO: This doesn't handle rollback on DSI correctly
  constexpr u32 flags = BackPatchInfo::FLAG_STORE | BackPatchInfo::FLAG_SIZE_32;
  for (u32 i = s; i < 32; i++)
  {
    ARM64Reg src_reg = gpr.R(i);

    if (a_is_addr_base_reg)
      ADDI2R(addr_reg, gpr.R(a), offset + (i - s) * 4);
    else if (i != s)
      ADDI2R(addr_reg, addr_base_reg, (i - s) * 4);

    BitSet32 regs_in_use = gpr.GetCallerSavedUsed();
    BitSet32 fprs_in_use = fpr.GetCallerSavedUsed();
    regs_in_use[DecodeReg(ARM64Reg::W1)] = 0;
    regs_in_use[DecodeReg(addr_reg)] = 0;
    if (!jo.fastmem)
      regs_in_use[DecodeReg(ARM64Reg::W0)] = 0;

    EmitBackpatchRoutine(inst, flags, MemAccessMode::Auto, src_reg, EncodeRegTo64(addr_reg),
                         regs_in_use, fprs_in_use);

    // To reduce register pressure and to avoid getting a pipeline-unfriendly long run of stores
    // after this instruction, flush registers that would be flushed after this instruction anyway.
    //
    // We try to store two registers at a time when possible to let the register cache use STP.
    if (gprs_to_discard[i])
    {
      gpr.DiscardRegisters(BitSet32{int(i)});
    }
    else if (dirty_gprs_to_flush[i])
    {
      BitSet32 gprs_to_flush_this_time{};
      if (i != 0 && dirty_gprs_to_flush[i - 1])
        gprs_to_flush_this_time = BitSet32{int(i - 1), int(i)};
      else if (i == 31 || !dirty_gprs_to_flush[i + 1])
        gprs_to_flush_this_time = BitSet32{int(i)};
      else
        continue;

      gpr.StoreRegisters(gprs_to_flush_this_time);
      dirty_gprs_to_flush &= ~gprs_to_flush_this_time;
    }
    else if (!js.op->gprInUse[i])
    {
      // If this register can be flushed but it isn't dirty, no store instruction will be emitted
      // when flushing it, so it doesn't matter if we flush it together with another register or
      // not. Let's just flush it in the simplest way possible.
      gpr.StoreRegisters(BitSet32{int(i)});
    }
  }

  gpr.Unlock(ARM64Reg::W1, ARM64Reg::W2, ARM64Reg::W30);
  if (!jo.fastmem)
    gpr.Unlock(ARM64Reg::W0);
  if (!a_is_addr_base_reg)
    gpr.Unlock(addr_base_reg);
}

void JitArm64::dcbx(UGeckoInstruction inst)
{
  FALLBACK_IF(m_accurate_cpu_cache_enabled);

  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  u32 a = inst.RA, b = inst.RB;

  // Check if the next instructions match a known looping pattern:
  // - dcbx rX
  // - addi rX,rX,32
  // - bdnz+ -8
  const bool make_loop = a == 0 && b != 0 && CanMergeNextInstructions(2) &&
                         (js.op[1].inst.hex & 0xfc00'ffff) == 0x38000020 &&
                         js.op[1].inst.RA_6 == b && js.op[1].inst.RD_2 == b &&
                         js.op[2].inst.hex == 0x4200fff8;

  constexpr ARM64Reg WA = ARM64Reg::W0, WB = ARM64Reg::W1, loop_counter = ARM64Reg::W2;
  // Be careful, loop_counter is only locked when make_loop == true.
  gpr.Lock(WA, WB);

  if (make_loop)
  {
    gpr.Lock(loop_counter);
    gpr.BindToRegister(b, true);

    // We'll execute somewhere between one single cacheline invalidation and however many are needed
    // to reduce the downcount to zero, never exceeding the amount requested by the game.
    // To stay consistent with the rest of the code we adjust the involved registers (CTR and Rb)
    // by the amount of cache lines we invalidate minus one -- since we'll run the regular addi and
    // bdnz afterwards! So if we invalidate a single cache line, we don't adjust the registers at
    // all, if we invalidate 2 cachelines we adjust the registers by one step, and so on.

    const ARM64Reg reg_cycle_count = gpr.GetReg();
    const ARM64Reg reg_downcount = gpr.GetReg();

    // Figure out how many loops we want to do.
    const u8 cycle_count_per_loop =
        js.op[0].opinfo->num_cycles + js.op[1].opinfo->num_cycles + js.op[2].opinfo->num_cycles;

    LDR(IndexType::Unsigned, reg_downcount, PPC_REG, PPCSTATE_OFF(downcount));
    MOVI2R(WA, 0);
    CMP(reg_downcount, 0);                                          // if (downcount <= 0)
    FixupBranch downcount_is_zero_or_negative = B(CCFlags::CC_LE);  // only do 1 invalidation; else:
    LDR(IndexType::Unsigned, loop_counter, PPC_REG, PPCSTATE_OFF_SPR(SPR_CTR));
    MOVI2R(reg_cycle_count, cycle_count_per_loop);
    SDIV(WB, reg_downcount, reg_cycle_count);  // WB = downcount / cycle_count
    SUB(WA, loop_counter, 1);                  // WA = CTR - 1
    // ^ Note that this CTR-1 implicitly handles the CTR == 0 case correctly.
    CMP(WB, WA);
    CSEL(WA, WB, WA, CCFlags::CC_LO);  // WA = min(WB, WA)

    // WA now holds the amount of loops to execute minus 1, which is the amount we need to adjust
    // downcount, CTR, and Rb by to exit the loop construct with the right values in those
    // registers.

    // CTR -= WA
    SUB(loop_counter, loop_counter, WA);
    STR(IndexType::Unsigned, loop_counter, PPC_REG, PPCSTATE_OFF_SPR(SPR_CTR));

    // downcount -= (WA * reg_cycle_count)
    MUL(WB, WA, reg_cycle_count);
    // ^ Note that this cannot overflow because it's limited by (downcount/cycle_count).
    SUB(reg_downcount, reg_downcount, WB);
    STR(IndexType::Unsigned, reg_downcount, PPC_REG, PPCSTATE_OFF(downcount));

    SetJumpTarget(downcount_is_zero_or_negative);

    // Load the loop_counter register with the amount of invalidations to execute.
    ADD(loop_counter, WA, 1);

    if (IsDebuggingEnabled())
    {
      const ARM64Reg branch_watch = EncodeRegTo64(reg_cycle_count);
      MOVP2R(branch_watch, &m_branch_watch);
      LDRB(IndexType::Unsigned, WB, branch_watch, Core::BranchWatch::GetOffsetOfRecordingActive());
      FixupBranch branch_over = CBZ(WB);

      FixupBranch branch_in = B();
      SwitchToFarCode();
      SetJumpTarget(branch_in);

      const BitSet32 gpr_caller_save =
          gpr.GetCallerSavedUsed() &
          ~BitSet32{DecodeReg(WB), DecodeReg(reg_cycle_count), DecodeReg(reg_downcount)};
      ABI_PushRegisters(gpr_caller_save);
      const ARM64Reg float_emit_tmp = EncodeRegTo64(WB);
      const BitSet32 fpr_caller_save = fpr.GetCallerSavedUsed();
      m_float_emit.ABI_PushRegisters(fpr_caller_save, float_emit_tmp);
      const PPCAnalyst::CodeOp& op = js.op[2];
      ABI_CallFunction(m_ppc_state.msr.IR ? &Core::BranchWatch::HitVirtualTrue_fk_n :
                                            &Core::BranchWatch::HitPhysicalTrue_fk_n,
                       branch_watch, Core::FakeBranchWatchCollectionKey{op.address, op.branchTo},
                       op.inst.hex, WA);
      m_float_emit.ABI_PopRegisters(fpr_caller_save, float_emit_tmp);
      ABI_PopRegisters(gpr_caller_save);

      FixupBranch branch_out = B();
      SwitchToNearCode();
      SetJumpTarget(branch_out);
      SetJumpTarget(branch_over);
    }

    gpr.Unlock(reg_cycle_count, reg_downcount);
  }

  constexpr ARM64Reg effective_addr = WB;
  const ARM64Reg physical_addr = gpr.GetReg();

  if (a)
    ADD(effective_addr, gpr.R(a), gpr.R(b));
  else
    MOV(effective_addr, gpr.R(b));

  if (make_loop)
  {
    // This is the best place to adjust Rb to what it should be since WA still has the
    // adjusted loop count and we're done reading from Rb.
    ADD(gpr.R(b), gpr.R(b), WA, ArithOption(WA, ShiftType::LSL, 5));  // Rb += (WA * 32)
  }

  // Translate effective address to physical address.
  const u8* loop_start = GetCodePtr();
  FixupBranch bat_lookup_failed;
  if (m_ppc_state.feature_flags & FEATURE_FLAG_MSR_IR)
  {
    bat_lookup_failed =
        BATAddressLookup(physical_addr, effective_addr, WA, m_mmu.GetIBATTable().data());
    BFI(physical_addr, effective_addr, 0, PowerPC::BAT_INDEX_SHIFT);
  }

  // Check whether a JIT cache line needs to be invalidated.
  LSR(physical_addr, physical_addr, 5 + 5);  // >> 5 for cache line size, >> 5 for width of bitset
  MOVP2R(EncodeRegTo64(WA), GetBlockCache()->GetBlockBitSet());
  LDR(physical_addr, EncodeRegTo64(WA), ArithOption(EncodeRegTo64(physical_addr), true));

  LSR(WA, effective_addr, 5);  // mask sizeof cacheline, & 0x1f is the position within the bitset

  LSRV(physical_addr, physical_addr, WA);  // move current bit to bit 0

  FixupBranch bit_not_set = TBZ(physical_addr, 0);
  FixupBranch invalidate_needed = B();
  SetJumpTarget(bit_not_set);

  if (make_loop)
  {
    ADD(effective_addr, effective_addr, 32);
    SUBS(loop_counter, loop_counter, 1);
    B(CCFlags::CC_NEQ, loop_start);
  }

  SwitchToFarCode();
  SetJumpTarget(invalidate_needed);
  if (m_ppc_state.feature_flags & FEATURE_FLAG_MSR_IR)
    SetJumpTarget(bat_lookup_failed);

  BitSet32 gprs_to_push = gpr.GetCallerSavedUsed();
  BitSet32 fprs_to_push = fpr.GetCallerSavedUsed();
  gprs_to_push[DecodeReg(effective_addr)] = false;
  gprs_to_push[DecodeReg(physical_addr)] = false;
  gprs_to_push[DecodeReg(WA)] = false;
  if (make_loop)
    gprs_to_push[DecodeReg(loop_counter)] = false;

  ABI_PushRegisters(gprs_to_push);
  m_float_emit.ABI_PushRegisters(fprs_to_push, EncodeRegTo64(WA));

  // For efficiency, effective_addr and loop_counter are already in W1 and W2 respectively
  if (make_loop)
  {
    ABI_CallFunction(&JitInterface::InvalidateICacheLinesFromJIT, &m_system.GetJitInterface(),
                     effective_addr, loop_counter);
  }
  else
  {
    ABI_CallFunction(&JitInterface::InvalidateICacheLineFromJIT, &m_system.GetJitInterface(),
                     effective_addr);
  }

  m_float_emit.ABI_PopRegisters(fprs_to_push, EncodeRegTo64(WA));
  ABI_PopRegisters(gprs_to_push);

  FixupBranch near_addr = B();
  SwitchToNearCode();
  SetJumpTarget(near_addr);

  gpr.Unlock(WA, WB, physical_addr);
  if (make_loop)
    gpr.Unlock(loop_counter);
}

void JitArm64::dcbt(UGeckoInstruction inst)
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

void JitArm64::dcbz(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  int a = inst.RA, b = inst.RB;

  gpr.Lock(ARM64Reg::W1, ARM64Reg::W30);
  if (!jo.fastmem)
    gpr.Lock(ARM64Reg::W0);

  Common::ScopeGuard register_guard([&] {
    gpr.Unlock(ARM64Reg::W1, ARM64Reg::W30);
    if (!jo.fastmem)
      gpr.Unlock(ARM64Reg::W0);
  });

  constexpr ARM64Reg addr_reg = ARM64Reg::W1;
  constexpr ARM64Reg temp_reg = ARM64Reg::W30;

  // HACK: Don't clear any memory in the [0x8000'0000, 0x8000'8000) region.
  FixupBranch end_dcbz_hack;
  bool using_dcbz_hack = false;
  const auto emit_low_dcbz_hack = [&](ARM64Reg reg) {
    if (m_low_dcbz_hack)
    {
      CMPI2R(reg, 0x8000'8000, temp_reg);
      end_dcbz_hack = B(CCFlags::CC_LT);
      using_dcbz_hack = true;
    }
  };

  if (a)
  {
    bool is_imm_a, is_imm_b;
    is_imm_a = gpr.IsImm(a);
    is_imm_b = gpr.IsImm(b);
    if (is_imm_a && is_imm_b)
    {
      // full imm_addr
      u32 imm_addr = gpr.GetImm(b) + gpr.GetImm(a);
      if (m_low_dcbz_hack && imm_addr >= 0x8000'0000 && imm_addr < 0x8000'8000)
        return;
      MOVI2R(addr_reg, imm_addr & ~31);
    }
    else if (is_imm_a || is_imm_b)
    {
      // Only one register is an immediate
      ARM64Reg base = is_imm_a ? gpr.R(b) : gpr.R(a);
      u32 imm_offset = is_imm_a ? gpr.GetImm(a) : gpr.GetImm(b);
      ADDI2R(addr_reg, base, imm_offset, addr_reg);
      emit_low_dcbz_hack(addr_reg);
      AND(addr_reg, addr_reg, LogicalImm(~31, GPRSize::B32));
    }
    else
    {
      // Both are registers
      ADD(addr_reg, gpr.R(a), gpr.R(b));
      emit_low_dcbz_hack(addr_reg);
      AND(addr_reg, addr_reg, LogicalImm(~31, GPRSize::B32));
    }
  }
  else
  {
    // RA isn't used, only RB
    if (gpr.IsImm(b))
    {
      u32 imm_addr = gpr.GetImm(b);
      if (m_low_dcbz_hack && imm_addr >= 0x8000'0000 && imm_addr < 0x8000'8000)
        return;
      MOVI2R(addr_reg, imm_addr & ~31);
    }
    else
    {
      emit_low_dcbz_hack(gpr.R(b));
      AND(addr_reg, gpr.R(b), LogicalImm(~31, GPRSize::B32));
    }
  }

  BitSet32 gprs_to_push = gpr.GetCallerSavedUsed();
  BitSet32 fprs_to_push = fpr.GetCallerSavedUsed();
  gprs_to_push[DecodeReg(ARM64Reg::W1)] = 0;
  if (!jo.fastmem)
    gprs_to_push[DecodeReg(ARM64Reg::W0)] = 0;

  EmitBackpatchRoutine(inst, BackPatchInfo::FLAG_ZERO_256, MemAccessMode::Auto, ARM64Reg::W1,
                       EncodeRegTo64(addr_reg), gprs_to_push, fprs_to_push);

  if (using_dcbz_hack)
    SetJumpTarget(end_dcbz_hack);
}

void JitArm64::eieio(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITLoadStoreOff);

  // optimizeGatherPipe generally postpones FIFO checks to the end of the JIT block,
  // which is generally safe. However postponing FIFO writes across eieio instructions
  // is incorrect (would crash NBA2K11 strap screen if we improve our FIFO detection).
  if (jo.optimizeGatherPipe && js.fifoBytesSinceCheck > 0)
    js.mustCheckFifo = true;
}
