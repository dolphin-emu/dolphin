// Copyright (C) 2003-2009 Dolphin Project.

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

#ifndef _DSPSYMBOLS_H
#define _DSPSYMBOLS_H

#include "Common.h"
#include "SymbolDB.h"
#include "AudioCommon.h"

#include <stdio.h>

namespace DSPSymbols {

class DSPSymbolDB : public SymbolDB 
{
public:
	DSPSymbolDB() {}
	~DSPSymbolDB() {}
	
	Symbol *GetSymbolFromAddr(u32 addr);

};

extern DSPSymbolDB g_dsp_symbol_db;

bool ReadAnnotatedAssembly(const char *filename);
void AutoDisassembly(u16 start_addr, u16 end_addr);

void Clear();

int Addr2Line(u16 address);
int Line2Addr(int line);   // -1 for not found

const char *GetLineText(int line);

}  // namespace DSPSymbols

#endif

