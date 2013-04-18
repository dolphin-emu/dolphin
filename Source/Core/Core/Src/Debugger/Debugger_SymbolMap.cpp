// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"
#include "StringUtil.h"
#include "Debugger_SymbolMap.h"
#include "../Core.h"
#include "../HW/Memmap.h"
#include "../PowerPC/PowerPC.h"
#include "../PowerPC/PPCAnalyst.h"
#include "../PowerPC/PPCSymbolDB.h"
#include "PowerPCDisasm.h"

namespace Dolphin_Debugger
{

void AddAutoBreakpoints()
{
#if defined(_DEBUG) || defined(DEBUGFAST)
#if 1
	const char *bps[] = {
		"PPCHalt",
	};

	for (u32 i = 0; i < sizeof(bps) / sizeof(const char *); i++)
	{
		Symbol *symbol = g_symbolDB.GetSymbolFromName(bps[i]);
		if (symbol)
			PowerPC::breakpoints.Add(symbol->address, false);
	}
#endif
#endif
}

// Returns callstack "formatted for debugging" - meaning that it
// includes LR as the last item, and all items are the last step,
// instead of "pointing ahead"
bool GetCallstack(std::vector<CallstackEntry> &output) 
{
	if (Core::GetState() == Core::CORE_UNINITIALIZED)
		return false;

	if (!Memory::IsRAMAddress(PowerPC::ppcState.gpr[1]))
		return false;

	u32 addr = Memory::ReadUnchecked_U32(PowerPC::ppcState.gpr[1]);  // SP
	if (LR == 0)
	{
		CallstackEntry entry;
		entry.Name = "(error: LR=0)";
		entry.vAddress = 0x0;
		output.push_back(entry);
		return false;
	}

	int count = 1;

	CallstackEntry entry;
	entry.Name = StringFromFormat(" * %s [ LR = %08x ]\n", g_symbolDB.GetDescription(LR), LR - 4);
	entry.vAddress = LR - 4;
	output.push_back(entry);
	count++;

	//walk the stack chain
	while ((addr != 0xFFFFFFFF) && (addr != 0) && (count++ < 20) && (PowerPC::ppcState.gpr[1] != 0))
	{
		if (!Memory::IsRAMAddress(addr + 4))
			return false;

		u32 func = Memory::ReadUnchecked_U32(addr + 4);
		const char *str = g_symbolDB.GetDescription(func);
		if (!str || strlen(str) == 0 || !strcmp(str, "Invalid"))
			str = "(unknown)";

		entry.Name = StringFromFormat(" * %s [ addr = %08x ]\n", str, func - 4);
		entry.vAddress = func - 4;
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

	printf("== STACK TRACE - SP = %08x ==", PowerPC::ppcState.gpr[1]);
	
	if (LR == 0) {
		printf(" LR = 0 - this is bad");	
	}
	int count = 1;
	if (g_symbolDB.GetDescription(PowerPC::ppcState.pc) != g_symbolDB.GetDescription(LR))
	{
		printf(" * %s  [ LR = %08x ]", g_symbolDB.GetDescription(LR), LR);
		count++;
	}

	//walk the stack chain
	while ((addr != 0xFFFFFFFF) && (addr != 0) && (count++ < 20) && (PowerPC::ppcState.gpr[1] != 0))
	{
		u32 func = Memory::ReadUnchecked_U32(addr + 4);
		const char *str = g_symbolDB.GetDescription(func);
		if (!str || strlen(str) == 0 || !strcmp(str, "Invalid"))
			str = "(unknown)";
		printf( " * %s [ addr = %08x ]", str, func);
		addr = Memory::ReadUnchecked_U32(addr);
	}
}

void PrintCallstack(LogTypes::LOG_TYPE type, LogTypes::LOG_LEVELS level)
{
	u32 addr = Memory::ReadUnchecked_U32(PowerPC::ppcState.gpr[1]);  // SP

	GENERIC_LOG(type, level, "== STACK TRACE - SP = %08x ==", 
				PowerPC::ppcState.gpr[1]);

	if (LR == 0) {
		GENERIC_LOG(type, level, " LR = 0 - this is bad");	
	}
	int count = 1;
	if (g_symbolDB.GetDescription(PowerPC::ppcState.pc) != g_symbolDB.GetDescription(LR))
	{
		GENERIC_LOG(type, level, " * %s  [ LR = %08x ]", 
					g_symbolDB.GetDescription(LR), LR);
		count++;
	}

	//walk the stack chain
	while ((addr != 0xFFFFFFFF) && (addr != 0) && (count++ < 20) && (PowerPC::ppcState.gpr[1] != 0))
	{
		u32 func = Memory::ReadUnchecked_U32(addr + 4);
		const char *str = g_symbolDB.GetDescription(func);
		if (!str || strlen(str) == 0 || !strcmp(str, "Invalid"))
			str = "(unknown)";
		GENERIC_LOG(type, level, " * %s [ addr = %08x ]", str, func);
		addr = Memory::ReadUnchecked_U32(addr);
	}
}

void PrintDataBuffer(LogTypes::LOG_TYPE type, u8* _pData, size_t _Size, const char* _title)
{
	GENERIC_LOG(type, LogTypes::LDEBUG, "%s", _title);		
	for (u32 j = 0; j < _Size;)
	{
		std::string Temp;
		for (int i = 0; i < 16; i++)
		{
			char Buffer[128];
			sprintf(Buffer, "%02x ", _pData[j++]);
			Temp.append(Buffer);

			if (j >= _Size)
				break;
		}
		GENERIC_LOG(type, LogTypes::LDEBUG, "   Data: %s", Temp.c_str());
	}
}

}  // end of namespace Debugger
