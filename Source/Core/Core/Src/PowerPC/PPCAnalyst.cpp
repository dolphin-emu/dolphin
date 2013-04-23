// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <string>
#include <queue>

#include "StringUtil.h"
#include "Interpreter/Interpreter.h"
#include "../HW/Memmap.h"
#include "JitInterface.h"
#include "PPCTables.h"
#include "PPCSymbolDB.h"
#include "SignatureDB.h"
#include "PPCAnalyst.h"
#include "../ConfigManager.h"
#include "../GeckoCode.h"

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
					if (target != INVALID_TARGET && instr.LK)
					{
						//we found a branch-n-link!
						func.calls.push_back(SCall(target,addr));
						func.flags &= ~FFLAG_LEAF;
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
	for (size_t i = 0; i < func->calls.size(); i++)
	{
		SCall c = func->calls[i];
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

// Does not yet perform inlining - although there are plans for that.
// Returns the exit address of the next PC
u32 Flatten(u32 address, int *realsize, BlockStats *st, BlockRegStats *gpa,
			BlockRegStats *fpa, bool &broken_block, CodeBuffer *buffer,
			int blockSize, u32* merged_addresses,
			int capacity_of_merged_addresses, int& size_of_merged_addresses)
{
	if (capacity_of_merged_addresses < FUNCTION_FOLLOWING_THRESHOLD) {
		PanicAlert("Capacity of merged_addresses is too small!");
	}
	std::fill_n(merged_addresses, capacity_of_merged_addresses, 0);
	merged_addresses[0] = address;
	size_of_merged_addresses = 1;

	memset(st, 0, sizeof(*st));
	
	// Disabled the following optimization in preference of FAST_ICACHE
	//UGeckoInstruction previnst = Memory::Read_Opcode_JIT_LC(address - 4);
	//if (previnst.hex == 0x4e800020)
	//	st->isFirstBlockOfFunction = true;

	gpa->any = true;
	fpa->any = false;
	
	for (int i = 0; i < 32; i++)
	{
		gpa->firstRead[i]  = -1;
		gpa->firstWrite[i] = -1;
		gpa->numReads[i] = 0;
		gpa->numWrites[i] = 0;
	}

	u32 blockstart = address;
	int maxsize = blockSize;

	int num_inst = 0;
	int numFollows = 0;
	int numCycles = 0;

	CodeOp *code = buffer->codebuffer;
	bool foundExit = false;

	u32 returnAddress = 0;

	// Used for Gecko CST1 code. (See GeckoCode/GeckoCode.h)
	// We use std::queue but it is not so slow
	// because cst1_instructions does not allocate memory so many times.
	std::queue<UGeckoInstruction> cst1_instructions;
	const std::map<u32, std::vector<u32> >& inserted_asm_codes =
		Gecko::GetInsertedAsmCodes();

	// Do analysis of the code, look for dependencies etc
	int numSystemInstructions = 0;
	for (int i = 0; i < maxsize; i++)
	{
		UGeckoInstruction inst;

		if (!cst1_instructions.empty())
		{
			// If the Gecko CST1 instruction queue is not empty,
			// we consume the first instruction.
			inst = UGeckoInstruction(cst1_instructions.front());
			cst1_instructions.pop();
			address -= 4;

		}
		else
		{
			// If the address is the insertion point of Gecko CST1 code,
			// we push the code into the instruction queue and
			// consume the first instruction.
			std::map<u32, std::vector<u32> >::const_iterator it =
				inserted_asm_codes.find(address);
			if (it != inserted_asm_codes.end())
			{
				const std::vector<u32>& codes = it->second;
				for (std::vector<u32>::const_iterator itCur = codes.begin(),
					itEnd = codes.end(); itCur != itEnd; ++itCur)
				{
					cst1_instructions.push(*itCur);
				}
				inst = UGeckoInstruction(cst1_instructions.front());
				cst1_instructions.pop();

			}
			else
			{
				inst = JitInterface::Read_Opcode_JIT(address);
			}
		}
		
		if (inst.hex != 0)
		{
			num_inst++;
			memset(&code[i], 0, sizeof(CodeOp));
			GekkoOPInfo *opinfo = GetOpInfo(inst);
			code[i].opinfo = opinfo;
			// FIXME: code[i].address may not be correct due to CST1 code.
			code[i].address = address;
			code[i].inst = inst;
			code[i].branchTo = -1;
			code[i].branchToIndex = -1;
			code[i].skip = false;
			numCycles += opinfo->numCyclesMinusOne + 1;

			code[i].wantsCR0 = false;
			code[i].wantsCR1 = false;
			code[i].wantsPS1 = false;

			int flags = opinfo->flags;

			if (flags & FL_USE_FPU)
				fpa->any = true;

			if (flags & FL_TIMER)
				gpa->anyTimer = true;

			// Does the instruction output CR0?
			if (flags & FL_RC_BIT)
				code[i].outputCR0 = inst.hex & 1; //todo fix
			else if ((flags & FL_SET_CRn) && inst.CRFD == 0)
				code[i].outputCR0 = true;
			else
				code[i].outputCR0 = (flags & FL_SET_CR0) ? true : false;

			// Does the instruction output CR1?
			if (flags & FL_RC_BIT_F)
				code[i].outputCR1 = inst.hex & 1; //todo fix
			else if ((flags & FL_SET_CRn) && inst.CRFD == 1)
				code[i].outputCR1 = true;
			else
				code[i].outputCR1 = (flags & FL_SET_CR1) ? true : false;

			int numOut = 0;
			int numIn = 0;
			if (flags & FL_OUT_A)
			{
				code[i].regsOut[numOut++] = inst.RA;
				gpa->SetOutputRegister(inst.RA, i);
			}
			if (flags & FL_OUT_D)
			{
				code[i].regsOut[numOut++] = inst.RD;
				gpa->SetOutputRegister(inst.RD, i);
			}
			if (flags & FL_OUT_S)
			{
				code[i].regsOut[numOut++] = inst.RS;
				gpa->SetOutputRegister(inst.RS, i);
			}
			if ((flags & FL_IN_A) || ((flags & FL_IN_A0) && inst.RA != 0))
			{
				code[i].regsIn[numIn++] = inst.RA;
				gpa->SetInputRegister(inst.RA, i);
			}
			if (flags & FL_IN_B)
			{
				code[i].regsIn[numIn++] = inst.RB;
				gpa->SetInputRegister(inst.RB, i);
			}
			if (flags & FL_IN_C)
			{
				code[i].regsIn[numIn++] = inst.RC;
				gpa->SetInputRegister(inst.RC, i);
			}
			if (flags & FL_IN_S)
			{
				code[i].regsIn[numIn++] = inst.RS;
				gpa->SetInputRegister(inst.RS, i);
			}

			// Set remaining register slots as unused (-1)
			for (int j = numIn; j < 3; j++)
				code[i].regsIn[j] = -1;
			for (int j = numOut; j < 2; j++)
				code[i].regsOut[j] = -1;
			for (int j = 0; j < 3; j++)
				code[i].fregsIn[j] = -1;
			code[i].fregOut = -1;

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
				if (code[i].inst.hex == 0x4e800020)
				{
					// For analysis purposes, we can assume that blr eats flags.
					code[i].outputCR0 = true;
					code[i].outputCR1 = true;
				}
				break;
			case OPTYPE_SYSTEM:
			case OPTYPE_SYSTEMFP:
				numSystemInstructions++;
				break;
			}

			bool follow = false;
			u32 destination = 0;
			if (inst.OPCD == 18 && blockSize > 1)
			{
				//Is bx - should we inline? yes!
				if (inst.AA)
					destination = SignExt26(inst.LI << 2);
				else
					destination = address + SignExt26(inst.LI << 2);
				if (destination != blockstart)
					follow = true;
			}
			else if (inst.OPCD == 19 && inst.SUBOP10 == 16 &&
				(inst.BO & (1 << 4)) && (inst.BO & (1 << 2)) &&
				returnAddress != 0)
			{
				// bclrx with unconditional branch = return
				follow = true;
				destination = returnAddress;
				returnAddress = 0;

				if (inst.LK)
					returnAddress = address + 4;
			}
			else if (inst.OPCD == 31 && inst.SUBOP10 == 467)
			{
				// mtspr
				const u32 index = (inst.SPRU << 5) | (inst.SPRL & 0x1F);
				if (index == SPR_LR) {
					// We give up to follow the return address
					// because we have to check the register usage.
					returnAddress = 0;
				}
			}

			if (follow)
				numFollows++;
			// TODO: Find the optimal value for FUNCTION_FOLLOWING_THRESHOLD.
			//       If it is small, the performance will be down.
			//       If it is big, the size of generated code will be big and
			//       cache clearning will happen many times.
			// TODO: Investivate the reason why
			//       "0" is fastest in some games, MP2 for example.
			if (numFollows > FUNCTION_FOLLOWING_THRESHOLD)
				follow = false;

			if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bMergeBlocks) {
				follow = false;
			}

			if (!follow)
			{
				if (opinfo->flags & FL_ENDBLOCK) //right now we stop early
				{
					foundExit = true;
					break;
				}
				address += 4;
			}
			else
			{
				// We don't "code[i].skip = true" here
				// because bx may store a certain value to the link register.
				// Instead, we skip a part of bx in Jit**::bx().
				address = destination;
				merged_addresses[size_of_merged_addresses++] = address;
			}
		}
		else
		{
			// ISI exception or other critical memory exception occurred (game over)
			break;
		}
	}
	st->numCycles = numCycles;

	// Instruction Reordering Pass
	if (num_inst > 1)
	{
		// Bubble down compares towards branches, so that they can be merged.
		// -2: -1 for the pair, -1 for not swapping with the final instruction which is probably the branch.
		for (int i = 0; i < num_inst - 2; i++)
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

	if (!foundExit && num_inst > 0)
	{
		// A broken block is a block that does not end in a branch
		broken_block = true;
	}
	
	// Scan for CR0 dependency
	// assume next block wants CR0 to be safe
	bool wantsCR0 = true;
	bool wantsCR1 = true;
	bool wantsPS1 = true;
	for (int i = num_inst - 1; i >= 0; i--)
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

	*realsize = num_inst;
	// ...
	return address;
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

	for (PPCSymbolDB::XFuncMap::iterator iter = func_db->GetIterator(); iter != func_db->End(); ++iter)
		funcAddrs.push_back(iter->second.address + iter->second.size);

	for (vector<u32>::iterator iter = funcAddrs.begin(); iter != funcAddrs.end(); ++iter)
	{
		u32 location = *iter;
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
	for (PPCSymbolDB::XFuncMap::iterator iter = func_db->GetIterator(); iter != func_db->End(); ++iter)
	{
		if (iter->second.address == 4)
		{
			WARN_LOG(OSHLE, "Weird function");
			continue;
		}
		AnalyzeFunction2(&(iter->second));
		Symbol &f = iter->second;
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

}  // namespace
