// Copyright (C) 2003-2008 Dolphin Project.

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

#include "Interpreter.h"
#include "../../Core.h"

#ifndef _WIN32

inline u32 _rotl(u32 x, int shift) {
    return (x << shift) | (x >> (32 - shift));
}

#endif


void CInterpreter::Helper_UpdateCR0(u32 _uValue)
{
	u32 Flags = 0;
	int sValue = (int)_uValue;
	if (sValue > 0)
		Flags |= 0x40000000;
	else if (sValue < 0)
		Flags |= 0x80000000;
	else
		Flags |= 0x20000000;
	Flags |= PowerPC::ppcState.spr[SPR_XER*4] & 0x10000000;
	PowerPC::ppcState.cr = (PowerPC::ppcState.cr & 0xFFFFFFF) | Flags;
}

void CInterpreter::Helper_UpdateCRx(int _x, u32 _uValue)
{
	int shiftamount = _x*4;
	int crmask = 0xFFFFFFFF ^ (0xF0000000 >> shiftamount);

	u32 Flags = 0;
	int sValue = (int)_uValue;
	if (sValue > 0)
		Flags |= 0x40000000;
	else if (sValue < 0)
		Flags |= 0x80000000;
	else
		Flags |= 0x20000000;
	Flags |= PowerPC::ppcState.spr[SPR_XER*4] & 0x10000000;
	PowerPC::ppcState.cr = (PowerPC::ppcState.cr & crmask) | (Flags >> shiftamount);
}

u32 CInterpreter::Helper_Carry(u32 _uValue1, u32 _uValue2)
{
	return _uValue2 > (~_uValue1);
}

u32 CInterpreter::Helper_Mask(int mb, int me)
{
	//first make 001111111111111 part
	u32 begin = 0xFFFFFFFF >> mb;
	//then make 000000000001111 part, which is used to flip the bits of the first one
	u32 end = me < 31 ? (0xFFFFFFFF >> (me + 1)) : 0;
	//do the bitflip
	u32 mask = begin ^ end;
	//and invert if backwards
	if (me < mb) 
		return ~mask;
	else
		return mask;
}

void CInterpreter::addi(UGeckoInstruction _inst)
{
	if (_inst.RA)
		m_GPR[_inst.RD] = m_GPR[_inst.RA] + _inst.SIMM_16;
	else
		m_GPR[_inst.RD] = _inst.SIMM_16;
}

void CInterpreter::addic(UGeckoInstruction _inst)
{
	u32 a = m_GPR[_inst.RA];
	u32 imm = (u32)(s32)_inst.SIMM_16;
	// TODO(ector): verify this thing 
	m_GPR[_inst.RD] = a + imm;
	SetCarry(Helper_Carry(a, imm));
}

void CInterpreter::addic_rc(UGeckoInstruction _inst)
{
	addic(_inst);
	Helper_UpdateCR0(m_GPR[_inst.RD]);
}

void CInterpreter::addis(UGeckoInstruction _inst)
{
	if (_inst.RA)
		m_GPR[_inst.RD] = m_GPR[_inst.RA] + (_inst.SIMM_16 << 16);
	else
		m_GPR[_inst.RD] = (_inst.SIMM_16 << 16);
}

void CInterpreter::andi_rc(UGeckoInstruction _inst)
{
	m_GPR[_inst.RA] = m_GPR[_inst.RS] & _inst.UIMM;
	Helper_UpdateCR0(m_GPR[_inst.RA]);
}

void CInterpreter::andis_rc(UGeckoInstruction _inst)
{
	m_GPR[_inst.RA] = m_GPR[_inst.RS] & ((u32)_inst.UIMM<<16);
	Helper_UpdateCR0(m_GPR[_inst.RA]);
}

void CInterpreter::cmpi(UGeckoInstruction _inst)
{
	Helper_UpdateCRx(_inst.CRFD, m_GPR[_inst.RA]-_inst.SIMM_16);
}

void CInterpreter::cmpli(UGeckoInstruction _inst)
{
	u32 a = m_GPR[_inst.RA];
	u32 b = _inst.UIMM;
	int f;
	if (a < b)      f = 0x8;
	else if (a > b) f = 0x4;
	else            f = 0x2; //equals
	if (XER.SO)     f = 0x1;
	SetCRField(_inst.CRFD, f);
}

void CInterpreter::mulli(UGeckoInstruction _inst)
{
	m_GPR[_inst.RD] = (s32)m_GPR[_inst.RA] * _inst.SIMM_16;
}

void CInterpreter::ori(UGeckoInstruction _inst)
{
	m_GPR[_inst.RA] = m_GPR[_inst.RS] | _inst.UIMM;
}

