// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

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

void Patch(u32 pc, const char* func_name);
u32 UnPatch(const std::string& patchName);
bool UnPatch(u32 addr, const std::string& name = {});
void Execute(u32 _CurrentPC, u32 _Instruction);

// Returns the HLE function index if the address is located in the function
u32 GetFunctionIndex(u32 address);
// Returns the HLE function index if the address matches the function start
u32 GetFirstFunctionIndex(u32 address);
HookType GetFunctionTypeByIndex(u32 index);
HookFlag GetFunctionFlagsByIndex(u32 index);

bool IsEnabled(HookFlag flag);
}
