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
#include "PPCAnalyst.h"

// Analyzes PowerPC code in memory to find functions
// After running, for each function we will know what functions it calls
// and what functions calls it. That is, we will have an incomplete call graph,
// but only missing indirect branches.

// The results of this analysis are currently not really used for anything, other than
// finding function boundaries so that we can find, fingerprint and detect library functions.

using namespace std;

PPCAnalyst::XFuncMap PPCAnalyst::functions;
PPCAnalyst::XFuncPtrMap PPCAnalyst::checksumToFunction;
PPCAnalyst::XFuncDB PPCAnalyst::database;

#define INVALID_TARGET ((u32)-1)

//Adds a known function to the hash database
void PPCAnalyst::AddToFuncDB(u32 startAddr, u32 size, const TCHAR *name)
{
	if (startAddr < 0x80000000 || startAddr>0x80000000+Memory::RAM_SIZE)
		return;

	SFunction tf;
	SFunction *tempfunc=&tf;
	
	XFuncMap::iterator iter = functions.find(startAddr);
	if (iter != functions.end())
	{
		tempfunc = &iter->second;
	}
	else
	{
		AnalyzeFunction(startAddr,tf);
		functions[startAddr] = *tempfunc;
		checksumToFunction[tempfunc->hash] = &(functions[startAddr]);
	}

	tempfunc->name = name;

	SDBFunc temp;
	temp.size = size;
	temp.name = name;


	XFuncDB::iterator iter2 = database.find(tempfunc->hash);
	if (iter2 == database.end())
		database[tempfunc->hash] = temp;
}


u32 PPCAnalyst::EvaluateBranchTarget(UGeckoInstruction instr, u32 pc)
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
		break;
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

u32 PPCAnalyst::AnalyzeFunction(u32 startAddr, SFunction &func)
{
	//if (startAddr <0x80000008 || startAddr>0x80000000+Memory::RAM_MASK)
	//	return 0;

	func.name = StringFromFormat("zzz_%08x ??", startAddr);
	func.calls.clear();
	func.callers.clear();
	func.size = 0;
	func.flags = FFLAG_LEAF;
	u32 addr = startAddr; 
	
	u32 farthestInternalBranchTarget = startAddr;
	int numInternalBranches = 0;
	while (true)
	{
		func.size++;
		if (func.size > 1024*16) //weird
			return 0;

		UGeckoInstruction instr = (UGeckoInstruction)Memory::ReadUnchecked_U32(addr);

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
					func.hash = PPCAnalyst::FuncChecksum(startAddr, addr);
					if (numInternalBranches == 0)
						func.flags |= FFLAG_STRAIGHT;
					return addr;
				}
			}
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
					u32 target = SignExt16(instr.BD<<2);
					
					if (!instr.AA)
						target += addr;

					if (target > farthestInternalBranchTarget)
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
			return 0;
		}
		addr+=4;
	}
}


// Second pass analysis, done after the first pass is done for all functions
// so we have more information to work with
void PPCAnalyst::AnalyzeFunction2(SFunction &func)
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
	for (size_t i = 0; i<func.calls.size(); i++)
	{
		SCall c = func.calls[i];
		XFuncMap::iterator iter = functions.find(c.function);
		if (iter != functions.end())
		{
			if (((*iter).second.flags & FFLAG_LEAF) == 0)
			{
				nonleafcall = true;
				break;
			}
		}
	}

	if (nonleafcall && !(flags & FFLAG_EVIL) && !(flags & FFLAG_RFI))
		flags |= FFLAG_ONLYCALLSNICELEAFS;

	func.flags = flags;
}

// Currently not used
void PPCAnalyst::FixUpInternalBranches(CodeOp *code, int begin, int end)
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

//yaya ugly
PPCAnalyst::CodeOp codebuffer[20000];

