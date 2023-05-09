// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// This file contains a generic symbol map implementation. For CPU-specific
// magic, derive and extend.

#pragma once

#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"

namespace Core
{
class CPUThreadGuard;
}

namespace Common
{
struct SCall
{
  SCall(u32 a, u32 b) : function(a), call_address(b) {}
  u32 function;
  u32 call_address;
};

struct Symbol
{
  enum class Type
  {
    Function,
    Data,
  };

  Symbol() = default;
  explicit Symbol(const std::string& name) { Rename(name); }

  void Rename(const std::string& symbol_name);

  std::string name;
  std::string function_name;   // stripped function name
  std::vector<SCall> callers;  // addresses of functions that call this function
  std::vector<SCall> calls;    // addresses of functions that are called by this function
  u32 hash = 0;                // use for HLE function finding
  u32 address = 0;
  u32 flags = 0;
  u32 size = 0;
  int num_calls = 0;
  Type type = Type::Function;
  int index = 0;  // only used for coloring the disasm view
  bool analyzed = false;
};

enum
{
  FFLAG_TIMERINSTRUCTIONS = (1 << 0),
  FFLAG_LEAF = (1 << 1),
  FFLAG_ONLYCALLSNICELEAFS = (1 << 2),
  FFLAG_EVIL = (1 << 3),
  FFLAG_RFI = (1 << 4),
  FFLAG_STRAIGHT = (1 << 5)
};

class SymbolDB
{
public:
  using XFuncMap = std::map<u32, Symbol>;
  using XFuncPtrMap = std::map<u32, std::set<Symbol*>>;

  SymbolDB();
  virtual ~SymbolDB();

  virtual Symbol* GetSymbolFromAddr(u32 addr) { return nullptr; }
  virtual Symbol* AddFunction(const Core::CPUThreadGuard& guard, u32 start_addr) { return nullptr; }
  void AddCompleteSymbol(const Symbol& symbol);

  Symbol* GetSymbolFromName(std::string_view name);
  std::vector<Symbol*> GetSymbolsFromName(std::string_view name);
  Symbol* GetSymbolFromHash(u32 hash);
  std::vector<Symbol*> GetSymbolsFromHash(u32 hash);

  const XFuncMap& Symbols() const { return m_functions; }
  XFuncMap& AccessSymbols() { return m_functions; }
  bool IsEmpty() const;
  void Clear(const char* prefix = "");
  void List();
  void Index();

protected:
  XFuncMap m_functions;
  XFuncPtrMap m_checksum_to_function;
};
}  // namespace Common
