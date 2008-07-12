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
#include "../../HW/CPU.h"
#include "../../HLE/HLE.h"
#include "../PPCAnalyst.h"


void CInterpreter::bx(UGeckoInstruction _inst)
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
void CInterpreter::bcx(UGeckoInstruction _inst)
{
	if ((_inst.BO & BO_DONT_DECREMENT_FLAG) == 0)
		CTR--;

	const bool true_false = ((_inst.BO >> 3) & 1);
	const bool only_counter_check = ((_inst.BO >> 4) & 1);
	const bool only_condition_check = ((_inst.BO >> 2) & 1);
	int ctr_check = ((CTR != 0) ^ (_inst.BO >> 1)) & 1;
	bool counter = only_condition_check || ctr_check;
	bool condition = only_counter_check || (GetCRBit(_inst.BI) == (int)true_false);

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

void CInterpreter::bcctrx(UGeckoInstruction _inst)
{
	if ((_inst.BO & BO_DONT_DECREMENT_FLAG) == 0)
		CTR--;

	int condition = ((_inst.BO>>4) | (GetCRBit(_inst.BI) == ((_inst.BO>>3) & 1))) & 1;

	if (condition)
	{
		if (_inst.LK)
			LR = PC + 4;
		NPC = CTR & (~3);
	}
	m_EndBlock = true;
}

void CInterpreter::bclrx(UGeckoInstruction _inst)
{
	if ((_inst.BO & BO_DONT_DECREMENT_FLAG) == 0)
		CTR--;

	int counter = ((_inst.BO >> 2) | ((CTR != 0) ^ (_inst.BO >> 1)))&1;
	int condition = ((_inst.BO >> 4) | (GetCRBit(_inst.BI) == ((_inst.BO >> 3) & 1))) & 1;

	if (counter & condition)
	{
		NPC = LR & (~3); 
		if (_inst.LK)
			LR = PC+4;
	}
	m_EndBlock = true;
}

void CInterpreter::HLEFunction(UGeckoInstruction _inst)
{
	m_EndBlock = true;
	HLE::Execute(PC, _inst.hex);
}

void CInterpreter::CompiledBlock(UGeckoInstruction _inst)
{
	_assert_msg_(GEKKO, 0, "CInterpreter::CompiledBlock - shouldn't be here!");
}

void CInterpreter::rfi(UGeckoInstruction _inst)
{
	//Bits SRR1[0,5-9,16�23, 25�27, 30�31] are placed into the corresponding bits of the MSR.
	//MSR[13] is set to 0.
	const int mask = 0x87C0FF73;
	MSR = (MSR & ~mask) | (SRR1 & mask);
	MSR &= 0xFFFDFFFF; //TODO: VERIFY
	NPC = SRR0; // TODO: VERIFY
	m_EndBlock = true;
	// After an RFI, exceptions should be checked IMMEDIATELY without going back into
	// other code! TODO(ector): fix this properly
	// PowerPC::CheckExceptions();
}

void CInterpreter::rfid(UGeckoInstruction _inst) 
{
	_dbg_assert_msg_(GEKKO,0,"Instruction unimplemented (does this instruction even exist?)","rfid");
	m_EndBlock = true;
}

// sc isn't really used for anything important in gc games (just for a write barrier) so we really don't have to emulate it.
// We do it anyway, though :P
void CInterpreter::sc(UGeckoInstruction _inst)
{
	PowerPC::ppcState.Exceptions |= EXCEPTION_SYSCALL;
	PowerPC::CheckExceptions();
	m_EndBlock = true;
}

