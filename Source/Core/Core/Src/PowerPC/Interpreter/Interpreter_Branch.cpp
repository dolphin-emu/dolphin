// Copyright (C) 2003 Dolphin Project.

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
#include "../../HW/CPU.h"
#include "../../HLE/HLE.h"
#include "../PPCAnalyst.h"

namespace Interpreter
{

void bx(UGeckoInstruction _inst)
{
	if (_inst.LK)
		LR = PC + 4;
	if (_inst.AA)
		NPC = SignExt26(_inst.LI << 2);
	else
		NPC = PC+SignExt26(_inst.LI << 2);
/*
#ifdef _DEBUG
	if (_inst.LK)
	{
		PPCAnalyst::LogFunctionCall(NPC);
	}
#endif*/

	m_EndBlock = true;
}

// bcx - ugly, straight from PPC manual equations :)
void bcx(UGeckoInstruction _inst)
{
	if ((_inst.BO & BO_DONT_DECREMENT_FLAG) == 0)
		CTR--;

	const bool true_false = ((_inst.BO >> 3) & 1);
	const bool only_counter_check = ((_inst.BO >> 4) & 1);
	const bool only_condition_check = ((_inst.BO >> 2) & 1);
	int ctr_check = ((CTR != 0) ^ (_inst.BO >> 1)) & 1;
	bool counter = only_condition_check || ctr_check;
	bool condition = only_counter_check || (GetCRBit(_inst.BI) == u32(true_false));

	if (counter && condition)
	{
		if (_inst.LK)
			LR = PC + 4;
		if (_inst.AA)
			NPC = SignExt16(_inst.BD << 2);
		else
			NPC = PC + SignExt16(_inst.BD << 2);
	}
	m_EndBlock = true;
}

void bcctrx(UGeckoInstruction _inst)
{
	_dbg_assert_msg_(POWERPC, _inst.BO_2 & BO_DONT_DECREMENT_FLAG, "bcctrx with decrement and test CTR option is invalid!");

	int condition = ((_inst.BO_2>>4) | (GetCRBit(_inst.BI_2) == ((_inst.BO_2>>3) & 1))) & 1;

	if (condition)
	{
		NPC = CTR & (~3);
		if (_inst.LK_3)
			LR = PC + 4;
	}
	m_EndBlock = true;
}

void bclrx(UGeckoInstruction _inst)
{
	if ((_inst.BO_2 & BO_DONT_DECREMENT_FLAG) == 0)
		CTR--;

	int counter = ((_inst.BO_2 >> 2) | ((CTR != 0) ^ (_inst.BO_2 >> 1))) & 1;
	int condition = ((_inst.BO_2 >> 4) | (GetCRBit(_inst.BI_2) == ((_inst.BO_2 >> 3) & 1))) & 1;

	if (counter & condition)
	{
		NPC = LR & (~3);
		if (_inst.LK_3)
			LR = PC + 4;
	}
	m_EndBlock = true;
}

void HLEFunction(UGeckoInstruction _inst)
{
	m_EndBlock = true;
	HLE::Execute(PC, _inst.hex);
}

void CompiledBlock(UGeckoInstruction _inst)
{
	_assert_msg_(POWERPC, 0, "CompiledBlock - shouldn't be here!");
}

void rfi(UGeckoInstruction _inst)
{
	// Restore saved bits from SRR1 to MSR.
	// Gecko/Broadway can save more bits than explicitly defined in ppc spec
	const int mask = 0x87C0FFFF;
	MSR = (MSR & ~mask) | (SRR1 & mask);
	//MSR[13] is set to 0.
	MSR &= 0xFFFDFFFF;
	// Here we should check if there are pending exceptions, and if their corresponding enable bits are set
	// if above is true, we'd do:
	//PowerPC::CheckExceptions();
	//else
	// set NPC to saved offset and resume
	NPC = SRR0;
	m_EndBlock = true;
}

void rfid(UGeckoInstruction _inst) 
{
	_dbg_assert_msg_(POWERPC,0,"Instruction unimplemented (does this instruction even exist?)","rfid");
	m_EndBlock = true;
}

// sc isn't really used for anything important in gc games (just for a write barrier) so we really don't have to emulate it.
// We do it anyway, though :P
void sc(UGeckoInstruction _inst)
{
	PowerPC::ppcState.Exceptions |= EXCEPTION_SYSCALL;
	PowerPC::CheckExceptions();
	m_EndBlock = true;
}

}  // namespace
