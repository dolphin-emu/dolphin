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

namespace HLE
{
using HookFunction = void (*)(const Core::CPUThreadGuard&);

enum class HookType
{
  Start,    // Hook the beginning of the function and execute the function afterwards
  Replace,  // Replace the function with the HLE version
  None,     // Do not hook the function
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

void PatchFixedFunctions(Core::System& system);
void PatchFunctions(Core::System& system);
void Clear();
void Reload(Core::System& system);

void Patch(Core::System& system, u32 pc, std::string_view func_name);
u32 UnPatch(Core::System& system, std::string_view patch_name);
u32 UnpatchRange(Core::System& system, u32 start_addr, u32 end_addr);
void Execute(const Core::CPUThreadGuard& guard, u32 current_pc, u32 hook_index);
void ExecuteFromJIT(u32 current_pc, u32 hook_index);

// Returns the HLE hook index of the address
u32 GetHookByAddress(u32 address);
// Returns the HLE hook index if the address matches the function start
u32 GetHookByFunctionAddress(u32 address);
HookType GetHookTypeByIndex(u32 index);
HookFlag GetHookFlagsByIndex(u32 index);

bool IsEnabled(HookFlag flag);

// Performs the backend-independent preliminary checking before calling a
// FunctionObject to do the actual replacing. Typically, this callback will
// be in the backend itself, containing the backend-specific portions
// required in replacing a function.
//
// fn may be any object that satisfies the FunctionObject concept in the C++
// standard library. That is, any lambda, object with an overloaded function
// call operator, or regular function pointer.
//
// fn must return a bool indicating whether or not function replacing occurred.
// fn must also accept a parameter list of the form: fn(u32 function, HLE::HookType type).
template <typename FunctionObject>
bool ReplaceFunctionIfPossible(u32 address, FunctionObject fn)
{
  const u32 hook_index = GetHookByFunctionAddress(address);
  if (hook_index == 0)
    return false;

  const HookType type = GetHookTypeByIndex(hook_index);
  if (type != HookType::Start && type != HookType::Replace)
    return false;

  const HookFlag flags = GetHookFlagsByIndex(hook_index);
  if (!IsEnabled(flags))
    return false;

  return fn(hook_index, type);
}
}  // namespace HLE
