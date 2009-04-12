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

// The ones that end with _D are the opposite one - if the bit specify
// ACC0, then ACC_D will be ACC1.
enum partype_t
{
	P_NONE		= 0x0000,
	P_VAL		= 0x0001,
	P_IMM		= 0x0002,
	P_MEM		= 0x0003,
	P_STR		= 0x0004,
	P_REG		= 0x8000,
	P_REG08		= P_REG | 0x0800,
	P_REG18		= P_REG | 0x1800,
	P_REGM18	= P_REG | 0x1810, // used in multiply instructions
	P_REG19		= P_REG | 0x1900,
	P_REGM19	= P_REG | 0x1910, // used in multiply instructions
	P_REG1A		= P_REG | 0x1a00,
	P_REG1C		= P_REG | 0x1c00,
//	P_ACC		= P_REG | 0x1c10, // used for global accum (gcdsptool's value)
	P_ACC_D		= P_REG | 0x1c80,
	P_ACCL		= P_REG | 0x1c00, // used for mid accum
	P_ACCM		= P_REG | 0x1e00, // used for mid accum
	// The following are not in gcdsptool
	P_ACCM_D	= P_REG | 0x1e80,
	P_ACC		= P_REG | 0x2000, // used for global accum.
	P_AX		= P_REG | 0x2200,
	P_REGS_MASK	= 0x03f80, // gcdsptool's value = 0x01f80
	P_REF       = P_REG | 0x4000,
	P_PRG       = P_REF | P_REG,

	// The following seem like junk:
	//	P_REG10		= P_REG | 0x1000,
	//	P_AX_D		= P_REG | 0x2280,

};

#define P_EXT   0x80

#define OPTABLE_SIZE 65536

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

	// TODO: Figure out more instruction structures (add structs here)
};

typedef void (*dspInstFunc)(const UDSPInstruction&);

struct param2_t
{
	partype_t type;
	u8 size;
	u8 loc;
	s8 lshift;
	u16 mask;
};

typedef struct
{
	const char *name;
	u16 opcode;
	u16 opcode_mask;

	dspInstFunc interpFunc;
	dspInstFunc jitFunc;

	u8 size;
	u8 param_count;
	param2_t params[8];
	dspInstFunc prologue;
	dspInstFunc epilogue;
} DSPOPCTemplate;

typedef DSPOPCTemplate opc_t;

// Opcodes
extern const DSPOPCTemplate opcodes[];
extern const u32 opcodes_size;
extern const DSPOPCTemplate opcodes_ext[];
extern const u32 opcodes_ext_size;
extern u8 opSize[OPTABLE_SIZE];
extern const DSPOPCTemplate cw;

extern dspInstFunc opTable[];
extern dspInstFunc prologueTable[OPTABLE_SIZE];
extern dspInstFunc epilogueTable[OPTABLE_SIZE];

// Predefined labels
struct pdlabel_t
{
	u16 addr;
	const char* name;
	const char* description;
};

extern const pdlabel_t regnames[];
extern const pdlabel_t pdlabels[];
extern const u32 pdlabels_size;

const char *pdname(u16 val);
const char *pdregname(int val);
const char *pdregnamelong(int val);

void InitInstructionTable();

inline void ExecuteInstruction(const UDSPInstruction& inst)
{
	// TODO: Move the prologuetable calls into the relevant instructions themselves.
	// Better not do things like this until things work correctly though.
	if (prologueTable[inst.hex])
		prologueTable[inst.hex](inst);
	opTable[inst.hex](inst);
	if (epilogueTable[inst.hex])
		epilogueTable[inst.hex](inst);
}

// This one's pretty slow, try to use it only at init or seldomly.
// returns NULL if no matching instruction.
const DSPOPCTemplate *GetOpTemplate(const UDSPInstruction &inst);

#endif // _DSPTABLES_H
