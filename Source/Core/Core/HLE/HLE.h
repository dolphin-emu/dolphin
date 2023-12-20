// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string_view>

#include "Common/CommonTypes.h"

namespace Core
{
class CPUThreadGuard;
class System;
}  // namespace Core

namespace PowerPC
{
enum class CoreMode;
}

namespace HLE
{
using HookFunction = void (*)(const Core::CPUThreadGuard&);

enum class HookType
{
  None,     // Do not hook the function
  Start,    // Hook the beginning of the function and execute the function afterwards
  Replace,  // Replace the function with the HLE version
};

enum class HookFlag
{
  Generic,  // Miscellaneous function
  Debug,    // Debug output function
  Fixed,    // An arbitrary hook mapped to a fixed address instead of a symbol
};

struct Hook
{
  char name[128];
  HookFunction function;
  HookType type;
  HookFlag flags;
};

struct TryReplaceFunctionResult
{
  HookType type = HookType::None;
  u32 hook_index = 0;

  explicit operator bool() const { return type != HookType::None; }
};

void PatchFixedFunctions(Core::System& system);
void PatchFunctions(Core::System& system);
void Clear();
void Reload(Core::System& system);

void Patch(Core::System& system, u32 pc, std::string_view func_name);
u32 UnPatch(Core::System& system, std::string_view patch_name);
u32 UnpatchRange(Core::System& system, u32 start_addr, u32 end_addr);
void Execute(const Core::CPUThreadGuard& guard, u32 current_pc, u32 hook_index);
void ExecuteFromJIT(u32 current_pc, u32 hook_index, Core::System& system);

// Returns the HLE hook index of the address
u32 GetHookByAddress(u32 address);
// Returns the HLE hook index if the address matches the function start
u32 GetHookByFunctionAddress(u32 address);
HookType GetHookTypeByIndex(u32 index);
HookFlag GetHookFlagsByIndex(u32 index);

bool IsEnabled(HookFlag flag, PowerPC::CoreMode mode);

// Performs the backend-independent preliminary checking for whether a function
// can be HLEd. If it can be, the information needed for HLEing it is returned.
TryReplaceFunctionResult TryReplaceFunction(u32 address, PowerPC::CoreMode mode);

}  // namespace HLE
