// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Interpreter/Interpreter.h"

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/Swap.h"

#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/PowerPC/Interpreter/ExceptionUtils.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

static u32 Helper_Get_EA(const PowerPC::PowerPCState& ppcs, const UGeckoInstruction inst)
{
  return inst.RA ? (ppcs.gpr[inst.RA] + u32(inst.SIMM_16)) : u32(inst.SIMM_16);
}

static u32 Helper_Get_EA_U(const PowerPC::PowerPCState& ppcs, const UGeckoInstruction inst)
{
  return (ppcs.gpr[inst.RA] + u32(inst.SIMM_16));
}

static u32 Helper_Get_EA_X(const PowerPC::PowerPCState& ppcs, const UGeckoInstruction inst)
{
  return inst.RA ? (ppcs.gpr[inst.RA] + ppcs.gpr[inst.RB]) : ppcs.gpr[inst.RB];
}

static u32 Helper_Get_EA_UX(const PowerPC::PowerPCState& ppcs, const UGeckoInstruction inst)
{
  return (ppcs.gpr[inst.RA] + ppcs.gpr[inst.RB]);
}

void Interpreter::lbz(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 temp = interpreter.m_mmu.Read_U8(Helper_Get_EA(ppc_state, inst), inst);

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
    ppc_state.gpr[inst.RD] = temp;
}

void Interpreter::lbzu(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_U(ppc_state, inst);
  const u32 temp = interpreter.m_mmu.Read_U8(address, inst);

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RD] = temp;
    ppc_state.gpr[inst.RA] = address;
  }
}

void Interpreter::lfd(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA(ppc_state, inst);
  const u64 temp = interpreter.m_mmu.Read_U64(address, inst);

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
    ppc_state.ps[inst.FD].SetPS0(temp);
}

void Interpreter::lfdu(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_U(ppc_state, inst);
  const u64 temp = interpreter.m_mmu.Read_U64(address, inst);

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.ps[inst.FD].SetPS0(temp);
    ppc_state.gpr[inst.RA] = address;
  }
}

void Interpreter::lfdux(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_UX(ppc_state, inst);
  const u64 temp = interpreter.m_mmu.Read_U64(address, inst);

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.ps[inst.FD].SetPS0(temp);
    ppc_state.gpr[inst.RA] = address;
  }
}

void Interpreter::lfdx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_X(ppc_state, inst);
  const u64 temp = interpreter.m_mmu.Read_U64(address, inst);

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
    ppc_state.ps[inst.FD].SetPS0(temp);
}

void Interpreter::lfs(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA(ppc_state, inst);
  const u32 temp = interpreter.m_mmu.Read_U32(address, inst);

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    const u64 value = ConvertToDouble(temp);
    ppc_state.ps[inst.FD].Fill(value);
  }
}

void Interpreter::lfsu(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_U(ppc_state, inst);
  const u32 temp = interpreter.m_mmu.Read_U32(address, inst);

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    const u64 value = ConvertToDouble(temp);
    ppc_state.ps[inst.FD].Fill(value);
    ppc_state.gpr[inst.RA] = address;
  }
}

void Interpreter::lfsux(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_UX(ppc_state, inst);
  const u32 temp = interpreter.m_mmu.Read_U32(address, inst);

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    const u64 value = ConvertToDouble(temp);
    ppc_state.ps[inst.FD].Fill(value);
    ppc_state.gpr[inst.RA] = address;
  }
}

void Interpreter::lfsx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_X(ppc_state, inst);
  const u32 temp = interpreter.m_mmu.Read_U32(address, inst);

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    const u64 value = ConvertToDouble(temp);
    ppc_state.ps[inst.FD].Fill(value);
  }
}

void Interpreter::lha(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 temp = u32(s32(s16(interpreter.m_mmu.Read_U16(Helper_Get_EA(ppc_state, inst), inst))));

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RD] = temp;
  }
}

