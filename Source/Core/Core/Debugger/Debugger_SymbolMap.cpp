// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Debugger/Debugger_SymbolMap.h"

#include <cstdio>
#include <functional>
#include <string>

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

namespace Dolphin_Debugger
{
// Returns true if the address is not a valid RAM address or NULL.
static bool IsStackBottom(const Core::CPUThreadGuard& guard, u32 addr)
{
  return !addr || !PowerPC::MMU::HostIsRAMAddress(guard, addr);
}

static void WalkTheStack(const Core::CPUThreadGuard& guard,
                         const std::function<void(u32)>& stack_step)
{
  const auto& ppc_state = guard.GetSystem().GetPPCState();

  if (!IsStackBottom(guard, ppc_state.gpr[1]))
  {
    u32 addr = PowerPC::MMU::HostRead_U32(guard, ppc_state.gpr[1]);  // SP

    // Walk the stack chain
    for (int count = 0; !IsStackBottom(guard, addr + 4) && (count < 20); ++count)
    {
      u32 func_addr = PowerPC::MMU::HostRead_U32(guard, addr + 4);
      stack_step(func_addr);

      if (IsStackBottom(guard, addr))
        break;

      addr = PowerPC::MMU::HostRead_U32(guard, addr);
    }
  }
}

// Returns callstack "formatted for debugging" - meaning that it
// includes LR as the last item, and all items are the last step,
// instead of "pointing ahead"
bool GetCallstack(const Core::CPUThreadGuard& guard, std::vector<CallstackEntry>& output)
{
  auto& system = guard.GetSystem();
  auto& power_pc = system.GetPowerPC();
  const auto& ppc_state = power_pc.GetPPCState();

  if (!Core::IsRunning(system) || !PowerPC::MMU::HostIsRAMAddress(guard, ppc_state.gpr[1]))
    return false;

  if (LR(ppc_state) == 0)
  {
    output.push_back({
        .Name = "(error: LR=0)",
        .vAddress = 0,
    });
    return false;
  }

  auto& ppc_symbol_db = power_pc.GetSymbolDB();

  output.push_back({
      .Name = fmt::format(" * {} [ LR = {:08x} ]\n", ppc_symbol_db.GetDescription(LR(ppc_state)),
                          LR(ppc_state) - 4),
      .vAddress = LR(ppc_state) - 4,
  });

  WalkTheStack(guard, [&output, &ppc_symbol_db](u32 func_addr) {
    std::string_view func_desc = ppc_symbol_db.GetDescription(func_addr);
    if (func_desc.empty() || func_desc == "Invalid")
      func_desc = "(unknown)";

    output.push_back({
        .Name = fmt::format(" * {} [ addr = {:08x} ]\n", func_desc, func_addr - 4),
        .vAddress = func_addr - 4,
    });
  });

  return true;
}

void PrintCallstack(const Core::CPUThreadGuard& guard, Common::Log::LogType type,
                    Common::Log::LogLevel level)
{
  auto& power_pc = guard.GetSystem().GetPowerPC();
  const auto& ppc_state = power_pc.GetPPCState();
  auto& ppc_symbol_db = power_pc.GetSymbolDB();

  GENERIC_LOG_FMT(type, level, "== STACK TRACE - SP = {:08x} ==", ppc_state.gpr[1]);

  if (LR(ppc_state) == 0)
  {
    GENERIC_LOG_FMT(type, level, " LR = 0 - this is bad");
  }

  if (const std::string_view lr_desc = ppc_symbol_db.GetDescription(LR(ppc_state));
      lr_desc != ppc_symbol_db.GetDescription(ppc_state.pc))
  {
    GENERIC_LOG_FMT(type, level, " * {}  [ LR = {:08x} ]", lr_desc, LR(ppc_state));
  }

  WalkTheStack(guard, [type, level, &ppc_symbol_db](u32 func_addr) {
    std::string_view func_desc = ppc_symbol_db.GetDescription(func_addr);
    if (func_desc.empty() || func_desc == "Invalid")
      func_desc = "(unknown)";
    GENERIC_LOG_FMT(type, level, " * {} [ addr = {:08x} ]", func_desc, func_addr);
  });
}

void PrintDataBuffer(const Core::System& system, Common::Log::LogType type, u32 address, u32 size,
                     std::string_view title)
{
  const u8* data = system.GetMemory().GetPointerForRange(address, size);

  GENERIC_LOG_FMT(type, Common::Log::LogLevel::LDEBUG, "{}", title);
  for (u32 j = 0; j < size;)
  {
    std::string hex_line;
    for (int i = 0; i < 16; i++)
    {
      hex_line += fmt::format("{:02x} ", data[j++]);

      if (j >= size)
        break;
    }
    GENERIC_LOG_FMT(type, Common::Log::LogLevel::LDEBUG, "   Data: {}", hex_line);
  }
}

}  // namespace Dolphin_Debugger
