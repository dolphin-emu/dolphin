// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"

#include "Core/HW/DSP.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"


bool Interpreter::g_bReserve;
u32  Interpreter::g_reserveAddr;

u32 Interpreter::Helper_Get_EA(const UGeckoInstruction _inst)
{
	return _inst.RA ? (m_GPR[_inst.RA] + _inst.SIMM_16) : (u32)_inst.SIMM_16;
}

u32 Interpreter::Helper_Get_EA_U(const UGeckoInstruction _inst)
{
	return (m_GPR[_inst.RA] + _inst.SIMM_16);
}

u32 Interpreter::Helper_Get_EA_X(const UGeckoInstruction _inst)
{
	return _inst.RA ? (m_GPR[_inst.RA] + m_GPR[_inst.RB]) : m_GPR[_inst.RB];
}

u32 Interpreter::Helper_Get_EA_UX(const UGeckoInstruction _inst)
{
	return (m_GPR[_inst.RA] + m_GPR[_inst.RB]);
}

void Interpreter::lbz(UGeckoInstruction _inst)
{
	u32 temp = (u32)Memory::Read_U8(Helper_Get_EA(_inst));
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
		m_GPR[_inst.RD] = temp;
}

void Interpreter::lbzu(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_U(_inst);
	u32 temp = (u32)Memory::Read_U8(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RD] = temp;
		m_GPR[_inst.RA] = uAddress;
	}
}

void Interpreter::lfd(UGeckoInstruction _inst)
{
	u64 temp = Memory::Read_U64(Helper_Get_EA(_inst));
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
		riPS0(_inst.FD) = temp;
}

void Interpreter::lfdu(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_U(_inst);
	u64 temp = Memory::Read_U64(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		riPS0(_inst.FD) = temp;
		m_GPR[_inst.RA]  = uAddress;
	}
}

void Interpreter::lfdux(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_UX(_inst);
	u64 temp = Memory::Read_U64(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		riPS0(_inst.FD) = temp;
		m_GPR[_inst.RA] = uAddress;
	}
}

void Interpreter::lfdx(UGeckoInstruction _inst)
{
	u64 temp = Memory::Read_U64(Helper_Get_EA_X(_inst));
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
		riPS0(_inst.FD) = temp;
}

void Interpreter::lfs(UGeckoInstruction _inst)
{
	u32 uTemp = Memory::Read_U32(Helper_Get_EA(_inst));
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		u64 value = ConvertToDouble(uTemp);
		riPS0(_inst.FD) = value;
		riPS1(_inst.FD) = value;
	}
}

void Interpreter::lfsu(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_U(_inst);
	u32 uTemp = Memory::Read_U32(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		u64 value = ConvertToDouble(uTemp);
		riPS0(_inst.FD) = value;
		riPS1(_inst.FD) = value;
		m_GPR[_inst.RA] = uAddress;
	}

}

void Interpreter::lfsux(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_UX(_inst);
	u32 uTemp = Memory::Read_U32(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		u64 value = ConvertToDouble(uTemp);
		riPS0(_inst.FD) = value;
		riPS1(_inst.FD) = value;
		m_GPR[_inst.RA] = uAddress;
	}
}

void Interpreter::lfsx(UGeckoInstruction _inst)
{
	u32 uTemp = Memory::Read_U32(Helper_Get_EA_X(_inst));
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		u64 value = ConvertToDouble(uTemp);
		riPS0(_inst.FD) = value;
		riPS1(_inst.FD) = value;
	}
}

void Interpreter::lha(UGeckoInstruction _inst)
{
	u32 temp = (u32)(s32)(s16)Memory::Read_U16(Helper_Get_EA(_inst));
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RD] = temp;
	}
}

void Interpreter::lhau(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_U(_inst);
	u32 temp = (u32)(s32)(s16)Memory::Read_U16(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RD] = temp;
		m_GPR[_inst.RA] = uAddress;
	}
}

void Interpreter::lhz(UGeckoInstruction _inst)
{
	u32 temp = (u32)(u16)Memory::Read_U16(Helper_Get_EA(_inst));
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RD] = temp;
	}
}

void Interpreter::lhzu(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_U(_inst);
	u32 temp = (u32)(u16)Memory::Read_U16(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RD] = temp;
		m_GPR[_inst.RA] = uAddress;
	}
}