void Interpreter::lhau(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_U(ppc_state, inst);
  const u32 temp = u32(s32(s16(interpreter.m_mmu.Read_U16(address, inst))));

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RD] = temp;
    ppc_state.gpr[inst.RA] = address;
  }
}

void Interpreter::lhz(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 temp = interpreter.m_mmu.Read_U16(Helper_Get_EA(ppc_state, inst), inst);

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RD] = temp;
  }
}

void Interpreter::lhzu(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_U(ppc_state, inst);
  const u32 temp = interpreter.m_mmu.Read_U16(address, inst);

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RD] = temp;
    ppc_state.gpr[inst.RA] = address;
  }
}

// FIXME: lmw should do a total rollback if a DSI occurs
void Interpreter::lmw(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  u32 address = Helper_Get_EA(ppc_state, inst);

  if (ppc_state.msr.LE)
  {
    GenerateAlignmentException(ppc_state, address, inst);
    return;
  }

  for (u32 i = inst.RD; i <= 31; i++, address += 4)
  {
    const u32 temp_reg = interpreter.m_mmu.Read_U32(address, inst);

    if ((ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION) != 0)
    {
      if (!(ppc_state.Exceptions & EXCEPTION_ALIGNMENT))
      {
        PanicAlertFmt("DSI exception in lmw");
        NOTICE_LOG_FMT(POWERPC, "DSI exception in lmw");
      }
      return;
    }
    else
    {
      ppc_state.gpr[i] = temp_reg;
    }
  }
}

// FIXME: stmw should do a total rollback if a DSI occurs
void Interpreter::stmw(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  u32 address = Helper_Get_EA(ppc_state, inst);

  if (ppc_state.msr.LE)
  {
    GenerateAlignmentException(ppc_state, address, inst);
    return;
  }

  for (u32 i = inst.RS; i <= 31; i++, address += 4)
  {
    interpreter.m_mmu.Write_U32(ppc_state.gpr[i], address, inst);
    if ((ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION) != 0)
    {
      PanicAlertFmt("DSI exception in stmw");
      NOTICE_LOG_FMT(POWERPC, "DSI exception in stmw");
      return;
    }
  }
}

void Interpreter::lwz(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA(ppc_state, inst);
  const u32 temp = interpreter.m_mmu.Read_U32(address, inst);

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RD] = temp;
  }
}

void Interpreter::lwzu(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_U(ppc_state, inst);
  const u32 temp = interpreter.m_mmu.Read_U32(address, inst);

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RD] = temp;
    ppc_state.gpr[inst.RA] = address;
  }
}

void Interpreter::stb(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  interpreter.m_mmu.Write_U8(ppc_state.gpr[inst.RS], Helper_Get_EA(ppc_state, inst), inst);
}

void Interpreter::stbu(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_U(ppc_state, inst);

  interpreter.m_mmu.Write_U8(ppc_state.gpr[inst.RS], address, inst);
  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RA] = address;
  }
}

void Interpreter::stfd(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA(ppc_state, inst);

  interpreter.m_mmu.Write_U64(ppc_state.ps[inst.FS].PS0AsU64(), address, inst);
}

void Interpreter::stfdu(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_U(ppc_state, inst);

  interpreter.m_mmu.Write_U64(ppc_state.ps[inst.FS].PS0AsU64(), address, inst);
  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RA] = address;
  }
}

void Interpreter::stfs(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA(ppc_state, inst);

  interpreter.m_mmu.Write_U32(ConvertToSingle(ppc_state.ps[inst.FS].PS0AsU64()), address, inst);
}

void Interpreter::stfsu(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_U(ppc_state, inst);

  interpreter.m_mmu.Write_U32(ConvertToSingle(ppc_state.ps[inst.FS].PS0AsU64()), address, inst);
  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RA] = address;
  }
}

void Interpreter::sth(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  interpreter.m_mmu.Write_U16(ppc_state.gpr[inst.RS], Helper_Get_EA(ppc_state, inst), inst);
}

void Interpreter::sthu(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_U(ppc_state, inst);

  interpreter.m_mmu.Write_U16(ppc_state.gpr[inst.RS], address, inst);
  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RA] = address;
  }
}

