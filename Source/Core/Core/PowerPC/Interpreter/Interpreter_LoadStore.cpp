// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "Common/MathUtil.h"

#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"


bool Interpreter::g_bReserve;
u32  Interpreter::g_reserveAddr;

u32 Interpreter::Helper_Get_EA(const UGeckoInstruction inst)
{
	return inst.RA ? (m_GPR[inst.RA] + inst.SIMM_16) : (u32)inst.SIMM_16;
}

u32 Interpreter::Helper_Get_EA_U(const UGeckoInstruction inst)
{
	return (m_GPR[inst.RA] + inst.SIMM_16);
}

u32 Interpreter::Helper_Get_EA_X(const UGeckoInstruction inst)
{
	return inst.RA ? (m_GPR[inst.RA] + m_GPR[inst.RB]) : m_GPR[inst.RB];
}

u32 Interpreter::Helper_Get_EA_UX(const UGeckoInstruction inst)
{
	return (m_GPR[inst.RA] + m_GPR[inst.RB]);
}

void Interpreter::lbz(UGeckoInstruction inst)
{
	u32 temp = (u32)Memory::Read_U8(Helper_Get_EA(inst));
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
		m_GPR[inst.RD] = temp;
}

void Interpreter::lbzu(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA_U(inst);
	u32 temp = (u32)Memory::Read_U8(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RD] = temp;
		m_GPR[inst.RA] = uAddress;
	}
}

void Interpreter::lfd(UGeckoInstruction inst)
{
	u64 temp = Memory::Read_U64(Helper_Get_EA(inst));
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
		riPS0(inst.FD) = temp;
}

void Interpreter::lfdu(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA_U(inst);
	u64 temp = Memory::Read_U64(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		riPS0(inst.FD) = temp;
		m_GPR[inst.RA]  = uAddress;
	}
}

void Interpreter::lfdux(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA_UX(inst);
	u64 temp = Memory::Read_U64(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		riPS0(inst.FD) = temp;
		m_GPR[inst.RA] = uAddress;
	}
}

void Interpreter::lfdx(UGeckoInstruction inst)
{
	u64 temp = Memory::Read_U64(Helper_Get_EA_X(inst));
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
		riPS0(inst.FD) = temp;
}

void Interpreter::lfs(UGeckoInstruction inst)
{
	u32 uTemp = Memory::Read_U32(Helper_Get_EA(inst));
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
	u32 uTemp = Memory::Read_U32(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		u64 value = ConvertToDouble(uTemp);
		riPS0(inst.FD) = value;
		riPS1(inst.FD) = value;
		m_GPR[inst.RA] = uAddress;
	}

}

void Interpreter::lfsux(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA_UX(inst);
	u32 uTemp = Memory::Read_U32(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		u64 value = ConvertToDouble(uTemp);
		riPS0(inst.FD) = value;
		riPS1(inst.FD) = value;
		m_GPR[inst.RA] = uAddress;
	}
}

void Interpreter::lfsx(UGeckoInstruction inst)
{
	u32 uTemp = Memory::Read_U32(Helper_Get_EA_X(inst));
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		u64 value = ConvertToDouble(uTemp);
		riPS0(inst.FD) = value;
		riPS1(inst.FD) = value;
	}
}

void Interpreter::lha(UGeckoInstruction inst)
{
	u32 temp = (u32)(s32)(s16)Memory::Read_U16(Helper_Get_EA(inst));
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RD] = temp;
	}
}

void Interpreter::lhau(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA_U(inst);
	u32 temp = (u32)(s32)(s16)Memory::Read_U16(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RD] = temp;
		m_GPR[inst.RA] = uAddress;
	}
}

void Interpreter::lhz(UGeckoInstruction inst)
{
	u32 temp = (u32)(u16)Memory::Read_U16(Helper_Get_EA(inst));
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RD] = temp;
	}
}

void Interpreter::lhzu(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA_U(inst);
	u32 temp = (u32)(u16)Memory::Read_U16(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RD] = temp;
		m_GPR[inst.RA] = uAddress;
	}
}