void CInterpreter::oris(UGeckoInstruction _inst)
{
	m_GPR[_inst.RA] = m_GPR[_inst.RS] | (_inst.UIMM << 16);
}

void CInterpreter::subfic(UGeckoInstruction _inst)
{
/*	u32 rra = ~m_GPR[_inst.RA];
	s32 immediate = (s16)_inst.SIMM_16 + 1;
	

//	#define CALC_XER_CA(X,Y) (((X) + (Y) < X) ? SET_XER_CA : CLEAR_XER_CA)
	if ((rra + immediate) < rra)
		XER.CA = 1;
	else 
		XER.CA = 0;

	m_GPR[_inst.RD] = rra - immediate;
*/

	s32 immediate = _inst.SIMM_16;
	m_GPR[_inst.RD] = immediate - (signed)m_GPR[_inst.RA];
	SetCarry((m_GPR[_inst.RA] == 0) || (Helper_Carry(0-m_GPR[_inst.RA], immediate)));
}

void CInterpreter::twi(UGeckoInstruction _inst) 
{
	bool bFirst = true;
	if (bFirst)
		PanicAlert("twi - Instruction unimplemented");

	bFirst = false;
}

void CInterpreter::xori(UGeckoInstruction _inst)
{
	m_GPR[_inst.RA] = m_GPR[_inst.RS] ^ _inst.UIMM;
}

void CInterpreter::xoris(UGeckoInstruction _inst)
{
	m_GPR[_inst.RA] = m_GPR[_inst.RS] ^ (_inst.UIMM << 16);
}

void CInterpreter::rlwimix(UGeckoInstruction _inst)
{
	u32 mask = Helper_Mask(_inst.MB,_inst.ME);
	m_GPR[_inst.RA] = (m_GPR[_inst.RA] & ~mask) | (_rotl(m_GPR[_inst.RS],_inst.SH) & mask);
	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RA]);
}

void CInterpreter::rlwinmx(UGeckoInstruction _inst)
{
	u32 mask = Helper_Mask(_inst.MB,_inst.ME);
	m_GPR[_inst.RA] = _rotl(m_GPR[_inst.RS],_inst.SH) & mask;
	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RA]);
}

void CInterpreter::rlwnmx(UGeckoInstruction _inst)
{
	u32 mask = Helper_Mask(_inst.MB,_inst.ME);
	m_GPR[_inst.RA] = _rotl(m_GPR[_inst.RS], m_GPR[_inst.RB] & 0x1F) & mask;

	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RA]);
}

void CInterpreter::andx(UGeckoInstruction _inst)
{
	m_GPR[_inst.RA] = m_GPR[_inst.RS] & m_GPR[_inst.RB];

	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RA]);
}

void CInterpreter::andcx(UGeckoInstruction _inst)
{
	m_GPR[_inst.RA] = m_GPR[_inst.RS] & ~m_GPR[_inst.RB];

	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RA]);
}

void CInterpreter::cmp(UGeckoInstruction _inst)
{
	s32 a = (s32)m_GPR[_inst.RA];
	s32 b = (s32)m_GPR[_inst.RB];
	int fTemp;
	if (a < b)  fTemp = 0x8;
	else if (a > b)  fTemp = 0x4;
	else if (a == b) fTemp = 0x2;
	if (XER.SO) PanicAlert("cmp getting overflow flag"); // fTemp |= 0x1
	SetCRField(_inst.CRFD, fTemp);
}

void CInterpreter::cmpl(UGeckoInstruction _inst)
{
	u32 a = m_GPR[_inst.RA];
	u32 b = m_GPR[_inst.RB];
	u32 fTemp;
	if (a < b)  fTemp = 0x8;
	else if (a > b)  fTemp = 0x4;
	else if (a == b) fTemp = 0x2;
	if (XER.SO) PanicAlert("cmpl getting overflow flag"); // fTemp |= 0x1;
	SetCRField(_inst.CRFD, fTemp);
}

void CInterpreter::cntlzwx(UGeckoInstruction _inst)
{
	u32 val = m_GPR[_inst.RS];
	u32 mask = 0x80000000;
	int i = 0;
	for (; i < 32; i++, mask >>= 1)
		if (val & mask) 
			break;
	m_GPR[_inst.RA] = i;
	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RA]);
}

void CInterpreter::eqvx(UGeckoInstruction _inst)
{
	m_GPR[_inst.RA] = ~(m_GPR[_inst.RS] ^ m_GPR[_inst.RB]);

	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RA]);
}

void CInterpreter::extsbx(UGeckoInstruction _inst)
{
	m_GPR[_inst.RA] = (u32)(s32)(s8)m_GPR[_inst.RS];

	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RA]);
}

void CInterpreter::extshx(UGeckoInstruction _inst)
{
	m_GPR[_inst.RA] = (u32)(s32)(s16)m_GPR[_inst.RS];

	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RA]);
}