void Interpreter::stw(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  interpreter.m_mmu.Write_U32(ppc_state.gpr[inst.RS], Helper_Get_EA(ppc_state, inst), inst);
}

void Interpreter::stwu(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_U(ppc_state, inst);

  interpreter.m_mmu.Write_U32(ppc_state.gpr[inst.RS], address, inst);
  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RA] = address;
  }
}

void Interpreter::dcba(Interpreter& interpreter, UGeckoInstruction inst)
{
  ASSERT_MSG(POWERPC, 0, "dcba - Not implemented - not a Gekko instruction");
}

void Interpreter::dcbf(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_X(ppc_state, inst);
  if (!ppc_state.m_enable_dcache)
  {
    // Invalidate the JIT cache here as a heuristic to compensate for
    // the lack of precise L1 icache emulation in the JIT. (Portable software
    // should use icbi consistently, but games aren't portable.)
    interpreter.m_system.GetJitInterface().InvalidateICacheLine(address);
    return;
  }

  interpreter.m_mmu.FlushDCacheLine(address);
}

void Interpreter::dcbi(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  if (ppc_state.msr.PR)
  {
    GenerateProgramException(ppc_state, ProgramExceptionCause::PrivilegedInstruction);
    return;
  }

  const u32 address = Helper_Get_EA_X(ppc_state, inst);
  if (!ppc_state.m_enable_dcache)
  {
    // Invalidate the JIT cache here as a heuristic to compensate for
    // the lack of precise L1 icache emulation in the JIT. (Portable software
    // should use icbi consistently, but games aren't portable.)
    interpreter.m_system.GetJitInterface().InvalidateICacheLine(address);
    return;
  }

  interpreter.m_mmu.InvalidateDCacheLine(address);
}

void Interpreter::dcbst(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_X(ppc_state, inst);
  if (!ppc_state.m_enable_dcache)
  {
    // Invalidate the JIT cache here as a heuristic to compensate for
    // the lack of precise L1 icache emulation in the JIT. (Portable software
    // should use icbi consistently, but games aren't portable.)
    interpreter.m_system.GetJitInterface().InvalidateICacheLine(address);
    return;
  }

  interpreter.m_mmu.StoreDCacheLine(address);
}

// These instructions hint that it might be optimal to prefetch the specified cache line into the
// data cache. But the CPU is never guaranteed to do this fetch, and in practice it's not more
// performant to emulate it.

void Interpreter::dcbt(Interpreter& interpreter, UGeckoInstruction inst)
{
}

void Interpreter::dcbtst(Interpreter& interpreter, UGeckoInstruction inst)
{
}

void Interpreter::dcbz(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 dcbz_addr = Helper_Get_EA_X(ppc_state, inst);

  if (!HID0(ppc_state).DCE)
  {
    GenerateAlignmentException(ppc_state, dcbz_addr, inst);
    return;
  }

  if (!ppc_state.m_enable_dcache)
  {
    // Hack to stop dcbz/dcbi over low MEM1 trashing memory. This is not needed if data cache
    // emulation is enabled.
    if ((dcbz_addr < 0x80008000) && (dcbz_addr >= 0x80000000) &&
        Config::Get(Config::MAIN_LOW_DCBZ_HACK))
    {
      return;
    }
  }

  interpreter.m_mmu.ClearDCacheLine(dcbz_addr & (~31), inst);
}

void Interpreter::dcbz_l(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  if (!HID2(ppc_state).LCE)
  {
    GenerateProgramException(ppc_state, ProgramExceptionCause::IllegalInstruction);
    return;
  }

  const u32 address = Helper_Get_EA_X(ppc_state, inst);

  if (!HID0(ppc_state).DCE)
  {
    GenerateAlignmentException(ppc_state, address, inst);
    return;
  }

  interpreter.m_mmu.ClearDCacheLine(address & (~31), inst);
}

