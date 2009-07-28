// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include <algorithm>
#include <vector>

#include "Common.h"
#include "PPCTables.h"
#include "StringUtil.h"
#include "Interpreter/Interpreter.h"
#include "Interpreter/Interpreter_Tables.h"
#if !(defined(NOJIT) && NOJIT)
#include "JitCommon/Jit_Tables.h"

#if defined(_M_IX86) || defined(_M_X64)
#include "Jit64/Jit.h"
#else
#error Unknown architecture!
#endif
#endif

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
void InitTables()
{
	// Interpreter ALWAYS needs to be initialized
	InterpreterTables::InitTables();
	#if !(defined(NOJIT) && NOJIT)
	// Should be able to do this a better way than defines in this function
	JitTables::InitTables();
	#endif
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
	static int time = 0;
	FILE *f = fopen(StringFromFormat(FULL_LOGS_DIR "inst_log%i.txt", time).c_str(), "w");
	for (int i = 0; i < m_numInstructions; i++)
	{
		if (m_allInstructions[i]->compileCount > 0) {
			fprintf(f, "%s\t%i\t%i\t%08x\n", m_allInstructions[i]->opname, m_allInstructions[i]->compileCount, m_allInstructions[i]->runCount, m_allInstructions[i]->lastUse);
		}
	}
	fclose(f);
	f = fopen(StringFromFormat(FULL_LOGS_DIR "inst_not%i.txt", time).c_str(), "w");
	for (int i = 0; i < m_numInstructions; i++)
	{
		if (m_allInstructions[i]->compileCount == 0) {
			fprintf(f, "%s\t%i\t%i\n", m_allInstructions[i]->opname, m_allInstructions[i]->compileCount, m_allInstructions[i]->runCount);
		}
	}
	fclose(f);
#ifdef OPLOG
	f = fopen(StringFromFormat(FULL_LOGS_DIR OP_TO_LOG "_at.txt", time).c_str(), "w");
	for (size_t i = 0; i < rsplocations.size(); i++) {
		fprintf(f, OP_TO_LOG ": %08x\n", rsplocations[i]);
	}
	fclose(f);
#endif
	time++;
}

}  // namespace
