// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/SymbolDB.h"

#include "Core/Debugger/PPCDebugInterface.h"

// This has functionality overlapping Debugger_Symbolmap. Should merge that stuff in here later.
class PPCSymbolDB : public SymbolDB
{
private:
	DebugInterface* debugger;

public:
	typedef void (*functionGetterCallback)(Symbol *f);

	PPCSymbolDB();
	~PPCSymbolDB();

	Symbol *AddFunction(u32 startAddr) override;
	void AddKnownSymbol(u32 startAddr, u32 size, const std::string& name, int type = Symbol::SYMBOL_FUNCTION);

	Symbol *GetSymbolFromAddr(u32 addr) override;

	const std::string GetDescription(u32 addr);

	void FillInCallers();

	bool LoadMap(const std::string& filename, bool bad = false);
	bool SaveMap(const std::string& filename, bool WithCodes = false) const;

	void PrintCalls(u32 funcAddr) const;
	void PrintCallers(u32 funcAddr) const;
	void LogFunctionCall(u32 addr);
};

extern PPCSymbolDB g_symbolDB;