// eciwx/ecowx technically should access the specified device
// We just do it instantly from ppc...and hey, it works! :D
void Interpreter::eciwx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 EA = Helper_Get_EA_X(ppc_state, inst);

  if ((ppc_state.spr[SPR_EAR] & 0x80000000) == 0)
  {
    GenerateDSIException(ppc_state, EA);
    return;
  }

  const u32 temp = interpreter.m_mmu.Read_U32(EA, inst);
  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RD] = temp;
  }
}

void Interpreter::ecowx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 EA = Helper_Get_EA_X(ppc_state, inst);

  if ((ppc_state.spr[SPR_EAR] & 0x80000000) == 0)
  {
    GenerateDSIException(ppc_state, EA);
    return;
  }

  interpreter.m_mmu.Write_U32(ppc_state.gpr[inst.RS], EA, inst);
}

void Interpreter::eieio(Interpreter& interpreter, UGeckoInstruction inst)
{
  // Basically ensures that loads/stores before this instruction
  // have completed (in order) before executing the next op.
  // Prevents real ppc from "smartly" reordering loads/stores
  // But (at least in interpreter) we do everything realtime anyways.
}

void Interpreter::icbi(Interpreter& interpreter, UGeckoInstruction inst)
{
  // TODO: Raise DSI if translation fails (except for direct-store segments).
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_X(ppc_state, inst);
  auto& memory = interpreter.m_system.GetMemory();
  auto& jit_interface = interpreter.m_system.GetJitInterface();
  ppc_state.iCache.Invalidate(memory, jit_interface, address);
}

void Interpreter::lbzux(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_UX(ppc_state, inst);
  const u32 temp = interpreter.m_mmu.Read_U8(address, inst);

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RD] = temp;
    ppc_state.gpr[inst.RA] = address;
  }
}

void Interpreter::lbzx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 temp = interpreter.m_mmu.Read_U8(Helper_Get_EA_X(ppc_state, inst), inst);

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RD] = temp;
  }
}

void Interpreter::lhaux(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_UX(ppc_state, inst);
  const s32 temp = s32{s16(interpreter.m_mmu.Read_U16(address, inst))};

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RD] = u32(temp);
    ppc_state.gpr[inst.RA] = address;
  }
}

void Interpreter::lhax(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const s32 temp = s32{s16(interpreter.m_mmu.Read_U16(Helper_Get_EA_X(ppc_state, inst), inst))};

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RD] = u32(temp);
  }
}

void Interpreter::lhbrx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 temp =
      Common::swap16(interpreter.m_mmu.Read_U16(Helper_Get_EA_X(ppc_state, inst), inst));

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RD] = temp;
  }
}

void Interpreter::lhzux(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_UX(ppc_state, inst);
  const u32 temp = interpreter.m_mmu.Read_U16(address, inst);

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RD] = temp;
    ppc_state.gpr[inst.RA] = address;
  }
}

void Interpreter::lhzx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 temp = interpreter.m_mmu.Read_U16(Helper_Get_EA_X(ppc_state, inst), inst);

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RD] = temp;
  }
}

// FIXME: Should rollback if a DSI occurs
// TODO: Should this be able to cause alignment exceptions?
void Interpreter::lswx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  u32 EA = Helper_Get_EA_X(ppc_state, inst);

  if (ppc_state.msr.LE)
  {
    GenerateAlignmentException(ppc_state, EA, inst);
    return;
  }

  // Confirmed by hardware test that the zero case doesn't zero gpr[r]
  for (u32 n = 0; n < static_cast<u8>(ppc_state.xer_stringctrl); n++)
  {
    const u32 reg = (inst.RD + (n >> 2)) & 0x1f;
    const u32 offset = (n & 3) << 3;

    if ((n & 0b11) == 0)
      ppc_state.gpr[reg] = 0;

    const u32 temp_value = interpreter.m_mmu.Read_U8(EA, inst) << (24 - offset);
    // Not64 (Homebrew N64 Emulator for Wii) triggers the following case.
    if ((ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION) != 0)
    {
      NOTICE_LOG_FMT(POWERPC, "DSI exception in lswx");
      return;
    }
    ppc_state.gpr[reg] |= temp_value;

    EA++;
  }
}

