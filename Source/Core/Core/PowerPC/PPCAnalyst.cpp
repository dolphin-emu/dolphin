// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <queue>
#include <string>

#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/GeckoCode.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/SignatureDB.h"
#include "Core/PowerPC/JitCommon/JitCache.h"

// Analyzes PowerPC code in memory to find functions
// After running, for each function we will know what functions it calls
// and what functions calls it. That is, we will have an incomplete call graph,
// but only missing indirect branches.

// The results of this analysis is displayed in the code browsing sections at the bottom left
// of the disassembly window (debugger).

// It is also useful for finding function boundaries so that we can find, fingerprint and detect library functions.
// We don't do this much currently. Only for the special case Super Monkey Ball.

namespace PPCAnalyst {

using namespace std;

static const int CODEBUFFER_SIZE = 32000;
// 0 does not perform block merging
static const int FUNCTION_FOLLOWING_THRESHOLD = 16;

CodeBuffer::CodeBuffer(int size)
{
	codebuffer = new PPCAnalyst::CodeOp[size];
	size_ = size;
}

CodeBuffer::~CodeBuffer()
{
	delete[] codebuffer;
}

void AnalyzeFunction2(Symbol &func);
u32 EvaluateBranchTarget(UGeckoInstruction instr, u32 pc);

#define INVALID_TARGET ((u32)-1)

u32 EvaluateBranchTarget(UGeckoInstruction instr, u32 pc)
{
	switch (instr.OPCD)
	{
	case 18://branch instruction
		{
			u32 target = SignExt26(instr.LI<<2);
			if (!instr.AA)
				target += pc;

			return target;
		}
	default:
		return INVALID_TARGET;
	}
}

//To find the size of each found function, scan
//forward until we hit blr. In the meantime, collect information
//about which functions this function calls.
//Also collect which internal branch goes the farthest
//If any one goes farther than the blr, assume that there is more than
//one blr, and keep scanning.

bool AnalyzeFunction(u32 startAddr, Symbol &func, int max_size)
{
	if (!func.name.size())
		func.name = StringFromFormat("zz_%07x_", startAddr & 0x0FFFFFF);
	if (func.analyzed >= 1)
		return true;  // No error, just already did it.

	func.calls.clear();
	func.callers.clear();
	func.size = 0;
	func.flags = FFLAG_LEAF;
	u32 addr = startAddr;

	u32 farthestInternalBranchTarget = startAddr;
	int numInternalBranches = 0;
	while (true)
	{
		func.size += 4;
		if (func.size >= CODEBUFFER_SIZE * 4) //weird
			return false;

		UGeckoInstruction instr = (UGeckoInstruction)Memory::ReadUnchecked_U32(addr);
		if (max_size && func.size > max_size)
		{
			func.address = startAddr;
			func.analyzed = 1;
			func.hash = SignatureDB::ComputeCodeChecksum(startAddr, addr);
			if (numInternalBranches == 0)
				func.flags |= FFLAG_STRAIGHT;
			return true;
		}
		if (PPCTables::IsValidInstruction(instr))
		{
			if (instr.hex == 0x4e800020) //4e800021 is blrl, not the end of a function
			{
				//BLR
				if (farthestInternalBranchTarget > addr)
				{
					//bah, not this one, continue..
				}
				else
				{
					//a final blr!
					//We're done! Looks like we have a neat valid function. Perfect.
					//Let's calc the checksum and get outta here
					func.address = startAddr;
					func.analyzed = 1;
					func.hash = SignatureDB::ComputeCodeChecksum(startAddr, addr);
					if (numInternalBranches == 0)
						func.flags |= FFLAG_STRAIGHT;
					return true;
				}
			}
			/*
			else if ((instr.hex & 0xFC000000) == (0x4b000000 & 0xFC000000) && !instr.LK) {
				u32 target = addr + SignExt26(instr.LI << 2);
				if (target < startAddr || (max_size && target > max_size+startAddr))
				{
					//block ends by branching away. We're done!
					func.size *= 4; // into bytes
					func.address = startAddr;
					func.analyzed = 1;
					func.hash = SignatureDB::ComputeCodeChecksum(startAddr, addr);
					if (numInternalBranches == 0)
						func.flags |= FFLAG_STRAIGHT;
					return true;
				}
			}*/
			else if (instr.hex == 0x4e800021 || instr.hex == 0x4e800420 || instr.hex == 0x4e800421)
			{
				func.flags &= ~FFLAG_LEAF;
				func.flags |= FFLAG_EVIL;
			}
			else if (instr.hex == 0x4c000064)
			{
				func.flags &= ~FFLAG_LEAF;
				func.flags |= FFLAG_RFI;
			}
			else
			{
				if (instr.OPCD == 16)
				{
					u32 target = SignExt16(instr.BD << 2);

					if (!instr.AA)
						target += addr;

					if (target > farthestInternalBranchTarget && !instr.LK)
					{
						farthestInternalBranchTarget = target;
					}
					numInternalBranches++;
				}
				else
				{
					u32 target = EvaluateBranchTarget(instr, addr);
					if (target != INVALID_TARGET)
					{
						if (instr.LK)
						{
							//we found a branch-n-link!
							func.calls.push_back(SCall(target, addr));
							func.flags &= ~FFLAG_LEAF;
						}
						else
						{
							if (target > farthestInternalBranchTarget)
							{
								farthestInternalBranchTarget = target;
							}
							numInternalBranches++;
						}
					}
				}
			}
		}
		else
		{
			return false;
		}
		addr += 4;
	}
}


// Second pass analysis, done after the first pass is done for all functions
// so we have more information to work with
void AnalyzeFunction2(Symbol *func)
{
	u32 flags = func->flags;

	bool nonleafcall = false;
	for (const SCall& c : func->calls)
	{
		Symbol *called_func = g_symbolDB.GetSymbolFromAddr(c.function);
		if (called_func && (called_func->flags & FFLAG_LEAF) == 0)
		{
			nonleafcall = true;
			break;
		}
	}

	if (nonleafcall && !(flags & FFLAG_EVIL) && !(flags & FFLAG_RFI))
		flags |= FFLAG_ONLYCALLSNICELEAFS;

	func->flags = flags;
}

// IMPORTANT - CURRENTLY ASSUMES THAT A IS A COMPARE
bool CanSwapAdjacentOps(const CodeOp &a, const CodeOp &b)
{
	const GekkoOPInfo *b_info = b.opinfo;
	int b_flags = b_info->flags;
	if (b_flags & (FL_SET_CRx | FL_ENDBLOCK | FL_TIMER | FL_EVIL))
		return false;
	if ((b_flags & (FL_RC_BIT | FL_RC_BIT_F)) && (b.inst.hex & 1))
		return false;

	switch (b.inst.OPCD)
	{
	case 16:
	case 18:
		//branches. Do not swap.
	case 17: //sc
	case 46: //lmw
	case 19: //table19 - lots of tricky stuff
		return false;
	}

	// For now, only integer ops acceptable. Any instruction which can raise an
	// interrupt is *not* a possible swap candidate: see [1] for an example of
	// a crash caused by this error.
	//
	// [1] https://code.google.com/p/dolphin-emu/issues/detail?id=5864#c7
	if (b_info->type != OPTYPE_INTEGER)
		return false;

	// Check that we have no register collisions.
	// That is, check that none of b's outputs matches any of a's inputs,
	// and that none of a's outputs matches any of b's inputs.
	// The latter does not apply if a is a cmp, of course, but doesn't hurt to check.
	for (int j = 0; j < 3; j++)
	{
		int regInA = a.regsIn[j];
		int regInB = b.regsIn[j];
		if (regInA >= 0 &&
			(b.regsOut[0] == regInA ||
			 b.regsOut[1] == regInA))
		{
			// reg collision! don't swap
			return false;
		}
		if (regInB >= 0 &&
			(a.regsOut[0] == regInB ||
			 a.regsOut[1] == regInB))
		{
			// reg collision! don't swap
			return false;
		}
	}

	return true;
}

// Most functions that are relevant to analyze should be
// called by another function. Therefore, let's scan the
// entire space for bl operations and find what functions
// get called.
void FindFunctionsFromBranches(u32 startAddr, u32 endAddr, SymbolDB *func_db)
{
	for (u32 addr = startAddr; addr < endAddr; addr+=4)
	{
		UGeckoInstruction instr = (UGeckoInstruction)Memory::ReadUnchecked_U32(addr);

		if (PPCTables::IsValidInstruction(instr))
		{
			switch (instr.OPCD)
			{
			case 18://branch instruction
				{
					if (instr.LK) //bl
					{
						u32 target = SignExt26(instr.LI << 2);
						if (!instr.AA)
							target += addr;
						if (Memory::IsRAMAddress(target))
						{
							func_db->AddFunction(target);
						}
					}
				}
				break;
			default:
				break;
			}
		}
	}
}

void FindFunctionsAfterBLR(PPCSymbolDB *func_db)
{
	vector<u32> funcAddrs;

	for (const auto& func : func_db->Symbols())
		funcAddrs.push_back(func.second.address + func.second.size);

	for (u32& location : funcAddrs)
	{
		while (true)
		{
			if (PPCTables::IsValidInstruction(Memory::Read_Instruction(location)))
			{
				//check if this function is already mapped
				Symbol *f = func_db->AddFunction(location);
				if (!f)
					break;
				else
					location += f->size;
			}
			else
				break;
		}
	}
}

void FindFunctions(u32 startAddr, u32 endAddr, PPCSymbolDB *func_db)
{
	//Step 1: Find all functions
	FindFunctionsFromBranches(startAddr, endAddr, func_db);
	FindFunctionsAfterBLR(func_db);

	//Step 2:
	func_db->FillInCallers();

	int numLeafs = 0, numNice = 0, numUnNice = 0;
	int numTimer = 0, numRFI = 0, numStraightLeaf = 0;
	int leafSize = 0, niceSize = 0, unniceSize = 0;
	for (auto& func : func_db->AccessSymbols())
	{
		if (func.second.address == 4)
		{
			WARN_LOG(OSHLE, "Weird function");
			continue;
		}
		AnalyzeFunction2(&(func.second));
		Symbol &f = func.second;
		if (f.name.substr(0, 3) == "zzz")
		{
			if (f.flags & FFLAG_LEAF)
				f.name += "_leaf";
			if (f.flags & FFLAG_STRAIGHT)
				f.name += "_straight";
		}
		if (f.flags & FFLAG_LEAF)
		{
			numLeafs++;
			leafSize += f.size;
		}
		else if (f.flags & FFLAG_ONLYCALLSNICELEAFS)
		{
			numNice++;
			niceSize += f.size;
		}
		else
		{
			numUnNice++;
			unniceSize += f.size;
		}

		if (f.flags & FFLAG_TIMERINSTRUCTIONS)
			numTimer++;
		if (f.flags & FFLAG_RFI)
			numRFI++;
		if ((f.flags & FFLAG_STRAIGHT) && (f.flags & FFLAG_LEAF))
			numStraightLeaf++;
	}
	if (numLeafs == 0)
		leafSize = 0;
	else
		leafSize /= numLeafs;

	if (numNice == 0)
		niceSize = 0;
	else
		niceSize /= numNice;

	if (numUnNice == 0)
		unniceSize = 0;
	else
		unniceSize /= numUnNice;

	INFO_LOG(OSHLE, "Functions analyzed. %i leafs, %i nice, %i unnice."
		"%i timer, %i rfi. %i are branchless leafs.", numLeafs,
		numNice, numUnNice, numTimer, numRFI, numStraightLeaf);
	INFO_LOG(OSHLE, "Average size: %i (leaf), %i (nice), %i(unnice)",
		leafSize, niceSize, unniceSize);
}

void PPCAnalyzer::ReorderInstructions(u32 instructions, CodeOp *code)
{
	// Instruction Reordering Pass
	// Bubble down compares towards branches, so that they can be merged.
	// -2: -1 for the pair, -1 for not swapping with the final instruction which is probably the branch.
	for (u32 i = 0; i < (instructions - 2); ++i)
	{
		CodeOp &a = code[i];
		CodeOp &b = code[i + 1];
		// All integer compares can be reordered.
		if ((a.inst.OPCD == 10 || a.inst.OPCD == 11) ||
			(a.inst.OPCD == 31 && (a.inst.SUBOP10 == 0 || a.inst.SUBOP10 == 32)))
		{
			// Got a compare instruction.
			if (CanSwapAdjacentOps(a, b)) {
				// Alright, let's bubble it down!
				CodeOp c = a;
				a = b;
				b = c;
			}
		}
	}
}

void PPCAnalyzer::SetInstructionStats(CodeBlock *block, CodeOp *code, GekkoOPInfo *opinfo, u32 index)
{
	code->wantsCR0 = false;
	code->wantsCR1 = false;
	code->wantsPS1 = false;

	if (opinfo->flags & FL_USE_FPU)
		block->m_fpa->any = true;

	if (opinfo->flags & FL_TIMER)
		block->m_gpa->anyTimer = true;

	// Does the instruction output CR0?
	if (opinfo->flags & FL_RC_BIT)
		code->outputCR0 = code->inst.hex & 1; //todo fix
	else if ((opinfo->flags & FL_SET_CRn) && code->inst.CRFD == 0)
		code->outputCR0 = true;
	else
		code->outputCR0 = (opinfo->flags & FL_SET_CR0) ? true : false;

	// Does the instruction output CR1?
	if (opinfo->flags & FL_RC_BIT_F)
		code->outputCR1 = code->inst.hex & 1; //todo fix
	else if ((opinfo->flags & FL_SET_CRn) && code->inst.CRFD == 1)
		code->outputCR1 = true;
	else
		code->outputCR1 = (opinfo->flags & FL_SET_CR1) ? true : false;

	int numOut = 0;
	int numIn = 0;
	if (opinfo->flags & FL_OUT_A)
	{
		code->regsOut[numOut++] = code->inst.RA;
		block->m_gpa->SetOutputRegister(code->inst.RA, index);
	}
	if (opinfo->flags & FL_OUT_D)
	{
		code->regsOut[numOut++] = code->inst.RD;
		block->m_gpa->SetOutputRegister(code->inst.RD, index);
	}
	if (opinfo->flags & FL_OUT_S)
	{
		code->regsOut[numOut++] = code->inst.RS;
		block->m_gpa->SetOutputRegister(code->inst.RS, index);
	}
	if ((opinfo->flags & FL_IN_A) || ((opinfo->flags & FL_IN_A0) && code->inst.RA != 0))
	{
		code->regsIn[numIn++] = code->inst.RA;
		block->m_gpa->SetInputRegister(code->inst.RA, index);
	}
	if (opinfo->flags & FL_IN_B)
	{
		code->regsIn[numIn++] = code->inst.RB;
		block->m_gpa->SetInputRegister(code->inst.RB, index);
	}
	if (opinfo->flags & FL_IN_C)
	{
		code->regsIn[numIn++] = code->inst.RC;
		block->m_gpa->SetInputRegister(code->inst.RC, index);
	}
	if (opinfo->flags & FL_IN_S)
	{
		code->regsIn[numIn++] = code->inst.RS;
		block->m_gpa->SetInputRegister(code->inst.RS, index);
	}

	// Set remaining register slots as unused (-1)
	for (int j = numIn; j < 3; j++)
		code->regsIn[j] = -1;
	for (int j = numOut; j < 2; j++)
		code->regsOut[j] = -1;
	for (int j = 0; j < 3; j++)
		code->fregsIn[j] = -1;
	code->fregOut = -1;

	switch (opinfo->type)
	{
	case OPTYPE_INTEGER:
	case OPTYPE_LOAD:
	case OPTYPE_STORE:
	case OPTYPE_LOADFP:
	case OPTYPE_STOREFP:
		break;
	case OPTYPE_FPU:
		break;
	case OPTYPE_BRANCH:
		if (code->inst.hex == 0x4e800020)
		{
			// For analysis purposes, we can assume that blr eats opinfo->flags.
			code->outputCR0 = true;
			code->outputCR1 = true;
		}
		break;
	case OPTYPE_SYSTEM:
	case OPTYPE_SYSTEMFP:
		break;
	}
}

u32 PPCAnalyzer::Analyze(u32 address, CodeBlock *block, CodeBuffer *buffer, u32 blockSize)
{
	// Clear block stats
	memset(block->m_stats, 0, sizeof(BlockStats));

	// Clear register stats
	block->m_gpa->any = true;
	block->m_fpa->any = false;

	block->m_gpa->Clear();
	block->m_fpa->Clear();

	// Set the blocks start address
	block->m_address = address;

	// Reset our block state
	block->m_broken = false;
	block->m_memory_exception = false;
	block->m_num_instructions = 0;

	if (address == 0)
	{
		// Memory exception occurred during instruction fetch
		block->m_memory_exception = true;
		return address;
	}

	if (Core::g_CoreStartupParameter.bMMU && (address & JIT_ICACHE_VMEM_BIT))
	{
		if (!Memory::TranslateAddress(address, Memory::FLAG_OPCODE))
		{
			// Memory exception occurred during instruction fetch
			block->m_memory_exception = true;
			return address;
		}
	}

	CodeOp *code = buffer->codebuffer;

	bool found_exit = false;
	u32 return_address = 0;
	u32 numFollows = 0;
	u32 num_inst = 0;

	for (u32 i = 0; i < blockSize; ++i)
	{
		UGeckoInstruction inst = JitInterface::Read_Opcode_JIT(address);

		if (inst.hex != 0)
		{
			num_inst++;
			memset(&code[i], 0, sizeof(CodeOp));
			GekkoOPInfo *opinfo = GetOpInfo(inst);

			code[i].opinfo = opinfo;
			code[i].address = address;
			code[i].inst = inst;
			code[i].branchTo = -1;
			code[i].branchToIndex = -1;
			code[i].skip = false;
			block->m_stats->numCycles += opinfo->numCycles;

			SetInstructionStats(block, &code[i], opinfo, i);

			bool follow = false;
			u32 destination = 0;

			bool conditional_continue = false;

			// Do we inline leaf functions?
			if (HasOption(OPTION_LEAF_INLINE))
			{
				if (inst.OPCD == 18 && blockSize > 1)
				{
					//Is bx - should we inline? yes!
					if (inst.AA)
						destination = SignExt26(inst.LI << 2);
					else
						destination = address + SignExt26(inst.LI << 2);
					if (destination != block->m_address)
						follow = true;
				}
				else if (inst.OPCD == 19 && inst.SUBOP10 == 16 &&
					(inst.BO & (1 << 4)) && (inst.BO & (1 << 2)) &&
					return_address != 0)
				{
					// bclrx with unconditional branch = return
					follow = true;
					destination = return_address;
					return_address = 0;

					if (inst.LK)
						return_address = address + 4;
				}
				else if (inst.OPCD == 31 && inst.SUBOP10 == 467)
				{
					// mtspr
					const u32 index = (inst.SPRU << 5) | (inst.SPRL & 0x1F);
					if (index == SPR_LR) {
						// We give up to follow the return address
						// because we have to check the register usage.
						return_address = 0;
					}
				}

				// TODO: Find the optimal value for FUNCTION_FOLLOWING_THRESHOLD.
				//       If it is small, the performance will be down.
				//       If it is big, the size of generated code will be big and
				//       cache clearning will happen many times.
				// TODO: Investivate the reason why
				//       "0" is fastest in some games, MP2 for example.
				if (numFollows > FUNCTION_FOLLOWING_THRESHOLD)
					follow = false;
			}

			if (HasOption(OPTION_CONDITIONAL_CONTINUE))
			{
				if (inst.OPCD == 16 &&
				   ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0 || (inst.BO & BO_DONT_CHECK_CONDITION) == 0))
				{
					// bcx with conditional branch
					conditional_continue = true;
				}
				else if (inst.OPCD == 19 && inst.SUBOP10 == 16 &&
				        ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0 || (inst.BO & BO_DONT_CHECK_CONDITION) == 0))
				{
					// bclrx with conditional branch
					conditional_continue = true;
				}
				else if (inst.OPCD == 3 ||
					  (inst.OPCD == 31 && inst.SUBOP10 == 4))
				{
					// tw/twi tests and raises an exception
					conditional_continue = true;
				}
				else if (inst.OPCD == 19 && inst.SUBOP10 == 528 &&
				        (inst.BO_2 & BO_DONT_CHECK_CONDITION) == 0)
				{
					// Rare bcctrx with conditional branch
					// Seen in NES games
					conditional_continue = true;
				}
			}

			if (!follow)
			{
				if (!conditional_continue && opinfo->flags & FL_ENDBLOCK) //right now we stop early
				{
					found_exit = true;
					break;
				}
				address += 4;
			}
			// XXX: We don't support inlining yet.
#if 0
			else
			{
				numFollows++;
				// We don't "code[i].skip = true" here
				// because bx may store a certain value to the link register.
				// Instead, we skip a part of bx in Jit**::bx().
				address = destination;
				merged_addresses[size_of_merged_addresses++] = address;
			}
#endif
		}
		else
		{
			// ISI exception or other critical memory exception occured (game over)
			ERROR_LOG(DYNA_REC, "Instruction hex was 0!");
			break;
		}
	}

	if (block->m_num_instructions > 1)
		ReorderInstructions(block->m_num_instructions, code);

	if ((!found_exit && num_inst > 0) || blockSize == 1)
	{
		// We couldn't find an exit
		block->m_broken = true;
	}

	// Scan for CR0 dependency
	// assume next block wants CR0 to be safe
	bool wantsCR0 = true;
	bool wantsCR1 = true;
	bool wantsPS1 = true;
	for (int i = block->m_num_instructions - 1; i >= 0; i--)
	{
		if (code[i].outputCR0)
			wantsCR0 = false;
		if (code[i].outputCR1)
			wantsCR1 = false;
		if (code[i].outputPS1)
			wantsPS1 = false;
		wantsCR0 |= code[i].wantsCR0;
		wantsCR1 |= code[i].wantsCR1;
		wantsPS1 |= code[i].wantsPS1;
		code[i].wantsCR0 = wantsCR0;
		code[i].wantsCR1 = wantsCR1;
		code[i].wantsPS1 = wantsPS1;
	}
	block->m_num_instructions = num_inst;
	return address;
}


}  // namespace
