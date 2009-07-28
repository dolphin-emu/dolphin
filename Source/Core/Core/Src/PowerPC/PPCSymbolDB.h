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

#include "Common.h"

#include <map>
#include <string>
#include <vector>
#include "../Debugger/PPCDebugInterface.h"

#include "SymbolDB.h"

// This has functionality overlapping Debugger_Symbolmap. Should merge that stuff in here later.
class PPCSymbolDB : public SymbolDB
{
private:
	DebugInterface* debugger;

public:
	typedef void (*functionGetterCallback)(Symbol *f);
	
	PPCSymbolDB();
	~PPCSymbolDB();

	Symbol *AddFunction(u32 startAddr);
	void AddKnownSymbol(u32 startAddr, u32 size, const char *name, int type = Symbol::SYMBOL_FUNCTION);

	Symbol *GetSymbolFromAddr(u32 addr);

	const char *GetDescription(u32 addr);

	void FillInCallers();

	bool LoadMap(const char *filename);
	bool SaveMap(const char *filename, bool WithCodes = false) const;

	void PrintCalls(u32 funcAddr) const;
	void PrintCallers(u32 funcAddr) const;
	void LogFunctionCall(u32 addr);
};

extern PPCSymbolDB g_symbolDB;
