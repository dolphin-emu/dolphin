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

#include <string>

#include "StringUtil.h"
#include "Interpreter/Interpreter.h"
#include "../HW/Memmap.h"
#include "PPCTables.h"
#include "SymbolDB.h"
#include "SignatureDB.h"
#include "PPCAnalyst.h"

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

PPCAnalyst::CodeOp *codebuffer;

enum
{
	CODEBUFFER_SIZE = 32000,
};

void Init()
{
	codebuffer = new PPCAnalyst::CodeOp[CODEBUFFER_SIZE];
}

void Shutdown()
{
	delete [] codebuffer;
}


void AnalyzeFunction2(Symbol &func);
u32 EvaluateBranchTarget(UGeckoInstruction instr, u32 pc);

// void FixUpInternalBranches(CodeOp *code, int begin, int end);


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
void AnalyzeFunction2(Symbol &func)
{
	// u32 addr = func.address; 
	u32 flags = func.flags;
	/*
	for (int i = 0; i < func.size; i++)
	{
		UGeckoInstruction instr = (UGeckoInstruction)Memory::ReadUnchecked_U32(addr);

		GekkoOPInfo *info = GetOpInfo(instr);
		if (!info)
		{
			LOG(HLE,"Seems function %s contains bad op %08x",func.name,instr);
		}
		else
		{
			if (info->flags & FL_TIMER)
			{
				flags |= FFLAG_TIMERINSTRUCTIONS;
			}
		}
		addr+=4;
	}*/

	bool nonleafcall = false;
	for (size_t i = 0; i < func.calls.size(); i++)
	{
		SCall c = func.calls[i];
		Symbol *called_func = g_symbolDB.GetSymbolFromAddr(c.function);
		if (called_func && (called_func->flags & FFLAG_LEAF) == 0)
		{
			nonleafcall = true;
			break;
		}
	}

	if (nonleafcall && !(flags & FFLAG_EVIL) && !(flags & FFLAG_RFI))
		flags |= FFLAG_ONLYCALLSNICELEAFS;

	func.flags = flags;
}

// Currently not used
void FixUpInternalBranches(CodeOp *code, int begin, int end)
{
	for (int i = begin; i < end; i++)
	{
		if (code[i].branchTo != INVALID_TARGET) //check if this branch already processed
		{
			if (code[i].inst.OPCD == 16)
			{
				u32 target = SignExt16(code[i].inst.BD<<2);
				if (!code[i].inst.AA)
					target += code[i].address;
				//local branch
				code[i].branchTo = target;
			}
			else
				code[i].branchTo = INVALID_TARGET;
		}
	}

	//brute force
	for (int i = begin; i < end; i++)
	{
		if (code[i].branchTo != INVALID_TARGET)
		{
			bool found = false;
			for (int j = begin; j < end; j++)
				if (code[i].branchTo == code[j].address)
					code[i].branchToIndex = j;
			if (!found)
			{
				LOG(HLE, "ERROR: branch target missing");
			}
		}
	}
}

void ShuffleUp(CodeOp *code, int first, int last)
{
	CodeOp temp = code[first];
	for (int i = first; i < last; i++)
		code[i] = code[i + 1];
	code[last] = temp;
}

// IMPORTANT - CURRENTLY ASSUMES THAT A IS A COMPARE
bool CanSwapAdjacentOps(const CodeOp &a, const CodeOp &b)
{
	// Disabled for now
	return false;

	const GekkoOPInfo *a_info = GetOpInfo(a.inst);
	const GekkoOPInfo *b_info = GetOpInfo(b.inst);
	int a_flags = a_info->flags;
	int b_flags = b_info->flags;
	if (b_flags & (FL_SET_CRx | FL_ENDBLOCK | FL_TIMER | FL_EVIL))
		return false;
	if ((b_flags & (FL_RC_BIT | FL_RC_BIT_F)) && (b.inst.hex & 1))
		return false;

	// 10 cmpi, 11 cmpli - we got a compare!
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

	// For now, only integer ops acceptable.
	switch (b_info->type) {
	case OPTYPE_INTEGER:
		break;
	default:
		return false;
	}

	// Check that we have no register collisions.
	bool no_swap = false;
	for (int j = 0; j < 3; j++)
	{
		int regIn = a.regsIn[j];
		if (regIn < 0)
			continue;	
		if (b.regsOut[0] == regIn ||
			b.regsOut[1] == regIn)
		{
			// reg collision! don't swap
			return false;
		}
	}

	return true;
}

