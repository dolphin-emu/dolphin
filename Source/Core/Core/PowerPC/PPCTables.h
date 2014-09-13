// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"

enum
{
	FL_SET_CR0         = (1<<0), //
	FL_SET_CR1         = (1<<1), //
	FL_SET_CRn         = (1<<2), //
	FL_SET_CRx         = FL_SET_CR0 | FL_SET_CR1 | FL_SET_CRn, //
	FL_SET_CA          = (1<<3), // carry
	FL_READ_CA         = (1<<4), // carry
	FL_RC_BIT          = (1<<5),
	FL_RC_BIT_F        = (1<<6),
	FL_ENDBLOCK        = (1<<7),
	FL_IN_A            = (1<<8),
	FL_IN_A0           = (1<<9),
	FL_IN_B            = (1<<10),
	FL_IN_C            = (1<<11),
	FL_IN_S            = (1<<12),
	FL_IN_AB           = FL_IN_A | FL_IN_B,
	FL_IN_AC           = FL_IN_A | FL_IN_C,
	FL_IN_ABC          = FL_IN_A | FL_IN_B | FL_IN_C,
	FL_IN_SB           = FL_IN_S | FL_IN_B,
	FL_IN_A0B          = FL_IN_A0 | FL_IN_B,
	FL_IN_A0BC         = FL_IN_A0 | FL_IN_B | FL_IN_C,
	FL_OUT_D           = (1<<13),
	FL_OUT_S           = FL_OUT_D,
	FL_OUT_A           = (1<<14),
	FL_OUT_AD          = FL_OUT_A | FL_OUT_D,
	FL_TIMER           = (1<<15),
	FL_CHECKEXCEPTIONS = (1<<16),
	FL_EVIL            = (1<<17),
	FL_USE_FPU         = (1<<18),
	FL_LOADSTORE       = (1<<19),
	FL_SET_FPRF        = (1<<20),
	FL_READ_FPRF       = (1<<21),
	FL_SET_OE          = (1<<22),
	FL_IN_FLOAT_A      = (1<<23),
	FL_IN_FLOAT_B      = (1<<24),
	FL_IN_FLOAT_C      = (1<<25),
	FL_IN_FLOAT_S      = (1<<26),
	FL_IN_FLOAT_D      = (1<<27),
	FL_IN_FLOAT_AB     = FL_IN_FLOAT_A | FL_IN_FLOAT_B,
	FL_IN_FLOAT_AC     = FL_IN_FLOAT_A | FL_IN_FLOAT_C,
	FL_IN_FLOAT_ABC    = FL_IN_FLOAT_A | FL_IN_FLOAT_B | FL_IN_FLOAT_C,
	FL_OUT_FLOAT_D     = (1<<28),
	FL_OUT_FLOAT_S     = (1<<29),
	// Used in the case of double ops (they don't modify the top half of the output)
	FL_INOUT_FLOAT_D   = FL_IN_FLOAT_D | FL_OUT_FLOAT_D,
};

enum
{
	OPTYPE_INVALID ,
	OPTYPE_SUBTABLE,
	OPTYPE_INTEGER ,
	OPTYPE_CR      ,
	OPTYPE_SPR     ,
	OPTYPE_SYSTEM  ,
	OPTYPE_SYSTEMFP,
	OPTYPE_LOAD    ,
	OPTYPE_STORE   ,
	OPTYPE_LOADFP  ,
	OPTYPE_STOREFP ,
	OPTYPE_DOUBLEFP,
	OPTYPE_SINGLEFP,
	OPTYPE_PS      ,
	OPTYPE_DCACHE  ,
	OPTYPE_ICACHE  ,
	OPTYPE_BRANCH  ,
	OPTYPE_UNKNOWN ,
};

enum
{
	OPCD_HLEFUNCTION   = 1,
	OPCD_COMPILEDBLOCK = 2,
	OPCD_BCx           = 16,
	OPCD_SC            = 17,
	OPCD_Bx            = 18,
};

enum
{
	OP_BLR = 0x4e800020,
};

struct GekkoOPInfo
{
	const char *opname;
	int type;
	int flags;
	int numCycles;
	u64 runCount;
	int compileCount;
	u32 lastUse;
};
extern GekkoOPInfo *m_infoTable[64];
extern GekkoOPInfo *m_infoTable4[1024];
extern GekkoOPInfo *m_infoTable19[1024];
extern GekkoOPInfo *m_infoTable31[1024];
extern GekkoOPInfo *m_infoTable59[32];
extern GekkoOPInfo *m_infoTable63[1024];

extern GekkoOPInfo *m_allInstructions[512];

extern int m_numInstructions;

GekkoOPInfo *GetOpInfo(UGeckoInstruction _inst);
Interpreter::_interpreterInstruction GetInterpreterOp(UGeckoInstruction _inst);

class cJit64;

namespace PPCTables
{

void InitTables(int cpu_core);
bool IsValidInstruction(UGeckoInstruction _instCode);
bool UsesFPU(UGeckoInstruction _inst);

void CountInstruction(UGeckoInstruction _inst);
void PrintInstructionRunCounts();
void LogCompiledInstructions();
const char *GetInstructionName(UGeckoInstruction _inst);

}  // namespace