void PPCAnalyst::ShuffleUp(CodeOp *code, int first, int last)
{
	CodeOp temp = code[first];
	for (int i=first; i<last; i++)
		code[i] = code[i+1];
	code[last] = temp;
}


PPCAnalyst::CodeOp *PPCAnalyst::Flatten(u32 address, u32 &realsize, BlockStats &st, BlockRegStats &gpa, BlockRegStats &fpa)
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

	XFuncMap::iterator iter = functions.find(address);
	int maxsize = 20000;

	//for now, all will return JustCopy :P
	if (iter != functions.end())
	{
		SFunction &f = iter->second;

		if (f.flags & FFLAG_LEAF)
		{
			//no reason to flatten
			todo = JustCopy;
		}
		else if (f.flags & FFLAG_ONLYCALLSNICELEAFS)
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

		maxsize = f.size;
	}
	else
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
			code[i].x86ptr = 0;
			GekkoOPInfo *opinfo = GetOpInfo(inst);
			numCycles += opinfo->numCyclesMinusOne + 1;
			_assert_msg_(GEKKO, opinfo != 0,"Invalid Op - Error flattening %08x op %08x",address+i*4,inst);
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
		return 0;

	// Do analysis of the code, look for dependencies etc
	int numSystemInstructions = 0;

	for (int i=0; i<32; i++)
	{
		gpa.firstRead[i]  = -1;
		gpa.firstWrite[i] = -1;
		gpa.numReads[i] = 0;
		gpa.numWrites[i] = 0;
	}

	gpa.any = true;
	for (size_t i=0; i<realsize; i++)
	{
		UGeckoInstruction inst = code[i].inst;
		if (PPCTables::UsesFPU(inst))
		{
			fpa.any = true;
		}
		GekkoOPInfo *opinfo = GetOpInfo(code[i].inst);
		_assert_msg_(GEKKO,opinfo!=0,"Invalid Op - Error scanning %08x op %08x",address+i*4,inst);
		int flags = opinfo->flags;

		if (flags & FL_TIMER)
			gpa.anyTimer = true;

		// Does the instruction output CR0?
		if (flags & FL_RC_BIT)
			code[i].outputCR0 = inst.hex&1; //todo fix
		else if ((flags & FL_SET_CRn) && inst.CRFD == 0)
			code[i].outputCR0 = true;
		else
			code[i].outputCR0 = (flags & FL_SET_CR0) ? true : false;

		// Does the instruction output CR1?
		if (flags & FL_RC_BIT_F)
			code[i].outputCR1 = inst.hex&1; //todo fix
		else if ((flags & FL_SET_CRn) && inst.CRFD == 1)
			code[i].outputCR1 = true;
		else
			code[i].outputCR1 = (flags & FL_SET_CR1) ? true : false;

		for (int j=0; j<3; j++)
		{
			code[i].fregsIn[j] = -1;
			code[i].regsIn[j] = -1;
		}
		for (int j=0; j<2; j++)
			code[i].regsOut[j] = -1;

		code[i].fregOut=-1;

		int numOut = 0;
		int numIn = 0;
		switch (opinfo->type) 
		{
		case OPTYPE_INTEGER:
		case OPTYPE_LOAD:
		case OPTYPE_STORE:
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
			break;
		case OPTYPE_FPU:
			break;
		case OPTYPE_LOADFP:
			break;

		case OPTYPE_SYSTEM:
		case OPTYPE_SYSTEMFP:
			numSystemInstructions++;
			break;
		}

		for (int j=0; j<numIn; j++)
		{
			int r = code[i].regsIn[j];
			if (gpa.firstRead[r] == -1)
				gpa.firstRead[r] = (short)(i);
			gpa.lastRead[r] = (short)(i);
			gpa.numReads[r]++;
		}

		for (int j=0; j<numOut; j++)
		{
			int r = code[i].regsOut[j];
			if (gpa.firstWrite[r] == -1)
				gpa.firstWrite[r] = (short)(i);
			gpa.lastWrite[r] = (short)(i);
			gpa.numWrites[r]++;
		}
	}

	//Scan for CR0 dependency
	//assume next block wants CR0 to be safe
	bool wantsCR0 = true;
	bool wantsCR1 = true;
	bool wantsPS1 = true;
	for (int i=realsize-1; i; i--)
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

	// Decide what regs to potentially regcache
	return code;
}

