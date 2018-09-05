// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>

#include "Core/PowerPC/Gekko.h"

// Flags that indicate what an instruction can do.
enum : u32
{
  FL_SET_CR0 = (1 << 0),  // Sets CR0.
  FL_SET_CR1 = (1 << 1),  // Sets CR1.
  FL_SET_CRn = (1 << 2),  // Encoding decides which CR can be set.
  FL_SET_CRx = FL_SET_CR0 | FL_SET_CR1 | FL_SET_CRn,
  FL_SET_CA = (1 << 3),   // Sets the carry flag.
  FL_READ_CA = (1 << 4),  // Reads the carry flag.
  FL_RC_BIT = (1 << 5),   // Sets the record bit.
  FL_RC_BIT_F =
      (1 << 6),  // Sets the record bit. Used for floating point instructions that do this.
  FL_ENDBLOCK =
      (1 << 7),  // Specifies that the instruction can be used as an exit point for a JIT block.
  FL_IN_A = (1 << 8),   // Uses rA as an input.
  FL_IN_A0 = (1 << 9),  // Uses rA as an input. Indicates that if rA is zero, the value zero is
                        // used, not the contents of r0.
  FL_IN_B = (1 << 10),  // Uses rB as an input.
  FL_IN_C = (1 << 11),  // Uses rC as an input.
  FL_IN_S = (1 << 12),  // Uses rS as an input.
  FL_IN_AB = FL_IN_A | FL_IN_B,
  FL_IN_AC = FL_IN_A | FL_IN_C,
  FL_IN_ABC = FL_IN_A | FL_IN_B | FL_IN_C,
  FL_IN_SB = FL_IN_S | FL_IN_B,
  FL_IN_A0B = FL_IN_A0 | FL_IN_B,
  FL_IN_A0BC = FL_IN_A0 | FL_IN_B | FL_IN_C,
  FL_OUT_D = (1 << 13),  // rD is used as a destination.
  FL_OUT_A = (1 << 14),  // rA is used as a destination.
  FL_OUT_AD = FL_OUT_A | FL_OUT_D,
  FL_TIMER = (1 << 15),            // Used only for mftb.
  FL_CHECKEXCEPTIONS = (1 << 16),  // Used with rfi/rfid.
  FL_EVIL =
      (1 << 17),  // Historically used to refer to instructions that messed up Super Monkey Ball.
  FL_USE_FPU = (1 << 18),     // Used to indicate a floating point instruction.
  FL_LOADSTORE = (1 << 19),   // Used to indicate a load/store instruction.
  FL_SET_FPRF = (1 << 20),    // Sets bits in the FPRF.
  FL_READ_FPRF = (1 << 21),   // Reads bits from the FPRF.
  FL_SET_OE = (1 << 22),      // Sets the overflow flag.
  FL_IN_FLOAT_A = (1 << 23),  // frA is used as an input.
  FL_IN_FLOAT_B = (1 << 24),  // frB is used as an input.
  FL_IN_FLOAT_C = (1 << 25),  // frC is used as an input.
  FL_IN_FLOAT_S = (1 << 26),  // frS is used as an input.
  FL_IN_FLOAT_D = (1 << 27),  // frD is used as an input.
  FL_IN_FLOAT_AB = FL_IN_FLOAT_A | FL_IN_FLOAT_B,
  FL_IN_FLOAT_AC = FL_IN_FLOAT_A | FL_IN_FLOAT_C,
  FL_IN_FLOAT_ABC = FL_IN_FLOAT_A | FL_IN_FLOAT_B | FL_IN_FLOAT_C,
  FL_OUT_FLOAT_D = (1 << 28),  // frD is used as a destination.
  // Used in the case of double ops (they don't modify the top half of the output)
  FL_INOUT_FLOAT_D = FL_IN_FLOAT_D | FL_OUT_FLOAT_D,
};

enum class OpType : u8
{
  Invalid,
  Subtable,
  Integer,
  CR,
  SPR,
  System,
  SystemFP,
  Load,
  Store,
  LoadFP,
  StoreFP,
  DoubleFP,
  SingleFP,
  LoadPS,
  StorePS,
  PS,
  DataCache,
  InstructionCache,
  Branch,
  Unknown,
};

// The current (half-open) ranges of OpID.
// An OpID identifies a distinctive instruction encoding and constitutes
// a shared concept of a 'PPC operation' among the JITs and other
// CPU emulation components.
// Use PPCTables::GetOpID to get the right one for a given instruction.
enum class OpID : int
{
  Invalid = 0,
#include "Core/PowerPC/Tables/OpID_Ranges.gen.h"
};

namespace PPCTables
{
OpID GetOpID(UGeckoInstruction inst);
bool IsValidInstruction(UGeckoInstruction inst);
int Cycles(OpID opid);
u32 Flags(OpID opid);
OpType Type(OpID opid);
const char* OpName(OpID opid);

const char* GetInstructionName(UGeckoInstruction inst);
}  // namespace PPCTables
