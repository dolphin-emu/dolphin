// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HLE/HLE.h"

#include <algorithm>
#include <array>
#include <map>

#include "Common/CommonTypes.h"

#include "Core/ConfigManager.h"
#include "Core/GeckoCode.h"
#include "Core/HLE/HLE_Misc.h"
#include "Core/HLE/HLE_OS.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/ES/ES.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"

namespace HLE
{
using namespace PowerPC;

typedef void (*TPatchFunction)();

static std::map<u32, u32> s_original_instructions;

enum
{
  HLE_RETURNTYPE_BLR = 0,
  HLE_RETURNTYPE_RFI = 1,
};

struct SPatch
{
  char m_szPatchName[128];
  TPatchFunction PatchFunction;
  HookType type;
  HookFlag flags;
};

// clang-format off
constexpr std::array<SPatch, 21> OSPatches{{
    // Placeholder, OSPatches[0] is the "non-existent function" index
    {"FAKE_TO_SKIP_0",               HLE_Misc::UnimplementedFunction,       HookType::Replace, HookFlag::Generic},

    // Name doesn't matter, installed in CBoot::BootUp()
    {"HBReload",                     HLE_Misc::HBReload,                    HookType::Replace, HookFlag::Generic},

    // Debug/OS Support
    {"OSPanic",                      HLE_OS::HLE_OSPanic,                   HookType::Replace, HookFlag::Debug},

    // This needs to be put before vprintf (because vprintf is called indirectly by this)
    {"JUTWarningConsole_f",          HLE_OS::HLE_GeneralDebugPrint,         HookType::Start,   HookFlag::Debug},

    {"OSReport",                     HLE_OS::HLE_GeneralDebugPrint,         HookType::Start,   HookFlag::Debug},
    {"DEBUGPrint",                   HLE_OS::HLE_GeneralDebugPrint,         HookType::Start,   HookFlag::Debug},
    {"WUD_DEBUGPrint",               HLE_OS::HLE_GeneralDebugPrint,         HookType::Start,   HookFlag::Debug},
    {"vprintf",                      HLE_OS::HLE_GeneralDebugVPrint,        HookType::Start,   HookFlag::Debug},
    {"printf",                       HLE_OS::HLE_GeneralDebugPrint,         HookType::Start,   HookFlag::Debug},
    {"vdprintf",                     HLE_OS::HLE_LogVDPrint,                HookType::Start,   HookFlag::Debug},
    {"dprintf",                      HLE_OS::HLE_LogDPrint,                 HookType::Start,   HookFlag::Debug},
    {"vfprintf",                     HLE_OS::HLE_LogVFPrint,                HookType::Start,   HookFlag::Debug},
    {"fprintf",                      HLE_OS::HLE_LogFPrint,                 HookType::Start,   HookFlag::Debug},
    {"nlPrintf",                     HLE_OS::HLE_GeneralDebugPrint,         HookType::Start,   HookFlag::Debug},
    {"DWC_Printf",                   HLE_OS::HLE_GeneralDebugPrint,         HookType::Start,   HookFlag::Debug},
    {"puts",                         HLE_OS::HLE_GeneralDebugPrint,         HookType::Start,   HookFlag::Debug}, // gcc-optimized printf?
    {"___blank",                     HLE_OS::HLE_GeneralDebugPrint,         HookType::Start,   HookFlag::Debug}, // used for early init things (normally)
    {"__write_console",              HLE_OS::HLE_write_console,             HookType::Start,   HookFlag::Debug}, // used by sysmenu (+more?)

    {"GeckoCodehandler",             HLE_Misc::GeckoCodeHandlerICacheFlush, HookType::Start,   HookFlag::Fixed},
    {"GeckoHandlerReturnTrampoline", HLE_Misc::GeckoReturnTrampoline,       HookType::Replace, HookFlag::Fixed},
    {"AppLoaderReport",              HLE_OS::HLE_GeneralDebugPrint,         HookType::Replace, HookFlag::Fixed} // apploader needs OSReport-like function
}};

constexpr std::array<SPatch, 1> OSBreakPoints{{
    {"FAKE_TO_SKIP_0", HLE_Misc::UnimplementedFunction, HookType::Start, HookFlag::Generic},
}};
// clang-format on

void Patch(u32 addr, std::string_view func_name)
{
  for (u32 i = 1; i < OSPatches.size(); ++i)
  {
    if (OSPatches[i].m_szPatchName == func_name)
    {
      s_original_instructions[addr] = i;
      PowerPC::ppcState.iCache.Invalidate(addr);
      return;
    }
  }
}

void PatchFixedFunctions()
{
  // HLE jump to loader (homebrew).  Disabled when Gecko is active as it interferes with the code
  // handler
  if (!SConfig::GetInstance().bEnableCheats)
  {
    Patch(0x80001800, "HBReload");
    Memory::CopyToEmu(0x00001804, "STUBHAXX", 8);
  }

  // Not part of the binary itself, but either we or Gecko OS might insert
  // this, and it doesn't clear the icache properly.
  Patch(Gecko::ENTRY_POINT, "GeckoCodehandler");
  // This has to always be installed even if cheats are not enabled because of the possiblity of
  // loading a savestate where PC is inside the code handler while cheats are disabled.
  Patch(Gecko::HLE_TRAMPOLINE_ADDRESS, "GeckoHandlerReturnTrampoline");
}

void PatchFunctions()
{
  // Remove all hooks that aren't fixed address hooks
  for (auto i = s_original_instructions.begin(); i != s_original_instructions.end();)
  {
    if (OSPatches[i->second].flags != HookFlag::Fixed)
    {
      PowerPC::ppcState.iCache.Invalidate(i->first);
      i = s_original_instructions.erase(i);
    }
    else
    {
      ++i;
    }
  }

  for (u32 i = 1; i < OSPatches.size(); ++i)
  {
    // Fixed hooks don't map to symbols
    if (OSPatches[i].flags == HookFlag::Fixed)
      continue;

    for (const auto& symbol : g_symbolDB.GetSymbolsFromName(OSPatches[i].m_szPatchName))
    {
      for (u32 addr = symbol->address; addr < symbol->address + symbol->size; addr += 4)
      {
        s_original_instructions[addr] = i;
        PowerPC::ppcState.iCache.Invalidate(addr);
      }
      INFO_LOG(OSHLE, "Patching %s %08x", OSPatches[i].m_szPatchName, symbol->address);
    }
  }

  if (SConfig::GetInstance().bEnableDebugging)
  {
    for (size_t i = 1; i < OSBreakPoints.size(); ++i)
    {
      for (const auto& symbol : g_symbolDB.GetSymbolsFromName(OSBreakPoints[i].m_szPatchName))
      {
        PowerPC::breakpoints.Add(symbol->address, false);
        INFO_LOG(OSHLE, "Adding BP to %s %08x", OSBreakPoints[i].m_szPatchName, symbol->address);
      }
    }
  }

  // CBreakPoints::AddBreakPoint(0x8000D3D0, false);
}

void Clear()
{
  s_original_instructions.clear();
}

void Reload()
{
  Clear();
  PatchFixedFunctions();
  PatchFunctions();
}

void Execute(u32 _CurrentPC, u32 _Instruction)
{
  unsigned int FunctionIndex = _Instruction & 0xFFFFF;
  if (FunctionIndex > 0 && FunctionIndex < OSPatches.size())
  {
    OSPatches[FunctionIndex].PatchFunction();
  }
  else
  {
    PanicAlert("HLE system tried to call an undefined HLE function %i.", FunctionIndex);
  }
}

u32 GetFunctionIndex(u32 address)
{
  auto iter = s_original_instructions.find(address);
  return (iter != s_original_instructions.end()) ? iter->second : 0;
}

u32 GetFirstFunctionIndex(u32 address)
{
  u32 index = GetFunctionIndex(address);
  auto first = std::find_if(
      s_original_instructions.begin(), s_original_instructions.end(),
      [=](const auto& entry) { return entry.second == index && entry.first < address; });
  return first == std::end(s_original_instructions) ? index : 0;
}

HookType GetFunctionTypeByIndex(u32 index)
{
  return OSPatches[index].type;
}

HookFlag GetFunctionFlagsByIndex(u32 index)
{
  return OSPatches[index].flags;
}

bool IsEnabled(HookFlag flag)
{
  return flag != HLE::HookFlag::Debug || SConfig::GetInstance().bEnableDebugging ||
         PowerPC::GetMode() == PowerPC::CoreMode::Interpreter;
}

u32 UnPatch(std::string_view patch_name)
{
  const auto patch = std::find_if(std::begin(OSPatches), std::end(OSPatches),
                                  [&](const SPatch& p) { return patch_name == p.m_szPatchName; });
  if (patch == std::end(OSPatches))
    return 0;

  if (patch->flags == HookFlag::Fixed)
  {
    const u32 patch_idx = static_cast<u32>(std::distance(OSPatches.begin(), patch));
    u32 addr = 0;
    // Reverse search by OSPatch key instead of address
    for (auto i = s_original_instructions.begin(); i != s_original_instructions.end();)
    {
      if (i->second == patch_idx)
      {
        addr = i->first;
        PowerPC::ppcState.iCache.Invalidate(i->first);
        i = s_original_instructions.erase(i);
      }
      else
      {
        ++i;
      }
    }
    return addr;
  }

  const auto& symbols = g_symbolDB.GetSymbolsFromName(patch_name);
  if (!symbols.empty())
  {
    const auto& symbol = symbols[0];
    for (u32 addr = symbol->address; addr < symbol->address + symbol->size; addr += 4)
    {
      s_original_instructions.erase(addr);
      PowerPC::ppcState.iCache.Invalidate(addr);
    }
    return symbol->address;
  }

  return 0;
}

bool UnPatch(u32 addr, std::string_view name)
{
  auto itr = s_original_instructions.find(addr);
  if (itr == s_original_instructions.end())
    return false;

  if (!name.empty() && name != OSPatches[itr->second].m_szPatchName)
    return false;

  s_original_instructions.erase(itr);
  PowerPC::ppcState.iCache.Invalidate(addr);
  return true;
}

}  // end of namespace HLE
