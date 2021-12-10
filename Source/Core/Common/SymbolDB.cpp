// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/SymbolDB.h"

#include <cstring>
#include <map>
#include <string>
#include <utility>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

namespace Common
{
static std::string GetStrippedFunctionName(const std::string& symbol_name)
{
  std::string name = symbol_name.substr(0, symbol_name.find('('));
  size_t position = name.find(' ');
  if (position != std::string::npos)
    name.erase(position);
  return name;
}

SymbolDB::SymbolDB() = default;

SymbolDB::~SymbolDB() = default;

void Symbol::Rename(const std::string& symbol_name)
{
  this->name = symbol_name;
  this->function_name = GetStrippedFunctionName(symbol_name);
}

void SymbolDB::List()
{
  for (const auto& func : m_functions)
  {
    DEBUG_LOG_FMT(OSHLE, "{} @ {:08x}: {} bytes (hash {:08x}) : {} calls", func.second.name,
                  func.second.address, func.second.size, func.second.hash, func.second.num_calls);
  }
  INFO_LOG_FMT(OSHLE, "{} functions known in this program above.", m_functions.size());
}

bool SymbolDB::IsEmpty() const
{
  return m_functions.empty();
}

void SymbolDB::Clear(const char* prefix)
{
  // TODO: honor prefix
  m_functions.clear();
  m_checksum_to_function.clear();
}

void SymbolDB::Index()
{
  int i = 0;
  for (auto& func : m_functions)
  {
    func.second.index = i++;
  }
}

Symbol* SymbolDB::GetSymbolFromName(std::string_view name)
{
  for (auto& func : m_functions)
  {
    if (func.second.function_name == name)
      return &func.second;
  }

  return nullptr;
}

std::vector<Symbol*> SymbolDB::GetSymbolsFromName(std::string_view name)
{
  std::vector<Symbol*> symbols;

  for (auto& func : m_functions)
  {
    if (func.second.function_name == name)
      symbols.push_back(&func.second);
  }

  return symbols;
}

Symbol* SymbolDB::GetSymbolFromHash(u32 hash)
{
  auto iter = m_checksum_to_function.find(hash);
  if (iter == m_checksum_to_function.end())
    return nullptr;

  return *iter->second.begin();
}

std::vector<Symbol*> SymbolDB::GetSymbolsFromHash(u32 hash)
{
  const auto iter = m_checksum_to_function.find(hash);

  if (iter == m_checksum_to_function.cend())
    return {};

  return {iter->second.cbegin(), iter->second.cend()};
}

void SymbolDB::AddCompleteSymbol(const Symbol& symbol)
{
  m_functions.emplace(symbol.address, symbol);
}
}  // namespace Common
