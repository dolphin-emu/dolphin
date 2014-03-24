// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/PowerPC/Interpreter/Interpreter.h"

void Interpreter::Helper_UpdateCR0(u32 uValue)
{
	u32 new_cr0;
	int sValue = (int)uValue;
	if (sValue > 0)
		new_cr0 = 0x4;
	else if (sValue < 0)
		new_cr0 = 0x8;
	else
		new_cr0 = 0x2;
	new_cr0 |= GetXER_SO();
	SetCRField(0, new_cr0);
}

void Interpreter::Helper_UpdateCRx(int x, u32 uValue)
{
	u32 new_crX;
	int sValue = (int)uValue;
	if (sValue > 0)
		new_crX = 0x4;
	else if (sValue < 0)
		new_crX = 0x8;
	else
		new_crX = 0x2;
	new_crX |= GetXER_SO();
	SetCRField(x, new_crX);
}

u32 Interpreter::Helper_Carry(u32 uValue1, u32 uValue2)
{
	return uValue2 > (~uValue1);
}

u32 Interpreter::Helper_Mask(int mb, int me)
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

void Interpreter::addi(UGeckoInstruction inst)
{
	if (inst.RA)
		m_GPR[inst.RD] = m_GPR[inst.RA] + inst.SIMM_16;
	else
		m_GPR[inst.RD] = inst.SIMM_16;
}

void Interpreter::addic(UGeckoInstruction inst)
{
	u32 a = m_GPR[inst.RA];
	u32 imm = (u32)(s32)inst.SIMM_16;
	// TODO(ector): verify this thing
	m_GPR[inst.RD] = a + imm;
	SetCarry(Helper_Carry(a, imm));
}

void Interpreter::addic_rc(UGeckoInstruction inst)
{
	addic(inst);
	Helper_UpdateCR0(m_GPR[inst.RD]);
}

void Interpreter::addis(UGeckoInstruction inst)
{
	if (inst.RA)
		m_GPR[inst.RD] = m_GPR[inst.RA] + (inst.SIMM_16 << 16);
	else
		m_GPR[inst.RD] = (inst.SIMM_16 << 16);
}

void Interpreter::andi_rc(UGeckoInstruction inst)
{
	m_GPR[inst.RA] = m_GPR[inst.RS] & inst.UIMM;
	Helper_UpdateCR0(m_GPR[inst.RA]);
}

void Interpreter::andis_rc(UGeckoInstruction inst)
{
	m_GPR[inst.RA] = m_GPR[inst.RS] & ((u32)inst.UIMM<<16);
	Helper_UpdateCR0(m_GPR[inst.RA]);
}

void Interpreter::cmpi(UGeckoInstruction inst)
{
	Helper_UpdateCRx(inst.CRFD, m_GPR[inst.RA] - inst.SIMM_16);
}

void Interpreter::cmpli(UGeckoInstruction inst)
{
	u32 a = m_GPR[inst.RA];
	u32 b = inst.UIMM;
	int f;
	if (a < b)      f = 0x8;
	else if (a > b) f = 0x4;
	else            f = 0x2; //equals
	if (GetXER_SO()) f |= 0x1;
	SetCRField(inst.CRFD, f);
}

void Interpreter::mulli(UGeckoInstruction inst)
{
	m_GPR[inst.RD] = (s32)m_GPR[inst.RA] * inst.SIMM_16;
}

void Interpreter::ori(UGeckoInstruction inst)
{
	m_GPR[inst.RA] = m_GPR[inst.RS] | inst.UIMM;
}

void Interpreter::oris(UGeckoInstruction inst)
{
	m_GPR[inst.RA] = m_GPR[inst.RS] | (inst.UIMM << 16);
}

void Interpreter::subfic(UGeckoInstruction inst)
{
/*	u32 rra = ~m_GPR[inst.RA];
	s32 immediate = (s16)inst.SIMM_16 + 1;

//	#define CALC_XER_CA(X,Y) (((X) + (Y) < X) ? SET_XER_CA : CLEAR_XER_CA)
	if ((rra + immediate) < rra)
		SetCarry(1);
	else
		SetCarry(0);

	m_GPR[inst.RD] = rra - immediate;
*/

	s32 immediate = inst.SIMM_16;
	m_GPR[inst.RD] = immediate - (signed)m_GPR[inst.RA];
	SetCarry((m_GPR[inst.RA] == 0) || (Helper_Carry(0-m_GPR[inst.RA], immediate)));
}

