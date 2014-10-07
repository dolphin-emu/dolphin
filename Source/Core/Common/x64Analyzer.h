// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

struct InstructionInfo
{
	int operandSize; //8, 16, 32, 64
	int instructionSize;
	int regOperandReg;
	int otherReg;
	int scaledReg;
	bool zeroExtend;
	bool signExtend;
	bool hasImmediate;
	bool isMemoryWrite;
	bool byteSwap;
	u64 immediate;
	s32 displacement;

	bool operator==(const InstructionInfo &other) const;
};

struct ModRM
{
	int mod, reg, rm;
	ModRM(u8 modRM, u8 rex)
	{
		mod = modRM >> 6;
		reg = ((modRM >> 3) & 7) | ((rex & 4)?8:0);
		rm = modRM & 7;
	}
};

enum AccessType
{
	OP_ACCESS_READ = 0,
	OP_ACCESS_WRITE = 1
};

bool DisassembleMov(const unsigned char *codePtr, InstructionInfo *info);