void Interpreter::lwbrx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 temp =
      Common::swap32(interpreter.m_mmu.Read_U32(Helper_Get_EA_X(ppc_state, inst), inst));

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RD] = temp;
  }
}

void Interpreter::lwzux(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_UX(ppc_state, inst);
  const u32 temp = interpreter.m_mmu.Read_U32(address, inst);

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RD] = temp;
    ppc_state.gpr[inst.RA] = address;
  }
}

void Interpreter::lwzx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_X(ppc_state, inst);
  const u32 temp = interpreter.m_mmu.Read_U32(address, inst);

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RD] = temp;
  }
}

void Interpreter::stbux(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_UX(ppc_state, inst);

  interpreter.m_mmu.Write_U8(ppc_state.gpr[inst.RS], address, inst);
  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RA] = address;
  }
}

void Interpreter::stbx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  interpreter.m_mmu.Write_U8(ppc_state.gpr[inst.RS], Helper_Get_EA_X(ppc_state, inst), inst);
}

void Interpreter::stfdux(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_UX(ppc_state, inst);

  interpreter.m_mmu.Write_U64(ppc_state.ps[inst.FS].PS0AsU64(), address, inst);
  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RA] = address;
  }
}

void Interpreter::stfdx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_X(ppc_state, inst);

  interpreter.m_mmu.Write_U64(ppc_state.ps[inst.FS].PS0AsU64(), address, inst);
}

// Stores Floating points into Integers indeXed
void Interpreter::stfiwx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_X(ppc_state, inst);

  interpreter.m_mmu.Write_U32(ppc_state.ps[inst.FS].PS0AsU32(), address, inst);
}

void Interpreter::stfsux(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_UX(ppc_state, inst);

  interpreter.m_mmu.Write_U32(ConvertToSingle(ppc_state.ps[inst.FS].PS0AsU64()), address, inst);
  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RA] = address;
  }
}

void Interpreter::stfsx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_X(ppc_state, inst);

  interpreter.m_mmu.Write_U32(ConvertToSingle(ppc_state.ps[inst.FS].PS0AsU64()), address, inst);
}

void Interpreter::sthbrx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  interpreter.m_mmu.Write_U16_Swap(ppc_state.gpr[inst.RS], Helper_Get_EA_X(ppc_state, inst), inst);
}

void Interpreter::sthux(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_UX(ppc_state, inst);

  interpreter.m_mmu.Write_U16(ppc_state.gpr[inst.RS], address, inst);
  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RA] = address;
  }
}

void Interpreter::sthx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  interpreter.m_mmu.Write_U16(ppc_state.gpr[inst.RS], Helper_Get_EA_X(ppc_state, inst), inst);
}

// lswi - bizarro string instruction
// FIXME: Should rollback if a DSI occurs
// TODO: Should this be able to cause alignment exceptions?
void Interpreter::lswi(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  u32 EA = 0;
  if (inst.RA != 0)
    EA = ppc_state.gpr[inst.RA];

  if (ppc_state.msr.LE)
  {
    GenerateAlignmentException(ppc_state, EA, inst);
    return;
  }

  u32 n = 32;
  if (inst.NB != 0)
    n = inst.NB;

  u32 r = u32{inst.RD} - 1;
  u32 i = 0;
  while (n > 0)
  {
    if (i == 0)
    {
      r++;
      r &= 31;
      ppc_state.gpr[r] = 0;
    }

    const u32 temp_value = interpreter.m_mmu.Read_U8(EA, inst) << (24 - i);
    if ((ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION) != 0)
    {
      PanicAlertFmt("DSI exception in lsw.");
      return;
    }

    ppc_state.gpr[r] |= temp_value;

    i += 8;
    if (i == 32)
      i = 0;
    EA++;
    n--;
  }
}