// Adds the function to the list, unless it's already there
PPCAnalyst::SFunction *PPCAnalyst::AddFunction(u32 startAddr)
{
	if (startAddr<0x80000010)
		return 0;
	XFuncMap::iterator iter = functions.find(startAddr);
	if (iter != functions.end())
	{
		// it's already in the list
		return 0;
	}
	else
	{
		SFunction tempFunc; //the current one we're working on
		u32 targetEnd = AnalyzeFunction(startAddr, tempFunc);
		if (targetEnd == 0)
			return 0; //found a dud :(

		//LOG(HLE,"SFunction found at %08x",startAddr);
		functions[startAddr] = tempFunc;
		checksumToFunction[tempFunc.hash] = &(functions[startAddr]);
		return &functions[startAddr];
	}
}

//Most functions that are relevant to analyze should be
//called by another function. Therefore, let's scan the 
//entire space for bl operations and find what functions
//get called. 

void PPCAnalyst::FindFunctionsFromBranches(u32 startAddr, u32 endAddr)
{
	functions.clear();
	checksumToFunction.clear();

	for (u32 addr = startAddr; addr<endAddr; addr+=4)
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
						u32 target = SignExt26(instr.LI<<2);
						if (!instr.AA)
							target += addr;
						PPCAnalyst::AddFunction(target);
					}
				}
				break;
			default:
				break;
			}
		}
	}
}

void PPCAnalyst::FindFunctionsAfterBLR()
{
	vector<u32> funcAddrs;

	for (XFuncMap::iterator iter = functions.begin(); iter!=functions.end(); iter++)
		funcAddrs.push_back(iter->second.address + iter->second.size*4);

	for (vector<u32>::iterator iter = funcAddrs.begin(); iter!=funcAddrs.end(); iter++)
	{
		u32 location = *iter;
		while (true)
		{
			if (PPCTables::IsValidInstruction(Memory::Read_Instruction(location)))
			{
				//check if this function is already mapped	
				SFunction *f = AddFunction(location);
				if (!f)
					break;
				else
				{
					location += f->size * 4;
				}
			}
			else
				break;
		}
	}
}

