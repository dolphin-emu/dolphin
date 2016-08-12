// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/PowerPC/JitCommon/JitBase.h"

#include "Common/x64Emitter.h"
#include "Core/PowerPC/Jit64Common/Jit64AsmCommon.h"
#include "Core/PowerPC/Jit64Common/JitCache.h"
#include "Core/PowerPC/Jit64Common/Jit_Util.h"
#include "Core/PowerPC/Jit64Common/TrampolineCache.h"

// TODO: find a better place for x86-specific stuff
// The following register assignments are common to Jit64 and Jit64IL:
// RSCRATCH and RSCRATCH2 are always scratch registers and can be used without
// limitation.
#define RSCRATCH RAX
#define RSCRATCH2 RDX
// RSCRATCH_EXTRA may be in the allocation order, so it has to be flushed
// before use.
#define RSCRATCH_EXTRA RCX
// RMEM points to the start of emulated memory.
#define RMEM RBX
// RPPCSTATE points to ppcState + 0x80.  It's offset because we want to be able
// to address as much as possible in a one-byte offset form.
#define RPPCSTATE RBP

class Jitx86Base : public JitBase, public QuantizedMemoryRoutines
{
protected:
  bool BackPatch(u32 emAddress, SContext* ctx);
  JitBlockCache blocks;
  TrampolineCache trampolines;

public:
  JitBlockCache* GetBlockCache() override { return &blocks; }
  bool HandleFault(uintptr_t access_address, SContext* ctx) override;
};

// Merged routines that should be moved somewhere better
void LogGeneratedX86(int size, PPCAnalyst::CodeBuffer* code_buffer, const u8* normalEntry,
                     JitBlock* b);
