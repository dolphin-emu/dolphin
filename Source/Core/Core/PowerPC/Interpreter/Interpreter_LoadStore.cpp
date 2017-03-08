// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/Swap.h"

#include "Core/ConfigManager.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"

bool Interpreter::m_reserve;
u32 Interpreter::m_reserve_address;

u32 Interpreter::Helper_Get_EA(const UGeckoInstruction inst)
{
  return inst.RA ? (rGPR[inst.RA] + inst.SIMM_16) : (u32)inst.SIMM_16;
}

u32 Interpreter::Helper_Get_EA_U(const UGeckoInstruction inst)
{
  return (rGPR[inst.RA] + inst.SIMM_16);
}

u32 Interpreter::Helper_Get_EA_X(const UGeckoInstruction inst)
{
  return inst.RA ? (rGPR[inst.RA] + rGPR[inst.RB]) : rGPR[inst.RB];
}

u32 Interpreter::Helper_Get_EA_UX(const UGeckoInstruction inst)
{
  return (rGPR[inst.RA] + rGPR[inst.RB]);
}

void Interpreter::lbz(UGeckoInstruction inst)
{
  u32 temp = (u32)PowerPC::Read_U8(Helper_Get_EA(inst));
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
    rGPR[inst.RD] = temp;
}

void Interpreter::lbzu(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA_U(inst);
  u32 temp = (u32)PowerPC::Read_U8(uAddress);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
    rGPR[inst.RA] = uAddress;
  }
}

void Interpreter::lfd(UGeckoInstruction inst)
{
  u64 temp = PowerPC::Read_U64(Helper_Get_EA(inst));
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
    riPS0(inst.FD) = temp;
}

void Interpreter::lfdu(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA_U(inst);
  u64 temp = PowerPC::Read_U64(uAddress);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    riPS0(inst.FD) = temp;
    rGPR[inst.RA] = uAddress;
  }
}

void Interpreter::lfdux(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA_UX(inst);
  u64 temp = PowerPC::Read_U64(uAddress);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    riPS0(inst.FD) = temp;
    rGPR[inst.RA] = uAddress;
  }
}

void Interpreter::lfdx(UGeckoInstruction inst)
{
  u64 temp = PowerPC::Read_U64(Helper_Get_EA_X(inst));
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
    riPS0(inst.FD) = temp;
}

void Interpreter::lfs(UGeckoInstruction inst)
{
  u32 uTemp = PowerPC::Read_U32(Helper_Get_EA(inst));
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    u64 value = ConvertToDouble(uTemp);
    riPS0(inst.FD) = value;
    riPS1(inst.FD) = value;
  }
}

void Interpreter::lfsu(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA_U(inst);
  u32 uTemp = PowerPC::Read_U32(uAddress);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    u64 value = ConvertToDouble(uTemp);
    riPS0(inst.FD) = value;
    riPS1(inst.FD) = value;
    rGPR[inst.RA] = uAddress;
  }
}

void Interpreter::lfsux(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA_UX(inst);
  u32 uTemp = PowerPC::Read_U32(uAddress);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    u64 value = ConvertToDouble(uTemp);
    riPS0(inst.FD) = value;
    riPS1(inst.FD) = value;
    rGPR[inst.RA] = uAddress;
  }
}

void Interpreter::lfsx(UGeckoInstruction inst)
{
  u32 uTemp = PowerPC::Read_U32(Helper_Get_EA_X(inst));
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    u64 value = ConvertToDouble(uTemp);
    riPS0(inst.FD) = value;
    riPS1(inst.FD) = value;
  }
}

void Interpreter::lha(UGeckoInstruction inst)
{
  u32 temp = (u32)(s32)(s16)PowerPC::Read_U16(Helper_Get_EA(inst));
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
  }
}

void Interpreter::lhau(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA_U(inst);
  u32 temp = (u32)(s32)(s16)PowerPC::Read_U16(uAddress);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
    rGPR[inst.RA] = uAddress;
  }
}

void Interpreter::lhz(UGeckoInstruction inst)
{
  u32 temp = (u32)(u16)PowerPC::Read_U16(Helper_Get_EA(inst));
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
  }
}

void Interpreter::lhzu(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA_U(inst);
  u32 temp = (u32)(u16)PowerPC::Read_U16(uAddress);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
    rGPR[inst.RA] = uAddress;
  }
}

// FIXME: lmw should do a total rollback if a DSI occurs
void Interpreter::lmw(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA(inst);
  for (int iReg = inst.RD; iReg <= 31; iReg++, uAddress += 4)
  {
    u32 TempReg = PowerPC::Read_U32(uAddress);
    if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
    {
      PanicAlert("DSI exception in lmw");
      NOTICE_LOG(POWERPC, "DSI exception in lmw");
      return;
    }
    else
    {
      rGPR[iReg] = TempReg;
    }
  }
}

