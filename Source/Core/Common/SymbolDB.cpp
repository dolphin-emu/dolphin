// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstring>
#include <map>
#include <string>
#include <utility>

#include "Common/CommonTypes.h"
#include "Common/SymbolDB.h"
#include "Common/Logging/Log.h"

void SymbolDB::List()
{
	for (const auto& func : functions)
	{
		DEBUG_LOG(OSHLE, "%s @ %08x: %i bytes (hash %08x) : %i calls",
		          func.second.name.c_str(), func.second.address,
		          func.second.size, func.second.hash,
		          func.second.numCalls);
	}
	INFO_LOG(OSHLE, "%lu functions known in this program above.",
	         (unsigned long)functions.size());
}

void SymbolDB::Clear(const char *prefix)
{
	// TODO: honor prefix
	functions.clear();
	checksumToFunction.clear();
}

void SymbolDB::Index()
{
	int i = 0;
	for (auto& func : functions)
	{
		func.second.index = i++;
	}
}

Symbol* SymbolDB::GetSymbolFromName(const std::string& name)
{
	for (auto& func : functions)
	{
		if (func.second.name == name)
			return &func.second;
	}

	return nullptr;
}

void SymbolDB::AddCompleteSymbol(const Symbol &symbol)
{
	functions.insert(std::pair<u32, Symbol>(symbol.address, symbol));
}
