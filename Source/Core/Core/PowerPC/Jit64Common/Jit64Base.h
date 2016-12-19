// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstdint>

#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"
#include "Common/x64Reg.h"
#include "Core/PowerPC/Jit64Common/BlockCache.h"
#include "Core/PowerPC/Jit64Common/Jit64AsmCommon.h"
#include "Core/PowerPC/Jit64Common/TrampolineCache.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

namespace PPCAnalyst
{
class CodeBuffer;
}

// The following register assignments are common to Jit64 and Jit64IL:
// RSCRATCH and RSCRATCH2 are always scratch registers and can be used without
// limitation.
constexpr Gen::X64Reg RSCRATCH = Gen::RAX;
constexpr Gen::X64Reg RSCRATCH2 = Gen::RDX;
// RSCRATCH_EXTRA may be in the allocation order, so it has to be flushed
// before use.
constexpr Gen::X64Reg RSCRATCH_EXTRA = Gen::RCX;
// RMEM points to the start of emulated memory.
constexpr Gen::X64Reg RMEM = Gen::RBX;
// RPPCSTATE points to ppcState + 0x80.  It's offset because we want to be able
// to address as much as possible in a one-byte offset form.
constexpr Gen::X64Reg RPPCSTATE = Gen::RBP;

class Jitx86Base : public JitBase, public QuantizedMemoryRoutines
{
protected:
  struct Jit64State final
  {
    // If this is set, a load or store already prepared a jump to the exception handler for us,
    // so just fixup that branch instead of testing for a DSI again.
    bool fixup_exception_handler = false;
    Gen::FixupBranch exception_handler{};
  };

  bool BackPatch(u32 emAddress, SContext* ctx);

  JitBlockCache blocks;
  TrampolineCache trampolines;
  Jit64State m_jit64_state;

public:
  JitBlockCache* GetBlockCache() override { return &blocks; }
  bool HandleFault(uintptr_t access_address, SContext* ctx) override;

  Jit64State& JIT64State() { return m_jit64_state; }
  const Jit64State& JIT64State() const { return m_jit64_state; }
};

void LogGeneratedX86(int size, PPCAnalyst::CodeBuffer* code_buffer, const u8* normalEntry,
                     JitBlock* b);
