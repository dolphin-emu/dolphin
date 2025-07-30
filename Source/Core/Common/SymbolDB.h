// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// This file contains a generic symbol map implementation. For CPU-specific
// magic, derive and extend.

#pragma once

#include <map>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Common/Assert.h"
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

struct Note
{
  std::string name;
  u32 address = 0;
  u32 size = 0;
  int layer = 0;

  Note() = default;
};

struct Symbol
{
  enum class Type
  {
    Function,
    Data,
  };

  Symbol() = default;
  explicit Symbol(const std::string& symbol_name) { Rename(symbol_name); }

  void Rename(const std::string& symbol_name);

  std::string name;
  std::string function_name;   // stripped function name
  std::string object_name;     // name of object/source file symbol belongs to
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
  using XNoteMap = std::map<u32, Note>;
  using XFuncPtrMap = std::map<u32, std::set<Symbol*>>;

  SymbolDB();
  virtual ~SymbolDB();

  virtual const Symbol* GetSymbolFromAddr(u32 addr) const { return nullptr; }
  virtual const Symbol* AddFunction(const Core::CPUThreadGuard& guard, u32 start_addr)
  {
    return nullptr;
  }
  void AddCompleteSymbol(const Symbol& symbol);
  bool RenameSymbol(const Symbol& symbol, const std::string& symbol_name);
  bool RenameSymbol(const Symbol& symbol, const std::string& symbol_name,
                    const std::string& object_name);

  const Symbol* GetSymbolFromName(std::string_view name) const;
  std::vector<const Symbol*> GetSymbolsFromName(std::string_view name) const;
  const Symbol* GetSymbolFromHash(u32 hash) const;
  std::vector<const Symbol*> GetSymbolsFromHash(u32 hash) const;

  template <typename F>
  void ForEachSymbol(F f) const
  {
    std::lock_guard lock(m_mutex);
    for (const auto& [addr, symbol] : m_functions)
      f(symbol);
  }

  template <typename F>
  void ForEachSymbolWithMutation(F f)
  {
    std::lock_guard lock(m_mutex);
    for (auto& [addr, symbol] : m_functions)
    {
      f(symbol);
      ASSERT_MSG(COMMON, addr == symbol.address, "Symbol address was unexpectedly changed");
    }
  }

  template <typename F>
  void ForEachNote(F f) const
  {
    std::lock_guard lock(m_mutex);
    for (const auto& [addr, note] : m_notes)
      f(note);
  }

  template <typename F>
  void ForEachNoteWithMutation(F f)
  {
    std::lock_guard lock(m_mutex);
    for (auto& [addr, note] : m_notes)
    {
      f(note);
      ASSERT_MSG(COMMON, addr == note.address, "Note address was unexpectedly changed");
    }
  }

  bool IsEmpty() const;
  bool Clear(const char* prefix = "");
  void List();
  void Index();

protected:
  static void Index(XFuncMap* functions);

  XFuncMap m_functions;
  XNoteMap m_notes;
  XFuncPtrMap m_checksum_to_function;
  std::string m_map_name;
  mutable std::recursive_mutex m_mutex;
};
}  // namespace Common