// FIXME: lmw should do a total rollback if a DSI occurs
void Interpreter::lmw(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA(_inst);
	for (int iReg = _inst.RD; iReg <= 31; iReg++, uAddress += 4)
	{
		u32 TempReg = Memory::Read_U32(uAddress);
		if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
		{
			PanicAlert("DSI exception in lmw");
			NOTICE_LOG(POWERPC, "DSI exception in lmw");
			return;
		}
		else
		{
			m_GPR[iReg] = TempReg;
		}
	}
}

// FIXME: stmw should do a total rollback if a DSI occurs
void Interpreter::stmw(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA(_inst);
	for (int iReg = _inst.RS; iReg <= 31; iReg++, uAddress+=4)
	{
		Memory::Write_U32(m_GPR[iReg], uAddress);
		if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
		{
			PanicAlert("DSI exception in stmw");
			NOTICE_LOG(POWERPC, "DSI exception in stmw");
			return;
		}
	}
}

void Interpreter::lwz(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA(_inst);
	u32 temp = Memory::Read_U32(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RD] = temp;
	}
}

void Interpreter::lwzu(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_U(_inst);
	u32 temp = Memory::Read_U32(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RD] = temp;
		m_GPR[_inst.RA] = uAddress;
	}
}

void Interpreter::stb(UGeckoInstruction _inst)
{
	Memory::Write_U8((u8)m_GPR[_inst.RS], Helper_Get_EA(_inst));
}

void Interpreter::stbu(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_U(_inst);
	Memory::Write_U8((u8)m_GPR[_inst.RS], uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RA] = uAddress;
	}
}

void Interpreter::stfd(UGeckoInstruction _inst)
{
	Memory::Write_U64(riPS0(_inst.FS), Helper_Get_EA(_inst));
}

void Interpreter::stfdu(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_U(_inst);
	Memory::Write_U64(riPS0(_inst.FS), uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RA] = uAddress;
	}
}

void Interpreter::stfs(UGeckoInstruction _inst)
{
	Memory::Write_U32(ConvertToSingle(riPS0(_inst.FS)), Helper_Get_EA(_inst));
}

void Interpreter::stfsu(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_U(_inst);
	Memory::Write_U32(ConvertToSingle(riPS0(_inst.FS)), uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RA] = uAddress;
	}
}

void Interpreter::sth(UGeckoInstruction _inst)
{
	Memory::Write_U16((u16)m_GPR[_inst.RS], Helper_Get_EA(_inst));
}

void Interpreter::sthu(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_U(_inst);
	Memory::Write_U16((u16)m_GPR[_inst.RS], uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RA] = uAddress;
	}
}

void Interpreter::stw(UGeckoInstruction _inst)
{
	Memory::Write_U32(m_GPR[_inst.RS], Helper_Get_EA(_inst));
}

void Interpreter::stwu(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_U(_inst);
	Memory::Write_U32(m_GPR[_inst.RS], uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RA] = uAddress;
	}
}

void Interpreter::dcba(UGeckoInstruction _inst)
{
	_assert_msg_(POWERPC,0,"dcba - Not implemented - not a Gekko instruction");
}

void Interpreter::dcbf(UGeckoInstruction _inst)
{
	//This should tell GFX backend to throw out any cached data here
	// !!! SPEEDUP HACK for OSProtectRange !!!
/*	u32 tmp1 = Memory::Read_U32(PC+4);
	u32 tmp2 = Memory::Read_U32(PC+8);

	if ((tmp1 == 0x38630020) &&
		(tmp2 == 0x4200fff8))
	{
		NPC = PC + 12;
	}*/
	u32 address = Helper_Get_EA_X(_inst);
	JitInterface::InvalidateICache(address & ~0x1f, 32);
}

