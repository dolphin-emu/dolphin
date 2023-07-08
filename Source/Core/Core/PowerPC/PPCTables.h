// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Core/PowerPC/Gekko.h"

// Flags that indicate what an instruction can do.
enum InstructionFlags : u64
{
  FL_SET_CR0 = (1ull << 0),  // Sets CR0.
  FL_SET_CR1 = (1ull << 1),  // Sets CR1.
  FL_SET_CRn = (1ull << 2),  // Sets a CR determined by the CRFD field.
  FL_SET_CA = (1ull << 3),   // Sets the carry flag.
  FL_READ_CA = (1ull << 4),  // Reads the carry flag.
  FL_RC_BIT = (1ull << 5),   // Sets the record bit.
  FL_RC_BIT_F =
      (1ull << 6),  // Sets the record bit. Used for floating point instructions that do this.
  FL_ENDBLOCK =
      (1ull << 7),  // Specifies that the instruction can be used as an exit point for a JIT block.
  FL_IN_A = (1ull << 8),   // Uses rA as an input.
  FL_IN_A0 = (1ull << 9),  // Uses rA as an input. Indicates that if rA is zero, the value zero is
                           // used, not the contents of r0.
  FL_IN_B = (1ull << 10),  // Uses rB as an input.
  FL_IN_C = (1ull << 11),  // Uses rC as an input.
  FL_IN_S = (1ull << 12),  // Uses rS as an input.
  FL_IN_AB = FL_IN_A | FL_IN_B,
  FL_IN_AC = FL_IN_A | FL_IN_C,
  FL_IN_ABC = FL_IN_A | FL_IN_B | FL_IN_C,
  FL_IN_SB = FL_IN_S | FL_IN_B,
  FL_IN_A0B = FL_IN_A0 | FL_IN_B,
  FL_IN_A0BC = FL_IN_A0 | FL_IN_B | FL_IN_C,
  FL_OUT_D = (1ull << 13),  // rD is used as a destination.
  FL_OUT_A = (1ull << 14),  // rA is used as a destination.
  FL_OUT_AD = FL_OUT_A | FL_OUT_D,
  FL_TIMER = (1ull << 15),            // Used only for mftb.
  FL_CHECKEXCEPTIONS = (1ull << 16),  // Used with rfi/rfid.
  FL_EVIL =
      (1ull << 17),  // Historically used to refer to instructions that messed up Super Monkey Ball.
  FL_USE_FPU = (1ull << 18),     // Used to indicate a floating point instruction.
  FL_LOADSTORE = (1ull << 19),   // Used to indicate a load/store instruction.
  FL_SET_FPRF = (1ull << 20),    // Sets bits in the FPRF.
  FL_READ_FPRF = (1ull << 21),   // Reads bits from the FPRF.
  FL_SET_OE = (1ull << 22),      // Sets the overflow flag.
  FL_IN_FLOAT_A = (1ull << 23),  // frA is used as an input.
  FL_IN_FLOAT_B = (1ull << 24),  // frB is used as an input.
  FL_IN_FLOAT_C = (1ull << 25),  // frC is used as an input.
  FL_IN_FLOAT_S = (1ull << 26),  // frS is used as an input.
  FL_IN_FLOAT_D = (1ull << 27),  // frD is used as an input.
  FL_IN_FLOAT_AB = FL_IN_FLOAT_A | FL_IN_FLOAT_B,
  FL_IN_FLOAT_AC = FL_IN_FLOAT_A | FL_IN_FLOAT_C,
  FL_IN_FLOAT_ABC = FL_IN_FLOAT_A | FL_IN_FLOAT_B | FL_IN_FLOAT_C,
  FL_OUT_FLOAT_D = (1ull << 28),  // frD is used as a destination.
  // Used in the case of double ops (they don't modify the top half of the output)
  FL_INOUT_FLOAT_D = FL_IN_FLOAT_D | FL_OUT_FLOAT_D,
  FL_IN_FLOAT_A_BITEXACT = (1ull << 29),  // The output is based on the exact bits in frA.
  FL_IN_FLOAT_B_BITEXACT = (1ull << 30),  // The output is based on the exact bits in frB.
  FL_IN_FLOAT_C_BITEXACT = (1ull << 31),  // The output is based on the exact bits in frC.
  FL_IN_FLOAT_AB_BITEXACT = FL_IN_FLOAT_A_BITEXACT | FL_IN_FLOAT_B_BITEXACT,
  FL_IN_FLOAT_BC_BITEXACT = FL_IN_FLOAT_B_BITEXACT | FL_IN_FLOAT_C_BITEXACT,
  FL_PROGRAMEXCEPTION = (1ull << 32),  // May generate a program exception (not floating point).
  FL_FLOAT_EXCEPTION = (1ull << 33),   // May generate a program exception (floating point).
  FL_FLOAT_DIV = (1ull << 34),    // May generate a program exception (FP) due to division by 0.
  FL_SET_ALL_CR = (1ull << 35),   // Sets every CR.
  FL_READ_CRn = (1ull << 36),     // Reads a CR determined by the CRFS field.
  FL_READ_CR_BI = (1ull << 37),   // Reads a CR determined by the BI field.
  FL_READ_ALL_CR = (1ull << 38),  // Reads every CR.
  FL_SET_CRx = FL_SET_CR0 | FL_SET_CR1 | FL_SET_CRn | FL_SET_ALL_CR,
  FL_READ_CRx = FL_READ_CRn | FL_READ_CR_BI | FL_READ_ALL_CR,
};

enum class OpType
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

struct GekkoOPStats
{
  u64 run_count;
  u32 compile_count;
  u32 last_use;
};

struct GekkoOPInfo
{
  const char* opname;
  OpType type;
  u32 num_cycles;
  u64 flags;
  // Mutable
  GekkoOPStats* stats;
};

namespace PPCTables
{
const GekkoOPInfo* GetOpInfo(UGeckoInstruction inst, u32 pc);

bool IsValidInstruction(UGeckoInstruction inst, u32 pc);

void CountInstruction(UGeckoInstruction inst, u32 pc);
void CountInstructionCompile(const GekkoOPInfo* info, u32 pc);
void PrintInstructionRunCounts();
void LogCompiledInstructions();
const char* GetInstructionName(UGeckoInstruction inst, u32 pc);
}  // namespace PPCTables
