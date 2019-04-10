// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/Swap.h"

#include "Core/ConfigManager.h"
#include "Core/PowerPC/Interpreter/ExceptionUtils.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"

bool Interpreter::m_reserve;
u32 Interpreter::m_reserve_address;

static u32 Helper_Get_EA(const PowerPC::PowerPCState& ppcs, const UGeckoInstruction inst)
{
  return inst.RA ? (ppcs.gpr[inst.RA] + inst.SIMM_16) : (u32)inst.SIMM_16;
}

static u32 Helper_Get_EA_U(const PowerPC::PowerPCState& ppcs, const UGeckoInstruction inst)
{
  return (ppcs.gpr[inst.RA] + inst.SIMM_16);
}

static u32 Helper_Get_EA_X(const PowerPC::PowerPCState& ppcs, const UGeckoInstruction inst)
{
  return inst.RA ? (ppcs.gpr[inst.RA] + ppcs.gpr[inst.RB]) : ppcs.gpr[inst.RB];
}

static u32 Helper_Get_EA_UX(const PowerPC::PowerPCState& ppcs, const UGeckoInstruction inst)
{
  return (ppcs.gpr[inst.RA] + ppcs.gpr[inst.RB]);
}

void Interpreter::lbz(UGeckoInstruction inst)
{
  const u32 temp = PowerPC::Read_U8(Helper_Get_EA(PowerPC::ppcState, inst));

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
    rGPR[inst.RD] = temp;
}

void Interpreter::lbzu(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_U(PowerPC::ppcState, inst);
  const u32 temp = PowerPC::Read_U8(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
    rGPR[inst.RA] = address;
  }
}

void Interpreter::lfd(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  const u64 temp = PowerPC::Read_U64(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
    rPS(inst.FD).SetPS0(temp);
}

void Interpreter::lfdu(UGeckoInstruction inst)
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
    rPS(inst.FD).SetPS0(temp);
    rGPR[inst.RA] = address;
  }
}

void Interpreter::lfdux(UGeckoInstruction inst)
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
    rPS(inst.FD).SetPS0(temp);
    rGPR[inst.RA] = address;
  }
}

void Interpreter::lfdx(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  const u64 temp = PowerPC::Read_U64(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
    rPS(inst.FD).SetPS0(temp);
}

void Interpreter::lfs(UGeckoInstruction inst)
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
    rPS(inst.FD).Fill(value);
  }
}

void Interpreter::lfsu(UGeckoInstruction inst)
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
    rPS(inst.FD).Fill(value);
    rGPR[inst.RA] = address;
  }
}

void Interpreter::lfsux(UGeckoInstruction inst)
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
    rPS(inst.FD).Fill(value);
    rGPR[inst.RA] = address;
  }
}

void Interpreter::lfsx(UGeckoInstruction inst)
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
    rPS(inst.FD).Fill(value);
  }
}

void Interpreter::lha(UGeckoInstruction inst)
{
  const u32 temp = (u32)(s32)(s16)PowerPC::Read_U16(Helper_Get_EA(PowerPC::ppcState, inst));

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
  }
}

void Interpreter::lhau(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_U(PowerPC::ppcState, inst);
  const u32 temp = (u32)(s32)(s16)PowerPC::Read_U16(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
    rGPR[inst.RA] = address;
  }
}

void Interpreter::lhz(UGeckoInstruction inst)
{
  const u32 temp = PowerPC::Read_U16(Helper_Get_EA(PowerPC::ppcState, inst));

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
  }
}

void Interpreter::lhzu(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_U(PowerPC::ppcState, inst);
  const u32 temp = PowerPC::Read_U16(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
    rGPR[inst.RA] = address;
  }
}

// FIXME: lmw should do a total rollback if a DSI occurs
void Interpreter::lmw(UGeckoInstruction inst)
{
  u32 address = Helper_Get_EA(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0 || MSR.LE)
  {
    GenerateAlignmentException(address);
    return;
  }

  for (int i = inst.RD; i <= 31; i++, address += 4)
  {
    const u32 temp_reg = PowerPC::Read_U32(address);

    if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
    {
      PanicAlert("DSI exception in lmw");
      NOTICE_LOG(POWERPC, "DSI exception in lmw");
      return;
    }
    else
    {
      rGPR[i] = temp_reg;
    }
  }
}

