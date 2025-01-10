// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"
#include "Core/PowerPC/Gekko.h"

// Stores information we need to batch-patch a MOV with a call to the slow read/write path after
// it faults. There will be 10s of thousands of these structs live, so be wary of making this too
// big.
struct TrampolineInfo final
{
  // The start of the store operation that failed -- we will patch a JMP here
  u8* start = nullptr;

  // The start + len = end of the store operation (points to the next instruction)
  u32 len = 0;

  // The PPC PC for the current load/store block
  u32 pc = 0;

  // The instruction which is being emulated
  UGeckoInstruction inst;

  // Saved because we need these to make the ABI call in the trampoline
  BitSet32 registersInUse{};

  // The MOV operation
  Gen::X64Reg nonAtomicSwapStoreSrc{};

  // src/dest for load/store
  s32 offset = 0;
  Gen::X64Reg op_reg{};
  Gen::OpArg op_arg{};

  // Original SafeLoadXXX/SafeStoreXXX flags
  u8 flags = 0;

  // Memory access size (in bytes)
  u8 accessSize : 4 = 0;

  // true if this is a read op vs a write
  bool read : 1 = false;

  // for read operations, true if needs sign-extension after load
  bool signExtend : 1 = false;

  // Set to true if we added the offset to the address and need to undo it
  bool offsetAddedToAddress : 1 = false;
};
