// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/PowerPC/PPCTables.h"

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
	s8 fregsIn[4];
	bool isBranchTarget;
	bool wantsCR0;
	bool wantsCR1;
	bool wantsFPRF;
	bool wantsCA;
	bool wantsCAInFlags;
	bool outputCR0;
	bool outputCR1;
	bool outputFPRF;
	bool outputCA;
	bool canEndBlock;
	bool skip;  // followed BL-s for example
	// which registers are still needed after this instruction in this block
	u32 gprInUse;
	u32 fprInUse;
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
	int GetUseRange(int reg)
	{
		return std::max(lastRead[reg], lastWrite[reg]) -
			   std::min(firstRead[reg], firstWrite[reg]);
	}

	bool IsUsed(int reg)
	{
		return (numReads[reg] + numWrites[reg]) > 0;
	}

	inline void SetInputRegister(int reg, short opindex)
	{
		if (firstRead[reg] == -1)
			firstRead[reg] = (short)(opindex);
		lastRead[reg] = (short)(opindex);
		numReads[reg]++;
	}

	inline void SetOutputRegister(int reg, short opindex)
	{
		if (firstWrite[reg] == -1)
			firstWrite[reg] = (short)(opindex);
		lastWrite[reg] = (short)(opindex);
		numWrites[reg]++;
	}

	inline void Clear()
	{
		for (int i = 0; i < 32; ++i)
		{
			firstRead[i] = -1;
			firstWrite[i] = -1;
			numReads[i] = 0;
			numWrites[i] = 0;
		}
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

struct CodeBlock
{
	// Beginning PPC address.
	u32 m_address;

	// Number of instructions
	// Gives us the size of the block.
	u32 m_num_instructions;

	// Some basic statistics about the block.
	BlockStats *m_stats;

	// Register statistics about the block.
	BlockRegStats *m_gpa, *m_fpa;

	// Are we a broken block?
	bool m_broken;

	// Did we have a memory_exception?
	bool m_memory_exception;
};

class PPCAnalyzer
{
private:

	enum ReorderType
	{
		REORDER_CARRY,
		REORDER_CMP
	};

	void ReorderInstructionsCore(u32 instructions, CodeOp* code, bool reverse, ReorderType type);
	void ReorderInstructions(u32 instructions, CodeOp *code);
	void SetInstructionStats(CodeBlock *block, CodeOp *code, GekkoOPInfo *opinfo, u32 index);

	// Options
	u32 m_options;
public:

	enum AnalystOption
	{
		// Conditional branch continuing
		// If the JIT core supports conditional branches within the blocks
		// Block will end on unconditional branch or other ENDBLOCK flagged instruction.
		// Requires JIT support to be enabled.
		OPTION_CONDITIONAL_CONTINUE = (1 << 0),

		// If there is a unconditional branch that jumps to a leaf function then inline it.
		// Might require JIT intervention to support it correctly.
		// Requires JITBLock support for inlined code
		// XXX: NOT COMPLETE
		OPTION_LEAF_INLINE = (1 << 1),

		// Complex blocks support jumping backwards on to themselves.
		// Happens commonly in loops, pretty complex to support.
		// May require register caches to use register usage metrics.
		// XXX: NOT COMPLETE
		OPTION_COMPLEX_BLOCK = (1 << 2),

		// Similar to complex blocks.
		// Instead of jumping backwards, this jumps forwards within the block.
		// Requires JIT support to work.
		// XXX: NOT COMPLETE
		OPTION_FORWARD_JUMP = (1 << 3),

		// Reorder compare/Rc instructions next to their associated branches and
		// merge in the JIT (for common cases, anyway).
		OPTION_BRANCH_MERGE = (1 << 4),

		// Reorder carry instructions next to their associated branches and pass
		// carry flags in the x86 flags between them, instead of in XER.
		OPTION_CARRY_MERGE = (1 << 5),
	};


	PPCAnalyzer() : m_options(0) {}

	// Option setting/getting
	void SetOption(AnalystOption option) { m_options |= option; }
	void ClearOption(AnalystOption option) { m_options &= ~(option); }
	bool HasOption(AnalystOption option) { return !!(m_options & option); }

	u32 Analyze(u32 address, CodeBlock *block, CodeBuffer *buffer, u32 blockSize);
};

void LogFunctionCall(u32 addr);
void FindFunctions(u32 startAddr, u32 endAddr, PPCSymbolDB *func_db);
bool AnalyzeFunction(u32 startAddr, Symbol &func, int max_size = 0);

}  // namespace
