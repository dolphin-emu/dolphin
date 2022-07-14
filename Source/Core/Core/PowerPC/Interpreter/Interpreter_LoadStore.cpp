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

static u32 Helper_Get_EA(const PowerPC::PowerPCState& ppcs, const GeckoInstruction inst)
{
  return inst.RA() ? (ppcs.gpr[inst.RA()] + u32(inst.SIMM_16())) : u32(inst.SIMM_16());
}

static u32 Helper_Get_EA_U(const PowerPC::PowerPCState& ppcs, const GeckoInstruction inst)
{
  return (ppcs.gpr[inst.RA()] + u32(inst.SIMM_16()));
}

static u32 Helper_Get_EA_X(const PowerPC::PowerPCState& ppcs, const GeckoInstruction inst)
{
  return inst.RA() ? (ppcs.gpr[inst.RA()] + ppcs.gpr[inst.RB()]) : ppcs.gpr[inst.RB()];
}

static u32 Helper_Get_EA_UX(const PowerPC::PowerPCState& ppcs, const GeckoInstruction inst)
{
  return (ppcs.gpr[inst.RA()] + ppcs.gpr[inst.RB()]);
}

void Interpreter::lbz(GeckoInstruction inst)
{
  const u32 temp = PowerPC::Read_U8(Helper_Get_EA(PowerPC::ppcState, inst));

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
    rGPR[inst.RD()] = temp;
}

void Interpreter::lbzu(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_U(PowerPC::ppcState, inst);
  const u32 temp = PowerPC::Read_U8(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD()] = temp;
    rGPR[inst.RA()] = address;
  }
}

void Interpreter::lfd(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  const u64 temp = PowerPC::Read_U64(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
    rPS(inst.FD()).SetPS0(temp);
}

void Interpreter::lfdu(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_U(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  const u64 temp = PowerPC::Read_U64(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rPS(inst.FD()).SetPS0(temp);
    rGPR[inst.RA()] = address;
  }
}

void Interpreter::lfdux(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_UX(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  const u64 temp = PowerPC::Read_U64(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rPS(inst.FD()).SetPS0(temp);
    rGPR[inst.RA()] = address;
  }
}

void Interpreter::lfdx(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  const u64 temp = PowerPC::Read_U64(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
    rPS(inst.FD()).SetPS0(temp);
}

void Interpreter::lfs(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  const u32 temp = PowerPC::Read_U32(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    const u64 value = ConvertToDouble(temp);
    rPS(inst.FD()).Fill(value);
  }
}

void Interpreter::lfsu(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_U(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  const u32 temp = PowerPC::Read_U32(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    const u64 value = ConvertToDouble(temp);
    rPS(inst.FD()).Fill(value);
    rGPR[inst.RA()] = address;
  }
}

void Interpreter::lfsux(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_UX(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  const u32 temp = PowerPC::Read_U32(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    const u64 value = ConvertToDouble(temp);
    rPS(inst.FD()).Fill(value);
    rGPR[inst.RA()] = address;
  }
}

void Interpreter::lfsx(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  const u32 temp = PowerPC::Read_U32(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    const u64 value = ConvertToDouble(temp);
    rPS(inst.FD()).Fill(value);
  }
}

void Interpreter::lha(GeckoInstruction inst)
{
  const u32 temp = u32(s32(s16(PowerPC::Read_U16(Helper_Get_EA(PowerPC::ppcState, inst)))));

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD()] = temp;
  }
}

void Interpreter::lhau(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_U(PowerPC::ppcState, inst);
  const u32 temp = u32(s32(s16(PowerPC::Read_U16(address))));

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD()] = temp;
    rGPR[inst.RA()] = address;
  }
}

void Interpreter::lhz(GeckoInstruction inst)
{
  const u32 temp = PowerPC::Read_U16(Helper_Get_EA(PowerPC::ppcState, inst));

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD()] = temp;
  }
}

void Interpreter::lhzu(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_U(PowerPC::ppcState, inst);
  const u32 temp = PowerPC::Read_U16(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD()] = temp;
    rGPR[inst.RA()] = address;
  }
}