// FIXME: stmw should do a total rollback if a DSI occurs
void Interpreter::stmw(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA(inst);
  for (int iReg = inst.RS; iReg <= 31; iReg++, uAddress += 4)
  {
    PowerPC::Write_U32(rGPR[iReg], uAddress);
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
  u32 uAddress = Helper_Get_EA(inst);
  u32 temp = PowerPC::Read_U32(uAddress);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
  }
}

void Interpreter::lwzu(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA_U(inst);
  u32 temp = PowerPC::Read_U32(uAddress);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
    rGPR[inst.RA] = uAddress;
  }
}

void Interpreter::stb(UGeckoInstruction inst)
{
  PowerPC::Write_U8((u8)rGPR[inst.RS], Helper_Get_EA(inst));
}

void Interpreter::stbu(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA_U(inst);
  PowerPC::Write_U8((u8)rGPR[inst.RS], uAddress);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA] = uAddress;
  }
}

void Interpreter::stfd(UGeckoInstruction inst)
{
  PowerPC::Write_U64(riPS0(inst.FS), Helper_Get_EA(inst));
}

void Interpreter::stfdu(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA_U(inst);
  PowerPC::Write_U64(riPS0(inst.FS), uAddress);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA] = uAddress;
  }
}

void Interpreter::stfs(UGeckoInstruction inst)
{
  PowerPC::Write_U32(ConvertToSingle(riPS0(inst.FS)), Helper_Get_EA(inst));
}

void Interpreter::stfsu(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA_U(inst);
  PowerPC::Write_U32(ConvertToSingle(riPS0(inst.FS)), uAddress);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA] = uAddress;
  }
}

void Interpreter::sth(UGeckoInstruction inst)
{
  PowerPC::Write_U16((u16)rGPR[inst.RS], Helper_Get_EA(inst));
}

void Interpreter::sthu(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA_U(inst);
  PowerPC::Write_U16((u16)rGPR[inst.RS], uAddress);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA] = uAddress;
  }
}

void Interpreter::stw(UGeckoInstruction inst)
{
  PowerPC::Write_U32(rGPR[inst.RS], Helper_Get_EA(inst));
}

void Interpreter::stwu(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA_U(inst);
  PowerPC::Write_U32(rGPR[inst.RS], uAddress);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA] = uAddress;
  }
}

void Interpreter::dcba(UGeckoInstruction inst)
{
  _assert_msg_(POWERPC, 0, "dcba - Not implemented - not a Gekko instruction");
}

void Interpreter::dcbf(UGeckoInstruction inst)
{
  // TODO: Implement some sort of L2 emulation.
  // TODO: Raise DSI if translation fails (except for direct-store segments).

  // Invalidate the JIT cache here as a heuristic to compensate for
  // the lack of precise L1 icache emulation in the JIT. (Portable software
  // should use icbi consistently, but games aren't portable.)
  u32 address = Helper_Get_EA_X(inst);
  JitInterface::InvalidateICache(address & ~0x1f, 32, false);
}

void Interpreter::dcbi(UGeckoInstruction inst)
{
  // TODO: Implement some sort of L2 emulation.
  // TODO: Raise DSI if translation fails (except for direct-store segments).

  // Invalidate the JIT cache here as a heuristic to compensate for
  // the lack of precise L1 icache emulation in the JIT. (Portable software
  // should use icbi consistently, but games aren't portable.)
  u32 address = Helper_Get_EA_X(inst);
  JitInterface::InvalidateICache(address & ~0x1f, 32, false);
}

void Interpreter::dcbst(UGeckoInstruction inst)
{
  // TODO: Implement some sort of L2 emulation.
  // TODO: Raise DSI if translation fails (except for direct-store segments).

  // Invalidate the JIT cache here as a heuristic to compensate for
  // the lack of precise L1 icache emulation in the JIT. (Portable software
  // should use icbi consistently, but games aren't portable.)
  u32 address = Helper_Get_EA_X(inst);
  JitInterface::InvalidateICache(address & ~0x1f, 32, false);
}

void Interpreter::dcbt(UGeckoInstruction inst)
{
  // TODO: Implement some sort of L2 emulation.
}

void Interpreter::dcbtst(UGeckoInstruction inst)
{
  // TODO: Implement some sort of L2 emulation.
}

