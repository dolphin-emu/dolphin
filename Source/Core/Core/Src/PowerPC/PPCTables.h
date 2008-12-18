// Copyright (C) 2003-2008 Dolphin Project.

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

#ifndef _PPCTABLES_H
#define _PPCTABLES_H

#include "Gekko.h"
#include "Interpreter/Interpreter.h"

enum
{
	FL_SET_CR0 = (1<<0), //  
	FL_SET_CR1 = (1<<1), //
	FL_SET_CRn = (1<<2), //
	FL_SET_CRx = FL_SET_CR0 | FL_SET_CR1 | FL_SET_CRn, //
	FL_SET_CA  = (1<<3), //	carry
	FL_READ_CA = (1<<4), //	carry
	FL_RC_BIT =  (1<<5),
	FL_RC_BIT_F = (1<<6),
	FL_ENDBLOCK = (1<<7),
	FL_IN_A = (1<<8),
	FL_IN_A0 = (1<<9),
	FL_IN_B = (1<<10),
	FL_IN_C = (1<<11),
	FL_IN_S = (1<<12),
	FL_IN_AB = FL_IN_A | FL_IN_B,
	FL_IN_SB = FL_IN_S | FL_IN_B,
	FL_IN_A0B = FL_IN_A0 | FL_IN_B,
	FL_IN_A0BC = FL_IN_A0 | FL_IN_B | FL_IN_C,
	FL_OUT_D = (1<<13),
	FL_OUT_S = FL_OUT_D,
	FL_OUT_A = (1<<14),
	FL_OUT_AD = FL_OUT_A | FL_OUT_D,
	FL_TIMER =    (1<<15),
	FL_CHECKEXCEPTIONS = (1<<16),
	FL_EVIL = (1<<17),
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
	OPTYPE_FPU     ,
	OPTYPE_PS      ,
	OPTYPE_DCACHE  ,
	OPTYPE_ICACHE  ,
	OPTYPE_BRANCH  ,
	OPTYPE_UNKNOWN ,
};

enum {
	OPCD_HLEFUNCTION = 1,
	OPCD_COMPILEDBLOCK = 2,
	OPCD_BCx = 16,
	OPCD_SC = 17,
	OPCD_Bx = 18,
};

enum {
	OP_BLR = 0x4e800020,
};

struct GekkoOPInfo
{
	const char *opname;
	int type;
	int flags;
	int numCyclesMinusOne;
	int runCount;
	int compileCount;
	u32 lastUse;
};

GekkoOPInfo *GetOpInfo(UGeckoInstruction _inst);
Interpreter::_interpreterInstruction GetInterpreterOp(UGeckoInstruction _inst);

class Jit64;

namespace PPCTables
{



typedef void (Jit64::*_recompilerInstruction) (UGeckoInstruction instCode);
typedef void (*_interpreterInstruction)(UGeckoInstruction instCode);

void InitTables();
bool IsValidInstruction(UGeckoInstruction _instCode);
bool UsesFPU(UGeckoInstruction _inst);

void CountInstruction(UGeckoInstruction _inst);
void PrintInstructionRunCounts();
void LogCompiledInstructions();
void CompileInstruction(UGeckoInstruction _inst);

}  // namespace

#endif
