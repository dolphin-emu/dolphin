// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Common/SymbolDB.h"

namespace Core
{
class CPUThreadGuard;
class DebugInterface;
}  // namespace Core

// This has functionality overlapping Debugger_Symbolmap. Should merge that stuff in here later.
class PPCSymbolDB : public Common::SymbolDB
{
public:
  PPCSymbolDB();
  ~PPCSymbolDB() override;

  Common::Symbol* AddFunction(const Core::CPUThreadGuard& guard, u32 start_addr) override;
  void AddKnownSymbol(const Core::CPUThreadGuard& guard, u32 startAddr, u32 size,
                      const std::string& name,
                      Common::Symbol::Type type = Common::Symbol::Type::Function);
  void AddKnownNote(u32 startAddr, u32 size, const std::string& name,
                    Common::Symbol::Type type = Common::Symbol::Type::Note);

  Common::Symbol* GetSymbolFromAddr(u32 addr) override;
  Common::Note* GetNoteFromAddr(u32 addr);
  void DetermineNoteLayers();
  void DeleteFunction(u32 startAddress);
  void DeleteNote(u32 startAddress);

  std::string GetDescription(u32 addr);

  void FillInCallers();

  bool LoadMap(const Core::CPUThreadGuard& guard, const std::string& filename, bool bad = false);
  bool SaveSymbolMap(const std::string& filename) const;
  bool SaveCodeMap(const Core::CPUThreadGuard& guard, const std::string& filename) const;

  void PrintCalls(u32 funcAddr) const;
  void PrintCallers(u32 funcAddr) const;
  void LogFunctionCall(u32 addr);

private:
  Core::DebugInterface* debugger;
};

extern PPCSymbolDB g_symbolDB;