void PPCAnalyst::FindFunctions(u32 startAddr, u32 endAddr)
{
	//Step 1: Find all functions
	FindFunctionsFromBranches(startAddr,endAddr);
	
	
	LOG(HLE,"Memory scan done. Found %i functions.",functions.size());
	FindFunctionsAfterBLR();

	LOG(HLE,"Scanning after every function. Found %i new functions.",functions.size());
	//Step 2: 
	FillInCallers();

	int numLeafs = 0, numNice = 0, numUnNice=0, numTimer=0, numRFI=0, numStraightLeaf=0;
	int leafSize = 0, niceSize = 0, unniceSize = 0;
	for (XFuncMap::iterator iter = functions.begin(); iter!=functions.end(); iter++)
	{
		if (iter->second.address == 4)
		{
			LOG(HLE,"weird function");
			continue;
		}
		AnalyzeFunction2(iter->second);
		SFunction &f=iter->second;
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

	LOG(HLE,"Functions analyzed. %i leafs, %i nice, %i unnice. %i timer, %i rfi. %i are branchless leafs.",numLeafs,numNice,numUnNice,numTimer,numRFI,numStraightLeaf);
	LOG(HLE,"Average size: %i (leaf), %i (nice), %i(unnice)",leafSize,niceSize,unniceSize);
//	PPCAnalyst::LoadDolwinFuncDB();
}

void PPCAnalyst::ListFunctions()
{
	for (XFuncMap::iterator iter = functions.begin(); iter!=functions.end(); iter++)
	{
		LOG(HLE,"%s @ %08x: %i bytes (hash %08x) : %i calls",iter->second.name.c_str(),iter->second.address,iter->second.size,iter->second.hash,iter->second.numCalls);
	}
	LOG(HLE,"%i functions known in this program above.",functions.size());
}

void PPCAnalyst::ListDB()
{
	for (XFuncDB::iterator iter = database.begin(); iter != database.end(); iter++)
	{
		LOG(HLE,"%s : %i bytes, hash = %08x",iter->second.name.c_str(), iter->second.size, iter->first);
	}
	LOG(HLE,"%i functions known in current database above.",database.size());
}

void PPCAnalyst::FillInCallers()
{
	// TODO(ector): I HAVE NO IDEA WHY, but this function overwrites the some 
	// data of "PPCAnalyst::XFuncMap PPCAnalyst::functions"
	for (XFuncMap::iterator iter = functions.begin(); iter!=functions.end(); iter++)
	{
		SFunction &f = iter->second;
		for (size_t i=0; i<f.calls.size(); i++)
        {
            SCall NewCall(iter->first, f.calls[i].callAddress);
            u32 FunctioAddress = f.calls[i].function;
            XFuncMap::iterator FuncIterator = functions.find(FunctioAddress);
            if (FuncIterator != functions.end())
            {
                SFunction& rCalledFunction = FuncIterator->second;
                rCalledFunction.callers.push_back(NewCall);
            }
            else
            {
                LOG(HLE,"FillInCallers tries to fill data in an unknown fuction 0x%08x.", FunctioAddress);
            }
        }
	}

}



u32 PPCAnalyst::FuncChecksum (u32 offsetStart, u32 offsetEnd)
{
	u32 sum = 0, offset;
	u32 opcode, auxop, op, op2, op3;

	for (offset = offsetStart; offset <= offsetEnd; offset+=4) 
	{
		opcode = Memory::Read_Instruction(offset);
		op = opcode & 0xFC000000; 
		op2 = 0;
		op3 = 0;
		auxop = op >> 26;
		switch (auxop) 
		{
		case 4: //PS instructions
			op2 = opcode & 0x0000003F;
			switch ( op2 ) 
			{
			case 0:
			case 8:
			case 16:
			case 21:
			case 22:
				op3 = opcode & 0x000007C0;
			}
			break;

		case 7: //addi muli etc
		case 8:
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
			op2 = opcode & 0x03FF0000;
			break;
		
		case 19: // MCRF??
		case 31: //integer
		case 63: //fpu
			op2 = opcode & 0x000007FF;
			//why not bring in the RA RB RD/S too?
			break;
		case 59: //fpu
			op2 = opcode & 0x0000003F;
			if ( op2 < 16 ) 
				op3 = opcode & 0x000007C0;
			break;
		default:
			if (auxop >= 32  && auxop < 56)
				op2 = opcode & 0x03FF0000;
			break;
		}
		// Checksum only uses opcode, not opcode data, because opcode data changes 
		// in all compilations, but opcodes dont!
		sum = ( ( (sum << 17 ) & 0xFFFE0000 ) | ( (sum >> 15) & 0x0001FFFF ) );
		sum = sum ^ (op | op2 | op3);
	}
	return sum;
}

struct FuncDesc
{
	u32 checkSum;
	u32 size;
	char name[128];
};

bool PPCAnalyst::SaveFuncDB(const TCHAR *filename)
{
	FILE *f = fopen(filename,"wb");
	if (!f)
	{
		LOG(HLE,"Database save failed");
		return false;
	}
	int fcount = (int)database.size();
	fwrite(&fcount,4,1,f);
	for (XFuncDB::iterator iter = database.begin(); iter!=database.end(); iter++)
	{
		FuncDesc temp;
		memset(&temp,0,sizeof(temp));
		temp.checkSum = iter->first;
		temp.size = iter->second.size;
		strncpy(temp.name, iter->second.name.c_str(), 127);
		fwrite(&temp,sizeof(temp),1,f);
	}
	fclose(f);
	LOG(HLE,"Database save successful");
	return true;
}

bool PPCAnalyst::LoadFuncDB(const TCHAR *filename)
{
	FILE *f = fopen(filename,"rb");
	if (!f)
	{
		LOG(HLE,"Database load failed");
		return false;
	}
	u32 fcount=0;
	fread(&fcount,4,1,f);
	for (size_t i=0; i<fcount; i++)
	{
		FuncDesc temp;
        memset(&temp, 0, sizeof(temp));

        fread(&temp,sizeof(temp),1,f);
		SDBFunc f;
		f.name = temp.name;
		f.size = temp.size;

	    database[temp.checkSum] = f;
	}
	fclose(f);

	UseFuncDB();
	LOG(HLE,"Database load successful");
	return true;
}


void PPCAnalyst::ClearDB()
{
	database.clear();
}

void PPCAnalyst::UseFuncDB()
{
	for (XFuncDB::iterator iter = database.begin(); iter!=database.end(); iter++)
	{
		u32 checkSum = iter->first;
		XFuncPtrMap::iterator i = checksumToFunction.find(checkSum);
		if (i != checksumToFunction.end())
		{
			SFunction *f = (i->second);
			if (iter->second.size == (unsigned int)f->size*4)
			{
				f->name = iter->second.name;
			}
			else
			{
				// This message is really useless.
				//LOG(HLE,"%s, Right hash, wrong size! %08x %i %i",iter->second.name.c_str(),f->address,f->size,iter->second.size);
			}
			//LOG(HLE,"Found %s at %08x (size: %08x)!",iter->second.name.c_str(),f->address,f->size);
			//found a function!
		}
		else
		{
			//function with this checksum not found
			//			LOG(HLE,"Did not find %s!",name);
		}
	}
}

void PPCAnalyst::PrintCalls(u32 funcAddr)
{
	XFuncMap::iterator iter = functions.find(funcAddr);
	if (iter != functions.end())
	{
		SFunction &f = iter->second;
		LOG(HLE,"The function %s at %08x calls:",f.name.c_str(), f.address);
		for (vector<SCall>::iterator fiter = f.calls.begin(); fiter!=f.calls.end(); fiter++)
		{
			XFuncMap::iterator n = functions.find(fiter->function);
			if (n != functions.end())
			{
				LOG(CONSOLE,"* %08x : %s", fiter->callAddress, n->second.name.c_str());
			}
		}
	}
	else
	{
		LOG(CONSOLE,"SFunction does not exist");
	}
}

void PPCAnalyst::PrintCallers(u32 funcAddr)
{
	XFuncMap::iterator iter = functions.find(funcAddr);
	if (iter != functions.end())
	{
		SFunction &f = iter->second;
		LOG(CONSOLE,"The function %s at %08x is called by:",f.name.c_str(),f.address);
		for (vector<SCall>::iterator fiter = f.callers.begin(); fiter!=f.callers.end(); fiter++)
		{
			XFuncMap::iterator n = functions.find(fiter->function);
			if (n != functions.end())
			{
				LOG(CONSOLE,"* %08x : %s", fiter->callAddress, n->second.name.c_str());
			}
		}
	}
}

void PPCAnalyst::GetAllFuncs(functionGetterCallback callback)
{
    XFuncMap::iterator iter = functions.begin();
    while(iter!=functions.end())
    {
        callback(&(iter->second));
        iter++;
    }
}

void PPCAnalyst::LogFunctionCall(u32 addr)
{
	//u32 from = PC;
	XFuncMap::iterator iter = functions.find(addr);
	if (iter != functions.end())
	{
		SFunction &f = iter->second;
		f.numCalls++;
	}
}
