// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"

// Stores information we need to batch-patch a MOV with a call to the slow read/write path after
// it faults. There will be 10s of thousands of these structs live, so be wary of making this too
// big.
struct TrampolineInfo final
{
  // The start of the store operation that failed -- we will patch a JMP here
  u8* start;

  // The start + len = end of the store operation (points to the next instruction)
  u32 len;

  // The PPC PC for the current load/store block
  u32 pc;

  // Saved because we need these to make the ABI call in the trampoline
  BitSet32 registersInUse;

  // The MOV operation
  Gen::X64Reg nonAtomicSwapStoreSrc;

  // src/dest for load/store
  Gen::OpArg op_value;
  Gen::X64Reg reg_addr;

  // Original SafeLoadXXX/SafeStoreXXX flags
  u8 flags;

  // Memory access size (in bytes)
  u8 accessSize : 4;

  // true if this is a read op vs a write
  bool read : 1;

  // for read operations, true if needs sign-extension after load
  bool signExtend : 1;
};