void CInterpreter::nandx(UGeckoInstruction _inst)
{
	m_GPR[_inst.RA] = ~(m_GPR[_inst.RS] & m_GPR[_inst.RB]);

	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RA]);
}

void CInterpreter::norx(UGeckoInstruction _inst)
{
	m_GPR[_inst.RA] = ~(m_GPR[_inst.RS] | m_GPR[_inst.RB]);

	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RA]);
}

void CInterpreter::orx(UGeckoInstruction _inst)
{
	m_GPR[_inst.RA] = m_GPR[_inst.RS] | m_GPR[_inst.RB];

	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RA]);
}

void CInterpreter::orcx(UGeckoInstruction _inst)
{
	m_GPR[_inst.RA] = m_GPR[_inst.RS] | (~m_GPR[_inst.RB]);

	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RA]);
}

void CInterpreter::slwx(UGeckoInstruction _inst)
{
	// TODO(ector): wtf is this code?
/*	u32 amount = m_GPR[_inst.RB];
	m_GPR[_inst.RA] = (amount&0x20) ? 0 : m_GPR[_inst.RS] << amount;

	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RA]);
*/

	u32 nBits = m_GPR[_inst.RB];
	m_GPR[_inst.RA] = (nBits < 32) ? m_GPR[_inst.RS] << nBits : 0;

	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RA]);
}

void CInterpreter::srawx(UGeckoInstruction _inst)
{
	int rb = m_GPR[_inst.RB];
	if (rb & 0x20)
	{
		if (m_GPR[_inst.RS] & 0x80000000)
		{
			m_GPR[_inst.RA] = 0xFFFFFFFF;
			SetCarry(1);
		}
		else
		{
			m_GPR[_inst.RA] = 0x00000000;
			SetCarry(0);
		}
	}
	else
	{
		int amount = rb & 0x1f;
		if (amount == 0)
		{
			m_GPR[_inst.RA] = m_GPR[_inst.RS];
			SetCarry(0);
		}
		else
		{
			m_GPR[_inst.RA] = (u32)((s32)m_GPR[_inst.RS] >> amount);
			if (m_GPR[_inst.RS] & 0x80000000)
				SetCarry(1);
			else
				SetCarry(0);
		}
	}

	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RA]);
}

void CInterpreter::srawix(UGeckoInstruction _inst)
{
	int amount = _inst.SH;

	if (amount != 0)
	{
		s32 rrs = m_GPR[_inst.RS];
		m_GPR[_inst.RA] = rrs >> amount;

		if ((rrs < 0) && (rrs << (32 - amount)))
			SetCarry(1);
		else
			SetCarry(0);
	}
	else
	{
		SetCarry(0);
		m_GPR[_inst.RA] = m_GPR[_inst.RS];
	}

	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RA]);
}

void CInterpreter::srwx(UGeckoInstruction _inst)
{
	u32 amount = m_GPR[_inst.RB];
	m_GPR[_inst.RA] = (amount & 0x20) ? 0 : (m_GPR[_inst.RS] >> amount);

	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RA]);
}

void CInterpreter::tw(UGeckoInstruction _inst)  
{
	static bool bFirst = true;
	if (bFirst)
		PanicAlert("tw - Instruction unimplemented");
	bFirst = false;
}

void CInterpreter::xorx(UGeckoInstruction _inst)
{
	m_GPR[_inst.RA] = m_GPR[_inst.RS] ^ m_GPR[_inst.RB];

	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RA]);
}

void CInterpreter::addx(UGeckoInstruction _inst)
{
	m_GPR[_inst.RD] = m_GPR[_inst.RA] + m_GPR[_inst.RB];

	if (_inst.OE) PanicAlert("OE: addx");
	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RD]);
}

void CInterpreter::addcx(UGeckoInstruction _inst)
{
	u32 a = m_GPR[_inst.RA];
	u32 b = m_GPR[_inst.RB];
	m_GPR[_inst.RD] = a + b;
	SetCarry(Helper_Carry(a,b));

	if (_inst.OE) PanicAlert("OE: addcx");
	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RD]);
}

void CInterpreter::addex(UGeckoInstruction _inst)
{
	int carry = GetCarry();
	int a = m_GPR[_inst.RA];
	int b = m_GPR[_inst.RB];
	m_GPR[_inst.RD] = a + b + carry;
	SetCarry(Helper_Carry(a,b) || (carry != 0 && Helper_Carry(a + b, carry)));

	if (_inst.OE) PanicAlert("OE: addex");
	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RD]);
}