// FIXME: lmw should do a total rollback if a DSI occurs
void Interpreter::lmw(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA(inst);
	for (int iReg = inst.RD; iReg <= 31; iReg++, uAddress += 4)
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
void Interpreter::stmw(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA(inst);
	for (int iReg = inst.RS; iReg <= 31; iReg++, uAddress+=4)
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

void Interpreter::lwz(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA(inst);
	u32 temp = Memory::Read_U32(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RD] = temp;
	}

	// hack to detect SelectThread loop
	// should probably run a pass through memory instead before execution
	// but that would be dangerous

	// Enable idle skipping?
	/*
	if ((inst.hex & 0xFFFF0000)==0x800D0000 &&
		Memory::ReadUnchecked_U32(PC+4)==0x28000000 &&
		Memory::ReadUnchecked_U32(PC+8)==0x4182fff8)
	{
		if (CommandProcessor::AllowIdleSkipping() && PixelEngine::AllowIdleSkipping())
		{
			CoreTiming::Idle();
		}
	}*/
}

void Interpreter::lwzu(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA_U(inst);
	u32 temp = Memory::Read_U32(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RD] = temp;
		m_GPR[inst.RA] = uAddress;
	}
}

void Interpreter::stb(UGeckoInstruction inst)
{
	Memory::Write_U8((u8)m_GPR[inst.RS], Helper_Get_EA(inst));
}

void Interpreter::stbu(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA_U(inst);
	Memory::Write_U8((u8)m_GPR[inst.RS], uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RA] = uAddress;
	}
}

void Interpreter::stfd(UGeckoInstruction inst)
{
	Memory::Write_U64(riPS0(inst.FS), Helper_Get_EA(inst));
}

void Interpreter::stfdu(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA_U(inst);
	Memory::Write_U64(riPS0(inst.FS), uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RA] = uAddress;
	}
}

void Interpreter::stfs(UGeckoInstruction inst)
{
	Memory::Write_U32(ConvertToSingle(riPS0(inst.FS)), Helper_Get_EA(inst));
}

void Interpreter::stfsu(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA_U(inst);
	Memory::Write_U32(ConvertToSingle(riPS0(inst.FS)), uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RA] = uAddress;
	}
}

void Interpreter::sth(UGeckoInstruction inst)
{
	Memory::Write_U16((u16)m_GPR[inst.RS], Helper_Get_EA(inst));
}

void Interpreter::sthu(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA_U(inst);
	Memory::Write_U16((u16)m_GPR[inst.RS], uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RA] = uAddress;
	}
}

void Interpreter::stw(UGeckoInstruction inst)
{
	Memory::Write_U32(m_GPR[inst.RS], Helper_Get_EA(inst));
}

void Interpreter::stwu(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA_U(inst);
	Memory::Write_U32(m_GPR[inst.RS], uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RA] = uAddress;
	}
}

void Interpreter::dcba(UGeckoInstruction inst)
{
	_assert_msg_(POWERPC,0,"dcba - Not implemented - not a Gekko instruction");
}

void Interpreter::dcbf(UGeckoInstruction inst)
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
		u32 address = Helper_Get_EA_X(inst);
		JitInterface::InvalidateICache(address & ~0x1f, 32);
}

void Interpreter::dcbi(UGeckoInstruction inst)
{
	// Removes a block from data cache. Since we don't emulate the data cache, we don't need to do anything to the data cache
	// However, we invalidate the jit block cache on dcbi
		u32 address = Helper_Get_EA_X(inst);
		JitInterface::InvalidateICache(address & ~0x1f, 32);
}

void Interpreter::dcbst(UGeckoInstruction inst)
{
	// Cache line flush. Since we don't emulate the data cache, we don't need to do anything.
	// Invalidate the jit block cache on dcbst in case new code has been loaded via the data cache
		u32 address = Helper_Get_EA_X(inst);
		JitInterface::InvalidateICache(address & ~0x1f, 32);
}

void Interpreter::dcbt(UGeckoInstruction inst)
{
	// Prefetch. Since we don't emulate the data cache, we don't need to do anything.
}

void Interpreter::dcbtst(UGeckoInstruction inst)
{
	// This is just some sort of store "prefetching".
	// Since we don't emulate the data cache, we don't need to do anything.
}

void Interpreter::dcbz(UGeckoInstruction inst)
{
	// HACK but works... we think
	if (!Core::g_CoreStartupParameter.bDCBZOFF)
		Memory::Memset(Helper_Get_EA_X(inst) & (~31), 0, 32);
	if (!JitInterface::GetCore())
		PowerPC::CheckExceptions();
}

