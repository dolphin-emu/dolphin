// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Common/SymbolDB.h"

namespace DSP::Symbols
{
class DSPSymbolDB : public Common::SymbolDB
{
public:
  DSPSymbolDB() {}
  ~DSPSymbolDB() {}
  Common::Symbol* GetSymbolFromAddr(u32 addr) override;
};

extern DSPSymbolDB g_dsp_symbol_db;

void AutoDisassembly(u16 start_addr, u16 end_addr);

void Clear();

int Addr2Line(u16 address);
int Line2Addr(int line);  // -1 for not found

const char* GetLineText(int line);
}  // namespace DSP::Symbols
