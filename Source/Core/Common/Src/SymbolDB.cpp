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

#include "SymbolDB.h"


void SymbolDB::List()
{
	for (XFuncMap::iterator iter = functions.begin(); iter != functions.end(); iter++)
	{
		DEBUG_LOG(HLE,"%s @ %08x: %i bytes (hash %08x) : %i calls", iter->second.name.c_str(), iter->second.address, iter->second.size, iter->second.hash,iter->second.numCalls);
	}
	INFO_LOG(HLE,"%i functions known in this program above.", functions.size());
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
	for (XFuncMap::iterator iter = functions.begin(); iter != functions.end(); iter++)
	{
		iter->second.index = i++;
	}
}

Symbol *SymbolDB::GetSymbolFromName(const char *name)
{
	for (XFuncMap::iterator iter = functions.begin(); iter != functions.end(); iter++)
	{
		if (!strcmp(iter->second.name.c_str(), name))
			return &iter->second;
	}
	return 0;
}

void SymbolDB::AddCompleteSymbol(const Symbol &symbol)
{
	functions.insert(std::pair<u32, Symbol>(symbol.address, symbol));
}