void Interpreter::dcbi(UGeckoInstruction _inst)
{
	// Removes a block from data cache. Since we don't emulate the data cache, we don't need to do anything to the data cache
	// However, we invalidate the jit block cache on dcbi
	u32 address = Helper_Get_EA_X(_inst);
	JitInterface::InvalidateICache(address & ~0x1f, 32);

	// The following detects a situation where the game is writing to the dcache at the address being DMA'd. As we do not
	// have dcache emulation, invalid data is being DMA'd causing audio glitches. The following code detects this and
	// enables the DMA to complete instantly before the invalid data is written.
	u64 dma_in_progress = DSP::DMAInProgress();
	if (dma_in_progress != 0)
	{
		u32 start_addr = (dma_in_progress >> 32) & Memory::RAM_MASK;
		u32 end_addr = (dma_in_progress & Memory::RAM_MASK) & 0xffffffff;
		u32 invalidated_addr = (address & Memory::RAM_MASK) & ~0x1f;

		if (invalidated_addr >= start_addr && invalidated_addr <= end_addr)
		{
			DSP::EnableInstantDMA();
		}
	}
}

void Interpreter::dcbst(UGeckoInstruction _inst)
{
	// Cache line flush. Since we don't emulate the data cache, we don't need to do anything.
	// Invalidate the jit block cache on dcbst in case new code has been loaded via the data cache
	u32 address = Helper_Get_EA_X(_inst);
	JitInterface::InvalidateICache(address & ~0x1f, 32);
}

void Interpreter::dcbt(UGeckoInstruction _inst)
{
	// Prefetch. Since we don't emulate the data cache, we don't need to do anything.
}

void Interpreter::dcbtst(UGeckoInstruction _inst)
{
	// This is just some sort of store "prefetching".
	// Since we don't emulate the data cache, we don't need to do anything.
}

void Interpreter::dcbz(UGeckoInstruction _inst)
{
	// HACK but works... we think
	if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bDCBZOFF)
		Memory::ClearCacheLine(Helper_Get_EA_X(_inst) & (~31));
	if (!JitInterface::GetCore())
		PowerPC::CheckExceptions();
}

// eciwx/ecowx technically should access the specified device
// We just do it instantly from ppc...and hey, it works! :D
void Interpreter::eciwx(UGeckoInstruction _inst)
{
	u32 EA, b;
	if (_inst.RA == 0)
		b = 0;
	else
		b = m_GPR[_inst.RA];
	EA = b + m_GPR[_inst.RB];

	if (!(PowerPC::ppcState.spr[SPR_EAR] & 0x80000000))
	{
		Common::AtomicOr(PowerPC::ppcState.Exceptions, EXCEPTION_DSI);
	}
	if (EA & 3)
		Common::AtomicOr(PowerPC::ppcState.Exceptions, EXCEPTION_ALIGNMENT);

// 	_assert_msg_(POWERPC,0,"eciwx - fill r%i with word @ %08x from device %02x",
// 		_inst.RS, EA, PowerPC::ppcState.spr[SPR_EAR] & 0x1f);

	m_GPR[_inst.RS] = Memory::Read_U32(EA);
}

void Interpreter::ecowx(UGeckoInstruction _inst)
{
	u32 EA, b;
	if (_inst.RA == 0)
		b = 0;
	else
		b = m_GPR[_inst.RA];
	EA = b + m_GPR[_inst.RB];

	if (!(PowerPC::ppcState.spr[SPR_EAR] & 0x80000000))
	{
		Common::AtomicOr(PowerPC::ppcState.Exceptions, EXCEPTION_DSI);
	}
	if (EA & 3)
		Common::AtomicOr(PowerPC::ppcState.Exceptions, EXCEPTION_ALIGNMENT);

// 	_assert_msg_(POWERPC,0,"ecowx - send stw request (%08x@%08x) to device %02x",
// 		m_GPR[_inst.RS], EA, PowerPC::ppcState.spr[SPR_EAR] & 0x1f);

	Memory::Write_U32(m_GPR[_inst.RS], EA);
}

void Interpreter::eieio(UGeckoInstruction _inst)
{
	// Basically ensures that loads/stores before this instruction
	// have completed (in order) before executing the next op.
	// Prevents real ppc from "smartly" reordering loads/stores
	// But (at least in interpreter) we do everything realtime anyways.
}

void Interpreter::icbi(UGeckoInstruction _inst)
{
	u32 address = Helper_Get_EA_X(_inst);
	PowerPC::ppcState.iCache.Invalidate(address);
}

void Interpreter::lbzux(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_UX(_inst);
	u32 temp = (u32)Memory::Read_U8(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RD] = temp;
		m_GPR[_inst.RA] = uAddress;
	}
}