// FIXME: stmw should do a total rollback if a DSI occurs
void Interpreter::stmw(UGeckoInstruction inst)
{
  u32 address = Helper_Get_EA(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0 || MSR.LE)
  {
    GenerateAlignmentException(address);
    return;
  }

  for (int i = inst.RS; i <= 31; i++, address += 4)
  {
    PowerPC::Write_U32(rGPR[i], address);
    if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
    {
      PanicAlert("DSI exception in stmw");
      NOTICE_LOG(POWERPC, "DSI exception in stmw");
      return;
    }
  }
}

void Interpreter::lwz(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA(PowerPC::ppcState, inst);
  const u32 temp = PowerPC::Read_U32(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
  }
}

void Interpreter::lwzu(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_U(PowerPC::ppcState, inst);
  const u32 temp = PowerPC::Read_U32(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
    rGPR[inst.RA] = address;
  }
}

void Interpreter::stb(UGeckoInstruction inst)
{
  PowerPC::Write_U8((u8)rGPR[inst.RS], Helper_Get_EA(PowerPC::ppcState, inst));
}

void Interpreter::stbu(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_U(PowerPC::ppcState, inst);

  PowerPC::Write_U8((u8)rGPR[inst.RS], address);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA] = address;
  }
}

void Interpreter::stfd(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  PowerPC::Write_U64(rPS(inst.FS).PS0AsU64(), address);
}

void Interpreter::stfdu(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_U(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  PowerPC::Write_U64(rPS(inst.FS).PS0AsU64(), address);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA] = address;
  }
}

void Interpreter::stfs(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  PowerPC::Write_U32(ConvertToSingle(rPS(inst.FS).PS0AsU64()), address);
}

void Interpreter::stfsu(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_U(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  PowerPC::Write_U32(ConvertToSingle(rPS(inst.FS).PS0AsU64()), address);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA] = address;
  }
}

void Interpreter::sth(UGeckoInstruction inst)
{
  PowerPC::Write_U16((u16)rGPR[inst.RS], Helper_Get_EA(PowerPC::ppcState, inst));
}

void Interpreter::sthu(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_U(PowerPC::ppcState, inst);

  PowerPC::Write_U16((u16)rGPR[inst.RS], address);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA] = address;
  }
}

void Interpreter::stw(UGeckoInstruction inst)
{
  PowerPC::Write_U32(rGPR[inst.RS], Helper_Get_EA(PowerPC::ppcState, inst));
}

void Interpreter::stwu(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_U(PowerPC::ppcState, inst);

  PowerPC::Write_U32(rGPR[inst.RS], address);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA] = address;
  }
}

void Interpreter::dcba(UGeckoInstruction inst)
{
  ASSERT_MSG(POWERPC, 0, "dcba - Not implemented - not a Gekko instruction");
}

void Interpreter::dcbf(UGeckoInstruction inst)
{
  // TODO: Implement some sort of L2 emulation.
  // TODO: Raise DSI if translation fails (except for direct-store segments).

  // Invalidate the JIT cache here as a heuristic to compensate for
  // the lack of precise L1 icache emulation in the JIT. (Portable software
  // should use icbi consistently, but games aren't portable.)
  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);
  JitInterface::InvalidateICache(address & ~0x1f, 32, false);
}

void Interpreter::dcbi(UGeckoInstruction inst)
{
  if (MSR.PR)
  {
    GenerateProgramException();
    return;
  }

  // TODO: Implement some sort of L2 emulation.
  // TODO: Raise DSI if translation fails (except for direct-store segments).

  // Invalidate the JIT cache here as a heuristic to compensate for
  // the lack of precise L1 icache emulation in the JIT. (Portable software
  // should use icbi consistently, but games aren't portable.)
  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);
  JitInterface::InvalidateICache(address & ~0x1f, 32, false);
}

