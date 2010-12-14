// Copyright (C) 2003 Dolphin Project.

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

#ifndef _DEBUGGER_SYMBOLMAP_H
#define _DEBUGGER_SYMBOLMAP_H

#include <vector>
#include <string>

#include "Common.h"

namespace Dolphin_Debugger
{

struct CallstackEntry 
{
    std::string Name;
    u32 vAddress;
};

bool GetCallstack(std::vector<CallstackEntry> &output);
void PrintCallstack();
void PrintCallstack(LogTypes::LOG_TYPE type, LogTypes::LOG_LEVELS level);
void PrintDataBuffer(LogTypes::LOG_TYPE _Log, u8* _pData, size_t _Size, const char* _title);
void AddAutoBreakpoints();


} // end of namespace Debugger

#endif