void Interpreter::lbzx(UGeckoInstruction _inst)
{
	u32 temp = (u32)Memory::Read_U8(Helper_Get_EA_X(_inst));
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RD] = temp;
	}
}

void Interpreter::lhaux(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_UX(_inst);
	s32 temp = (s32)(s16)Memory::Read_U16(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RD] = temp;
		m_GPR[_inst.RA] = uAddress;
	}
}

void Interpreter::lhax(UGeckoInstruction _inst)
{
	s32 temp = (s32)(s16)Memory::Read_U16(Helper_Get_EA_X(_inst));
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RD] = temp;
	}
}

void Interpreter::lhbrx(UGeckoInstruction _inst)
{
	u32 temp = (u32)Common::swap16(Memory::Read_U16(Helper_Get_EA_X(_inst)));
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RD] = temp;
	}
}

void Interpreter::lhzux(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_UX(_inst);
	u32 temp = (u32)Memory::Read_U16(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RD] = temp;
		m_GPR[_inst.RA] = uAddress;
	}
}

void Interpreter::lhzx(UGeckoInstruction _inst)
{
	u32 temp = (u32)Memory::Read_U16(Helper_Get_EA_X(_inst));
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RD] = temp;
	}
}

// TODO: is this right?
// FIXME: Should rollback if a DSI occurs
void Interpreter::lswx(UGeckoInstruction _inst)
{
	u32 EA = Helper_Get_EA_X(_inst);
	u32 n = (u8)PowerPC::ppcState.xer_stringctrl;
	int r = _inst.RD;
	int i = 0;

	if (n > 0)
	{
		m_GPR[r] = 0;
		do
		{
			u32 TempValue = Memory::Read_U8(EA) << (24 - i);
			if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
			{
				PanicAlert("DSI exception in lswx.");
				NOTICE_LOG(POWERPC, "DSI exception in lswx");
				return;
			}
			m_GPR[r] |= TempValue;

			EA++;
			n--;
			i += 8;
			if (i == 32)
			{
				i = 0;
				r = (r + 1) & 31;
				m_GPR[r] = 0;
			}
		} while (n > 0);
	}
}

void Interpreter::lwbrx(UGeckoInstruction _inst)
{
	u32 temp = Common::swap32(Memory::Read_U32(Helper_Get_EA_X(_inst)));
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RD] = temp;
	}
}

void Interpreter::lwzux(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_UX(_inst);
	u32 temp = Memory::Read_U32(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RD] = temp;
		m_GPR[_inst.RA] = uAddress;
	}
}

void Interpreter::lwzx(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_X(_inst);
	u32 temp = Memory::Read_U32(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RD] = temp;
	}
}

void Interpreter::stbux(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_UX(_inst);
	Memory::Write_U8((u8)m_GPR[_inst.RS], uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RA] = uAddress;
	}
}

void Interpreter::stbx(UGeckoInstruction _inst)
{
	Memory::Write_U8((u8)m_GPR[_inst.RS], Helper_Get_EA_X(_inst));
}

void Interpreter::stfdux(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_UX(_inst);
	Memory::Write_U64(riPS0(_inst.FS), uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RA] = uAddress;
	}
}

void Interpreter::stfdx(UGeckoInstruction _inst)
{
	Memory::Write_U64(riPS0(_inst.FS), Helper_Get_EA_X(_inst));
}

// __________________________________________________________________________________________________
// stfiwx
// TODO - examine what this really does
// Stores Floating points into Integers indeXed
void Interpreter::stfiwx(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_X(_inst);

	Memory::Write_U32((u32)riPS0(_inst.FS), uAddress);
}


void Interpreter::stfsux(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_UX(_inst);
	Memory::Write_U32(ConvertToSingle(riPS0(_inst.FS)), uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RA] = uAddress;
	}
}

void Interpreter::stfsx(UGeckoInstruction _inst)
{
	Memory::Write_U32(ConvertToSingle(riPS0(_inst.FS)), Helper_Get_EA_X(_inst));
}

void Interpreter::sthbrx(UGeckoInstruction _inst)
{
	Memory::Write_U16(Common::swap16((u16)m_GPR[_inst.RS]), Helper_Get_EA_X(_inst));
}

