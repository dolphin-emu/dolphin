// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <cstdint>

#include "Common/CommonTypes.h"
#include "Common/x64Reg.h"
#include "Core/PowerPC/Jit64Common/BlockCache.h"
#include "Core/PowerPC/Jit64Common/Jit64AsmCommon.h"
#include "Core/PowerPC/Jit64Common/TrampolineCache.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

namespace PPCAnalyst
{
class CodeBuffer;
}

#if defined(_MSC_VER) && _MSC_VER <= 1800
#define constexpr const
#endif
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

constexpr size_t CODE_SIZE = 1024 * 1024 * 32;

class Jitx86Base : public JitBase, public QuantizedMemoryRoutines
{
protected:
  bool BackPatch(u32 emAddress, SContext* ctx);
  JitBlockCache blocks{*this};
  TrampolineCache trampolines;

public:
  JitBlockCache* GetBlockCache() override { return &blocks; }
  bool HandleFault(uintptr_t access_address, SContext* ctx) override;
};

void LogGeneratedX86(size_t size, const PPCAnalyst::CodeBuffer* code_buffer, const u8* normalEntry,
                     const JitBlock* b);
