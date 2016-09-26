// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>
#include <map>
#include <string>
#include <utility>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/SymbolDB.h"

void SymbolDB::List()
{
  for (const auto& func : functions)
  {
    DEBUG_LOG(OSHLE, "%s @ %08x: %i bytes (hash %08x) : %i calls", func.second.name.c_str(),
              func.second.address, func.second.size, func.second.hash, func.second.numCalls);
  }
  INFO_LOG(OSHLE, "%zu functions known in this program above.", functions.size());
}

void SymbolDB::Clear(const char* prefix)
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
    // If name contains more than only the function name, only look for exact matches.
    if (name.find('(') != std::string::npos && func.second.name == name)
      return &func.second;
    // If name only contains a function name, it is assumed that we only want to match by the name.
    if (func.second.name.find(name) == 0)
      return &func.second;
  }

  return nullptr;
}

void SymbolDB::AddCompleteSymbol(const Symbol& symbol)
{
  functions.emplace(symbol.address, symbol);
}