// FIXME: lmw should do a total rollback if a DSI occurs
void Interpreter::lmw(GeckoInstruction inst)
{
  u32 address = Helper_Get_EA(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0 || MSR.LE())
  {
    GenerateAlignmentException(address);
    return;
  }

  for (u32 i = inst.RD(); i <= 31; i++, address += 4)
  {
    const u32 temp_reg = PowerPC::Read_U32(address);

    if ((PowerPC::ppcState.Exceptions & EXCEPTION_DSI) != 0)
    {
      PanicAlertFmt("DSI exception in lmw");
      NOTICE_LOG_FMT(POWERPC, "DSI exception in lmw");
      return;
    }
    else
    {
      rGPR[i] = temp_reg;
    }
  }
}

// FIXME: stmw should do a total rollback if a DSI occurs
void Interpreter::stmw(GeckoInstruction inst)
{
  u32 address = Helper_Get_EA(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0 || MSR.LE())
  {
    GenerateAlignmentException(address);
    return;
  }

  for (u32 i = inst.RS(); i <= 31; i++, address += 4)
  {
    PowerPC::Write_U32(rGPR[i], address);
    if ((PowerPC::ppcState.Exceptions & EXCEPTION_DSI) != 0)
    {
      PanicAlertFmt("DSI exception in stmw");
      NOTICE_LOG_FMT(POWERPC, "DSI exception in stmw");
      return;
    }
  }
}

void Interpreter::lwz(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA(PowerPC::ppcState, inst);
  const u32 temp = PowerPC::Read_U32(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD()] = temp;
  }
}

void Interpreter::lwzu(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_U(PowerPC::ppcState, inst);
  const u32 temp = PowerPC::Read_U32(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD()] = temp;
    rGPR[inst.RA()] = address;
  }
}

void Interpreter::stb(GeckoInstruction inst)
{
  PowerPC::Write_U8(rGPR[inst.RS()], Helper_Get_EA(PowerPC::ppcState, inst));
}

void Interpreter::stbu(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_U(PowerPC::ppcState, inst);

  PowerPC::Write_U8(rGPR[inst.RS()], address);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA()] = address;
  }
}

void Interpreter::stfd(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  PowerPC::Write_U64(rPS(inst.FS()).PS0AsU64(), address);
}

void Interpreter::stfdu(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_U(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  PowerPC::Write_U64(rPS(inst.FS()).PS0AsU64(), address);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA()] = address;
  }
}

void Interpreter::stfs(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  PowerPC::Write_U32(ConvertToSingle(rPS(inst.FS()).PS0AsU64()), address);
}

void Interpreter::stfsu(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_U(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  PowerPC::Write_U32(ConvertToSingle(rPS(inst.FS()).PS0AsU64()), address);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA()] = address;
  }
}

void Interpreter::sth(GeckoInstruction inst)
{
  PowerPC::Write_U16(rGPR[inst.RS()], Helper_Get_EA(PowerPC::ppcState, inst));
}

void Interpreter::sthu(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_U(PowerPC::ppcState, inst);

  PowerPC::Write_U16(rGPR[inst.RS()], address);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA()] = address;
  }
}

void Interpreter::stw(GeckoInstruction inst)
{
  PowerPC::Write_U32(rGPR[inst.RS()], Helper_Get_EA(PowerPC::ppcState, inst));
}

void Interpreter::stwu(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_U(PowerPC::ppcState, inst);

  PowerPC::Write_U32(rGPR[inst.RS()], address);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA()] = address;
  }
}

void Interpreter::dcba(GeckoInstruction inst)
{
  ASSERT_MSG(POWERPC, 0, "dcba - Not implemented - not a Gekko instruction");
}

void Interpreter::dcbf(GeckoInstruction inst)
{
  // TODO: Implement some sort of L2 emulation.
  // TODO: Raise DSI if translation fails (except for direct-store segments).

  // Invalidate the JIT cache here as a heuristic to compensate for
  // the lack of precise L1 icache emulation in the JIT. (Portable software
  // should use icbi consistently, but games aren't portable.)
  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);
  JitInterface::InvalidateICacheLine(address);
}

void Interpreter::dcbi(GeckoInstruction inst)
{
  if (MSR.PR())
  {
    GenerateProgramException(ProgramExceptionCause::PrivilegedInstruction);
    return;
  }

  // TODO: Implement some sort of L2 emulation.
  // TODO: Raise DSI if translation fails (except for direct-store segments).

  // Invalidate the JIT cache here as a heuristic to compensate for
  // the lack of precise L1 icache emulation in the JIT. (Portable software
  // should use icbi consistently, but games aren't portable.)
  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);
  JitInterface::InvalidateICacheLine(address);
}

