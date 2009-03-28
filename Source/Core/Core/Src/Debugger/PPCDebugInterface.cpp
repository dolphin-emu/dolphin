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

#include "Debugger_BreakPoints.h"
#include "Debugger_SymbolMap.h"
#include "DebugInterface.h"
#include "PPCDebugInterface.h"
#include "PowerPCDisasm.h"
#include "../Core.h"
#include "../HW/Memmap.h"
#include "../PowerPC/PowerPC.h"
#include "../PowerPC/Jit64/Jit.h"
#include "../PowerPC/SymbolDB.h"

void PPCDebugInterface::disasm(unsigned int address, char *dest, int max_size) 
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		if (Memory::IsRAMAddress(address, true))
		{
			u32 op = Memory::Read_Instruction(address);
			DisassembleGekko(op, address, dest, max_size);
			UGeckoInstruction inst;
			inst.hex = Memory::ReadUnchecked_U32(address);
			if (inst.OPCD == 1) {
				strcat(dest, " (hle)");
			}
		}
		else
		{
			strcpy(dest, "(No RAM here)");
		}
	}
	else
	{
		strcpy(dest, "<unknown>");
	}
}

void PPCDebugInterface::getRawMemoryString(unsigned int address, char *dest, int max_size)
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		if (Memory::IsRAMAddress(address, true))
		{
			snprintf(dest, max_size, "%08X", readMemory(address));
		}
		else
		{
			strcpy(dest, "--------");
		}
	}
	else
	{
		strcpy(dest, "<unknwn>");  // bad spelling - 8 chars
	}
}

unsigned int PPCDebugInterface::readMemory(unsigned int address)
{
	return Memory::ReadUnchecked_U32(address);
}

unsigned int PPCDebugInterface::readInstruction(unsigned int address)
{
	return Memory::Read_Instruction(address);
}

bool PPCDebugInterface::isAlive()
{
	return Core::GetState() != Core::CORE_UNINITIALIZED;
}

bool PPCDebugInterface::isBreakpoint(unsigned int address) 
{
	return BreakPoints::IsAddressBreakPoint(address);
}

void PPCDebugInterface::setBreakpoint(unsigned int address)
{
	if (BreakPoints::Add(address))
		jit.NotifyBreakpoint(address, true);
}

void PPCDebugInterface::clearBreakpoint(unsigned int address)
{
	if (BreakPoints::Remove(address))
		jit.NotifyBreakpoint(address, false);
}

void PPCDebugInterface::clearAllBreakpoints() {}

void PPCDebugInterface::toggleBreakpoint(unsigned int address)
{
	if (BreakPoints::IsAddressBreakPoint(address))
		BreakPoints::Remove(address);
	else
		BreakPoints::Add(address);
}

void PPCDebugInterface::insertBLR(unsigned int address) 
{
	Memory::Write_U32(0x4e800020, address);
}


// =======================================================
// Separate the blocks with colors.
// -------------
int PPCDebugInterface::getColor(unsigned int address)
{
	if (!Memory::IsRAMAddress(address, true))
		return 0xeeeeee;
	static const int colors[6] =
	{ 
		0xd0FFFF,  // light cyan
		0xFFd0d0,  // light red
		0xd8d8FF,  // light blue
		0xFFd0FF,  // light purple
		0xd0FFd0,  // light green
		0xFFFFd0,  // light yellow
	};
	Symbol *symbol = g_symbolDB.GetSymbolFromAddr(address);
	if (!symbol)
		return 0xFFFFFF;
	if (symbol->type != Symbol::SYMBOL_FUNCTION)
		return 0xEEEEFF;
	return colors[symbol->index % 6];
}
// =============


std::string PPCDebugInterface::getDescription(unsigned int address) 
{
	return g_symbolDB.GetDescription(address);
}

unsigned int PPCDebugInterface::getPC() 
{
	return PowerPC::ppcState.pc;
}

void PPCDebugInterface::setPC(unsigned int address) 
{
	PowerPC::ppcState.pc = address;
}

void PPCDebugInterface::runToBreakpoint() 
{

}
