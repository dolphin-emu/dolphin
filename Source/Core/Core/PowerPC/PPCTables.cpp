// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/PPCTables.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <vector>

#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/PowerPC.h"

std::array<GekkoOPInfo*, 64> m_infoTable;
std::array<GekkoOPInfo*, 1024> m_infoTable4;
std::array<GekkoOPInfo*, 1024> m_infoTable19;
std::array<GekkoOPInfo*, 1024> m_infoTable31;
std::array<GekkoOPInfo*, 32> m_infoTable59;
std::array<GekkoOPInfo*, 1024> m_infoTable63;

std::array<GekkoOPInfo*, 512> m_allInstructions;
size_t m_numInstructions;

namespace PPCTables
{
GekkoOPInfo* GetOpInfo(UGeckoInstruction inst)
{
  const GekkoOPInfo* info = m_infoTable[inst.OPCD];
  if (info->type == OpType::Subtable)
  {
    switch (inst.OPCD)
    {
    case 4:
      return m_infoTable4[inst.SUBOP10];
    case 19:
      return m_infoTable19[inst.SUBOP10];
    case 31:
      return m_infoTable31[inst.SUBOP10];
    case 59:
      return m_infoTable59[inst.SUBOP5];
    case 63:
      return m_infoTable63[inst.SUBOP10];
    default:
      ASSERT_MSG(POWERPC, 0, "GetOpInfo - invalid subtable op {:08x} @ {:08x}", inst.hex, PC);
      return nullptr;
    }
  }
  else
  {
    if (info->type == OpType::Invalid)
    {
      ASSERT_MSG(POWERPC, 0, "GetOpInfo - invalid op {:08x} @ {:08x}", inst.hex, PC);
      return nullptr;
    }
    return m_infoTable[inst.OPCD];
  }
}

Interpreter::Instruction GetInterpreterOp(UGeckoInstruction inst)
{
  const GekkoOPInfo* info = m_infoTable[inst.OPCD];
  if (info->type == OpType::Subtable)
  {
    switch (inst.OPCD)
    {
    case 4:
      return Interpreter::m_op_table4[inst.SUBOP10];
    case 19:
      return Interpreter::m_op_table19[inst.SUBOP10];
    case 31:
      return Interpreter::m_op_table31[inst.SUBOP10];
    case 59:
      return Interpreter::m_op_table59[inst.SUBOP5];
    case 63:
      return Interpreter::m_op_table63[inst.SUBOP10];
    default:
      ASSERT_MSG(POWERPC, 0, "GetInterpreterOp - invalid subtable op {:08x} @ {:08x}", inst.hex,
                 PC);
      return nullptr;
    }
  }
  else
  {
    if (info->type == OpType::Invalid)
    {
      ASSERT_MSG(POWERPC, 0, "GetInterpreterOp - invalid op {:08x} @ {:08x}", inst.hex, PC);
      return nullptr;
    }
    return Interpreter::m_op_table[inst.OPCD];
  }
}

bool UsesFPU(UGeckoInstruction inst)
{
  GekkoOPInfo* const info = GetOpInfo(inst);

  return (info->flags & FL_USE_FPU) != 0;
}

#define OPLOG
#define OP_TO_LOG "mtfsb0x"

#ifdef OPLOG
namespace
{
std::vector<u32> rsplocations;
}
#endif

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

void CountInstruction(UGeckoInstruction inst)
{
  GekkoOPInfo* info = GetOpInfo(inst);
  if (info)
  {
    info->runCount++;
  }
}

void PrintInstructionRunCounts()
{
  typedef std::pair<const char*, u64> OpInfo;
  std::vector<OpInfo> temp;
  temp.reserve(m_numInstructions);
  for (size_t i = 0; i < m_numInstructions; ++i)
  {
    GekkoOPInfo* pInst = m_allInstructions[i];
    temp.emplace_back(pInst->opname, pInst->runCount);
  }
  std::sort(temp.begin(), temp.end(),
            [](const OpInfo& a, const OpInfo& b) { return a.second > b.second; });

  for (auto& inst : temp)
  {
    if (inst.second == 0)
      break;

    DEBUG_LOG_FMT(POWERPC, "{} : {}", inst.first, inst.second);
  }
}

void LogCompiledInstructions()
{
  static unsigned int time = 0;

  File::IOFile f(fmt::format("{}inst_log{}.txt", File::GetUserPath(D_LOGS_IDX), time), "w");
  for (size_t i = 0; i < m_numInstructions; i++)
  {
    GekkoOPInfo* pInst = m_allInstructions[i];
    if (pInst->compileCount > 0)
    {
      f.WriteString(fmt::format("{0}\t{1}\t{2}\t{3:08x}\n", pInst->opname, pInst->compileCount,
                                pInst->runCount, pInst->lastUse));
    }
  }

  f.Open(fmt::format("{}inst_not{}.txt", File::GetUserPath(D_LOGS_IDX), time), "w");
  for (size_t i = 0; i < m_numInstructions; i++)
  {
    GekkoOPInfo* pInst = m_allInstructions[i];
    if (pInst->compileCount == 0)
    {
      f.WriteString(
          fmt::format("{0}\t{1}\t{2}\n", pInst->opname, pInst->compileCount, pInst->runCount));
    }
  }

#ifdef OPLOG
  f.Open(fmt::format("{}" OP_TO_LOG "_at{}.txt", File::GetUserPath(D_LOGS_IDX), time), "w");
  for (auto& rsplocation : rsplocations)
  {
    f.WriteString(fmt::format(OP_TO_LOG ": {0:08x}\n", rsplocation));
  }
#endif

  ++time;
}

}  // namespace PPCTables
