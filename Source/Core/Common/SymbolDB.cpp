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

static std::string GetStrippedFunctionName(const std::string& symbol_name)
{
  std::string name = symbol_name.substr(0, symbol_name.find('('));
  size_t position = name.find(' ');
  if (position != std::string::npos)
    name.erase(position);
  return name;
}

void Symbol::Rename(const std::string& symbol_name)
{
  this->name = symbol_name;
  this->function_name = GetStrippedFunctionName(symbol_name);
}

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
    if (func.second.function_name == name)
      return &func.second;
  }

  return nullptr;
}

std::vector<Symbol*> SymbolDB::GetSymbolsFromName(const std::string& name)
{
  std::vector<Symbol*> symbols;

  for (auto& func : functions)
  {
    if (func.second.function_name == name)
      symbols.push_back(&func.second);
  }

  return symbols;
}

Symbol* SymbolDB::GetSymbolFromHash(u32 hash)
{
  XFuncPtrMap::iterator iter = checksumToFunction.find(hash);
  if (iter != checksumToFunction.end())
    return *iter->second.begin();
  else
    return nullptr;
}

std::vector<Symbol*> SymbolDB::GetSymbolsFromHash(u32 hash)
{
  const auto iter = checksumToFunction.find(hash);

  if (iter == checksumToFunction.cend())
    return {};

  return {iter->second.cbegin(), iter->second.cend()};
}

void SymbolDB::AddCompleteSymbol(const Symbol& symbol)
{
  functions.emplace(symbol.address, symbol);
}
