// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HLE/HLE.h"

#include <algorithm>
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
static const SPatch OSPatches[] = {
    // Placeholder, OSPatches[0] is the "non-existent function" index
    {"FAKE_TO_SKIP_0",               HLE_Misc::UnimplementedFunction,       HLE_HOOK_REPLACE, HLE_TYPE_GENERIC},

    {"PanicAlert",                   HLE_Misc::HLEPanicAlert,               HLE_HOOK_REPLACE, HLE_TYPE_DEBUG},

    // Name doesn't matter, installed in CBoot::BootUp()
    {"HBReload",                     HLE_Misc::HBReload,                    HLE_HOOK_REPLACE, HLE_TYPE_GENERIC},

    // Debug/OS Support
    {"OSPanic",                      HLE_OS::HLE_OSPanic,                   HLE_HOOK_REPLACE, HLE_TYPE_DEBUG},

    // This needs to be put before vprintf (because vprintf is called indirectly by this)
    {"JUTWarningConsole_f",          HLE_OS::HLE_GeneralDebugPrint,         HLE_HOOK_START,   HLE_TYPE_DEBUG},

    {"OSReport",                     HLE_OS::HLE_GeneralDebugPrint,         HLE_HOOK_START,   HLE_TYPE_DEBUG},
    {"DEBUGPrint",                   HLE_OS::HLE_GeneralDebugPrint,         HLE_HOOK_START,   HLE_TYPE_DEBUG},
    {"WUD_DEBUGPrint",               HLE_OS::HLE_GeneralDebugPrint,         HLE_HOOK_START,   HLE_TYPE_DEBUG},
    {"vprintf",                      HLE_OS::HLE_GeneralDebugVPrint,        HLE_HOOK_START,   HLE_TYPE_DEBUG},
    {"printf",                       HLE_OS::HLE_GeneralDebugPrint,         HLE_HOOK_START,   HLE_TYPE_DEBUG},
    {"vdprintf",                     HLE_OS::HLE_LogVDPrint,                HLE_HOOK_START,   HLE_TYPE_DEBUG},
    {"dprintf",                      HLE_OS::HLE_LogDPrint,                 HLE_HOOK_START,   HLE_TYPE_DEBUG},
    {"vfprintf",                     HLE_OS::HLE_LogVFPrint,                HLE_HOOK_START,   HLE_TYPE_DEBUG},
    {"fprintf",                      HLE_OS::HLE_LogFPrint,                 HLE_HOOK_START,   HLE_TYPE_DEBUG},
    {"nlPrintf",                     HLE_OS::HLE_GeneralDebugPrint,         HLE_HOOK_START,   HLE_TYPE_DEBUG},
    {"puts",                         HLE_OS::HLE_GeneralDebugPrint,         HLE_HOOK_START,   HLE_TYPE_DEBUG}, // gcc-optimized printf?
    {"___blank",                     HLE_OS::HLE_GeneralDebugPrint,         HLE_HOOK_START,   HLE_TYPE_DEBUG}, // used for early init things (normally)
    {"__write_console",              HLE_OS::HLE_write_console,             HLE_HOOK_START,   HLE_TYPE_DEBUG}, // used by sysmenu (+more?)

    {"GeckoCodehandler",             HLE_Misc::GeckoCodeHandlerICacheFlush, HLE_HOOK_START,   HLE_TYPE_FIXED},
    {"GeckoHandlerReturnTrampoline", HLE_Misc::GeckoReturnTrampoline,       HLE_HOOK_REPLACE, HLE_TYPE_FIXED},
    {"AppLoaderReport",              HLE_OS::HLE_GeneralDebugPrint,         HLE_HOOK_REPLACE, HLE_TYPE_FIXED} // apploader needs OSReport-like function
};

static const SPatch OSBreakPoints[] = {
    {"FAKE_TO_SKIP_0", HLE_Misc::UnimplementedFunction, HLE_HOOK_START, HLE_TYPE_GENERIC},
};
// clang-format on

void Patch(u32 addr, const char* hle_func_name)
{
  for (u32 i = 1; i < ArraySize(OSPatches); ++i)
  {
    if (!strcmp(OSPatches[i].m_szPatchName, hle_func_name))
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
    if (OSPatches[i->second].flags != HLE_TYPE_FIXED)
    {
      PowerPC::ppcState.iCache.Invalidate(i->first);
      i = s_original_instructions.erase(i);
    }
    else
    {
      ++i;
    }
  }

  for (u32 i = 1; i < ArraySize(OSPatches); ++i)
  {
    // Fixed hooks don't map to symbols
    if (OSPatches[i].flags == HLE_TYPE_FIXED)
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
    for (size_t i = 1; i < ArraySize(OSBreakPoints); ++i)
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
  if (FunctionIndex > 0 && FunctionIndex < ArraySize(OSPatches))
  {
    OSPatches[FunctionIndex].PatchFunction();
  }
  else
  {
    PanicAlert("HLE system tried to call an undefined HLE function %i.", FunctionIndex);
  }

  // _dbg_assert_msg_(HLE,NPC == LR, "Broken HLE function (doesn't set NPC)",
  // OSPatches[pos].m_szPatchName);
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

int GetFunctionTypeByIndex(u32 index)
{
  return OSPatches[index].type;
}

int GetFunctionFlagsByIndex(u32 index)
{
  return OSPatches[index].flags;
}

bool IsEnabled(int flags)
{
  return flags != HLE::HLE_TYPE_DEBUG || SConfig::GetInstance().bEnableDebugging ||
         PowerPC::GetMode() == PowerPC::CoreMode::Interpreter;
}

u32 UnPatch(const std::string& patch_name)
{
  auto* patch = std::find_if(std::begin(OSPatches), std::end(OSPatches),
                             [&](const SPatch& p) { return patch_name == p.m_szPatchName; });
  if (patch == std::end(OSPatches))
    return 0;

  if (patch->flags == HLE_TYPE_FIXED)
  {
    u32 patch_idx = static_cast<u32>(patch - OSPatches);
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
  if (symbols.size())
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

bool UnPatch(u32 addr, const std::string& name)
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
