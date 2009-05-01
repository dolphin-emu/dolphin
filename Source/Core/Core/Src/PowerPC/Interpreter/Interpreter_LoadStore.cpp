// Copyright (C) 2003-2009 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "../../HW/Memmap.h"
#include "../../HW/CommandProcessor.h"
#include "../../HW/PixelEngine.h"

#include "Interpreter.h"
#include "../../Core.h"

#include "../Jit64/Jit.h"
#include "../JitCommon/JitCache.h"

namespace Interpreter
{

// TODO: These should really be in the save state, although it's unlikely to matter much.
// They are for lwarx and its friend stwcxd.
static bool g_bReserve = false;
static u32  g_reserveAddr;

u32 Helper_Get_EA(const UGeckoInstruction _inst)
{
	return _inst.RA ? (m_GPR[_inst.RA] + _inst.SIMM_16) : _inst.SIMM_16;
}

u32 Helper_Get_EA_U(const UGeckoInstruction _inst)
{
	return (m_GPR[_inst.RA] + _inst.SIMM_16);
}

u32 Helper_Get_EA_X(const UGeckoInstruction _inst)
{
	return _inst.RA ? (m_GPR[_inst.RA] + m_GPR[_inst.RB]) : m_GPR[_inst.RB];
}

u32 Helper_Get_EA_UX(const UGeckoInstruction _inst)
{
	return (m_GPR[_inst.RA] + m_GPR[_inst.RB]);
}

void lbz(UGeckoInstruction _inst)
{
	m_GPR[_inst.RD] = (u32)Memory::Read_U8(Helper_Get_EA(_inst));
}

void lbzu(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_U(_inst);
	m_GPR[_inst.RD] = (u32)Memory::Read_U8(uAddress);
	m_GPR[_inst.RA] = uAddress;
}

void lfd(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = Memory::Read_U64(Helper_Get_EA(_inst));
}

void lfdu(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_U(_inst);
	riPS0(_inst.FD) = Memory::Read_U64(uAddress);
	m_GPR[_inst.RA]  = uAddress;
}

void lfdux(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_UX(_inst);
	riPS0(_inst.FD) = Memory::Read_U64(uAddress);
	m_GPR[_inst.RA] = uAddress;
}

void lfdx(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = Memory::Read_U64(Helper_Get_EA_X(_inst));
}

void lfs(UGeckoInstruction _inst)
{
	u32 uTemp = Memory::Read_U32(Helper_Get_EA(_inst));
	rPS0(_inst.FD) = *(float*)&uTemp;
	rPS1(_inst.FD) = rPS0(_inst.FD);
}

void lfsu(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_U(_inst);
	u32 uTemp = Memory::Read_U32(uAddress);
	rPS0(_inst.FD) = *(float*)&uTemp;
	rPS1(_inst.FD) = rPS0(_inst.FD);
	m_GPR[_inst.RA] = uAddress;
}

void lfsux(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_UX(_inst);
	u32 uTemp = Memory::Read_U32(uAddress);
	rPS0(_inst.FD) = *(float*)&uTemp;
	rPS1(_inst.FD) = rPS0(_inst.FD);
	m_GPR[_inst.RA] = uAddress;
}

void lfsx(UGeckoInstruction _inst)
{
	u32 uTemp = Memory::Read_U32(Helper_Get_EA_X(_inst));
	rPS0(_inst.FD) = *(float*)&uTemp;
	rPS1(_inst.FD) = rPS0(_inst.FD);
}

void lha(UGeckoInstruction _inst)
{
	m_GPR[_inst.RD] = (u32)(s32)(s16)Memory::Read_U16(Helper_Get_EA(_inst));
}

void lhau(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_U(_inst);
	m_GPR[_inst.RD] = (u32)(s32)(s16)Memory::Read_U16(uAddress);
	m_GPR[_inst.RA] = uAddress;
}

void lhz(UGeckoInstruction _inst)
{
	m_GPR[_inst.RD] = (u32)(u16)Memory::Read_U16(Helper_Get_EA(_inst));
}

void lhzu(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_U(_inst);
	m_GPR[_inst.RD] = (u32)(u16)Memory::Read_U16(uAddress);
	m_GPR[_inst.RA] = uAddress;
}

void lmw(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA(_inst);
	for (int iReg = _inst.RD; iReg <= 31; iReg++, uAddress += 4)
	{
		u32 TempReg = Memory::Read_U32(uAddress);		
		if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
		{
			PanicAlert("DSI exception in lmv.");
			return;
		}

		m_GPR[iReg] = TempReg;
	}
}

void stmw(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA(_inst);
	for (int iReg = _inst.RS; iReg <= 31; iReg++, uAddress+=4)
	{		
		Memory::Write_U32(m_GPR[iReg], uAddress);
		if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
			return;
	}
}

void lwz(UGeckoInstruction _inst)
{ 
	u32 uAddress = Helper_Get_EA(_inst);
	m_GPR[_inst.RD] = Memory::Read_U32(uAddress);

	// hack to detect SelectThread loop
	// should probably run a pass through memory instead before execution
	// but that would be dangerous

	// Enable idle skipping?
	/*
	if ((_inst.hex & 0xFFFF0000)==0x800D0000 &&
		Memory::ReadUnchecked_U32(PC+4)==0x28000000 &&
		Memory::ReadUnchecked_U32(PC+8)==0x4182fff8)
	{
		if (CommandProcessor::AllowIdleSkipping() && PixelEngine::AllowIdleSkipping())
		{
			CoreTiming::Idle();
		}
	}*/
}

void lwzu(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_U(_inst);
	m_GPR[_inst.RD] = Memory::Read_U32(uAddress);
	m_GPR[_inst.RA] = uAddress;
}

void stb(UGeckoInstruction _inst)
{
	Memory::Write_U8((u8)m_GPR[_inst.RS], Helper_Get_EA(_inst));
}

void stbu(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_U(_inst);
	Memory::Write_U8((u8)m_GPR[_inst.RS], uAddress);
	m_GPR[_inst.RA] = uAddress;
}

void stfd(UGeckoInstruction _inst)
{
	Memory::Write_U64(riPS0(_inst.FS), Helper_Get_EA(_inst));
}

void stfdu(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_U(_inst);
	Memory::Write_U64(riPS0(_inst.FS), uAddress);
	m_GPR[_inst.RA] = uAddress;
}

void stfs(UGeckoInstruction _inst)
{
	float fTemp = (float)rPS0(_inst.FS);
	Memory::Write_U32(*(u32*)&fTemp, Helper_Get_EA(_inst));
}

void stfsu(UGeckoInstruction _inst)
{
	float fTemp = (float)rPS0(_inst.FS);
	u32 uAddress = Helper_Get_EA_U(_inst);
	Memory::Write_U32(*(u32*)&fTemp, uAddress);
	m_GPR[_inst.RA] = uAddress;
}

void sth(UGeckoInstruction _inst)
{
	Memory::Write_U16((u16)m_GPR[_inst.RS], Helper_Get_EA(_inst));
}

void sthu(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_U(_inst);
	Memory::Write_U16((u16)m_GPR[_inst.RS], uAddress);
	m_GPR[_inst.RA] = uAddress;
}

void stw(UGeckoInstruction _inst)
{
	Memory::Write_U32(m_GPR[_inst.RS], Helper_Get_EA(_inst));
}

void stwu(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_U(_inst);
	Memory::Write_U32(m_GPR[_inst.RS], uAddress);
	m_GPR[_inst.RA] = uAddress;
}

void dcba(UGeckoInstruction _inst)
{
	_assert_msg_(POWERPC,0,"dcba - Not implemented - not a Gekko instruction");
}

void dcbf(UGeckoInstruction _inst)
{
	//This should tell GFX plugin to throw out any cached data here
	// !!! SPEEDUP HACK for OSProtectRange !!!
/*	u32 tmp1 = Memory::Read_U32(PC+4);
	u32 tmp2 = Memory::Read_U32(PC+8);

	if ((tmp1 == 0x38630020) && 
		(tmp2 == 0x4200fff8))
	{
		NPC = PC + 12;
	}*/
}

void dcbi(UGeckoInstruction _inst)
{
	//Used during initialization
	//_assert_msg_(POWERPC,0,"dcbi - Not implemented");
}

void dcbst(UGeckoInstruction _inst)
{
	//_assert_msg_(POWERPC,0,"dcbst - Not implemented");
}

void dcbt(UGeckoInstruction _inst)
{
	//This should tell GFX plugin to throw out any cached data here
	//Used by Ikaruga
	//_assert_msg_(POWERPC,0,"dcbt - Not implemented");
}

void dcbtst(UGeckoInstruction _inst)
{
	_assert_msg_(POWERPC,0,"dcbtst - Not implemented");
}

// __________________________________________________________________________________________________
// dcbz
// TODO(ector) check docs
void dcbz(UGeckoInstruction _inst)
{
    // !!! after the dcbz follows a dcbf... dont clear the memory in this case !!!
    // 0x81330c2c
    u32 NextOpcode = Memory::Read_U32(PC+4);
    if (NextOpcode == 0x7C0400AC)
    {
        return;
    }

	// HACK but works... we think
	Memory::Memset(Helper_Get_EA_X(_inst) & (~31), 0, 32);
}
 
void eciwx(UGeckoInstruction _inst)
{
	_assert_msg_(POWERPC,0,"eciwx - Not implemented"); 
}

void ecowx(UGeckoInstruction _inst)
{
	_assert_msg_(POWERPC,0,"ecowx - Not implemented"); 
}

void eieio(UGeckoInstruction _inst)
{
	_assert_msg_(POWERPC,0,"eieio - Not implemented"); 
}

void icbi(UGeckoInstruction _inst)
{
	u32 address = Helper_Get_EA_X(_inst);
	// block size seems to be 0x20
	address &= ~0x1f;

	// this comment is slightly outdated but still relevant:
	// Inform the JIT to kill off this area of code NOW
	// VERY IMPORTANT when we start linking blocks
	// There are a TON of these so hopefully we can make this mechanism
	// fast in the JIT
	jit.GetBlockCache()->InvalidateCodeRange(address, 0x20);
}

void lbzux(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_UX(_inst);
	m_GPR[_inst.RD] = (u32)Memory::Read_U8(uAddress);
	m_GPR[_inst.RA] = uAddress;
}

void lbzx(UGeckoInstruction _inst)
{
	m_GPR[_inst.RD] = (u32)Memory::Read_U8(Helper_Get_EA_X(_inst));
}

void lhaux(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_UX(_inst);
	m_GPR[_inst.RD] = (s32)(s16)Memory::Read_U16(uAddress);
	m_GPR[_inst.RA] = uAddress;
}

void lhax(UGeckoInstruction _inst)
{
	m_GPR[_inst.RD] = (s32)(s16)Memory::Read_U16(Helper_Get_EA_X(_inst));
}

void lhbrx(UGeckoInstruction _inst)
{
	m_GPR[_inst.RD] = (u32)Common::swap16(Memory::Read_U16(Helper_Get_EA_X(_inst)));
}

void lhzux(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_UX(_inst);
	m_GPR[_inst.RD] = (u32)Memory::Read_U16(uAddress);
	m_GPR[_inst.RA] = uAddress;
}

void lhzx(UGeckoInstruction _inst)
{
	m_GPR[_inst.RD] = (u32)Memory::Read_U16(Helper_Get_EA_X(_inst));
}

void lswx(UGeckoInstruction _inst)
{
	static bool bFirst = true;
	if (bFirst)
		PanicAlert("lswx - Instruction unimplemented");
	bFirst = false;
}

void lwbrx(UGeckoInstruction _inst)
{
	m_GPR[_inst.RD] = Common::swap32(Memory::Read_U32(Helper_Get_EA_X(_inst)));
}

void lwzux(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_UX(_inst);
	m_GPR[_inst.RD] = Memory::Read_U32(uAddress);
	m_GPR[_inst.RA] = uAddress;
}

void lwzx(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_X(_inst);
	m_GPR[_inst.RD] = Memory::Read_U32(uAddress);
}

void stbux(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_UX(_inst);
	Memory::Write_U8((u8)m_GPR[_inst.RS], uAddress);
	m_GPR[_inst.RA] = uAddress;
}

void stbx(UGeckoInstruction _inst)
{
	Memory::Write_U8((u8)m_GPR[_inst.RS], Helper_Get_EA_X(_inst));
}

void stfdux(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_UX(_inst);
	Memory::Write_U64(riPS0(_inst.FS), uAddress);
	m_GPR[_inst.RA] = uAddress;
}

void stfdx(UGeckoInstruction _inst)
{
	Memory::Write_U64(riPS0(_inst.FS), Helper_Get_EA_X(_inst));
}

// __________________________________________________________________________________________________
// stfiwx
// TODO - examine what this really does
// Stores Floating points into Integers indeXed
void stfiwx(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_X(_inst);

	Memory::Write_U32((u32)riPS0(_inst.FS), uAddress);
}

// __________________________________________________________________________________________________
// stfsux
//
// no paired ??
//
void stfsux(UGeckoInstruction _inst)
{
	float fTemp = (float)rPS0(_inst.FS);
	u32 uAddress = Helper_Get_EA_UX(_inst);
	Memory::Write_U32(*(u32*)&fTemp, uAddress);
	m_GPR[_inst.RA] = uAddress;
}

// __________________________________________________________________________________________________
// stfsx
//
// no paired ??
//
void stfsx(UGeckoInstruction _inst)
{
	float fTemp = (float)rPS0(_inst.FS);
	Memory::Write_U32(*(u32 *)&fTemp, Helper_Get_EA_X(_inst));
}

void sthbrx(UGeckoInstruction _inst)
{
	Memory::Write_U16(Common::swap16((u16)m_GPR[_inst.RS]), Helper_Get_EA_X(_inst));
}

void sthux(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_UX(_inst);
	Memory::Write_U16((u16)m_GPR[_inst.RS], uAddress);
	m_GPR[_inst.RA] = uAddress;
}

void sthx(UGeckoInstruction _inst)
{
	Memory::Write_U16((u16)m_GPR[_inst.RS], Helper_Get_EA_X(_inst));
}

// __________________________________________________________________________________________________
// lswi - bizarro string instruction
//
void lswi(UGeckoInstruction _inst)
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
//
void stswi(UGeckoInstruction _inst)
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
			return;