void Interpreter::dcbst(GeckoInstruction inst)
{
  // TODO: Implement some sort of L2 emulation.
  // TODO: Raise DSI if translation fails (except for direct-store segments).

  // Invalidate the JIT cache here as a heuristic to compensate for
  // the lack of precise L1 icache emulation in the JIT. (Portable software
  // should use icbi consistently, but games aren't portable.)
  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);
  JitInterface::InvalidateICacheLine(address);
}

void Interpreter::dcbt(GeckoInstruction inst)
{
  if (HID0.NOOPTI())
    return;

  // TODO: Implement some sort of L2 emulation.
}

void Interpreter::dcbtst(GeckoInstruction inst)
{
  if (HID0.NOOPTI())
    return;

  // TODO: Implement some sort of L2 emulation.
}

void Interpreter::dcbz(GeckoInstruction inst)
{
  const u32 dcbz_addr = Helper_Get_EA_X(PowerPC::ppcState, inst);

  if (!HID0.DCE())
  {
    GenerateAlignmentException(dcbz_addr);
    return;
  }

  // Hack to stop dcbz/dcbi over low MEM1 trashing memory.
  if ((dcbz_addr < 0x80008000) && (dcbz_addr >= 0x80000000) &&
      Config::Get(Config::MAIN_LOW_DCBZ_HACK))
  {
    return;
  }

  // TODO: Implement some sort of L2 emulation.
  PowerPC::ClearCacheLine(dcbz_addr & (~31));
}

void Interpreter::dcbz_l(GeckoInstruction inst)
{
  if (!HID2.LCE())
  {
    GenerateProgramException(ProgramExceptionCause::IllegalInstruction);
    return;
  }

  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);

  if (!HID0.DCE())
  {
    GenerateAlignmentException(address);
    return;
  }

  // FAKE: clear memory instead of clearing the cache block
  PowerPC::ClearCacheLine(address & (~31));
}

// eciwx/ecowx technically should access the specified device
// We just do it instantly from ppc...and hey, it works! :D
void Interpreter::eciwx(GeckoInstruction inst)
{
  const u32 EA = Helper_Get_EA_X(PowerPC::ppcState, inst);

  if ((PowerPC::ppcState.spr[SPR_EAR] & 0x80000000) == 0)
  {
    GenerateDSIException(EA);
    return;
  }

  if ((EA & 0b11) != 0)
  {
    GenerateAlignmentException(EA);
    return;
  }

  rGPR[inst.RD()] = PowerPC::Read_U32(EA);
}

void Interpreter::ecowx(GeckoInstruction inst)
{
  const u32 EA = Helper_Get_EA_X(PowerPC::ppcState, inst);

  if ((PowerPC::ppcState.spr[SPR_EAR] & 0x80000000) == 0)
  {
    GenerateDSIException(EA);
    return;
  }

  if ((EA & 0b11) != 0)
  {
    GenerateAlignmentException(EA);
    return;
  }

  PowerPC::Write_U32(rGPR[inst.RS()], EA);
}

void Interpreter::eieio(GeckoInstruction inst)
{
  // Basically ensures that loads/stores before this instruction
  // have completed (in order) before executing the next op.
  // Prevents real ppc from "smartly" reordering loads/stores
  // But (at least in interpreter) we do everything realtime anyways.
}

void Interpreter::icbi(GeckoInstruction inst)
{
  // TODO: Raise DSI if translation fails (except for direct-store segments).
  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);
  PowerPC::ppcState.iCache.Invalidate(address);
}

void Interpreter::lbzux(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_UX(PowerPC::ppcState, inst);
  const u32 temp = PowerPC::Read_U8(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD()] = temp;
    rGPR[inst.RA()] = address;
  }
}

void Interpreter::lbzx(GeckoInstruction inst)
{
  const u32 temp = PowerPC::Read_U8(Helper_Get_EA_X(PowerPC::ppcState, inst));

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD()] = temp;
  }
}

void Interpreter::lhaux(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_UX(PowerPC::ppcState, inst);
  const s32 temp = s32{s16(PowerPC::Read_U16(address))};

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD()] = u32(temp);
    rGPR[inst.RA()] = address;
  }
}