void CInterpreter::addmex(UGeckoInstruction _inst)
{
	int carry = GetCarry();
	int a = m_GPR[_inst.RA];
	m_GPR[_inst.RD] = a + carry - 1;
	SetCarry(Helper_Carry(a, carry-1));

	if (_inst.OE) PanicAlert("OE: addmex");
	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RD]);
}

void CInterpreter::addzex(UGeckoInstruction _inst)
{
	int carry = GetCarry();
	int a = m_GPR[_inst.RA];
	m_GPR[_inst.RD] = a + carry;
	SetCarry(Helper_Carry(a, carry));

	if (_inst.OE) PanicAlert("OE: addzex");
	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RD]);
}

void CInterpreter::divwx(UGeckoInstruction _inst)
{
	s32 a = m_GPR[_inst.RA];
	s32 b = m_GPR[_inst.RB];
	if (b == 0 || ((u32)a == 0x80000000 && b == -1))
	{
		if (_inst.OE) 
			PanicAlert("OE: divwx");
		//else PanicAlert("Div by zero", "divwux");
	}
	else
		m_GPR[_inst.RD] = (u32)(a / b);

	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RD]);
}


void CInterpreter::divwux(UGeckoInstruction _inst)
{
	u32 a = m_GPR[_inst.RA];
	u32 b = m_GPR[_inst.RB];

	if (b == 0 || (a == 0x80000000 && b == 0xFFFFFFFF))
	{
		if (_inst.OE) 
			PanicAlert("OE: divwux");
		//else PanicAlert("Div by zero", "divwux");
	}
	else
	{
		m_GPR[_inst.RD] = a / b;
		if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RD]);
	}
}

void CInterpreter::mulhwx(UGeckoInstruction _inst)
{
	u32 a = m_GPR[_inst.RA];
	u32 b = m_GPR[_inst.RB];
	u32 d = (u32)((u64)(((s64)(s32)a * (s64)(s32)b) ) >> 32); //wheeee
	m_GPR[_inst.RD] = d;
	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RD]);
}

void CInterpreter::mulhwux(UGeckoInstruction _inst)
{
	u32 a = m_GPR[_inst.RA];
	u32 b = m_GPR[_inst.RB];
	u32 d = (u32)(((u64)a * (u64)b) >> 32);
	m_GPR[_inst.RD] = d;
	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RD]);
}

void CInterpreter::mullwx(UGeckoInstruction _inst)
{
	u32 a = m_GPR[_inst.RA];
	u32 b = m_GPR[_inst.RB];
	u32 d = (u32)((s32)a * (s32)b);
	m_GPR[_inst.RD] = d;

	if (_inst.OE) PanicAlert("OE: mullwx");
	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RD]);
}

void CInterpreter::negx(UGeckoInstruction _inst)
{
	m_GPR[_inst.RD] = (~m_GPR[_inst.RA]) + 1;
	if (m_GPR[_inst.RD] == 0x80000000)
	{
		if (_inst.OE) PanicAlert("OE: negx");
	}
	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RD]);
}

void CInterpreter::subfx(UGeckoInstruction _inst)
{
	m_GPR[_inst.RD] = m_GPR[_inst.RB] - m_GPR[_inst.RA];

	if (_inst.OE) PanicAlert("OE: subfx");
	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RD]);
}

void CInterpreter::subfcx(UGeckoInstruction _inst)
{
	u32 a = m_GPR[_inst.RA];
	u32 b = m_GPR[_inst.RB];
	m_GPR[_inst.RD] = b - a;
	SetCarry(a == 0 || Helper_Carry(b, 0-a));

	if (_inst.OE) PanicAlert("OE: subfcx");
	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RD]);
}

void CInterpreter::subfex(UGeckoInstruction _inst)
{
	u32 a = m_GPR[_inst.RA];
	u32 b = m_GPR[_inst.RB];
	int carry = GetCarry();
	m_GPR[_inst.RD] = (~a) + b + carry;
	SetCarry(Helper_Carry(~a, b) || Helper_Carry((~a) + b, carry));

	if (_inst.OE) PanicAlert("OE: subfcx");
	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RD]);
}

// sub from minus one
void CInterpreter::subfmex(UGeckoInstruction _inst)
{
	u32 a = m_GPR[_inst.RA];
	int carry = GetCarry();
	m_GPR[_inst.RD] = (~a) + carry - 1;
	SetCarry(Helper_Carry(~a, carry - 1));

	if (_inst.OE) PanicAlert("OE: subfmex");
	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RD]);
}

// sub from zero
void CInterpreter::subfzex(UGeckoInstruction _inst)
{
	u32 a = m_GPR[_inst.RA];
	int carry = GetCarry();
	m_GPR[_inst.RD] = (~a) + carry;
	SetCarry(Helper_Carry(~a, carry));

	if (_inst.OE) PanicAlert("OE: subfzex");
	if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RD]);
}
