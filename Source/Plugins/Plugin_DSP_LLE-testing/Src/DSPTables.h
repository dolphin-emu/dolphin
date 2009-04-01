// Copyright (C) 2003-2009 Dolphin Project.

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

// Additional copyrights go to Duddie (c) 2005 (duddie@walla.com)

#ifndef _DSPTABLES_H
#define _DSPTABLES_H

#include "Common.h"
enum parameterType
{
	P_NONE = 0x0000,
	P_VAL = 0x0001,
	P_IMM = 0x0002,
	P_MEM = 0x0003,
	P_STR = 0x0004,
	P_REG = 0x8000,
	P_REG08 = P_REG | 0x0800,
	P_REG10 = P_REG | 0x1000,
	P_REG18 = P_REG | 0x1800,
	P_REG19 = P_REG | 0x1900,
	P_REG1A = P_REG | 0x1a00,
	P_REG1C = P_REG | 0x1c00,
	P_ACCM = P_REG | 0x1e00,
	P_ACCM_D = P_REG | 0x1e80,
	P_ACC = P_REG | 0x2000,
	P_ACC_D = P_REG | 0x2080,
	P_AX = P_REG | 0x2200,
	P_AX_D = P_REG | 0x2280,
	P_REGS_MASK = 0x03f80,
	P_REF       = P_REG | 0x4000,
	P_PRG       = P_REF | P_REG,
};

#define P_EXT   0x80

union UDSPInstruction
{
	u16 hex;

	UDSPInstruction(u16 _hex)	{ hex = _hex; }
	UDSPInstruction()	        { hex = 0; }

	struct
	{
		signed shift        : 6;
		unsigned negating   : 1;
		unsigned arithmetic : 1;
		unsigned areg       : 1;
		unsigned op         : 7;
	};
	struct
	{
		unsigned ushift     : 6;
	};

	// TODO(XK): Figure out more instruction structures (add structs here)
};

typedef void (*dspInstFunc)(UDSPInstruction&);

typedef struct DSPOParams
{
	parameterType type;
	u8 size;
	u8 loc;
	s8 lshift;
	u16 mask;
} opcpar_t;

typedef struct DSPOPCTemplate
{
	const char *name;
	u16 opcode;
	u16 opcode_mask;

	dspInstFunc interpFunc;
	dspInstFunc jitFunc;

	u8 size;
	u8 param_count;
	DSPOParams params[8];
} opc_t;

extern DSPOPCTemplate opcodes[];
extern const u32 opcodes_size;
extern opc_t opcodes_ext[];
extern const u32 opcodes_ext_size;

void InitInstructionTable();
void DestroyInstructionTable();

void ComputeInstruction(const UDSPInstruction& inst);

#endif // _DSPTABLES_H