void Interpreter::lhax(GeckoInstruction inst)
{
  const s32 temp = s32{s16(PowerPC::Read_U16(Helper_Get_EA_X(PowerPC::ppcState, inst)))};

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD()] = u32(temp);
  }
}

void Interpreter::lhbrx(GeckoInstruction inst)
{
  const u32 temp = Common::swap16(PowerPC::Read_U16(Helper_Get_EA_X(PowerPC::ppcState, inst)));

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD()] = temp;
  }
}

void Interpreter::lhzux(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_UX(PowerPC::ppcState, inst);
  const u32 temp = PowerPC::Read_U16(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD()] = temp;
    rGPR[inst.RA()] = address;
  }
}

void Interpreter::lhzx(GeckoInstruction inst)
{
  const u32 temp = PowerPC::Read_U16(Helper_Get_EA_X(PowerPC::ppcState, inst));

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD()] = temp;
  }
}

// FIXME: Should rollback if a DSI occurs
void Interpreter::lswx(GeckoInstruction inst)
{
  u32 EA = Helper_Get_EA_X(PowerPC::ppcState, inst);

  if (MSR.LE())
  {
    GenerateAlignmentException(EA);
    return;
  }

  // Confirmed by hardware test that the zero case doesn't zero rGPR[r]
  for (u32 n = 0; n < static_cast<u8>(PowerPC::ppcState.xer_stringctrl); n++)
  {
    const u32 reg = (inst.RD() + (n >> 2)) & 0x1f;
    const u32 offset = (n & 3) << 3;

    if ((n & 0b11) == 0)
      rGPR[reg] = 0;

    const u32 temp_value = PowerPC::Read_U8(EA) << (24 - offset);
    // Not64 (Homebrew N64 Emulator for Wii) triggers the following case.
    if ((PowerPC::ppcState.Exceptions & EXCEPTION_DSI) != 0)
    {
      NOTICE_LOG_FMT(POWERPC, "DSI exception in lswx");
      return;
    }
    rGPR[reg] |= temp_value;

    EA++;
  }
}

void Interpreter::lwbrx(GeckoInstruction inst)
{
  const u32 temp = Common::swap32(PowerPC::Read_U32(Helper_Get_EA_X(PowerPC::ppcState, inst)));

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD()] = temp;
  }
}

void Interpreter::lwzux(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_UX(PowerPC::ppcState, inst);
  const u32 temp = PowerPC::Read_U32(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD()] = temp;
    rGPR[inst.RA()] = address;
  }
}

void Interpreter::lwzx(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);
  const u32 temp = PowerPC::Read_U32(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD()] = temp;
  }
}

void Interpreter::stbux(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_UX(PowerPC::ppcState, inst);

  PowerPC::Write_U8(rGPR[inst.RS()], address);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA()] = address;
  }
}

void Interpreter::stbx(GeckoInstruction inst)
{
  PowerPC::Write_U8(rGPR[inst.RS()], Helper_Get_EA_X(PowerPC::ppcState, inst));
}

void Interpreter::stfdux(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_UX(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  PowerPC::Write_U64(rPS(inst.FS()).PS0AsU64(), address);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA()] = address;
  }
}

void Interpreter::stfdx(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  PowerPC::Write_U64(rPS(inst.FS()).PS0AsU64(), address);
}

// Stores Floating points into Integers indeXed
void Interpreter::stfiwx(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  PowerPC::Write_U32(rPS(inst.FS()).PS0AsU32(), address);
}

void Interpreter::stfsux(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_UX(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  PowerPC::Write_U32(ConvertToSingle(rPS(inst.FS()).PS0AsU64()), address);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA()] = address;
  }
}

void Interpreter::stfsx(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  PowerPC::Write_U32(ConvertToSingle(rPS(inst.FS()).PS0AsU64()), address);
}

void Interpreter::sthbrx(GeckoInstruction inst)
{
  PowerPC::Write_U16_Swap(rGPR[inst.RS()], Helper_Get_EA_X(PowerPC::ppcState, inst));
}

void Interpreter::sthux(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_UX(PowerPC::ppcState, inst);

  PowerPC::Write_U16(rGPR[inst.RS()], address);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA()] = address;
  }
}

