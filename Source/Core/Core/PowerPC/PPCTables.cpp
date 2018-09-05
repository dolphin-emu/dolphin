// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/PPCTables.h"

#include <algorithm>
#include <array>
#include <bitset>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/PowerPC/PowerPC.h"

namespace PowerPC
{
const std::array<u64, 16> m_crTable = {{
    PPCCRToInternal(0x0),
    PPCCRToInternal(0x1),
    PPCCRToInternal(0x2),
    PPCCRToInternal(0x3),
    PPCCRToInternal(0x4),
    PPCCRToInternal(0x5),
    PPCCRToInternal(0x6),
    PPCCRToInternal(0x7),
    PPCCRToInternal(0x8),
    PPCCRToInternal(0x9),
    PPCCRToInternal(0xA),
    PPCCRToInternal(0xB),
    PPCCRToInternal(0xC),
    PPCCRToInternal(0xD),
    PPCCRToInternal(0xE),
    PPCCRToInternal(0xF),
}};
}  // namespace PowerPC

namespace PPCTables
{
struct OpDispatch
{
  std::bitset<64> leaf_flags;
  std::bitset<64> subtables;
  u32 op_index_start;
  u16 dispatch_start;
  u8 subop_shift;
  u8 subop_len;
};
constexpr std::array<OpDispatch, 30> dispatch_table = {{
#include "Core/PowerPC/Tables/OpID_DecodingTable.gen.cpp"
}};

OpID GetOpID(UGeckoInstruction instruction)
{
  size_t subtable = 0;
  while (true)
  {
    const auto& disp = dispatch_table[subtable];
    const u32 opcode = (instruction.hex >> disp.subop_shift) & ((1 << disp.subop_len) - 1);
    const auto shifted = disp.leaf_flags >> (63 - opcode);
    if (shifted[0])
    {
      return static_cast<OpID>(disp.op_index_start + shifted.count() - 1);
    }
    else if ((disp.subtables >> (63 - opcode))[0])
    {
      subtable = disp.dispatch_start + (disp.subtables >> (63 - opcode)).count() - 1;
    }
    else
    {
      ERROR_LOG(POWERPC, "subtable %zd, value %d not found", subtable, opcode);
      return OpID::Invalid;
    }
  }
}

bool IsValidInstruction(UGeckoInstruction inst)
{
  return GetOpID(inst) != OpID::Invalid;
}

struct GekkoOPInfo
{
  const char* opname;
  u32 flags;
  OpType type;
  u8 cycles;
};

constexpr std::array<GekkoOPInfo, static_cast<size_t>(OpID::End)> opinfo = {{
    {"Invalid instruction", 0, OpType::Invalid, 0},
#include "Core/PowerPC/Tables/OpInfo.gen.cpp"
}};

int Cycles(OpID opid)
{
  return opinfo[static_cast<int>(opid)].cycles;
}

const char* OpName(OpID opid)
{
  return opinfo[static_cast<int>(opid)].opname;
}

const char* GetInstructionName(UGeckoInstruction inst)
{
  return opinfo[static_cast<int>(GetOpID(inst))].opname;
}

OpType Type(OpID opid)
{
  return opinfo[static_cast<int>(opid)].type;
}

u32 Flags(OpID opid)
{
  return opinfo[static_cast<int>(opid)].flags;
}

}  // namespace
