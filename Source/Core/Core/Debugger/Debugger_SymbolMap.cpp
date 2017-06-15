// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/Debugger/Debugger_SymbolMap.h"

#include <cstdio>
#include <functional>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"

namespace Dolphin_Debugger
{
void AddAutoBreakpoints()
{
#if defined(_DEBUG) || defined(DEBUGFAST)
#if 1
  const char* bps[] = {
      "PPCHalt",
  };

  for (const char* bp : bps)
  {
    Symbol* symbol = g_symbolDB.GetSymbolFromName(bp);
    if (symbol)
      PowerPC::breakpoints.Add(symbol->address, false);
  }
#endif
#endif
}

// Returns true if the address is not a valid RAM address or NULL.
static bool IsStackBottom(u32 addr)
{
  return !addr || !PowerPC::HostIsRAMAddress(addr);
}

static void WalkTheStack(const std::function<void(u32)>& stack_step)
{
  if (!IsStackBottom(PowerPC::ppcState.gpr[1]))
  {
    u32 addr = PowerPC::HostRead_U32(PowerPC::ppcState.gpr[1]);  // SP

    // Walk the stack chain
    for (int count = 0; !IsStackBottom(addr + 4) && (count++ < 20); ++count)
    {
      u32 func_addr = PowerPC::HostRead_U32(addr + 4);
      stack_step(func_addr);

      if (IsStackBottom(addr))
        break;

      addr = PowerPC::HostRead_U32(addr);
    }
  }
}

// Returns callstack "formatted for debugging" - meaning that it
// includes LR as the last item, and all items are the last step,
// instead of "pointing ahead"
bool GetCallstack(std::vector<CallstackEntry>& output)
{
  if (!Core::IsRunning() || !PowerPC::HostIsRAMAddress(PowerPC::ppcState.gpr[1]))
    return false;

  if (LR == 0)
  {
    CallstackEntry entry;
    entry.Name = "(error: LR=0)";
    entry.vAddress = 0x0;
    output.push_back(entry);
    return false;
  }

  CallstackEntry entry;
  entry.Name =
      StringFromFormat(" * %s [ LR = %08x ]\n", g_symbolDB.GetDescription(LR).c_str(), LR - 4);
  entry.vAddress = LR - 4;
  output.push_back(entry);

  WalkTheStack([&entry, &output](u32 func_addr) {
    std::string func_desc = g_symbolDB.GetDescription(func_addr);
    if (func_desc.empty() || func_desc == "Invalid")
      func_desc = "(unknown)";
    entry.Name = StringFromFormat(" * %s [ addr = %08x ]\n", func_desc.c_str(), func_addr - 4);
    entry.vAddress = func_addr - 4;
    output.push_back(entry);
  });

  return true;
}

void PrintCallstack()
{
  printf("== STACK TRACE - SP = %08x ==", PowerPC::ppcState.gpr[1]);

  if (LR == 0)
  {
    printf(" LR = 0 - this is bad");
  }

  if (g_symbolDB.GetDescription(PC) != g_symbolDB.GetDescription(LR))
  {
    printf(" * %s  [ LR = %08x ]", g_symbolDB.GetDescription(LR).c_str(), LR);
  }

  WalkTheStack([](u32 func_addr) {
    std::string func_desc = g_symbolDB.GetDescription(func_addr);
    if (func_desc.empty() || func_desc == "Invalid")
      func_desc = "(unknown)";
    printf(" * %s [ addr = %08x ]", func_desc.c_str(), func_addr);
  });
}

void PrintCallstack(LogTypes::LOG_TYPE type, LogTypes::LOG_LEVELS level)
{
  GENERIC_LOG(type, level, "== STACK TRACE - SP = %08x ==", PowerPC::ppcState.gpr[1]);

  if (LR == 0)
  {
    GENERIC_LOG(type, level, " LR = 0 - this is bad");
  }

  if (g_symbolDB.GetDescription(PC) != g_symbolDB.GetDescription(LR))
  {
    GENERIC_LOG(type, level, " * %s  [ LR = %08x ]", g_symbolDB.GetDescription(LR).c_str(), LR);
  }

  WalkTheStack([type, level](u32 func_addr) {
    std::string func_desc = g_symbolDB.GetDescription(func_addr);
    if (func_desc.empty() || func_desc == "Invalid")
      func_desc = "(unknown)";
    GENERIC_LOG(type, level, " * %s [ addr = %08x ]", func_desc.c_str(), func_addr);
  });
}

void PrintDataBuffer(LogTypes::LOG_TYPE type, const u8* data, size_t size, const std::string& title)
{
  GENERIC_LOG(type, LogTypes::LDEBUG, "%s", title.c_str());
  for (u32 j = 0; j < size;)
  {
    std::string hex_line = "";
    for (int i = 0; i < 16; i++)
    {
      hex_line += StringFromFormat("%02x ", data[j++]);

      if (j >= size)
        break;
    }
    GENERIC_LOG(type, LogTypes::LDEBUG, "   Data: %s", hex_line.c_str());
  }
}

}  // end of namespace Debugger
