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

#include "Debugger_BreakPoints.h"
#include "Debugger_SymbolMap.h"
#include "DebugInterface.h"
#include "PPCDebugInterface.h"
#include "PowerPCDisasm.h"
#include "../Core.h"
#include "../HW/Memmap.h"
#include "../PowerPC/PowerPC.h"
#include "../PowerPC/SymbolDB.h"

// Not thread safe.
const char *PPCDebugInterface::disasm(unsigned int address) 
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		if (Memory::IsRAMAddress(address))
		{
			u32 op = Memory::Read_Instruction(address);
			return DisassembleGekko(op, address);
		}
		return "No RAM here - invalid";
	}

	static const char tmp[] = "<unknown>";
	return tmp;
}

const char *PPCDebugInterface::getRawMemoryString(unsigned int address) 
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		if (address < 0xE0000000)
		{
			static char str[256] ={0};
			if (sprintf(str,"%08X",readMemory(address))!=8) {
				PanicAlert("getRawMemoryString -> WTF! ( as read somewhere;) )");
				return ":(";
			}
			return str;
		}
		return "No RAM";
	}
	static const char tmp[] = "<unknown>";
	return tmp;
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
	return CBreakPoints::IsAddressBreakPoint(address);
}

void PPCDebugInterface::setBreakpoint(unsigned int address)
{
	CBreakPoints::AddBreakPoint(address);
}

void PPCDebugInterface::clearBreakpoint(unsigned int address)
{
	CBreakPoints::RemoveBreakPoint(address);
}

void PPCDebugInterface::clearAllBreakpoints() {}

void PPCDebugInterface::toggleBreakpoint(unsigned int address)
{
	CBreakPoints::IsAddressBreakPoint(address) ? CBreakPoints::RemoveBreakPoint(address) : CBreakPoints::AddBreakPoint(address);
}

void PPCDebugInterface::insertBLR(unsigned int address) 
{
	Memory::Write_U32(0x4e800020, address);
}

int PPCDebugInterface::getColor(unsigned int address)
{
	if (!Memory::IsRAMAddress(address))
		return 0xeeeeee;
	int colors[6] = {0xe0FFFF, 0xFFe0e0, 0xe8e8FF, 0xFFe0FF, 0xe0FFe0, 0xFFFFe0};
	Symbol *symbol = g_symbolDB.GetSymbolFromAddr(address);
	if (!symbol) return 0xFFFFFF;
	if (symbol->type != Symbol::SYMBOL_FUNCTION)
		return 0xEEEEFF;
	return colors[symbol->index % 6];
}

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