// eciwx/ecowx technically should access the specified device
// We just do it instantly from ppc...and hey, it works! :D
void Interpreter::eciwx(UGeckoInstruction inst)
{
	u32 EA, b;
	if (inst.RA == 0)
		b = 0;
	else
		b = m_GPR[inst.RA];
	EA = b + m_GPR[inst.RB];

	if (!(PowerPC::ppcState.spr[SPR_EAR] & 0x80000000))
	{
		Common::AtomicOr(PowerPC::ppcState.Exceptions, EXCEPTION_DSI);
	}
	if (EA & 3)
		Common::AtomicOr(PowerPC::ppcState.Exceptions, EXCEPTION_ALIGNMENT);

// 	_assert_msg_(POWERPC,0,"eciwx - fill r%i with word @ %08x from device %02x",
// 		inst.RS, EA, PowerPC::ppcState.spr[SPR_EAR] & 0x1f);

	m_GPR[inst.RS] = Memory::Read_U32(EA);
}

void Interpreter::ecowx(UGeckoInstruction inst)
{
	u32 EA, b;
	if (inst.RA == 0)
		b = 0;
	else
		b = m_GPR[inst.RA];
	EA = b + m_GPR[inst.RB];

	if (!(PowerPC::ppcState.spr[SPR_EAR] & 0x80000000))
	{
		Common::AtomicOr(PowerPC::ppcState.Exceptions, EXCEPTION_DSI);
	}
	if (EA & 3)
		Common::AtomicOr(PowerPC::ppcState.Exceptions, EXCEPTION_ALIGNMENT);

// 	_assert_msg_(POWERPC,0,"ecowx - send stw request (%08x@%08x) to device %02x",
// 		m_GPR[inst.RS], EA, PowerPC::ppcState.spr[SPR_EAR] & 0x1f);

	Memory::Write_U32(m_GPR[inst.RS], EA);
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
	u32 address = Helper_Get_EA_X(inst);
	PowerPC::ppcState.iCache.Invalidate(address);
}

void Interpreter::lbzux(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA_UX(inst);
	u32 temp = (u32)Memory::Read_U8(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RD] = temp;
		m_GPR[inst.RA] = uAddress;
	}
}

void Interpreter::lbzx(UGeckoInstruction inst)
{
	u32 temp = (u32)Memory::Read_U8(Helper_Get_EA_X(inst));
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RD] = temp;
	}
}

void Interpreter::lhaux(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA_UX(inst);
	s32 temp = (s32)(s16)Memory::Read_U16(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RD] = temp;
		m_GPR[inst.RA] = uAddress;
	}
}

void Interpreter::lhax(UGeckoInstruction inst)
{
	s32 temp = (s32)(s16)Memory::Read_U16(Helper_Get_EA_X(inst));
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RD] = temp;
	}
}

void Interpreter::lhbrx(UGeckoInstruction inst)
{
	u32 temp = (u32)Common::swap16(Memory::Read_U16(Helper_Get_EA_X(inst)));
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RD] = temp;
	}
}

void Interpreter::lhzux(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA_UX(inst);
	u32 temp = (u32)Memory::Read_U16(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RD] = temp;
		m_GPR[inst.RA] = uAddress;
	}
}

void Interpreter::lhzx(UGeckoInstruction inst)
{
	u32 temp = (u32)Memory::Read_U16(Helper_Get_EA_X(inst));
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RD] = temp;
	}
}

// TODO: is this right?
// FIXME: Should rollback if a DSI occurs
void Interpreter::lswx(UGeckoInstruction inst)
{
	u32 EA = Helper_Get_EA_X(inst);
	u32 n = rSPR(SPR_XER) & 0x7F;
	int r = inst.RD;
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

void Interpreter::lwbrx(UGeckoInstruction inst)
{
	u32 temp = Common::swap32(Memory::Read_U32(Helper_Get_EA_X(inst)));
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RD] = temp;
	}
}

void Interpreter::lwzux(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA_UX(inst);
	u32 temp = Memory::Read_U32(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RD] = temp;
		m_GPR[inst.RA] = uAddress;
	}
}

void Interpreter::lwzx(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA_X(inst);
	u32 temp = Memory::Read_U32(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RD] = temp;
	}
}

