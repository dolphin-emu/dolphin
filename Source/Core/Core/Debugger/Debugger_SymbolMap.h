// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <string_view>
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

bool GetCallstack(std::vector<CallstackEntry>& output);
void PrintCallstack();
void PrintCallstack(Common::Log::LOG_TYPE type, Common::Log::LOG_LEVELS level);
void PrintDataBuffer(Common::Log::LOG_TYPE type, const u8* data, size_t size,
                     std::string_view title);
void AddAutoBreakpoints();

}  // namespace Dolphin_Debugger
