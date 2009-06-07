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

#include "DSPDebugInterface.h"

#include "DSPCore.h"
#include "disassemble.h"

void DSPDebugInterface::disasm(unsigned int address, char *dest, int max_size) 
{
	AssemblerSettings settings;
	settings.print_tabs = true;

	u16 pc = address;
	DSPDisassembler dis(settings);

	u16 base = 0;
	const u16 *binbuf = g_dsp.iram;
	if (pc & 0x8000)
	{
		binbuf = g_dsp.irom;
		base = 0x8000;
	}

	std::string text;
	dis.DisOpcode(binbuf, base, 2, &pc, text);
	strncpy(dest, text.c_str(), max_size);
	dest[max_size - 1] = '\0';

	/*
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
	}*/
}

void DSPDebugInterface::getRawMemoryString(unsigned int address, char *dest, int max_size)
{
	/*
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
	}*/
}

unsigned int DSPDebugInterface::readMemory(unsigned int address)
{
	return 0; //Memory::ReadUnchecked_U32(address);
}

unsigned int DSPDebugInterface::readInstruction(unsigned int address)
{
	return 0; //Memory::Read_Instruction(address);
}

bool DSPDebugInterface::isAlive()
{
	return true; //Core::GetState() != Core::CORE_UNINITIALIZED;
}

bool DSPDebugInterface::isBreakpoint(unsigned int address) 
{
	return false; //BreakPoints::IsAddressBreakPoint(address);
}

void DSPDebugInterface::setBreakpoint(unsigned int address)
{
	//if (BreakPoints::Add(address))
	//	jit.NotifyBreakpoint(address, true);
}

void DSPDebugInterface::clearBreakpoint(unsigned int address)
{
	//if (BreakPoints::Remove(address))
	//	jit.NotifyBreakpoint(address, false);
}

void DSPDebugInterface::clearAllBreakpoints() {}

void DSPDebugInterface::toggleBreakpoint(unsigned int address)
{
	//if (BreakPoints::IsAddressBreakPoint(address))
	//	BreakPoints::Remove(address);
	//else
	//	BreakPoints::Add(address);
}

void DSPDebugInterface::insertBLR(unsigned int address) 
{
	// Memory::Write_U32(0x4e800020, address);
}


// =======================================================
// Separate the blocks with colors.
// -------------
int DSPDebugInterface::getColor(unsigned int address)
{
	return 0xEEEEEE;
	/*
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
	return colors[symbol->index % 6];*/
}
// =============


std::string DSPDebugInterface::getDescription(unsigned int address) 
{
	return "asdf";  // g_symbolDB.GetDescription(address);
}

unsigned int DSPDebugInterface::getPC() 
{
	return 0;
}

void DSPDebugInterface::setPC(unsigned int address) 
{
	//PowerPC::ppcState.pc = address;
}

void DSPDebugInterface::runToBreakpoint() 
{

}
