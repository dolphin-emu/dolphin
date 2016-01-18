// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <utility>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/SymbolDB.h"
#include "Common/Logging/Log.h"
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
	std::fstream line_parse(filename);
	std::string line;
	while (std::getline(line_parse,line))
	{
		unsigned int address = 0;
		if (line.length() <= 8)
			continue;
		AsciiToHex(line.substr(0,8),address);
		std::string comment = line.substr(9);
		if (address)
			comments.emplace(address,comment);
	}
}

std::string SymbolDB::GetCommentFromAddress(unsigned int address) const
{
	XCommentMap::const_iterator temp = comments.find(address);
	if (temp != comments.end())
		return temp->second;
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
