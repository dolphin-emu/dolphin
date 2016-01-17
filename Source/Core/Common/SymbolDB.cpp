// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>

#include "Common/SymbolDB.h"
#include "Common/Logging/Log.h"
#include "Common/Fileutil.h"
#include "Core/ConfigManager.h"

void SymbolDB::List()
{
	for (const auto& func : functions)
	{
		DEBUG_LOG(OSHLE, "%s @ %08x: %i bytes (hash %08x) : %i calls",
		          func.second.name.c_str(), func.second.address,
		          func.second.size, func.second.hash,
		          func.second.numCalls);
	}
	INFO_LOG(OSHLE, "%zu functions known in this program above.", functions.size());
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

void SymbolDB::PopulateSymbolComments()
{
	std::string filename = File::GetUserPath(D_MAPS_IDX) + SConfig::GetInstance().m_strUniqueID + "_comments.txt";
	if (!File::Exists(filename))
		return;
	std::fstream line_parse;
	line_parse.open(filename);
	std::string line;
	u32 address;
	while(!(line_parse>>std::ws).eof())
	{
		std::getline(line_parse, line);
		if (line.empty())
			continue;
		AsciiToHex(line.substr(0, 8),address);
		std::string comment = line.substr(9, std::string::npos);
		if (address < 0x80000010)
			continue;
		Symbol* f = GetSymbolFromAddr(address);
		if (f)
		{
			char Offset = (address - f->address)/4;
			f->comments[Offset] = comment;
		}
	}
}

const std::string SymbolDB::GetCommentFromAddr(u32 addr)
{
	Symbol *symbol = GetSymbolFromAddr(addr);
	if (symbol)
	{
		unsigned int Offset = (addr - symbol->address)/4;
		return symbol->comments[Offset];
	}
	else
		return "";
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
	functions.emplace(symbol.address, symbol);
}