void Interpreter::sthux(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_UX(_inst);
	Memory::Write_U16((u16)m_GPR[_inst.RS], uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RA] = uAddress;
	}
}

void Interpreter::sthx(UGeckoInstruction _inst)
{
	Memory::Write_U16((u16)m_GPR[_inst.RS], Helper_Get_EA_X(_inst));
}

// __________________________________________________________________________________________________
// lswi - bizarro string instruction
// FIXME: Should rollback if a DSI occurs
void Interpreter::lswi(UGeckoInstruction _inst)
{
	u32 EA;
	if (_inst.RA == 0)
		EA = 0;
	else
		EA = m_GPR[_inst.RA];

	u32 n;
	if (_inst.NB == 0)
		n = 32;
	else
		n = _inst.NB;

	int r = _inst.RD - 1;
	int i = 0;
	while (n>0)
	{
		if (i==0)
		{
			r++;
			r &= 31;
			m_GPR[r] = 0;
		}

		u32 TempValue = Memory::Read_U8(EA) << (24 - i);
		if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
		{
			PanicAlert("DSI exception in lsw.");
			return;
		}

		m_GPR[r] |= TempValue;

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
void Interpreter::stswi(UGeckoInstruction _inst)
{
	u32 EA;
	if (_inst.RA == 0)
		EA = 0;
	else
		EA = m_GPR[_inst.RA];

	u32 n;
	if (_inst.NB == 0)
		n = 32;
	else
		n = _inst.NB;

	int r = _inst.RS - 1;
	int i = 0;
	while (n > 0)
	{
		if (i == 0)
		{
			r++;
			r &= 31;
		}
		Memory::Write_U8((m_GPR[r] >> (24 - i)) & 0xFF, EA);
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
void Interpreter::stswx(UGeckoInstruction _inst)
{
	u32 EA = Helper_Get_EA_X(_inst);
	u32 n = (u8)PowerPC::ppcState.xer_stringctrl;
	int r = _inst.RS;
	int i = 0;

	while (n > 0)
	{
		Memory::Write_U8((m_GPR[r] >> (24 - i)) & 0xFF, EA);

		EA++;
		n--;
		i += 8;
		if (i == 32)
		{
			i = 0;
			r++;
		}
	}
}

void Interpreter::stwbrx(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_X(_inst);
	Memory::Write_U32(Common::swap32(m_GPR[_inst.RS]), uAddress);
}


// The following two instructions are for SMP communications. On a single
// CPU, they cannot fail unless an interrupt happens in between.

void Interpreter::lwarx(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_X(_inst);
	u32 temp = Memory::Read_U32(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RD] = temp;
		g_bReserve = true;
		g_reserveAddr = uAddress;
	}
}

void Interpreter::stwcxd(UGeckoInstruction _inst)
{
	// Stores Word Conditional indeXed
	u32 uAddress;
	if (g_bReserve)
	{
		uAddress = Helper_Get_EA_X(_inst);

		if (uAddress == g_reserveAddr)
		{
			Memory::Write_U32(m_GPR[_inst.RS], uAddress);
			if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
			{
				g_bReserve = false;
				SetCRField(0, 2 | GetXER_SO());
				return;
			}
		}
	}

	SetCRField(0, GetXER_SO());
}

void Interpreter::stwux(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_UX(_inst);
	Memory::Write_U32(m_GPR[_inst.RS], uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[_inst.RA] = uAddress;
	}
}

void Interpreter::stwx(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_X(_inst);
	Memory::Write_U32(m_GPR[_inst.RS], uAddress);
}

void Interpreter::sync(UGeckoInstruction _inst)
{
	//ignored
}

void Interpreter::tlbia(UGeckoInstruction _inst)
{
	// Gekko does not support this instructions.
	PanicAlert("The GC CPU does not support tlbia");
	// invalid the whole TLB
	//MessageBox(0,"TLBIA","TLBIA",0);
}

void Interpreter::tlbie(UGeckoInstruction _inst)
{
	// Invalidate TLB entry
	u32 _Address = m_GPR[_inst.RB];
	Memory::InvalidateTLBEntry(_Address);
}

void Interpreter::tlbsync(UGeckoInstruction _inst)
{
	//MessageBox(0,"TLBsync","TLBsyncE",0);
}
