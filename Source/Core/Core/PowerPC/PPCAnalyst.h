// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Core/PowerPC/PPCTables.h"

class PPCSymbolDB;
struct Symbol;

namespace PPCAnalyst
{

struct CodeOp //16B
{
	UGeckoInstruction inst;
	GekkoOPInfo* opinfo;
	u32 address;

	// -----------------------------
	// Properties of the instruction
	// -----------------------------

	// Which registers are possible inputs/outputs. If an output register might not be modified
	// (e.g. in the case of a load that can exception) or is partially modified, it's listed in
	// the inputs, too.
	BitSet32 regsOut;
	BitSet32 regsIn;
	BitSet32 fregsIn;
	BitSet32 fregsOut;

	// Whether the instruction writes to certain flags.
	bool outputCR0;
	bool outputCR1;
	bool outputFPRF;
	bool outputCA;

	// -----------------------------
	// Results of block analysis
	// -----------------------------

	// Whether this instruction needs to maintain particular flags: false if the flag will be
	// clobbered before being used or block exit.
	bool wantsFPRF; // floating point result flags
	bool wantsCA;   // carry flag
	// This instruction wants the carry flag in the host carry flag to use it as input, e.g. for
	// instructions such as addex.
	// TODO: add a similar "keep the carry flag in the host carry flag" feature for ARM? This is currently
	// only used for x86.
	bool wantsCAInFlags;
	// Whether this instruction is the first FPU instruction in the block (for FP exception bailouts).
	// This includes the case where a forward branch skipped a previous first FPU instruction.
	bool firstFPUInst;
	// Whether this instruction can end a block, either by branch or possible exception path.
	bool canEndBlock;
	// Whether this instruction is the target of a forward branch from within the block.
	bool isBranchTarget;
	// Whether this instruction should be skipped (not currently used).
	bool skip;

	// Which registers are still used after this instruction in this block.
	// If a register isn't used again we store it in order to reclaim the host register it was stored in.
	BitSet32 fprInUse;
	BitSet32 gprInUse;

	// Which registers have values that we can't discard after this instruction, i.e. won't be clobbered
	// before being used.
	// We use these to discard values that will be clobbered before a block exit point. This is an "unsafe"
	// optimization: a mistake here will result in incorrect code.
	BitSet32 gprNeeded;
	BitSet32 fprNeeded;

	// Which registers we would prefer to be loaded into host registers if possible.
	// We use these to choose whether or not to load values into host registers: if we have the option
	// of using a memory operand now, but the value will be used after this instruction, try to put it
	// in a register.
	// Just because a register is in use doesn't mean we actually need or want it in a host register;
	// hence these aren't identical to gprInUse/fprInUse.
	// For example, we do double stores from GPRs, so we don't want to load a PowerPC floating point register
	// into an XMM only to move it again to a GPR afterwards. If we change how double stores work, the way
	// these are decided upon should be changed too.
	// TODO: optimization heuristics for ARM, too!
	BitSet32 gprInReg;
	BitSet32 fprInReg;

	// Knowledge we have about the values in registers (based on where they came from) that allow us to
	// perform further optimizations.
	// Whether an fpr is known to be an actual single-precision value at this point in the block.
	BitSet32 fprIsSingle;
	// Whether an fpr is known to have identical top and bottom halves (e.g. due to a single instruction)
	BitSet32 fprIsDuplicated;
	// Whether an fpr is the output of a single-precision arithmetic instruction, i.e. whether we can safely
	// skip denormal/sNaN handling in single-precision stores.
	BitSet32 fprIsStoreSafe;
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
			firstRead[reg] = opindex;
		lastRead[reg] = opindex;
		numReads[reg]++;
	}

	inline void SetOutputRegister(int reg, short opindex)
	{
		if (firstWrite[reg] == -1)
			firstWrite[reg] = opindex;
		lastWrite[reg] = opindex;
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
		REORDER_CMP,
		REORDER_CROR
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
