// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _PPCANALYST_H
#define _PPCANALYST_H

#include <vector>
#include <map>

#include <string>

#include "Common.h"
#include "Gekko.h"
#include "PPCTables.h"

class PPCSymbolDB;
struct Symbol;

namespace PPCAnalyst
{

struct CodeOp //16B
{
	UGeckoInstruction inst;
	GekkoOPInfo * opinfo;
	u32 address;
	u32 branchTo; //if 0, not a branch
	int branchToIndex; //index of target block
	s8 regsOut[2];
	s8 regsIn[3];
	s8 fregOut;
	s8 fregsIn[3];
	bool isBranchTarget;
	bool wantsCR0;
	bool wantsCR1;
	bool wantsPS1;
	bool outputCR0;
	bool outputCR1;
	bool outputPS1;
	bool skip;  // followed BL-s for example
};

struct BlockStats
{
	bool isFirstBlockOfFunction;
	bool isLastBlockOfFunction;
	int numCycles;
};

struct BlockRegStats
{
	short firstRead[32];
	short firstWrite[32];
	short lastRead[32];
	short lastWrite[32];
	short numReads[32];
	short numWrites[32];

	bool any;
	bool anyTimer;

	int GetTotalNumAccesses(int reg) {return numReads[reg] + numWrites[reg];}
	int GetUseRange(int reg) {
		return max(lastRead[reg], lastWrite[reg]) - 
			   min(firstRead[reg], firstWrite[reg]);}

	inline void SetInputRegister(int reg, short opindex) {
		if (firstRead[reg] == -1)
			firstRead[reg] = (short)(opindex);
		lastRead[reg] = (short)(opindex);
		numReads[reg]++;
	}

	inline void SetOutputRegister(int reg, short opindex) {
		if (firstWrite[reg] == -1)
			firstWrite[reg] = (short)(opindex);
		lastWrite[reg] = (short)(opindex);
		numWrites[reg]++;
	}
};


class CodeBuffer
{
	int size_;
public:
	CodeBuffer(int size);
	~CodeBuffer();

	int GetSize() const { return size_; }

	PPCAnalyst::CodeOp *codebuffer;


};

u32 Flatten(u32 address, int *realsize, BlockStats *st, BlockRegStats *gpa,
			BlockRegStats *fpa, bool &broken_block, CodeBuffer *buffer,
			int blockSize, u32* merged_addresses,
			int capacity_of_merged_addresses, int& size_of_merged_addresses);
void LogFunctionCall(u32 addr);
void FindFunctions(u32 startAddr, u32 endAddr, PPCSymbolDB *func_db);
bool AnalyzeFunction(u32 startAddr, Symbol &func, int max_size = 0);

}  // namespace

#endif