void Interpreter::stbux(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA_UX(inst);
	Memory::Write_U8((u8)m_GPR[inst.RS], uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RA] = uAddress;
	}
}

void Interpreter::stbx(UGeckoInstruction inst)
{
	Memory::Write_U8((u8)m_GPR[inst.RS], Helper_Get_EA_X(inst));
}

void Interpreter::stfdux(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA_UX(inst);
	Memory::Write_U64(riPS0(inst.FS), uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RA] = uAddress;
	}
}

void Interpreter::stfdx(UGeckoInstruction inst)
{
	Memory::Write_U64(riPS0(inst.FS), Helper_Get_EA_X(inst));
}

// __________________________________________________________________________________________________
// stfiwx
// TODO - examine what this really does
// Stores Floating points into Integers indeXed
void Interpreter::stfiwx(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA_X(inst);

	Memory::Write_U32((u32)riPS0(inst.FS), uAddress);
}


void Interpreter::stfsux(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA_UX(inst);
	Memory::Write_U32(ConvertToSingle(riPS0(inst.FS)), uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RA] = uAddress;
	}
}

void Interpreter::stfsx(UGeckoInstruction inst)
{
	Memory::Write_U32(ConvertToSingle(riPS0(inst.FS)), Helper_Get_EA_X(inst));
}

void Interpreter::sthbrx(UGeckoInstruction inst)
{
	Memory::Write_U16(Common::swap16((u16)m_GPR[inst.RS]), Helper_Get_EA_X(inst));
}

void Interpreter::sthux(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA_UX(inst);
	Memory::Write_U16((u16)m_GPR[inst.RS], uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RA] = uAddress;
	}
}

void Interpreter::sthx(UGeckoInstruction inst)
{
	Memory::Write_U16((u16)m_GPR[inst.RS], Helper_Get_EA_X(inst));
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
		EA = m_GPR[inst.RA];

	u32 n;
	if (inst.NB == 0)
		n = 32;
	else
		n = inst.NB;

	int r = inst.RD - 1;
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
void Interpreter::stswi(UGeckoInstruction inst)
{
	u32 EA;
	if (inst.RA == 0)
		EA = 0;
	else
		EA = m_GPR[inst.RA];

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
void Interpreter::stswx(UGeckoInstruction inst)
{
	u32 EA = Helper_Get_EA_X(inst);
	u32 n = rSPR(SPR_XER) & 0x7F;
	int r = inst.RS;
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

void Interpreter::stwbrx(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA_X(inst);
	Memory::Write_U32(Common::swap32(m_GPR[inst.RS]), uAddress);
}


// The following two instructions are for SMP communications. On a single
// CPU, they cannot fail unless an interrupt happens in between.

void Interpreter::lwarx(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA_X(inst);
	u32 temp = Memory::Read_U32(uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RD] = temp;
		g_bReserve = true;
		g_reserveAddr = uAddress;
	}
}

void Interpreter::stwcxd(UGeckoInstruction inst)
{
	// Stores Word Conditional indeXed
	u32 uAddress;
	if (g_bReserve) {
		uAddress = Helper_Get_EA_X(inst);
		if (uAddress == g_reserveAddr) {
			Memory::Write_U32(m_GPR[inst.RS], uAddress);
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

void Interpreter::stwux(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA_UX(inst);
	Memory::Write_U32(m_GPR[inst.RS], uAddress);
	if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))
	{
		m_GPR[inst.RA] = uAddress;
	}
}

void Interpreter::stwx(UGeckoInstruction inst)
{
	u32 uAddress = Helper_Get_EA_X(inst);
	Memory::Write_U32(m_GPR[inst.RS], uAddress);
}

void Interpreter::sync(UGeckoInstruction inst)
{
	//ignored
}

void Interpreter::tlbia(UGeckoInstruction inst)
{
	// Gekko does not support this instructions.
	PanicAlert("The GC CPU does not support tlbia");
	// invalid the whole TLB
	//MessageBox(0,"TLBIA","TLBIA",0);
}

void Interpreter::tlbie(UGeckoInstruction inst)
{
	// Invalidate TLB entry
	u32 Address = m_GPR[inst.RB];
	Memory::InvalidateTLBEntry(Address);
}

void Interpreter::tlbsync(UGeckoInstruction inst)
{
	//MessageBox(0,"TLBsync","TLBsyncE",0);
}
