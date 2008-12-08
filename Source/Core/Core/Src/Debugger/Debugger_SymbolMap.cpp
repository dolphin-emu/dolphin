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

#include "Common.h"
#include "StringUtil.h"
#include "Debugger_SymbolMap.h"
#include "../Core.h"
#include "../HW/Memmap.h"
#include "../PowerPC/PowerPC.h"
#include "../PowerPC/PPCAnalyst.h"
#include "../PowerPC/SymbolDB.h"
#include "../../../../Externals/Bochs_disasm/PowerPCDisasm.h"

namespace Debugger
{

bool GetCallstack(std::vector<CallstackEntry> &output) 
{
	if (Core::GetState() == Core::CORE_UNINITIALIZED)
		return false;

    if (!Memory::IsRAMAddress(PowerPC::ppcState.gpr[1]))
        return false;

	u32 addr = Memory::ReadUnchecked_U32(PowerPC::ppcState.gpr[1]);  // SP
	if (LR == 0) {
        CallstackEntry entry;
        entry.Name = "(error: LR=0)";
        entry.vAddress = 0x0;
		output.push_back(entry);
		return false;
	}
	int count = 1;
	if (g_symbolDB.GetDescription(PowerPC::ppcState.pc) != g_symbolDB.GetDescription(LR))
	{
        CallstackEntry entry;
        entry.Name = StringFromFormat(" * %s [ LR = %08x ]\n", g_symbolDB.GetDescription(LR), LR);
        entry.vAddress = 0x0;
		count++;
	}
	
	//walk the stack chain
	while ((addr != 0xFFFFFFFF) && (addr != 0) && (count++ < 20) && (PowerPC::ppcState.gpr[1] != 0))
	{
        if (!Memory::IsRAMAddress(addr + 4))
            return false;

		u32 func = Memory::ReadUnchecked_U32(addr + 4);
		const char *str = g_symbolDB.GetDescription(func);
		if (!str || strlen(str) == 0 || !strcmp(str, "Invalid"))
			str = "(unknown)";

        CallstackEntry entry;
        entry.Name = StringFromFormat(" * %s [ addr = %08x ]\n", str, func);
        entry.vAddress = func;
		output.push_back(entry);

        if (!Memory::IsRAMAddress(addr))
            return false;
             
	    addr = Memory::ReadUnchecked_U32(addr);
	}

    return true;
}

void PrintCallstack()
{
	u32 addr = Memory::ReadUnchecked_U32(PowerPC::ppcState.gpr[1]);  // SP
	
	printf("\n == STACK TRACE - SP = %08x ==\n", PowerPC::ppcState.gpr[1]);
	
	if (LR == 0) {
		printf(" LR = 0 - this is bad\n");	
	}
	int count = 1;
	if (g_symbolDB.GetDescription(PowerPC::ppcState.pc) != g_symbolDB.GetDescription(LR))
	{
		printf(" * %s  [ LR = %08x ]\n", g_symbolDB.GetDescription(LR), LR);
		count++;
	}
	
	//walk the stack chain
	while ((addr != 0xFFFFFFFF) && (addr != 0) && (count++ < 20) && (PowerPC::ppcState.gpr[1] != 0))
	{
		u32 func = Memory::ReadUnchecked_U32(addr + 4);
		const char *str = g_symbolDB.GetDescription(func);
		if (!str || strlen(str) == 0 || !strcmp(str, "Invalid"))
			str = "(unknown)";
		printf( " * %s [ addr = %08x ]\n", str, func);
		addr = Memory::ReadUnchecked_U32(addr);
	}
}

void PrintCallstack(LogTypes::LOG_TYPE _Log)
{
	u32 addr = Memory::ReadUnchecked_U32(PowerPC::ppcState.gpr[1]);  // SP

	__Logv(_Log, 1, "\n == STACK TRACE - SP = %08x ==\n", PowerPC::ppcState.gpr[1]);

	if (LR == 0) {
		__Logv(_Log, 1, " LR = 0 - this is bad\n");	
	}
	int count = 1;
	if (g_symbolDB.GetDescription(PowerPC::ppcState.pc) != g_symbolDB.GetDescription(LR))
	{
		__Log(_Log, " * %s  [ LR = %08x ]\n", g_symbolDB.GetDescription(LR), LR);
		count++;
	}

	//walk the stack chain
	while ((addr != 0xFFFFFFFF) && (addr != 0) && (count++ < 20) && (PowerPC::ppcState.gpr[1] != 0))
	{
		u32 func = Memory::ReadUnchecked_U32(addr + 4);
		const char *str = g_symbolDB.GetDescription(func);
		if (!str || strlen(str) == 0 || !strcmp(str, "Invalid"))
			str = "(unknown)";
		__Logv(_Log, 3, " * %s [ addr = %08x ]\n", str, func);
		addr = Memory::ReadUnchecked_U32(addr);
	}
}

void PrintDataBuffer(LogTypes::LOG_TYPE _Log, u8* _pData, size_t _Size, const char* _title)
{
	__Log(_Log, _title);		
	for (u32 j=0; j<_Size;)
	{
		std::string Temp;
		for (int i=0; i<16; i++)
		{
			char Buffer[128];
			sprintf(Buffer, "%02x ", _pData[j++]);
			Temp.append(Buffer);

			if (j >= _Size)
				break;
		}

		__Log(_Log, "   Data: %s", Temp.c_str());
	}
}

}  // end of namespace Debugger
