// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <vector>

#include "Common.h"
#include "PPCTables.h"
#include "StringUtil.h"
#include "FileUtil.h"
#include "Interpreter/Interpreter.h"
#include "Interpreter/Interpreter_Tables.h"
#include "JitInterface.h"

struct op_inf
{
	const char *name;
	int count;
	bool operator < (const op_inf &o) const
	{
		return count > o.count;
	}
};
 GekkoOPInfo *m_infoTable[64];
 GekkoOPInfo *m_infoTable4[1024];
 GekkoOPInfo *m_infoTable19[1024];
 GekkoOPInfo *m_infoTable31[1024];
 GekkoOPInfo *m_infoTable59[32];
 GekkoOPInfo *m_infoTable63[1024];

 GekkoOPInfo *m_allInstructions[512];
 int m_numInstructions;

GekkoOPInfo *GetOpInfo(UGeckoInstruction _inst)
{
	GekkoOPInfo *info = m_infoTable[_inst.OPCD];
	if ((info->type & 0xFFFFFF) == OPTYPE_SUBTABLE)
	{
		int table = info->type>>24;
		switch(table) 
		{
		case 4:  return m_infoTable4[_inst.SUBOP10];
		case 19: return m_infoTable19[_inst.SUBOP10];
		case 31: return m_infoTable31[_inst.SUBOP10];
		case 59: return m_infoTable59[_inst.SUBOP5];
		case 63: return m_infoTable63[_inst.SUBOP10];
		default:
			_assert_msg_(POWERPC,0,"GetOpInfo - invalid subtable op %08x @ %08x", _inst.hex, PC);
			return 0;
		}
	}
	else
	{
		if ((info->type & 0xFFFFFF) == OPTYPE_INVALID)
		{
			_assert_msg_(POWERPC,0,"GetOpInfo - invalid op %08x @ %08x", _inst.hex, PC);
			return 0;
		}
		return m_infoTable[_inst.OPCD];
	}
}

Interpreter::_interpreterInstruction GetInterpreterOp(UGeckoInstruction _inst)
{
	const GekkoOPInfo *info = m_infoTable[_inst.OPCD];
	if ((info->type & 0xFFFFFF) == OPTYPE_SUBTABLE)
	{
		int table = info->type>>24;
		switch(table) 
		{
		case 4:  return Interpreter::m_opTable4[_inst.SUBOP10];
		case 19: return Interpreter::m_opTable19[_inst.SUBOP10];
		case 31: return Interpreter::m_opTable31[_inst.SUBOP10];
		case 59: return Interpreter::m_opTable59[_inst.SUBOP5];
		case 63: return Interpreter::m_opTable63[_inst.SUBOP10];
		default:
			_assert_msg_(POWERPC,0,"GetInterpreterOp - invalid subtable op %08x @ %08x", _inst.hex, PC);
			return 0;
		}
	}
	else
	{
		if ((info->type & 0xFFFFFF) == OPTYPE_INVALID)
		{
			_assert_msg_(POWERPC,0,"GetInterpreterOp - invalid op %08x @ %08x", _inst.hex, PC);
			return 0;
		}
		return Interpreter::m_opTable[_inst.OPCD];
	}
}
namespace PPCTables
{

bool UsesFPU(UGeckoInstruction _inst)
{
	switch (_inst.OPCD)
	{
	case 04:	// PS
		return _inst.SUBOP10 != 1014;

	case 48:	// lfs
	case 49:	// lfsu
	case 50:	// lfd
	case 51:	// lfdu
	case 52:	// stfs
	case 53:	// stfsu
	case 54:	// stfd
	case 55:	// stfdu
	case 56:	// psq_l
	case 57:	// psq_lu

	case 59:	// FPU-sgl
	case 60:	// psq_st
	case 61:	// psq_stu
	case 63:	// FPU-dbl
		return true;

	case 31:
		switch (_inst.SUBOP10)
		{
		case 535:
		case 567:
		case 599:
		case 631:
		case 663:
		case 695:
		case 727:
		case 759:
		case 983:
			return true;
		default:
			return false;
		}
	default:
		return false;
	}
}

void InitTables(int cpu_core)
{
	// Interpreter ALWAYS needs to be initialized
	InterpreterTables::InitTables();
	switch (cpu_core)
	{
	case 0:
		{
			// Interpreter
			break;
		}
	default:
		{
			JitInterface::InitTables(cpu_core);
			break;
		}
	}
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
	return info ? info->opname : 0;
}

bool IsValidInstruction(UGeckoInstruction _inst)
{
	const GekkoOPInfo *info = GetOpInfo(_inst);
	return info != 0;
}

void CountInstruction(UGeckoInstruction _inst)
{
	GekkoOPInfo *info = GetOpInfo(_inst);
	if (info)
		info->runCount++;
}

void PrintInstructionRunCounts()
{
	std::vector<op_inf> temp;
	for (int i = 0; i < m_numInstructions; i++)
	{
		op_inf x;
		x.name = m_allInstructions[i]->opname;
		x.count = m_allInstructions[i]->runCount;
		temp.push_back(x);
	}
	std::sort(temp.begin(), temp.end());
	for (int i = 0; i < m_numInstructions; i++)
	{
		if (temp[i].count == 0) 
			break;
		DEBUG_LOG(POWERPC, "%s : %i", temp[i].name,temp[i].count);
		//PanicAlert("%s : %i", temp[i].name,temp[i].count);
	}
}

void LogCompiledInstructions()
{
	static unsigned int time = 0;

	File::IOFile f(StringFromFormat("%sinst_log%i.txt", File::GetUserPath(D_LOGS_IDX).c_str(), time), "w");
	for (int i = 0; i < m_numInstructions; i++)
	{
		if (m_allInstructions[i]->compileCount > 0)
		{
			fprintf(f.GetHandle(), "%s\t%i\t%lld\t%08x\n", m_allInstructions[i]->opname,
				m_allInstructions[i]->compileCount, m_allInstructions[i]->runCount, m_allInstructions[i]->lastUse);
		}
	}

	f.Open(StringFromFormat("%sinst_not%i.txt", File::GetUserPath(D_LOGS_IDX).c_str(), time), "w");
	for (int i = 0; i < m_numInstructions; i++)
	{
		if (m_allInstructions[i]->compileCount == 0)
		{
			fprintf(f.GetHandle(), "%s\t%i\t%lld\n", m_allInstructions[i]->opname,
				m_allInstructions[i]->compileCount, m_allInstructions[i]->runCount);
		}
	}

#ifdef OPLOG
	f.Open(StringFromFormat("%s" OP_TO_LOG "_at.txt", File::GetUserPath(D_LOGS_IDX).c_str(), time), "w");
	for (size_t i = 0; i < rsplocations.size(); i++)
	{
		fprintf(f.GetHandle(), OP_TO_LOG ": %08x\n", rsplocations[i]);
	}
#endif

	++time;
}

}  // namespace
