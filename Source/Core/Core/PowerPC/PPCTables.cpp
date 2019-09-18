// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/PPCTables.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <vector>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"

#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/PowerPC.h"

namespace
{
constexpr std::array<GekkoOPInfo, IForm::NUM_IFORMS> info_table = {{
    {"unknown_instruction", OpType::Unknown, FL_ENDBLOCK, 0},
#define ILLEGAL(name)
#define INST(name, type, flags, cycles) {#name, OpType::type, flags, cycles},
#define TABLE(name, bits, shift, start, end)
#include "Core/PowerPC/Instructions.in.cpp"
#undef ILLEGAL
#undef INST
#undef TABLE
}};

namespace DispatchCode
{
enum
{
#define ILLEGAL(name) Illegal_##name,
#define INST(name, type, flags, cycles) Inst_##name,
#define TABLE(name, bits, shift, start, end) Table_##name,
#include "Core/PowerPC/Instructions.in.cpp"
#undef ILLEGAL
#undef INST
#undef TABLE
  NUM_CODES
};
enum
{
#define ILLEGAL(name)
#define INST(name, type, flags, cycles)
#define TABLE(name, bits, shift, start, end) TableIndex_##name,
#include "Core/PowerPC/Instructions.in.cpp"
#undef ILLEGAL
#undef INST
#undef TABLE
  NUM_TABLES
};
#define ILLEGAL(name)
#define INST(name, type, flags, cycles)
#define TABLE(name, bits, shift, start, end)                                                       \
  static_assert((1 << bits) - 1 == end - start, "Table " #name " has incorrect size");
#include "Core/PowerPC/Instructions.in.cpp"
#undef ILLEGAL
#undef INST
#undef TABLE
}  // namespace DispatchCode

static_assert(IForm::NUM_IFORMS + DispatchCode::NUM_TABLES < 256,
              "too many instruction forms / dispatch tables");
constexpr std::array<u8, DispatchCode::NUM_CODES> dispatch_table = {{
#define ILLEGAL(name) 0,
#define INST(name, type, flags, cycles) IForm::name,
#define TABLE(name, bits, shift, start, end) IForm::NUM_IFORMS + DispatchCode::TableIndex_##name,
#include "Core/PowerPC/Instructions.in.cpp"
#undef ILLEGAL
#undef INST
#undef TABLE
}};

struct Table
{
  u16 start;
  u8 bits;
  u8 shift;
};
constexpr std::array<Table, DispatchCode::NUM_TABLES> tables_table = {{
#define ILLEGAL(name)
#define INST(name, type, flags, cycles)
#define TABLE(name, bits, shift, start, end) {DispatchCode::start, bits, shift},
#include "Core/PowerPC/Instructions.in.cpp"
#undef ILLEGAL
#undef INST
#undef TABLE
}};

}  // namespace

namespace PPCTables
{
IForm::IForm DecodeIForm(UGeckoInstruction inst)
{
  u32 field_value = inst.hex >> 26;
  size_t table_start = 0;
  while (1)
  {
    IForm::IForm dispatch_value = dispatch_table[table_start + field_value];
    if (dispatch_value < IForm::NUM_IFORMS)
    {
      return dispatch_value;
    }
    const Table& table_info = tables_table[dispatch_value - IForm::NUM_IFORMS];
    table_start = table_info.start;
    field_value = ((1 << table_info.bits) - 1) & inst.hex >> table_info.shift;
  }
}

const GekkoOPInfo& GetIFormInfo(IForm::IForm iform)
{
  return info_table[iform];
}

const GekkoOPInfo* GetOpInfo(UGeckoInstruction inst)
{
  IForm::IForm iform = DecodeIForm(inst);
  return iform != 0 ? &info_table[iform] : nullptr;
}

Interpreter::Instruction GetInterpreterOp(UGeckoInstruction inst)
{
  return Interpreter::m_op_table[DecodeIForm(inst)];
}

bool UsesFPU(UGeckoInstruction inst)
{
  return (info_table[DecodeIForm(inst.hex)].flags & FL_USE_FPU) != 0;
}

const char* GetInstructionName(UGeckoInstruction inst)
{
  const GekkoOPInfo* info = GetOpInfo(inst);
  return info ? info->opname : nullptr;
}

bool IsValidInstruction(UGeckoInstruction inst)
{
  const GekkoOPInfo* info = GetOpInfo(inst);
  return info != nullptr && info->type != OpType::Unknown;
}

}  // namespace PPCTables