void Interpreter::twi(UGeckoInstruction inst)
{
	s32 a = m_GPR[inst.RA];
	s32 b = inst.SIMM_16;
	s32 TO = inst.TO;

	ERROR_LOG(POWERPC, "twi rA %x SIMM %x TO %0x", a, b, TO);

	if (((a < b) && (TO & 0x10)) ||
	    ((a > b) && (TO & 0x08)) ||
	    ((a ==b) && (TO & 0x04)) ||
	    (((u32)a <(u32)b) && (TO & 0x02)) ||
	    (((u32)a >(u32)b) && (TO & 0x01)))
	{
		Common::AtomicOr(PowerPC::ppcState.Exceptions, EXCEPTION_PROGRAM);
		PowerPC::CheckExceptions();
		m_EndBlock = true; // Dunno about this
	}
}

void Interpreter::xori(UGeckoInstruction inst)
{
	m_GPR[inst.RA] = m_GPR[inst.RS] ^ inst.UIMM;
}

void Interpreter::xoris(UGeckoInstruction inst)
{
	m_GPR[inst.RA] = m_GPR[inst.RS] ^ (inst.UIMM << 16);
}

void Interpreter::rlwimix(UGeckoInstruction inst)
{
	u32 mask = Helper_Mask(inst.MB,inst.ME);
	m_GPR[inst.RA] = (m_GPR[inst.RA] & ~mask) | (_rotl(m_GPR[inst.RS],inst.SH) & mask);
	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RA]);
}

void Interpreter::rlwinmx(UGeckoInstruction inst)
{
	u32 mask = Helper_Mask(inst.MB,inst.ME);
	m_GPR[inst.RA] = _rotl(m_GPR[inst.RS],inst.SH) & mask;
	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RA]);
}

void Interpreter::rlwnmx(UGeckoInstruction inst)
{
	u32 mask = Helper_Mask(inst.MB,inst.ME);
	m_GPR[inst.RA] = _rotl(m_GPR[inst.RS], m_GPR[inst.RB] & 0x1F) & mask;

	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RA]);
}

void Interpreter::andx(UGeckoInstruction inst)
{
	m_GPR[inst.RA] = m_GPR[inst.RS] & m_GPR[inst.RB];

	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RA]);
}

void Interpreter::andcx(UGeckoInstruction inst)
{
	m_GPR[inst.RA] = m_GPR[inst.RS] & ~m_GPR[inst.RB];

	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RA]);
}

void Interpreter::cmp(UGeckoInstruction inst)
{
	s32 a = (s32)m_GPR[inst.RA];
	s32 b = (s32)m_GPR[inst.RB];
	int fTemp = 0x8; // a < b
	// if (a < b)  fTemp = 0x8; else
	if (a > b)  fTemp = 0x4;
	else if (a == b) fTemp = 0x2;
	if (GetXER_SO()) PanicAlert("cmp getting overflow flag"); // fTemp |= 0x1
	SetCRField(inst.CRFD, fTemp);
}

void Interpreter::cmpl(UGeckoInstruction inst)
{
	u32 a = m_GPR[inst.RA];
	u32 b = m_GPR[inst.RB];
	u32 fTemp = 0x8; // a < b

	// if (a < b)  fTemp = 0x8;else
	if (a > b)  fTemp = 0x4;
	else if (a == b) fTemp = 0x2;
	if (GetXER_SO()) PanicAlert("cmpl getting overflow flag"); // fTemp |= 0x1;
	SetCRField(inst.CRFD, fTemp);
}

void Interpreter::cntlzwx(UGeckoInstruction inst)
{
	u32 val = m_GPR[inst.RS];
	u32 mask = 0x80000000;
	int i = 0;
	for (; i < 32; i++, mask >>= 1)
		if (val & mask)
			break;
	m_GPR[inst.RA] = i;
	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RA]);
}

void Interpreter::eqvx(UGeckoInstruction inst)
{
	m_GPR[inst.RA] = ~(m_GPR[inst.RS] ^ m_GPR[inst.RB]);

	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RA]);
}

void Interpreter::extsbx(UGeckoInstruction inst)
{
	m_GPR[inst.RA] = (u32)(s32)(s8)m_GPR[inst.RS];

	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RA]);
}

void Interpreter::extshx(UGeckoInstruction inst)
{
	m_GPR[inst.RA] = (u32)(s32)(s16)m_GPR[inst.RS];

	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RA]);
}

void Interpreter::nandx(UGeckoInstruction inst)
{
	m_GPR[inst.RA] = ~(m_GPR[inst.RS] & m_GPR[inst.RB]);

	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RA]);
}

void Interpreter::norx(UGeckoInstruction inst)
{
	m_GPR[inst.RA] = ~(m_GPR[inst.RS] | m_GPR[inst.RB]);

	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RA]);
}