// todo : optimize ?
// stswi - bizarro string instruction
// FIXME: Should rollback if a DSI occurs
// TODO: Should this be able to cause alignment exceptions?
void Interpreter::stswi(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  u32 EA = 0;
  if (inst.RA != 0)
    EA = ppc_state.gpr[inst.RA];

  if (ppc_state.msr.LE)
  {
    GenerateAlignmentException(ppc_state, EA, inst);
    return;
  }

  u32 n = 32;
  if (inst.NB != 0)
    n = inst.NB;

  u32 r = u32{inst.RS} - 1;
  u32 i = 0;
  while (n > 0)
  {
    if (i == 0)
    {
      r++;
      r &= 31;
    }
    interpreter.m_mmu.Write_U8((ppc_state.gpr[r] >> (24 - i)) & 0xFF, EA, inst);
    if ((ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION) != 0)
    {
      return;
    }

    i += 8;
    if (i == 32)
      i = 0;
    EA++;
    n--;
  }
}

// TODO: is this right? is it DSI interruptible?
// TODO: Should this be able to cause alignment exceptions?
void Interpreter::stswx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  u32 EA = Helper_Get_EA_X(ppc_state, inst);

  if (ppc_state.msr.LE)
  {
    GenerateAlignmentException(ppc_state, EA, inst);
    return;
  }

  u32 n = u8(ppc_state.xer_stringctrl);
  u32 r = inst.RS;
  u32 i = 0;

  while (n > 0)
  {
    interpreter.m_mmu.Write_U8((ppc_state.gpr[r] >> (24 - i)) & 0xFF, EA, inst);

    EA++;
    n--;
    i += 8;
    if (i == 32)
    {
      i = 0;
      r = (r + 1) & 0x1f;  // wrap
    }
  }
}

void Interpreter::stwbrx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_X(ppc_state, inst);

  interpreter.m_mmu.Write_U32_Swap(ppc_state.gpr[inst.RS], address, inst);
}

// The following two instructions are for SMP communications. On a single
// CPU, they cannot fail unless an interrupt happens in between.

void Interpreter::lwarx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_X(ppc_state, inst);

  const u32 temp = interpreter.m_mmu.Read_U32(address, inst);

  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RD] = temp;
    ppc_state.reserve = true;
    ppc_state.reserve_address = address;
  }
}

// Stores Word Conditional indeXed
void Interpreter::stwcxd(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_X(ppc_state, inst);

  if (ppc_state.reserve)
  {
    if (address == ppc_state.reserve_address)
    {
      interpreter.m_mmu.Write_U32(ppc_state.gpr[inst.RS], address, inst);
      if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
      {
        ppc_state.reserve = false;
        ppc_state.cr.SetField(0, 2 | ppc_state.GetXER_SO());
        return;
      }
    }
  }

  ppc_state.cr.SetField(0, ppc_state.GetXER_SO());
}

void Interpreter::stwux(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_UX(ppc_state, inst);

  interpreter.m_mmu.Write_U32(ppc_state.gpr[inst.RS], address, inst);
  if (!(ppc_state.Exceptions & ANY_LOADSTORE_EXCEPTION))
  {
    ppc_state.gpr[inst.RA] = address;
  }
}

void Interpreter::stwx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 address = Helper_Get_EA_X(ppc_state, inst);

  interpreter.m_mmu.Write_U32(ppc_state.gpr[inst.RS], address, inst);
}

void Interpreter::sync(Interpreter& interpreter, UGeckoInstruction inst)
{
  // ignored
}

void Interpreter::tlbie(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  if (ppc_state.msr.PR)
  {
    GenerateProgramException(ppc_state, ProgramExceptionCause::PrivilegedInstruction);
    return;
  }

  // Invalidate TLB entry
  const u32 address = ppc_state.gpr[inst.RB];

  interpreter.m_mmu.InvalidateTLBEntry(address);
}

void Interpreter::tlbsync(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  if (ppc_state.msr.PR)
  {
    GenerateProgramException(ppc_state, ProgramExceptionCause::PrivilegedInstruction);
  }

  // Ignored
}