void Interpreter::dcbst(UGeckoInstruction inst)
{
  // TODO: Implement some sort of L2 emulation.
  // TODO: Raise DSI if translation fails (except for direct-store segments).

  // Invalidate the JIT cache here as a heuristic to compensate for
  // the lack of precise L1 icache emulation in the JIT. (Portable software
  // should use icbi consistently, but games aren't portable.)
  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);
  JitInterface::InvalidateICache(address & ~0x1f, 32, false);
}

void Interpreter::dcbt(UGeckoInstruction inst)
{
  if (HID0.NOOPTI)
    return;

  // TODO: Implement some sort of L2 emulation.
}

void Interpreter::dcbtst(UGeckoInstruction inst)
{
  if (HID0.NOOPTI)
    return;

  // TODO: Implement some sort of L2 emulation.
}

void Interpreter::dcbz(UGeckoInstruction inst)
{
  const u32 dcbz_addr = Helper_Get_EA_X(PowerPC::ppcState, inst);

  if (!HID0.DCE)
  {
    GenerateAlignmentException(dcbz_addr);
    return;
  }

  // Hack to stop dcbz/dcbi over low MEM1 trashing memory.
  if (SConfig::GetInstance().bLowDCBZHack && (dcbz_addr < 0x80008000) && (dcbz_addr >= 0x80000000))
    return;

  // TODO: Implement some sort of L2 emulation.
  PowerPC::ClearCacheLine(dcbz_addr & (~31));
}

void Interpreter::dcbz_l(UGeckoInstruction inst)
{
  if (!HID2.LCE)
  {
    GenerateProgramException();
    return;
  }

  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);

  if (!HID0.DCE)
  {
    GenerateAlignmentException(address);
    return;
  }

  // FAKE: clear memory instead of clearing the cache block
  PowerPC::ClearCacheLine(address & (~31));
}

// eciwx/ecowx technically should access the specified device
// We just do it instantly from ppc...and hey, it works! :D
void Interpreter::eciwx(UGeckoInstruction inst)
{
  const u32 EA = Helper_Get_EA_X(PowerPC::ppcState, inst);

  if (!(PowerPC::ppcState.spr[SPR_EAR] & 0x80000000))
  {
    GenerateDSIException(EA);
    return;
  }

  if (EA & 3)
  {
    GenerateAlignmentException(EA);
    return;
  }

  rGPR[inst.RD] = PowerPC::Read_U32(EA);
}

void Interpreter::ecowx(UGeckoInstruction inst)
{
  const u32 EA = Helper_Get_EA_X(PowerPC::ppcState, inst);

  if (!(PowerPC::ppcState.spr[SPR_EAR] & 0x80000000))
  {
    GenerateDSIException(EA);
    return;
  }

  if (EA & 3)
  {
    GenerateAlignmentException(EA);
    return;
  }

  PowerPC::Write_U32(rGPR[inst.RS], EA);
}

void Interpreter::eieio(UGeckoInstruction inst)
{
  // Basically ensures that loads/stores before this instruction
  // have completed (in order) before executing the next op.
  // Prevents real ppc from "smartly" reordering loads/stores
  // But (at least in interpreter) we do everything realtime anyways.
}

void Interpreter::icbi(UGeckoInstruction inst)
{
  // TODO: Raise DSI if translation fails (except for direct-store segments).
  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);
  PowerPC::ppcState.iCache.Invalidate(address);
}

void Interpreter::lbzux(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_UX(PowerPC::ppcState, inst);
  const u32 temp = PowerPC::Read_U8(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
    rGPR[inst.RA] = address;
  }
}

void Interpreter::lbzx(UGeckoInstruction inst)
{
  const u32 temp = PowerPC::Read_U8(Helper_Get_EA_X(PowerPC::ppcState, inst));

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
  }
}

void Interpreter::lhaux(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_UX(PowerPC::ppcState, inst);
  const s32 temp = (s32)(s16)PowerPC::Read_U16(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
    rGPR[inst.RA] = address;
  }
}

