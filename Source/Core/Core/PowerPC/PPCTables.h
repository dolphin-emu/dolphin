// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>

#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"

// Flags that indicate what an instruction can do.
enum
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

enum class OpType
{
  Invalid, //0
  Subtable, //1
  Integer, //2
  CR, //3, Control Register
  SPR, //4, Special-Purpose Register
  System, //5
  SystemFP, //6
  Load, //7
  Store, //8
  LoadFP, //9
  StoreFP, //10
  DoubleFP, //11
  SingleFP, //12
  LoadPS, //13
  StorePS, //14
  PS, //15
  DataCache, //16
  InstructionCache, //17
  Branch, //18
  Unknown,
};

struct GekkoOPInfo
{
  const char* opname;
  OpType type;
  int flags;
  int numCycles;
  u64 runCount;
  int compileCount;
  u32 lastUse;
};
extern std::array<GekkoOPInfo*, 64> m_infoTable;
extern std::array<GekkoOPInfo*, 1024> m_infoTable4;
extern std::array<GekkoOPInfo*, 1024> m_infoTable19;
extern std::array<GekkoOPInfo*, 1024> m_infoTable31;
extern std::array<GekkoOPInfo*, 32> m_infoTable59;
extern std::array<GekkoOPInfo*, 1024> m_infoTable63;

extern std::array<GekkoOPInfo*, 512> m_allInstructions;
extern size_t m_numInstructions;

namespace PPCTables
{
GekkoOPInfo* GetOpInfo(UGeckoInstruction inst);
Interpreter::Instruction GetInterpreterOp(UGeckoInstruction inst);

bool IsValidInstruction(UGeckoInstruction inst);
bool UsesFPU(UGeckoInstruction inst);

void CountInstruction(UGeckoInstruction inst);
void PrintInstructionRunCounts();
void LogCompiledInstructions();
const char* GetInstructionName(UGeckoInstruction inst);
}  // namespace PPCTables