void Interpreter::dcbz(UGeckoInstruction inst)
{
  // DCBZOFF is a hack to fix certain games which would otherwise require
  // accurate L2 emulation.
  if (SConfig::GetInstance().bDCBZOFF)
    return;

  u32 dcbz_addr = Helper_Get_EA_X(inst);
  // Hack to stop dcbz/dcbi over low MEM1 trashing memory.
  if (SConfig::GetInstance().bLowDCBZHack && (dcbz_addr < 0x80008000) && (dcbz_addr >= 0x80000000))
    return;

  // TODO: Implement some sort of L2 emulation.
  PowerPC::ClearCacheLine(dcbz_addr & (~31));
}

// eciwx/ecowx technically should access the specified device
// We just do it instantly from ppc...and hey, it works! :D
void Interpreter::eciwx(UGeckoInstruction inst)
{
  u32 EA = Helper_Get_EA_X(inst);

  if (!(PowerPC::ppcState.spr[SPR_EAR] & 0x80000000))
  {
    PowerPC::ppcState.Exceptions |= EXCEPTION_DSI;
  }
  if (EA & 3)
    PowerPC::ppcState.Exceptions |= EXCEPTION_ALIGNMENT;

  // _assert_msg_(POWERPC,0,"eciwx - fill r%i with word @ %08x from device %02x",
  //              inst.RS, EA, PowerPC::ppcState.spr[SPR_EAR] & 0x1f);

  rGPR[inst.RD] = PowerPC::Read_U32(EA);
}

void Interpreter::ecowx(UGeckoInstruction inst)
{
  u32 EA = Helper_Get_EA_X(inst);

  if (!(PowerPC::ppcState.spr[SPR_EAR] & 0x80000000))
  {
    PowerPC::ppcState.Exceptions |= EXCEPTION_DSI;
  }
  if (EA & 3)
    PowerPC::ppcState.Exceptions |= EXCEPTION_ALIGNMENT;

  // _assert_msg_(POWERPC,0,"ecowx - send stw request (%08x@%08x) to device %02x",
  //              rGPR[_inst.RS], EA, PowerPC::ppcState.spr[SPR_EAR] & 0x1f);

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
  u32 address = Helper_Get_EA_X(inst);
  PowerPC::ppcState.iCache.Invalidate(address);
}

void Interpreter::lbzux(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA_UX(inst);
  u32 temp = (u32)PowerPC::Read_U8(uAddress);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
    rGPR[inst.RA] = uAddress;
  }
}

void Interpreter::lbzx(UGeckoInstruction inst)
{
  u32 temp = (u32)PowerPC::Read_U8(Helper_Get_EA_X(inst));
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
  }
}

void Interpreter::lhaux(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA_UX(inst);
  s32 temp = (s32)(s16)PowerPC::Read_U16(uAddress);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
    rGPR[inst.RA] = uAddress;
  }
}

void Interpreter::lhax(UGeckoInstruction inst)
{
  s32 temp = (s32)(s16)PowerPC::Read_U16(Helper_Get_EA_X(inst));
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
  }
}

void Interpreter::lhbrx(UGeckoInstruction inst)
{
  u32 temp = (u32)Common::swap16(PowerPC::Read_U16(Helper_Get_EA_X(inst)));
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
  }
}

void Interpreter::lhzux(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA_UX(inst);
  u32 temp = (u32)PowerPC::Read_U16(uAddress);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
    rGPR[inst.RA] = uAddress;
  }
}

void Interpreter::lhzx(UGeckoInstruction inst)
{
  u32 temp = (u32)PowerPC::Read_U16(Helper_Get_EA_X(inst));
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
  }
}

// FIXME: Should rollback if a DSI occurs
void Interpreter::lswx(UGeckoInstruction inst)
{
  u32 EA = Helper_Get_EA_X(inst);

  // Confirmed by hardware test that the zero case doesn't zero rGPR[r]
  for (u32 n = 0; n < static_cast<u8>(PowerPC::ppcState.xer_stringctrl); n++)
  {
    int reg = (inst.RD + (n >> 2)) & 0x1f;
    int offset = (n & 3) << 3;
    if ((n & 3) == 0)
      rGPR[reg] = 0;

    u32 TempValue = PowerPC::Read_U8(EA) << (24 - offset);
    if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
    {
      PanicAlert("DSI exception in lswx.");
      NOTICE_LOG(POWERPC, "DSI exception in lswx");
      return;
    }
    rGPR[reg] |= TempValue;

    EA++;
  }
}

void Interpreter::lwbrx(UGeckoInstruction inst)
{
  u32 temp = Common::swap32(PowerPC::Read_U32(Helper_Get_EA_X(inst)));
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
  }
}

void Interpreter::lwzux(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA_UX(inst);
  u32 temp = PowerPC::Read_U32(uAddress);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
    rGPR[inst.RA] = uAddress;
  }
}

void Interpreter::lwzx(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA_X(inst);
  u32 temp = PowerPC::Read_U32(uAddress);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
  }
}