void Interpreter::lhax(UGeckoInstruction inst)
{
  const s32 temp = (s32)(s16)PowerPC::Read_U16(Helper_Get_EA_X(PowerPC::ppcState, inst));

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
  }
}

void Interpreter::lhbrx(UGeckoInstruction inst)
{
  const u32 temp = Common::swap16(PowerPC::Read_U16(Helper_Get_EA_X(PowerPC::ppcState, inst)));

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
  }
}

void Interpreter::lhzux(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_UX(PowerPC::ppcState, inst);
  const u32 temp = PowerPC::Read_U16(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
    rGPR[inst.RA] = address;
  }
}

void Interpreter::lhzx(UGeckoInstruction inst)
{
  const u32 temp = PowerPC::Read_U16(Helper_Get_EA_X(PowerPC::ppcState, inst));

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
  }
}

// FIXME: Should rollback if a DSI occurs
void Interpreter::lswx(UGeckoInstruction inst)
{
  u32 EA = Helper_Get_EA_X(PowerPC::ppcState, inst);

  if (MSR.LE)
  {
    GenerateAlignmentException(EA);
    return;
  }

  // Confirmed by hardware test that the zero case doesn't zero rGPR[r]
  for (u32 n = 0; n < static_cast<u8>(PowerPC::ppcState.xer_stringctrl); n++)
  {
    const int reg = (inst.RD + (n >> 2)) & 0x1f;
    const int offset = (n & 3) << 3;

    if ((n & 3) == 0)
      rGPR[reg] = 0;

    const u32 temp_value = PowerPC::Read_U8(EA) << (24 - offset);
    // Not64 (Homebrew N64 Emulator for Wii) triggers the following case.
    if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
    {
      NOTICE_LOG(POWERPC, "DSI exception in lswx");
      return;
    }
    rGPR[reg] |= temp_value;

    EA++;
  }
}

void Interpreter::lwbrx(UGeckoInstruction inst)
{
  const u32 temp = Common::swap32(PowerPC::Read_U32(Helper_Get_EA_X(PowerPC::ppcState, inst)));

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
  }
}

void Interpreter::lwzux(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_UX(PowerPC::ppcState, inst);
  const u32 temp = PowerPC::Read_U32(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
    rGPR[inst.RA] = address;
  }
}

void Interpreter::lwzx(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);
  const u32 temp = PowerPC::Read_U32(address);

  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
  }
}

void Interpreter::stbux(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_UX(PowerPC::ppcState, inst);

  PowerPC::Write_U8((u8)rGPR[inst.RS], address);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA] = address;
  }
}

void Interpreter::stbx(UGeckoInstruction inst)
{
  PowerPC::Write_U8((u8)rGPR[inst.RS], Helper_Get_EA_X(PowerPC::ppcState, inst));
}

void Interpreter::stfdux(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_UX(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  PowerPC::Write_U64(rPS(inst.FS).PS0AsU64(), address);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA] = address;
  }
}

void Interpreter::stfdx(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  PowerPC::Write_U64(rPS(inst.FS).PS0AsU64(), address);
}

// Stores Floating points into Integers indeXed
void Interpreter::stfiwx(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  PowerPC::Write_U32(rPS(inst.FS).PS0AsU32(), address);
}

void Interpreter::stfsux(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_UX(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  PowerPC::Write_U32(ConvertToSingle(rPS(inst.FS).PS0AsU64()), address);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA] = address;
  }
}

void Interpreter::stfsx(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  PowerPC::Write_U32(ConvertToSingle(rPS(inst.FS).PS0AsU64()), address);
}

void Interpreter::sthbrx(UGeckoInstruction inst)
{
  PowerPC::Write_U16(Common::swap16((u16)rGPR[inst.RS]), Helper_Get_EA_X(PowerPC::ppcState, inst));
}

void Interpreter::sthux(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_UX(PowerPC::ppcState, inst);

  PowerPC::Write_U16((u16)rGPR[inst.RS], address);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA] = address;
  }
}

