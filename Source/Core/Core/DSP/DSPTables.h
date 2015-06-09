// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Additional copyrights go to Duddie (c) 2005 (duddie@walla.com)

#pragma once

#include "Core/DSP/DSPCommon.h"
#include "Core/DSP/DSPEmitter.h"

// The non-ADDR ones that end with _D are the opposite one - if the bit specify
// ACC0, then ACC_D will be ACC1.

// The values of these are very important.
// For the reg ones, the value >> 8 is the base register.
// & 0x80  means it's a "D".

enum partype_t
{
	P_NONE      = 0x0000,
	P_VAL       = 0x0001,
	P_IMM       = 0x0002,
	P_MEM       = 0x0003,
	P_STR       = 0x0004,
	P_ADDR_I    = 0x0005,
	P_ADDR_D    = 0x0006,
	P_REG       = 0x8000,
	P_REG04     = P_REG | 0x0400, // IX
	P_REG08     = P_REG | 0x0800,
	P_REG18     = P_REG | 0x1800,
	P_REGM18    = P_REG | 0x1810, // used in multiply instructions
	P_REG19     = P_REG | 0x1900,
	P_REGM19    = P_REG | 0x1910, // used in multiply instructions
	P_REG1A     = P_REG | 0x1a80,
	P_REG1C     = P_REG | 0x1c00,
	// P_ACC       = P_REG | 0x1c10, // used for global accum (gcdsptool's value)
	P_ACCL      = P_REG | 0x1c00, // used for low part of accum
	P_ACCM      = P_REG | 0x1e00, // used for mid part of accum
	// The following are not in gcdsptool
	P_ACCM_D    = P_REG | 0x1e80,
	P_ACC       = P_REG | 0x2000, // used for full accum.
	P_ACC_D     = P_REG | 0x2080,
	P_AX        = P_REG | 0x2200,
	P_REGS_MASK = 0x03f80, // gcdsptool's value = 0x01f80
	P_REF       = P_REG | 0x4000,
	P_PRG       = P_REF | P_REG,

	// The following seem like junk:
	// P_REG10     = P_REG | 0x1000,
	// P_AX_D      = P_REG | 0x2280,
};

#define OPTABLE_SIZE 0xffff + 1
#define EXT_OPTABLE_SIZE 0xff + 1

void nop(const UDSPInstruction opc);

typedef void (*dspIntFunc)(const UDSPInstruction);
typedef void (DSPEmitter::*dspJitFunc)(const UDSPInstruction);

struct param2_t
{
	partype_t type;
	u8 size;
	u8 loc;
	s8 lshift;
	u16 mask;
};

struct DSPOPCTemplate
{
	const char *name;
	u16 opcode;
	u16 opcode_mask;

	dspIntFunc intFunc;
	dspJitFunc jitFunc;

	u8 size;
	u8 param_count;
	param2_t params[8];
	bool extended;
	bool branch;
	bool uncond_branch;
	bool reads_pc;
	bool updates_sr;
};

typedef DSPOPCTemplate opc_t;

// Opcodes
extern const DSPOPCTemplate opcodes[];
extern const int opcodes_size;
extern const DSPOPCTemplate opcodes_ext[];
extern const int opcodes_ext_size;
extern const DSPOPCTemplate cw;

#define WRITEBACKLOGSIZE 5

extern const DSPOPCTemplate *opTable[OPTABLE_SIZE];
extern const DSPOPCTemplate *extOpTable[EXT_OPTABLE_SIZE];
extern u16 writeBackLog[WRITEBACKLOGSIZE];
extern int writeBackLogIdx[WRITEBACKLOGSIZE];

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
void applyWriteBackLog();
void zeroWriteBackLog();
void zeroWriteBackLogPreserveAcc(u8 acc);

const DSPOPCTemplate *GetOpTemplate(const UDSPInstruction &inst);

inline void ExecuteInstruction(const UDSPInstruction inst)
{
	const DSPOPCTemplate *tinst = GetOpTemplate(inst);

	if (tinst->extended)
	{
		if ((inst >> 12) == 0x3)
			extOpTable[inst & 0x7F]->intFunc(inst);
		else
			extOpTable[inst & 0xFF]->intFunc(inst);
	}

	tinst->intFunc(inst);

	if (tinst->extended)
	{
		applyWriteBackLog();
	}
}