void Interpreter::stbux(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA_UX(inst);
  PowerPC::Write_U8((u8)rGPR[inst.RS], uAddress);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA] = uAddress;
  }
}

void Interpreter::stbx(UGeckoInstruction inst)
{
  PowerPC::Write_U8((u8)rGPR[inst.RS], Helper_Get_EA_X(inst));
}

void Interpreter::stfdux(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA_UX(inst);
  PowerPC::Write_U64(riPS0(inst.FS), uAddress);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA] = uAddress;
  }
}

void Interpreter::stfdx(UGeckoInstruction inst)
{
  PowerPC::Write_U64(riPS0(inst.FS), Helper_Get_EA_X(inst));
}

// Stores Floating points into Integers indeXed
void Interpreter::stfiwx(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA_X(inst);

  PowerPC::Write_U32((u32)riPS0(inst.FS), uAddress);
}

void Interpreter::stfsux(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA_UX(inst);
  PowerPC::Write_U32(ConvertToSingle(riPS0(inst.FS)), uAddress);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA] = uAddress;
  }
}

void Interpreter::stfsx(UGeckoInstruction inst)
{
  PowerPC::Write_U32(ConvertToSingle(riPS0(inst.FS)), Helper_Get_EA_X(inst));
}

void Interpreter::sthbrx(UGeckoInstruction inst)
{
  PowerPC::Write_U16(Common::swap16((u16)rGPR[inst.RS]), Helper_Get_EA_X(inst));
}

void Interpreter::sthux(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA_UX(inst);
  PowerPC::Write_U16((u16)rGPR[inst.RS], uAddress);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA] = uAddress;
  }
}

void Interpreter::sthx(UGeckoInstruction inst)
{
  PowerPC::Write_U16((u16)rGPR[inst.RS], Helper_Get_EA_X(inst));
}

// __________________________________________________________________________________________________
// lswi - bizarro string instruction
// FIXME: Should rollback if a DSI occurs
void Interpreter::lswi(UGeckoInstruction inst)
{
  u32 EA;
  if (inst.RA == 0)
    EA = 0;
  else
    EA = rGPR[inst.RA];

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

    u32 TempValue = PowerPC::Read_U8(EA) << (24 - i);
    if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
    {
      PanicAlert("DSI exception in lsw.");
      return;
    }

    rGPR[r] |= TempValue;

    i += 8;
    if (i == 32)
      i = 0;
    EA++;
    n--;
  }
}

// todo : optimize ?
// __________________________________________________________________________________________________
// stswi - bizarro string instruction
// FIXME: Should rollback if a DSI occurs
void Interpreter::stswi(UGeckoInstruction inst)
{
  u32 EA;
  if (inst.RA == 0)
    EA = 0;
  else
    EA = rGPR[inst.RA];

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
  u32 EA = Helper_Get_EA_X(inst);
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
  u32 uAddress = Helper_Get_EA_X(inst);
  PowerPC::Write_U32(Common::swap32(rGPR[inst.RS]), uAddress);
}

// The following two instructions are for SMP communications. On a single
// CPU, they cannot fail unless an interrupt happens in between.

void Interpreter::lwarx(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA_X(inst);
  u32 temp = PowerPC::Read_U32(uAddress);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RD] = temp;
    m_reserve = true;
    m_reserve_address = uAddress;
  }
}

void Interpreter::stwcxd(UGeckoInstruction inst)
{
  // Stores Word Conditional indeXed
  u32 uAddress;
  if (m_reserve)
  {
    uAddress = Helper_Get_EA_X(inst);

    if (uAddress == m_reserve_address)
    {
      PowerPC::Write_U32(rGPR[inst.RS], uAddress);
      if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
      {
        m_reserve = false;
        SetCRField(0, 2 | GetXER_SO());
        return;
      }
    }
  }

  SetCRField(0, GetXER_SO());
}

void Interpreter::stwux(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA_UX(inst);
  PowerPC::Write_U32(rGPR[inst.RS], uAddress);
  if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
  {
    rGPR[inst.RA] = uAddress;
  }
}

void Interpreter::stwx(UGeckoInstruction inst)
{
  u32 uAddress = Helper_Get_EA_X(inst);
  PowerPC::Write_U32(rGPR[inst.RS], uAddress);
}

void Interpreter::sync(UGeckoInstruction inst)
{
  // ignored
}

void Interpreter::tlbie(UGeckoInstruction inst)
{
  // Invalidate TLB entry
  u32 _Address = rGPR[inst.RB];
  PowerPC::InvalidateTLBEntry(_Address);
}

void Interpreter::tlbsync(UGeckoInstruction inst)
{
  // MessageBox(0,"TLBsync","TLBsyncE",0);
}
