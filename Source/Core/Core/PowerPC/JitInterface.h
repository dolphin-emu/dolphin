// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Core/MachineContext.h"

class CPUCoreBase;
class PointerWrap;
class JitBase;

namespace PowerPC
{
enum class CPUCore;
}

namespace Profiler
{
struct ProfileStats;
}

namespace JitInterface
{
enum class ExceptionType
{
  FIFOWrite,
  PairedQuantize,
  SpeculativeConstants
};

void DoState(PointerWrap& p);

CPUCoreBase* InitJitCore(PowerPC::CPUCore core);
CPUCoreBase* GetCore();

// Debugging
enum class ProfilingState
{
  Enabled,
  Disabled
};

void SetProfilingState(ProfilingState state);
void WriteProfileResults(const std::string& filename);
void GetProfileResults(Profiler::ProfileStats* prof_stats);
int GetHostCode(u32* address, const u8** code, u32* code_size);

// Memory Utilities
bool HandleFault(uintptr_t access_address, SContext* ctx);
bool HandleStackFault();

// Clearing CodeCache
void ClearCache();

// This clear is "safe" in the sense that it's okay to run from
// inside a JIT'ed block: it clears the instruction cache, but not
// the JIT'ed code.
void ClearSafe();

// If "forced" is true, a recompile is being requested on code that hasn't been modified.
void InvalidateICache(u32 address, u32 size, bool forced);

void CompileExceptionCheck(ExceptionType type);

/// used for the page fault unit test, don't use outside of tests!
void SetJit(JitBase* jit);

void Shutdown();
}  // namespace JitInterface
