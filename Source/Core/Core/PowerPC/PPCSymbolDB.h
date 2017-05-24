// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/SymbolDB.h"

#include "Core/Debugger/PPCDebugInterface.h"

// This has functionality overlapping Debugger_Symbolmap. Should merge that stuff in here later.
class PPCSymbolDB : public SymbolDB
{
public:
  typedef void (*functionGetterCallback)(Symbol* f);

  PPCSymbolDB();
  ~PPCSymbolDB();

  Symbol* AddFunction(u32 start_addr) override;
  void AddKnownSymbol(u32 startAddr, u32 size, const std::string& name,
                      Symbol::Type type = Symbol::Type::Function);

  Symbol* GetSymbolFromAddr(u32 addr) override;

  std::string GetDescription(u32 addr);

  void FillInCallers();

  bool LoadMap(const std::string& filename, bool bad = false);
  bool SaveSymbolMap(const std::string& filename) const;
  bool SaveCodeMap(const std::string& filename) const;

  void PrintCalls(u32 funcAddr) const;
  void PrintCallers(u32 funcAddr) const;
  void LogFunctionCall(u32 addr);

private:
  DebugInterface* debugger;
};

extern PPCSymbolDB g_symbolDB;