void Interpreter::sthx(GeckoInstruction inst)
{
  PowerPC::Write_U16(rGPR[inst.RS()], Helper_Get_EA_X(PowerPC::ppcState, inst));
}

// lswi - bizarro string instruction
// FIXME: Should rollback if a DSI occurs
void Interpreter::lswi(GeckoInstruction inst)
{
  u32 EA = 0;
  if (inst.RA() != 0)
    EA = rGPR[inst.RA()];

  if (MSR.LE())
  {
    GenerateAlignmentException(EA);
    return;
  }

  u32 n = 32;
  if (inst.NB() != 0)
    n = inst.NB();

  u32 r = u32{inst.RD()} - 1;
  u32 i = 0;
  while (n > 0)
  {
    if (i == 0)
    {
      r++;
      r &= 31;
      rGPR[r] = 0;
    }

    const u32 temp_value = PowerPC::Read_U8(EA) << (24 - i);
    if ((PowerPC::ppcState.Exceptions & EXCEPTION_DSI) != 0)
    {
      PanicAlertFmt("DSI exception in lsw.");
      return;
    }

    rGPR[r] |= temp_value;

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
void Interpreter::stswi(GeckoInstruction inst)
{
  u32 EA = 0;
  if (inst.RA() != 0)
    EA = rGPR[inst.RA()];

  if (MSR.LE())
  {
    GenerateAlignmentException(EA);
    return;
  }

  u32 n = 32;
  if (inst.NB() != 0)
    n = inst.NB();

  u32 r = u32{inst.RS()} - 1;
  u32 i = 0;
  while (n > 0)
  {
    if (i == 0)
    {
      r++;
      r &= 31;
    }
    PowerPC::Write_U8((rGPR[r] >> (24 - i)) & 0xFF, EA);
    if ((PowerPC::ppcState.Exceptions & EXCEPTION_DSI) != 0)
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
void Interpreter::stswx(GeckoInstruction inst)
{
  u32 EA = Helper_Get_EA_X(PowerPC::ppcState, inst);

  if (MSR.LE())
  {
    GenerateAlignmentException(EA);
    return;
  }

  u32 n = u8(PowerPC::ppcState.xer_stringctrl);
  u32 r = inst.RS();
  u32 i = 0;

  while (n > 0)
  {
    PowerPC::Write_U8((rGPR[r] >> (24 - i)) & 0xFF, EA);

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

void Interpreter::stwbrx(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);

  PowerPC::Write_U32_Swap(rGPR[inst.RS()], address);
}

// The following two instructions are for SMP communications. On a single
// CPU, they cannot fail unless an interrupt happens in between.

void Interpreter::lwarx(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  const u32 temp = PowerPC::Read_U32(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD()] = temp;
    PowerPC::ppcState.reserve = true;
    PowerPC::ppcState.reserve_address = address;
  }
}

// Stores Word Conditional indeXed
void Interpreter::stwcxd(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  if (PowerPC::ppcState.reserve)
  {
    if (address == PowerPC::ppcState.reserve_address)
    {
      PowerPC::Write_U32(rGPR[inst.RS()], address);
      if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
      {
        PowerPC::ppcState.reserve = false;
        PowerPC::ppcState.cr.SetField(0, 2 | PowerPC::GetXER_SO());
        return;
      }
    }
  }

  PowerPC::ppcState.cr.SetField(0, PowerPC::GetXER_SO());
}

void Interpreter::stwux(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_UX(PowerPC::ppcState, inst);

  PowerPC::Write_U32(rGPR[inst.RS()], address);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA()] = address;
  }
}

void Interpreter::stwx(GeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);

  PowerPC::Write_U32(rGPR[inst.RS()], address);
}

void Interpreter::sync(GeckoInstruction inst)
{
  // ignored
}

void Interpreter::tlbie(GeckoInstruction inst)
{
  if (MSR.PR())
  {
    GenerateProgramException(ProgramExceptionCause::PrivilegedInstruction);
    return;
  }

  // Invalidate TLB entry
  const u32 address = rGPR[inst.RB()];

  PowerPC::InvalidateTLBEntry(address);
}

void Interpreter::tlbsync(GeckoInstruction inst)
{
  if (MSR.PR())
  {
    GenerateProgramException(ProgramExceptionCause::PrivilegedInstruction);
  }

  // Ignored
}
