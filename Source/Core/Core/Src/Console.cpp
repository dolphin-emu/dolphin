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
#include <iostream>

#include "string.h"

#include "Common.h"
#include "Thread.h"
#include "HW/Memmap.h"
#include "PowerPC/PPCAnalyst.h"
#include "PowerPC/PPCTables.h"
#include "Console.h"
#include "CoreTiming.h"
#include "Core.h"
#include "PowerPC/Jit64/JitCache.h"
#include "PowerPC/SymbolDB.h"
#include "PowerPCDisasm.h"

#define CASE(x) else if (memcmp(cmd, x, 4*sizeof(TCHAR))==0)
#define CASE1(x) if (memcmp(cmd, x, 2*sizeof(TCHAR))==0)

/*
static Common::Thread *cons_thread;

THREAD_RETURN ConsoleThreadFunc(void *) {
	printf("Welcome to the console thread!\n\n");
	while (true) {
		std::string command;
		getline(std::cin, command);
		Console_Submit(command.c_str());
	}
}

void StartConsoleThread() {
	cons_thread = new Common::Thread(ConsoleThreadFunc, 0);
}*/

void Console_Submit(const char *cmd)
{
	CASE1("jits")
	{
#ifdef _M_X64
		Jit64::PrintStats();
#endif
	}
	CASE1("r")
	{
		Core::StartTrace(false);
		LOG(CONSOLE, "read tracing started.");			
	}
	CASE1("w")
	{
		Core::StartTrace(true);
		LOG(CONSOLE, "write tracing started.");			
	}
	CASE("trans")
	{
		TCHAR temp[256];
		u32 addr;
		sscanf(cmd, "%s %08x", temp, &addr);
		
		if (addr!=0)
		{
#ifdef LOGGING
			u32 EA =
#endif
				Memory::CheckDTLB(addr, Memory::FLAG_NO_EXCEPTION);
			LOG(CONSOLE, "EA 0x%08x to 0x%08x", addr, EA);
		}
		else
		{
			LOG(CONSOLE, "Syntax: trans ADDR");
		}
	}
	CASE("call")
	{
		TCHAR temp[256];
		u32 addr;
		sscanf(cmd, "%s %08x", temp, &addr);
		if (addr!=0)
		{
			g_symbolDB.PrintCalls(addr);
		}
		else
		{
			LOG(CONSOLE, "Syntax: call ADDR");
		}
	}
	CASE("llac")
	{
		TCHAR temp[256];
		u32 addr;
		sscanf(cmd, "%s %08x", temp, &addr);
		if (addr!=0)
		{
			g_symbolDB.PrintCallers(addr);
		}
		else
		{
			LOG(CONSOLE, "Syntax: llac ADDR");
		}
	}
	CASE("pend")
	{
		CoreTiming::LogPendingEvents();
	}
	CASE("dump")
	{
		TCHAR temp[256];
		TCHAR filename[256];
		u32 start;
		u32 end;
		sscanf(cmd, "%s %08x %08x %s", temp, &start, &end, filename);

		FILE *f = fopen(filename, "wb");
		for (u32 i=start; i<end; i++)
		{
			u8 b = Memory::ReadUnchecked_U8(i);
			fputc(b,f);
		}
		fclose(f);
		LOG(CONSOLE, "Dumped from %08x to %08x to %s",start,end,filename);
	}
	CASE("disa")
	{
		u32 start;
		u32 end;
		TCHAR temp[256];
		sscanf(cmd, "%s %08x %08x", temp, &start, &end);
		for (u32 addr = start; addr <= end; addr += 4) {
			u32 data = Memory::ReadUnchecked_U32(addr);
			printf("%08x: %08x: %s\n", addr, data, DisassembleGekko(data, addr));	
		}
	}
	CASE("help")
	{
		LOG(CONSOLE, "Dolphin Console Command List");
		LOG(CONSOLE, "scan ADDR - will find functions that are called by this function");
		LOG(CONSOLE, "call ADDR - will find functions that call this function");
		LOG(CONSOLE, "dump START_A END_A FILENAME - will dump memory between START_A and END_A");
		LOG(CONSOLE, "help - guess what this does :P");
		LOG(CONSOLE, "lisd - list signature database");
		LOG(CONSOLE, "lisf - list functions");
		LOG(CONSOLE, "trans ADDR - translate address");
	}
	CASE("lisd")
	{
		// PPCAnalyst::ListDB();
	}
	CASE("ipro")
	{
		PPCTables::PrintInstructionRunCounts();
	}
	CASE("lisf")
	{
		g_symbolDB.List();
	}
	else {
		printf("blach\n");
		LOG(CONSOLE, "Invalid command");	
	}
}
