// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

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
void PrintDataBuffer(LogTypes::LOG_TYPE _Log, u8* _pData, size_t _Size, const std::string& _title);
void AddAutoBreakpoints();


} // end of namespace Debugger
