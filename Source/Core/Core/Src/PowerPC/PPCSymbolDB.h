// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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
