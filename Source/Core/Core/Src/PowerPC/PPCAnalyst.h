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
#ifndef _PPCANALYST_H
#define _PPCANALYST_H

#include <vector>
#include <map>

#include <string>

#include "Common.h"
#include "Gekko.h"

class PPCAnalyst
{
private:

	struct SCall
	{
        SCall(u32 a, u32 b) :
            function(a),
            callAddress(b)
        {}
		u32 function;
		u32 callAddress;
	};

	struct SDBFunc
	{
		u32 size;
		std::string name;

        SDBFunc() :
            size(0)
        {
        }
	};

public:
	struct CodeOp //16B
	{
		UGeckoInstruction inst;
		u32 address;
		u32 branchTo; //if 0, not a branch
		int  branchToIndex; //index of target block
		u8 regsOut[2];
		u8 regsIn[3];
		u8 fregOut;
		u8 fregsIn[3];
		bool isBranchTarget;
		bool wantsCR0;
		bool wantsCR1;
		bool wantsPS1;
		bool outputCR0;
		bool outputCR1;
		bool outputPS1;
		const u8 *x86ptr;
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

		int GetTotalNumAccesses(int reg) {return numReads[reg]+numWrites[reg];}
		int GetUseRange(int reg) {
			return max(lastRead[reg], lastWrite[reg]) - 
				   min(firstRead[reg], firstWrite[reg]);}
	};

	void ShuffleUp(CodeOp *code, int first, int last);

	struct SFunction
	{
        SFunction() :            
            hash(0),
            address(0),
            flags(0),
            size(0),
            numCalls(0)
        {}

        ~SFunction()
        {
            callers.clear();
            calls.clear();
        }

		std::string name;
		std::vector<SCall> callers; //addresses of functions that call this function
		std::vector<SCall> calls;   //addresses of functions that are called by this function
		u32 hash;            //use for HLE function finding
		u32 address;
		u32 flags;
		int size;
		int numCalls;
	};

	enum
	{
		FFLAG_TIMERINSTRUCTIONS=(1<<0),
		FFLAG_LEAF=(1<<1),
		FFLAG_ONLYCALLSNICELEAFS=(1<<2),
		FFLAG_EVIL=(1<<3),
		FFLAG_RFI=(1<<4),
		FFLAG_STRAIGHT=(1<<5)
   	};	

private:
	typedef void (*functionGetterCallback)(SFunction *f);
	typedef std::map<u32, SFunction>  XFuncMap;
	typedef std::map<u32, SFunction*> XFuncPtrMap;
	typedef std::map<u32, SDBFunc>    XFuncDB;

	static XFuncMap    functions;
	static XFuncPtrMap checksumToFunction;
	static XFuncDB	   database;

	static u32 AnalyzeFunction(u32 startAddr, SFunction &func);
	static void AnalyzeFunction2(SFunction &func);
	static u32 EvaluateBranchTarget(UGeckoInstruction instr, u32 pc);
	static SFunction *AddFunction(u32 startAddr);
	static void FindFunctionsFromBranches(u32 startAddr, u32 endAddr);
	static u32 FuncChecksum (u32 offsetStart, u32 offsetEnd);
	static void FillInCallers();
	static void FindFunctionsAfterBLR();
	static void FixUpInternalBranches(CodeOp *code, int begin, int end);

public:
	static CodeOp *Flatten(u32 address, u32 &realsize, BlockStats &st, BlockRegStats &gpa, BlockRegStats &fpa);

	static void LogFunctionCall(u32 addr);

	static void FindFunctions(u32 startAddr, u32 endAddr);
	static void PrintCalls(u32 funcAddr);
	static void PrintCallers(u32 funcAddr);
	static void AddToFuncDB(u32 startAddr, u32 size, const TCHAR *name);
	static void ListFunctions();

	static bool SaveFuncDB(const TCHAR *filename);
	static bool LoadFuncDB(const TCHAR *filename);
	static void ClearDB();
	static void ListDB();
	static void UseFuncDB();
	static void CleanDB(); //remove zzz_

	static void GetAllFuncs(functionGetterCallback callback);

};

#endif