void Interpreter::orx(UGeckoInstruction inst)
{
	m_GPR[inst.RA] = m_GPR[inst.RS] | m_GPR[inst.RB];

	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RA]);
}

void Interpreter::orcx(UGeckoInstruction inst)
{
	m_GPR[inst.RA] = m_GPR[inst.RS] | (~m_GPR[inst.RB]);

	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RA]);
}

void Interpreter::slwx(UGeckoInstruction inst)
{
	u32 amount = m_GPR[inst.RB];
	m_GPR[inst.RA] = (amount & 0x20) ? 0 : m_GPR[inst.RS] << amount;

	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RA]);
}

void Interpreter::srawx(UGeckoInstruction inst)
{
	int rb = m_GPR[inst.RB];
	if (rb & 0x20)
	{
		if (m_GPR[inst.RS] & 0x80000000)
		{
			m_GPR[inst.RA] = 0xFFFFFFFF;
			SetCarry(1);
		}
		else
		{
			m_GPR[inst.RA] = 0x00000000;
			SetCarry(0);
		}
	}
	else
	{
		int amount = rb & 0x1f;
		if (amount == 0)
		{
			m_GPR[inst.RA] = m_GPR[inst.RS];
			SetCarry(0);
		}
		else
		{
			m_GPR[inst.RA] = (u32)((s32)m_GPR[inst.RS] >> amount);
			if (m_GPR[inst.RS] & 0x80000000)
				SetCarry(1);
			else
				SetCarry(0);
		}
	}

	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RA]);
}

void Interpreter::srawix(UGeckoInstruction inst)
{
	int amount = inst.SH;

	if (amount != 0)
	{
		s32 rrs = m_GPR[inst.RS];
		m_GPR[inst.RA] = rrs >> amount;

		if ((rrs < 0) && (rrs << (32 - amount)))
			SetCarry(1);
		else
			SetCarry(0);
	}
	else
	{
		SetCarry(0);
		m_GPR[inst.RA] = m_GPR[inst.RS];
	}

	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RA]);
}

void Interpreter::srwx(UGeckoInstruction inst)
{
	u32 amount = m_GPR[inst.RB];
	m_GPR[inst.RA] = (amount & 0x20) ? 0 : (m_GPR[inst.RS] >> (amount & 0x1f));

	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RA]);
}

void Interpreter::tw(UGeckoInstruction inst)
{
	s32 a = m_GPR[inst.RA];
	s32 b = m_GPR[inst.RB];
	s32 TO = inst.TO;

	ERROR_LOG(POWERPC, "tw rA %0x rB %0x TO %0x", a, b, TO);

	if (((a < b) && (TO & 0x10)) ||
	    ((a > b) && (TO & 0x08)) ||
	    ((a ==b) && (TO & 0x04)) ||
	    (((u32)a <(u32)b) && (TO & 0x02)) ||
	    (((u32)a >(u32)b) && (TO & 0x01)))
	{
		Common::AtomicOr(PowerPC::ppcState.Exceptions, EXCEPTION_PROGRAM);
		PowerPC::CheckExceptions();
		m_EndBlock = true; // Dunno about this
	}
}

void Interpreter::xorx(UGeckoInstruction inst)
{
	m_GPR[inst.RA] = m_GPR[inst.RS] ^ m_GPR[inst.RB];

	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RA]);
}

void Interpreter::addx(UGeckoInstruction inst)
{
	m_GPR[inst.RD] = m_GPR[inst.RA] + m_GPR[inst.RB];

	if (inst.OE) PanicAlert("OE: addx");
	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RD]);
}

void Interpreter::addcx(UGeckoInstruction inst)
{
	u32 a = m_GPR[inst.RA];
	u32 b = m_GPR[inst.RB];
	m_GPR[inst.RD] = a + b;
	SetCarry(Helper_Carry(a,b));

	if (inst.OE) PanicAlert("OE: addcx");
	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RD]);
}

void Interpreter::addex(UGeckoInstruction inst)
{
	int carry = GetCarry();
	int a = m_GPR[inst.RA];
	int b = m_GPR[inst.RB];
	m_GPR[inst.RD] = a + b + carry;
	SetCarry(Helper_Carry(a, b) || (carry != 0 && Helper_Carry(a + b, carry)));

	if (inst.OE) PanicAlert("OE: addex");
	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RD]);
}

void Interpreter::addmex(UGeckoInstruction inst)
{
	int carry = GetCarry();
	int a = m_GPR[inst.RA];
	m_GPR[inst.RD] = a + carry - 1;
	SetCarry(Helper_Carry(a, carry - 1));

	if (inst.OE) PanicAlert("OE: addmex");
	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RD]);
}

