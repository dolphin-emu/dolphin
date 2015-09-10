// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/Interpreter/Interpreter_Tables.h"

GekkoOPInfo *m_infoTable[64];
GekkoOPInfo *m_infoTable4[1024];
GekkoOPInfo *m_infoTable19[1024];
GekkoOPInfo *m_infoTable31[1024];
GekkoOPInfo *m_infoTable59[32];
GekkoOPInfo *m_infoTable63[1024];

GekkoOPInfo *m_allInstructions[512];
int m_numInstructions;

const u64 m_crTable[16] =
{
	PPCCRToInternal(0x0), PPCCRToInternal(0x1), PPCCRToInternal(0x2), PPCCRToInternal(0x3),
	PPCCRToInternal(0x4), PPCCRToInternal(0x5), PPCCRToInternal(0x6), PPCCRToInternal(0x7),
	PPCCRToInternal(0x8), PPCCRToInternal(0x9), PPCCRToInternal(0xA), PPCCRToInternal(0xB),
	PPCCRToInternal(0xC), PPCCRToInternal(0xD), PPCCRToInternal(0xE), PPCCRToInternal(0xF),
};

GekkoOPInfo *GetOpInfo(UGeckoInstruction _inst)
{
	GekkoOPInfo *info = m_infoTable[_inst.OPCD];
	if ((info->type & 0xFFFFFF) == OPTYPE_SUBTABLE)
	{
		int table = info->type>>24;
		switch (table)
		{
		case 4:  return m_infoTable4[_inst.SUBOP10];
		case 19: return m_infoTable19[_inst.SUBOP10];
		case 31: return m_infoTable31[_inst.SUBOP10];
		case 59: return m_infoTable59[_inst.SUBOP5];
		case 63: return m_infoTable63[_inst.SUBOP10];
		default:
			_assert_msg_(POWERPC,0,"GetOpInfo - invalid subtable op %08x @ %08x", _inst.hex, PC);
			return nullptr;
		}
	}
	else
	{
		if ((info->type & 0xFFFFFF) == OPTYPE_INVALID)
		{
			_assert_msg_(POWERPC,0,"GetOpInfo - invalid op %08x @ %08x", _inst.hex, PC);
			return nullptr;
		}
		return m_infoTable[_inst.OPCD];
	}
}

Interpreter::Instruction GetInterpreterOp(UGeckoInstruction _inst)
{
	const GekkoOPInfo *info = m_infoTable[_inst.OPCD];
	if ((info->type & 0xFFFFFF) == OPTYPE_SUBTABLE)
	{
		int table = info->type>>24;
		switch (table)
		{
		case 4:  return Interpreter::m_opTable4[_inst.SUBOP10];
		case 19: return Interpreter::m_opTable19[_inst.SUBOP10];
		case 31: return Interpreter::m_opTable31[_inst.SUBOP10];
		case 59: return Interpreter::m_opTable59[_inst.SUBOP5];
		case 63: return Interpreter::m_opTable63[_inst.SUBOP10];
		default:
			_assert_msg_(POWERPC,0,"GetInterpreterOp - invalid subtable op %08x @ %08x", _inst.hex, PC);
			return nullptr;
		}
	}
	else
	{
		if ((info->type & 0xFFFFFF) == OPTYPE_INVALID)
		{
			_assert_msg_(POWERPC,0,"GetInterpreterOp - invalid op %08x @ %08x", _inst.hex, PC);
			return nullptr;
		}
		return Interpreter::m_opTable[_inst.OPCD];
	}
}
namespace PPCTables
{

bool UsesFPU(UGeckoInstruction inst)
{
	GekkoOPInfo* const info = GetOpInfo(inst);

	return (info->flags & FL_USE_FPU) != 0;
}

void InitTables(int cpu_core)
{
	// Interpreter ALWAYS needs to be initialized
	InterpreterTables::InitTables();

	if (cpu_core != PowerPC::CORE_INTERPRETER)
		JitInterface::InitTables(cpu_core);
}

#define OPLOG
#define OP_TO_LOG "mtfsb0x"

#ifdef OPLOG
namespace {
	std::vector<u32> rsplocations;
}
#endif

const char *GetInstructionName(UGeckoInstruction _inst)
{
	const GekkoOPInfo *info = GetOpInfo(_inst);
	return info ? info->opname : nullptr;
}

bool IsValidInstruction(UGeckoInstruction _inst)
{
	const GekkoOPInfo *info = GetOpInfo(_inst);
	return info != nullptr;
}

void CountInstruction(UGeckoInstruction _inst)
{
	GekkoOPInfo *info = GetOpInfo(_inst);
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
	for (int i = 0; i < m_numInstructions; ++i)
	{
		GekkoOPInfo *pInst = m_allInstructions[i];
		temp.emplace_back(pInst->opname, pInst->runCount);
	}
	std::sort(temp.begin(), temp.end(),
		[](const OpInfo &a, const OpInfo &b)
		{
			return a.second > b.second;
		});

	for (auto &inst : temp)
	{
		if (inst.second == 0)
			break;

		DEBUG_LOG(POWERPC, "%s : %llu", inst.first, inst.second);
	}
}

void LogCompiledInstructions()
{
	static unsigned int time = 0;

	File::IOFile f(StringFromFormat("%sinst_log%i.txt", File::GetUserPath(D_LOGS_IDX).c_str(), time), "w");
	for (int i = 0; i < m_numInstructions; i++)
	{
		GekkoOPInfo *pInst = m_allInstructions[i];
		if (pInst->compileCount > 0)
		{
			fprintf(f.GetHandle(), "%s\t%i\t%lld\t%08x\n", pInst->opname,
				pInst->compileCount, pInst->runCount, pInst->lastUse);
		}
	}

	f.Open(StringFromFormat("%sinst_not%i.txt", File::GetUserPath(D_LOGS_IDX).c_str(), time), "w");
	for (int i = 0; i < m_numInstructions; i++)
	{
		GekkoOPInfo *pInst = m_allInstructions[i];
		if (pInst->compileCount == 0)
		{
			fprintf(f.GetHandle(), "%s\t%i\t%lld\n", pInst->opname,
				pInst->compileCount, pInst->runCount);
		}
	}

#ifdef OPLOG
	f.Open(StringFromFormat("%s" OP_TO_LOG "_at%i.txt", File::GetUserPath(D_LOGS_IDX).c_str(), time), "w");
	for (auto& rsplocation : rsplocations)
	{
		fprintf(f.GetHandle(), OP_TO_LOG ": %08x\n", rsplocation);
	}
#endif

	++time;
}

}  // namespace
