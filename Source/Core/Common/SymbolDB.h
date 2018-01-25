// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// This file contains a generic symbol map implementation. For CPU-specific
// magic, derive and extend.

#pragma once

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"

struct SCall
{
  SCall(u32 a, u32 b) : function(a), callAddress(b) {}
  u32 function;
  u32 callAddress;
};

struct Symbol
{
  enum class Type
  {
    Function,
    Data,
  };

  void Rename(const std::string& symbol_name);

  std::string name;
  std::string function_name;   // stripped function name
  std::vector<SCall> callers;  // addresses of functions that call this function
  std::vector<SCall> calls;    // addresses of functions that are called by this function
  u32 hash = 0;                // use for HLE function finding
  u32 address = 0;
  u32 flags = 0;
  int size = 0;
  int numCalls = 0;
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
  typedef std::map<u32, Symbol> XFuncMap;
  typedef std::map<u32, std::set<Symbol*>> XFuncPtrMap;

protected:
  XFuncMap functions;
  XFuncPtrMap checksumToFunction;

public:
  SymbolDB() {}
  virtual ~SymbolDB() {}
  virtual Symbol* GetSymbolFromAddr(u32 addr) { return nullptr; }
  virtual Symbol* AddFunction(u32 startAddr) { return nullptr; }
  void AddCompleteSymbol(const Symbol& symbol);

  Symbol* GetSymbolFromName(const std::string& name);
  std::vector<Symbol*> GetSymbolsFromName(const std::string& name);
  Symbol* GetSymbolFromHash(u32 hash);
  std::vector<Symbol*> GetSymbolsFromHash(u32 hash);

  const XFuncMap& Symbols() const { return functions; }
  XFuncMap& AccessSymbols() { return functions; }
  void Clear(const char* prefix = "");
  void List();
  void Index();
};

const std::string Demangle(const std::string& name);