void Interpreter::addzex(UGeckoInstruction inst)
{
	int carry = GetCarry();
	int a = m_GPR[inst.RA];
	m_GPR[inst.RD] = a + carry;
	SetCarry(Helper_Carry(a, carry));

	if (inst.OE) PanicAlert("OE: addzex");
	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RD]);
}

void Interpreter::divwx(UGeckoInstruction inst)
{
	s32 a = m_GPR[inst.RA];
	s32 b = m_GPR[inst.RB];
	if (b == 0 || ((u32)a == 0x80000000 && b == -1))
	{
		if (inst.OE)
			// should set OV
			PanicAlert("OE: divwx");
		if (((u32)a & 0x80000000) && b == 0)
			m_GPR[inst.RD] = -1;
		else
			m_GPR[inst.RD] = 0;
	}
	else
		m_GPR[inst.RD] = (u32)(a / b);

	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RD]);
}


void Interpreter::divwux(UGeckoInstruction inst)
{
	u32 a = m_GPR[inst.RA];
	u32 b = m_GPR[inst.RB];

	if (b == 0)
	{
		if (inst.OE)
			// should set OV
			PanicAlert("OE: divwux");
		m_GPR[inst.RD] = 0;
	}
	else
		m_GPR[inst.RD] = a / b;

	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RD]);
}

void Interpreter::mulhwx(UGeckoInstruction inst)
{
	u32 a = m_GPR[inst.RA];
	u32 b = m_GPR[inst.RB];
	u32 d = (u32)((u64)(((s64)(s32)a * (s64)(s32)b) ) >> 32);  // This can be done better. Not in plain C/C++ though.
	m_GPR[inst.RD] = d;
	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RD]);
}

void Interpreter::mulhwux(UGeckoInstruction inst)
{
	u32 a = m_GPR[inst.RA];
	u32 b = m_GPR[inst.RB];
	u32 d = (u32)(((u64)a * (u64)b) >> 32);
	m_GPR[inst.RD] = d;
	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RD]);
}

void Interpreter::mullwx(UGeckoInstruction inst)
{
	u32 a = m_GPR[inst.RA];
	u32 b = m_GPR[inst.RB];
	u32 d = (u32)((s32)a * (s32)b);
	m_GPR[inst.RD] = d;

	if (inst.OE) PanicAlert("OE: mullwx");
	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RD]);
}

void Interpreter::negx(UGeckoInstruction inst)
{
	m_GPR[inst.RD] = (~m_GPR[inst.RA]) + 1;
	if (m_GPR[inst.RD] == 0x80000000)
	{
		if (inst.OE) PanicAlert("OE: negx");
	}
	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RD]);
}

void Interpreter::subfx(UGeckoInstruction inst)
{
	m_GPR[inst.RD] = m_GPR[inst.RB] - m_GPR[inst.RA];

	if (inst.OE) PanicAlert("OE: subfx");
	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RD]);
}

void Interpreter::subfcx(UGeckoInstruction inst)
{
	u32 a = m_GPR[inst.RA];
	u32 b = m_GPR[inst.RB];
	m_GPR[inst.RD] = b - a;
	SetCarry(a == 0 || Helper_Carry(b, 0-a));

	if (inst.OE) PanicAlert("OE: subfcx");
	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RD]);
}

void Interpreter::subfex(UGeckoInstruction inst)
{
	u32 a = m_GPR[inst.RA];
	u32 b = m_GPR[inst.RB];
	int carry = GetCarry();
	m_GPR[inst.RD] = (~a) + b + carry;
	SetCarry(Helper_Carry(~a, b) || Helper_Carry((~a) + b, carry));

	if (inst.OE) PanicAlert("OE: subfex");
	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RD]);
}

// sub from minus one
void Interpreter::subfmex(UGeckoInstruction inst)
{
	u32 a = m_GPR[inst.RA];
	int carry = GetCarry();
	m_GPR[inst.RD] = (~a) + carry - 1;
	SetCarry(Helper_Carry(~a, carry - 1));

	if (inst.OE) PanicAlert("OE: subfmex");
	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RD]);
}

// sub from zero
void Interpreter::subfzex(UGeckoInstruction inst)
{
	u32 a = m_GPR[inst.RA];
	int carry = GetCarry();
	m_GPR[inst.RD] = (~a) + carry;
	SetCarry(Helper_Carry(~a, carry));

	if (inst.OE) PanicAlert("OE: subfzex");
	if (inst.Rc) Helper_UpdateCR0(m_GPR[inst.RD]);
}
