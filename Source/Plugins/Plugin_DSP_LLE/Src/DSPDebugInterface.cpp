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

#include "DSPSymbols.h"
#include "DSPMemoryMap.h"

void DSPDebugInterface::disasm(unsigned int address, char *dest, int max_size) 
{
	// we'll treat addresses as line numbers.
	strncpy(dest, DSPSymbols::GetLineText(address), max_size);
	dest[max_size-1] = 0;
}

void DSPDebugInterface::getRawMemoryString(int memory, unsigned int address, char *dest, int max_size)
{
	switch (memory) {
	case 0:  // IMEM
		switch (address >> 12) {
		case 0:
		case 0x8:
			sprintf(dest, "%04x", dsp_imem_read(address));
			break;
		default:
			sprintf(dest, "----");
			break;
		}
		break;
	case 1:  // DMEM
		switch (address >> 12) {
		case 0:
		case 1:
			sprintf(dest, "%04x", dsp_dmem_read(address));
			break;
		default:
			sprintf(dest, "----");
			break;
		}
		break;
	}
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
	int real_addr = DSPSymbols::Line2Addr(address);
	if (real_addr >= 0)
		return dsp_breakpoints.IsAddressBreakPoint(real_addr);
	else
		return false;
}

void DSPDebugInterface::setBreakpoint(unsigned int address)
{
	int real_addr = DSPSymbols::Line2Addr(address);
	if (real_addr >= 0) {
		if (dsp_breakpoints.Add(real_addr))
			;
	}
}

void DSPDebugInterface::clearBreakpoint(unsigned int address)
{
	int real_addr = DSPSymbols::Line2Addr(address);
	if (real_addr >= 0) {
		if (dsp_breakpoints.Remove(real_addr))
			;
	}
}

void DSPDebugInterface::clearAllBreakpoints() {
	dsp_breakpoints.Clear();
}

void DSPDebugInterface::toggleBreakpoint(unsigned int address)
{
	int real_addr = DSPSymbols::Line2Addr(address);
	if (real_addr >= 0) {
		if (dsp_breakpoints.IsAddressBreakPoint(real_addr))
			dsp_breakpoints.Remove(real_addr);
		else
			dsp_breakpoints.Add(real_addr);
	}
}

void DSPDebugInterface::insertBLR(unsigned int address) 
{
	PanicAlert("insertBLR functionality not supported in DSP module.");
}

// =======================================================
// Separate the blocks with colors.
// -------------
int DSPDebugInterface::getColor(unsigned int address)
{
	static const int colors[6] =
	{ 
		0xd0FFFF,  // light cyan
		0xFFd0d0,  // light red
		0xd8d8FF,  // light blue
		0xFFd0FF,  // light purple
		0xd0FFd0,  // light green
		0xFFFFd0,  // light yellow
	};

	// Scan backwards so we don't miss it. Hm, actually, let's not - it looks pretty good.
	int addr = -1;
	for (int i = 0; i < 1; i++)
	{
		addr = DSPSymbols::Line2Addr(address - i);
		if (addr >= 0)
			break;
	}
	if (addr == -1)
		return 0xFFFFFF;

	Symbol *symbol = DSPSymbols::g_dsp_symbol_db.GetSymbolFromAddr(addr);
	if (!symbol)
		return 0xFFFFFF;
	if (symbol->type != Symbol::SYMBOL_FUNCTION)
		return 0xEEEEFF;
	return colors[symbol->index % 6];
}
// =============


std::string DSPDebugInterface::getDescription(unsigned int address) 
{
	return "";  // g_symbolDB.GetDescription(address);
}

unsigned int DSPDebugInterface::getPC() 
{
	return DSPSymbols::Addr2Line(g_dsp.pc);
}

void DSPDebugInterface::setPC(unsigned int address) 
{
	int new_pc = DSPSymbols::Line2Addr(address);
	if (new_pc > 0)
		g_dsp.pc = new_pc;
}

void DSPDebugInterface::runToBreakpoint() 
{

}