CodeOp *Flatten(u32 address, int &realsize, BlockStats &st, BlockRegStats &gpa, BlockRegStats &fpa)
{
	int numCycles = 0;
	u32 blockstart = address;
	memset(&st, 0, sizeof(st));
	UGeckoInstruction previnst = Memory::Read_Instruction(address-4);
	if (previnst.hex == 0x4e800020)
	{
		st.isFirstBlockOfFunction = true;
	}

	gpa.any = true;
	fpa.any = false;

	enum Todo
	{
		JustCopy = 0, Flatten = 1, Nothing = 2
	};
	Todo todo = Nothing;

	//Symbol *f = g_symbolDB.GetSymbolFromAddr(address);
	int maxsize = CODEBUFFER_SIZE;
	//for now, all will return JustCopy :P
	/*
	if (f)
	{
		if (f->flags & FFLAG_LEAF)
		{
			//no reason to flatten
			todo = JustCopy;
		}
		else if (f->flags & FFLAG_ONLYCALLSNICELEAFS)
		{
			//inline calls if possible
			//todo = Flatten;
			todo = JustCopy;
		}
		else
		{
			todo = JustCopy;
		}
		todo = JustCopy;

		maxsize = f->size;
	}
	else*/
		todo = JustCopy;

	CodeOp *code = codebuffer; //new CodeOp[size];

	if (todo == JustCopy)
	{
		realsize = 0;
		bool foundExit = false;
		int numFollows = 0;
		for (int i = 0; i < maxsize; i++, realsize++)
		{
			memset(&code[i], 0, sizeof(CodeOp));
			code[i].address = address;
			UGeckoInstruction inst = Memory::Read_Instruction(code[i].address);
			_assert_msg_(GEKKO, inst.hex != 0, "Zero Op - Error flattening %08x op %08x",address+i*4,inst);
			code[i].inst = inst;
			code[i].branchTo = -1;
			code[i].branchToIndex = -1;
			GekkoOPInfo *opinfo = GetOpInfo(inst);
			if (opinfo)
				numCycles += opinfo->numCyclesMinusOne + 1;
			_assert_msg_(GEKKO, opinfo != 0, "Invalid Op - Error flattening %08x op %08x",address+i*4,inst);
			int flags = opinfo->flags;

			bool follow = false;
			u32 destination;
			if (inst.OPCD == 18)
			{
				//Is bx - should we inline? yes!
				if (inst.AA)
					destination = SignExt26(inst.LI << 2);
				else
					destination = address + SignExt26(inst.LI << 2);
				if (destination != blockstart)
					follow = true;
			}
			if (follow)
				numFollows++;
			if (numFollows > 1)
				follow = false;
			
			follow = false;
			if (!follow)
			{
				if (flags & FL_ENDBLOCK) //right now we stop early
				{
					foundExit = true;
					break;
				}
				address += 4;
			}
			else
			{
				address = destination;
			}
		}
		_assert_msg_(GEKKO,foundExit,"Analyzer ERROR - Function %08x too big", blockstart);
		realsize++;
		st.numCycles = numCycles;
		FixUpInternalBranches(code,0,realsize);
	}
	else if (todo == Flatten)
	{
		return 0;
	}
	else
	{
		return 0;
	}

	// Do analysis of the code, look for dependencies etc
	int numSystemInstructions = 0;
	for (int i = 0; i < 32; i++)
	{
		gpa.firstRead[i]  = -1;
		gpa.firstWrite[i] = -1;
		gpa.numReads[i] = 0;
		gpa.numWrites[i] = 0;
	}

	gpa.any = true;
	for (size_t i = 0; i < realsize; i++)
	{
		UGeckoInstruction inst = code[i].inst;
		if (PPCTables::UsesFPU(inst))
			fpa.any = true;

		code[i].wantsCR0 = false;
		code[i].wantsCR1 = false;
		code[i].wantsPS1 = false;

		const GekkoOPInfo *opinfo = GetOpInfo(code[i].inst);
		_assert_msg_(GEKKO, opinfo != 0, "Invalid Op - Error scanning %08x op %08x",address+i*4,inst);
		int flags = opinfo->flags;

		if (flags & FL_TIMER)
			gpa.anyTimer = true;

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

		for (int j = 0; j < 3; j++)
		{
			code[i].fregsIn[j] = -1;
			code[i].regsIn[j] = -1;
		}
		for (int j = 0; j < 2; j++)
			code[i].regsOut[j] = -1;

		code[i].fregOut = -1;

		int numOut = 0;
		int numIn = 0;
		if (flags & FL_OUT_A)
		{
			code[i].regsOut[numOut++] = inst.RA;
			gpa.numWrites[inst.RA]++;
		}
		if (flags & FL_OUT_D)
		{
			code[i].regsOut[numOut++] = inst.RD;
			gpa.numWrites[inst.RD]++;
		}
		if (flags & FL_OUT_S)
		{
			code[i].regsOut[numOut++] = inst.RS;
			gpa.numWrites[inst.RS]++;
		}
		if ((flags & FL_IN_A) || ((flags & FL_IN_A0) && inst.RA != 0))
		{
			code[i].regsIn[numIn++] = inst.RA;
			gpa.numReads[inst.RA]++;
		}
		if (flags & FL_IN_B)
		{
			code[i].regsIn[numIn++] = inst.RB;
			gpa.numReads[inst.RB]++;
		}
		if (flags & FL_IN_C)
		{
			code[i].regsIn[numIn++] = inst.RC;
			gpa.numReads[inst.RC]++;
		}
		if (flags & FL_IN_S)
		{
			code[i].regsIn[numIn++] = inst.RS;
			gpa.numReads[inst.RS]++;
		}

		switch (opinfo->type) 
		{
		case OPTYPE_INTEGER:
		case OPTYPE_LOAD:
		case OPTYPE_STORE:
			break;
		case OPTYPE_FPU:
			break;
		case OPTYPE_LOADFP:
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

		for (int j = 0; j < numIn; j++)
		{
			int r = code[i].regsIn[j];
			if (r < 0 || r > 31)
				PanicAlert("wtf");
			if (gpa.firstRead[r] == -1)
				gpa.firstRead[r] = (short)(i);
			gpa.lastRead[r] = (short)(i);
			gpa.numReads[r]++;
		}

		for (int j = 0; j < numOut; j++)
		{ 
			int r = code[i].regsOut[j];
			if (r < 0 || r > 31)
				PanicAlert("wtf");
			if (gpa.firstWrite[r] == -1)
				gpa.firstWrite[r] = (short)(i);
			gpa.lastWrite[r] = (short)(i);
			gpa.numWrites[r]++;
		}
	}

	// Instruction Reordering Pass

	// Bubble down compares towards branches, so that they can be merged (merging not yet implemented).
	// -2: -1 for the pair, -1 for not swapping with the final instruction which is probably the branch.
	for (int i = 0; i < (realsize - 2); i++)
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


	//Scan for CR0 dependency
	//assume next block wants CR0 to be safe
	bool wantsCR0 = true;
	bool wantsCR1 = true;
	bool wantsPS1 = true;
	for (int i = realsize - 1; i; i--)
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

	// Time for code shuffling, taking into account the above dependency analysis.
	bool successful_shuffle = false;
	//Move compares 
	// Try to push compares as close as possible to the following branch
	// this way we can do neat stuff like combining compare and branch
	// and avoid emitting any cr flags at all
	/*
		*	
		pseudo:
		if (op is cmp and sets CR0)
		{
        scan forward for branch
		if we hit any instruction that sets CR0, bail
		if we hit any instruction that writes to any of the cmp input variables, bail
		shuffleup(code, cmpaddr, branchaddr-1)
		}


		how to merge:
		if (op is cmp and nextop is condbranch)
        {
		check if nextnextop wants cr
		if it does, bail (or merge and write cr)
		else merge!
        }

	*/
	if (successful_shuffle) {
		// Disasm before and after, display side by side
	}
	// Decide what regs to potentially regcache
	return code;
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

void FindFunctionsAfterBLR(SymbolDB *func_db)
{
	vector<u32> funcAddrs;

	for (SymbolDB::XFuncMap::iterator iter = func_db->GetIterator(); iter != func_db->End(); iter++)
		funcAddrs.push_back(iter->second.address + iter->second.size);

	for (vector<u32>::iterator iter = funcAddrs.begin(); iter != funcAddrs.end(); iter++)
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

void FindFunctions(u32 startAddr, u32 endAddr, SymbolDB *func_db)
{
	//Step 1: Find all functions
	FindFunctionsFromBranches(startAddr, endAddr, func_db);
	FindFunctionsAfterBLR(func_db);

	//Step 2: 
	func_db->FillInCallers();

	int numLeafs = 0, numNice = 0, numUnNice = 0;
	int numTimer = 0, numRFI = 0, numStraightLeaf = 0;
	int leafSize = 0, niceSize = 0, unniceSize = 0;
	for (SymbolDB::XFuncMap::iterator iter = func_db->GetIterator(); iter != func_db->End(); iter++)
	{
		if (iter->second.address == 4)
		{
			LOG(HLE, "weird function");
			continue;
		}
		AnalyzeFunction2(iter->second);
		Symbol &f = iter->second;
		if (f.name.substr(0,3) == "zzz")
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

	LOG(HLE, "Functions analyzed. %i leafs, %i nice, %i unnice. %i timer, %i rfi. %i are branchless leafs.",numLeafs,numNice,numUnNice,numTimer,numRFI,numStraightLeaf);
	LOG(HLE, "Average size: %i (leaf), %i (nice), %i(unnice)", leafSize, niceSize, unniceSize);
}

/*
void AnalyzeBackwards()
{
#ifndef BWLINKS
    return;
#else
    for (int i=0; i<numEntries; i++)
    {
        u32 ptr = entries[i].vaddress;
        if (ptr && entries[i].type == ST_FUNCTION)
        {
            for (int a = 0; a<entries[i].size/4; a++)
            {
                u32 inst = Memory::ReadUnchecked_U32(ptr);
                switch (inst >> 26)
                {
                case 18:
                    if (LK) //LK
                    {
                        u32 addr;
                        if(AA)
                            addr = SignExt26(LI << 2);
                        else
                            addr = ptr + SignExt26(LI << 2);

                        int funNum = GetSymbolNum(addr);
                        if (funNum>=0) 
                            entries[funNum].backwardLinks.push_back(ptr);
                    }
                    break;
                default:
                    ;
                }
                ptr+=4;
            }
        }
    }
#endif
}

*/

}  // namespace
