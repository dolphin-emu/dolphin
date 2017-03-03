// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Core/MachineContext.h"

class CPUCoreBase;
class PointerWrap;
struct ProfileStats;

namespace JitInterface
{
enum class ExceptionType
{
  FIFOWrite,
  PairedQuantize,
  SpeculativeConstants
};

void DoState(PointerWrap& p);

CPUCoreBase* InitJitCore(int core);
CPUCoreBase* GetCore();

// Debugging
void WriteProfileResults(const std::string& filename);
void GetProfileResults(ProfileStats* prof_stats);
int GetHostCode(u32* address, const u8** code, u32* code_size);

// Memory Utilities
bool HandleFault(uintptr_t access_address, SContext* ctx);
bool HandleStackFault();

// Clearing CodeCache
void ClearCache();

void ClearSafe();

// If "forced" is true, a recompile is being requested on code that hasn't been modified.
void InvalidateICache(u32 address, u32 size, bool forced);

void CompileExceptionCheck(ExceptionType type);

void Shutdown();
}
