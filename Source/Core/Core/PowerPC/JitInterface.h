// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <variant>

#include "Common/CommonTypes.h"
#include "Core/MachineContext.h"

class CPUCoreBase;
class PointerWrap;
class JitBase;

namespace Core
{
class System;
}
namespace PowerPC
{
enum class CPUCore;
}

namespace Profiler
{
struct ProfileStats;
}

class JitInterface
{
public:
  explicit JitInterface(Core::System& system);
  JitInterface(const JitInterface&) = delete;
  JitInterface(JitInterface&&) = delete;
  JitInterface& operator=(const JitInterface&) = delete;
  JitInterface& operator=(JitInterface&&) = delete;
  ~JitInterface();

  void DoState(PointerWrap& p);

  CPUCoreBase* InitJitCore(PowerPC::CPUCore core);
  CPUCoreBase* GetCore() const;

  // Debugging
  enum class ProfilingState
  {
    Enabled,
    Disabled
  };
  enum class GetHostCodeError
  {
    NoJitActive,
    NoTranslation,
  };
  struct GetHostCodeResult
  {
    const u8* code;
    u32 code_size;
    u32 entry_address;
  };

  void SetProfilingState(ProfilingState state);
  void WriteProfileResults(const std::string& filename) const;
  void GetProfileResults(Profiler::ProfileStats* prof_stats) const;
  std::variant<GetHostCodeError, GetHostCodeResult> GetHostCode(u32 address) const;

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
  void InvalidateICacheLine(u32 address);
  void InvalidateICacheLines(u32 address, u32 count);
  static void InvalidateICacheLineFromJIT(JitInterface& jit_interface, u32 address);
  static void InvalidateICacheLinesFromJIT(JitInterface& jit_interface, u32 address, u32 count);

  enum class ExceptionType
  {
    FIFOWrite,
    PairedQuantize,
    SpeculativeConstants
  };
  void CompileExceptionCheck(ExceptionType type);
  static void CompileExceptionCheckFromJIT(JitInterface& jit_interface, ExceptionType type);

  /// used for the page fault unit test, don't use outside of tests!
  void SetJit(std::unique_ptr<JitBase> jit);

  void Shutdown();

private:
  std::unique_ptr<JitBase> m_jit;
  Core::System& m_system;
};
