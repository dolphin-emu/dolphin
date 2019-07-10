// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string_view>

#include "Common/CommonTypes.h"

namespace HLE
{
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

void PatchFixedFunctions();
void PatchFunctions();
void Clear();
void Reload();

void Patch(u32 pc, std::string_view func_name);
u32 UnPatch(std::string_view patch_name);
bool UnPatch(u32 addr, std::string_view name = {});
void Execute(u32 _CurrentPC, u32 _Instruction);

// Returns the HLE function index if the address is located in the function
u32 GetFunctionIndex(u32 address);
// Returns the HLE function index if the address matches the function start
u32 GetFirstFunctionIndex(u32 address);
HookType GetFunctionTypeByIndex(u32 index);
HookFlag GetFunctionFlagsByIndex(u32 index);

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
  const u32 function = GetFirstFunctionIndex(address);
  if (function == 0)
    return false;

  const HookType type = GetFunctionTypeByIndex(function);
  if (type != HookType::Start && type != HookType::Replace)
    return false;

  const HookFlag flags = GetFunctionFlagsByIndex(function);
  if (!IsEnabled(flags))
    return false;

  return fn(function, type);
}
}  // namespace HLE