		i += 8;
		if (i == 32)
			i = 0;
		EA++;
		n--;
	}
}

void stswx(UGeckoInstruction _inst)
{
	static bool bFirst = true;
	if (bFirst)
		PanicAlert("stswx - Instruction unimplemented");
	bFirst = false;
}

void stwbrx(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_X(_inst);
	Memory::Write_U32(Common::swap32(m_GPR[_inst.RS]), uAddress);
}


// The following two instructions are for SMP communications. On a single
// CPU, they cannot fail unless an interrupt happens in between.
    
void lwarx(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_X(_inst);
	m_GPR[_inst.RD] = Memory::Read_U32(uAddress);

	g_bReserve = true;
	g_reserveAddr = uAddress;
}

void stwcxd(UGeckoInstruction _inst)
{
	// Stores Word Conditional indeXed
	u32 uAddress;
	if (g_bReserve) {
		uAddress = Helper_Get_EA_X(_inst);
		if (uAddress == g_reserveAddr) {
			Memory::Write_U32(m_GPR[_inst.RS], uAddress);
			g_bReserve = false;
			SetCRField(0, 2 | GetXER_SO());
			return;
		}
	}

	SetCRField(0, GetXER_SO());
}

void stwux(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_UX(_inst);
	Memory::Write_U32(m_GPR[_inst.RS], uAddress);
	m_GPR[_inst.RA] = uAddress;
}

void stwx(UGeckoInstruction _inst)
{
	u32 uAddress = Helper_Get_EA_X(_inst);
	Memory::Write_U32(m_GPR[_inst.RS], uAddress);
}

void sync(UGeckoInstruction _inst)
{
	//ignored
}

void tlbia(UGeckoInstruction _inst)
{
	// Gekko does not support this instructions.
	PanicAlert("The GC CPU does not support tlbia");
	// invalid the whole TLB 
	//MessageBox(0,"TLBIA","TLBIA",0);
}

void tlbie(UGeckoInstruction _inst)
{
	// invalid entry
        // int entry = _inst.RB;

	//MessageBox(0,"TLBIE","TLBIE",0);
}

void tlbsync(UGeckoInstruction _inst)
{
	//MessageBox(0,"TLBsync","TLBsyncE",0);
}

}  // namespace