void Interpreter::sthx(UGeckoInstruction inst)
{
  PowerPC::Write_U16((u16)rGPR[inst.RS], Helper_Get_EA_X(PowerPC::ppcState, inst));
}

// lswi - bizarro string instruction
// FIXME: Should rollback if a DSI occurs
void Interpreter::lswi(UGeckoInstruction inst)
{
  u32 EA;
  if (inst.RA == 0)
    EA = 0;
  else
    EA = rGPR[inst.RA];

  if (MSR.LE)
  {
    GenerateAlignmentException(EA);
    return;
  }

  u32 n;
  if (inst.NB == 0)
    n = 32;
  else
    n = inst.NB;

  int r = inst.RD - 1;
  int i = 0;
  while (n > 0)
  {
    if (i == 0)
    {
      r++;
      r &= 31;
      rGPR[r] = 0;
    }

    const u32 temp_value = PowerPC::Read_U8(EA) << (24 - i);
    if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
    {
      PanicAlert("DSI exception in lsw.");
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
void Interpreter::stswi(UGeckoInstruction inst)
{
  u32 EA;
  if (inst.RA == 0)
    EA = 0;
  else
    EA = rGPR[inst.RA];

  if (MSR.LE)
  {
    GenerateAlignmentException(EA);
    return;
  }

  u32 n;
  if (inst.NB == 0)
    n = 32;
  else
    n = inst.NB;

  int r = inst.RS - 1;
  int i = 0;
  while (n > 0)
  {
    if (i == 0)
    {
      r++;
      r &= 31;
    }
    PowerPC::Write_U8((rGPR[r] >> (24 - i)) & 0xFF, EA);
    if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
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
void Interpreter::stswx(UGeckoInstruction inst)
{
  u32 EA = Helper_Get_EA_X(PowerPC::ppcState, inst);

  if (MSR.LE)
  {
    GenerateAlignmentException(EA);
    return;
  }

  u32 n = (u8)PowerPC::ppcState.xer_stringctrl;
  int r = inst.RS;
  int i = 0;

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

void Interpreter::stwbrx(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);

  PowerPC::Write_U32(Common::swap32(rGPR[inst.RS]), address);
}

// The following two instructions are for SMP communications. On a single
// CPU, they cannot fail unless an interrupt happens in between.

void Interpreter::lwarx(UGeckoInstruction inst)
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
    rGPR[inst.RD] = temp;
    m_reserve = true;
    m_reserve_address = address;
  }
}

// Stores Word Conditional indeXed
void Interpreter::stwcxd(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);

  if ((address & 0b11) != 0)
  {
    GenerateAlignmentException(address);
    return;
  }

  if (m_reserve)
  {
    if (address == m_reserve_address)
    {
      PowerPC::Write_U32(rGPR[inst.RS], address);
      if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
      {
        m_reserve = false;
        PowerPC::ppcState.cr.SetField(0, 2 | PowerPC::GetXER_SO());
        return;
      }
    }
  }

  PowerPC::ppcState.cr.SetField(0, PowerPC::GetXER_SO());
}

void Interpreter::stwux(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_UX(PowerPC::ppcState, inst);

  PowerPC::Write_U32(rGPR[inst.RS], address);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA] = address;
  }
}

void Interpreter::stwx(UGeckoInstruction inst)
{
  const u32 address = Helper_Get_EA_X(PowerPC::ppcState, inst);

  PowerPC::Write_U32(rGPR[inst.RS], address);
}

void Interpreter::sync(UGeckoInstruction inst)
{
  // ignored
}

void Interpreter::tlbie(UGeckoInstruction inst)
{
  if (MSR.PR)
  {
    GenerateProgramException();
    return;
  }

  // Invalidate TLB entry
  const u32 address = rGPR[inst.RB];

  PowerPC::InvalidateTLBEntry(address);
}

void Interpreter::tlbsync(UGeckoInstruction inst)
{
  if (MSR.PR)
  {
    GenerateProgramException();
  }

  // Ignored
}
