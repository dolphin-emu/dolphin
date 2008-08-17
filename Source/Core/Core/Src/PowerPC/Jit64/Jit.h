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

// Low hanging fruit:
// all used in zelda
// negx

#ifndef _JIT_H
#define _JIT_H

#include "../PPCAnalyst.h"
#include "JitCache.h"
#include "x64Emitter.h"

namespace Jit64
{
	struct JitStats
	{
		u32 compiledBlocks;
		float averageCodeExpansion;
		float ratioOpsCompiled; //how many really were compiled, how many became "call interpreter"?
	};

#define JIT_OPCODE 0

	struct JitBlock;
	const u8* DoJit(u32 emaddress, JitBlock &b);
	bool IsInJitCode(u8 *codePtr);

	struct JitState
	{
		u32 compilerPC;
		u32 blockStart;
		int blockSize;
		int instructionNumber;
		int downcountAmount;

		bool isLastInstruction;
		bool blockSetsQuantizers;

		int fifoBytesThisBlock;

		PPCAnalyst::BlockStats st;
		PPCAnalyst::BlockRegStats gpa;
		PPCAnalyst::BlockRegStats fpa;
		PPCAnalyst::CodeOp *op;

		JitBlock *curBlock;
	};

	struct JitOptions
	{
		bool optimizeStack;
		bool assumeFPLoadFromMem;
		bool enableBlocklink;
		bool fpAccurateFlags;
		bool enableFastMem;
		bool optimizeGatherPipe;
	};

	extern JitState js;
	extern JitOptions jo;

	void Default(UGeckoInstruction _inst);
	void DoNothing(UGeckoInstruction _inst);
	
	void WriteExit(u32 destination, int exit_num);
	void WriteExitDestInEAX(int exit_num);
	void WriteExceptionExit(u32 exception);
	void WriteRfiExitDestInEAX();

	void HLEFunction(UGeckoInstruction _inst);

	void addx(UGeckoInstruction inst);
	void orx(UGeckoInstruction inst);
	void andx(UGeckoInstruction inst);
	void mulli(UGeckoInstruction inst);
	void mulhwux(UGeckoInstruction inst);
	void mullwx(UGeckoInstruction inst);
	void divwux(UGeckoInstruction inst);
	void srawix(UGeckoInstruction inst);
	void addex(UGeckoInstruction inst);

	void extsbx(UGeckoInstruction inst);
	void extshx(UGeckoInstruction inst);

	void sc(UGeckoInstruction _inst);
	void rfi(UGeckoInstruction _inst);

	void bx(UGeckoInstruction inst);
	void bclrx(UGeckoInstruction _inst);
	void bcctrx(UGeckoInstruction _inst);
	void bcx(UGeckoInstruction inst);

	void mtspr(UGeckoInstruction inst);
	void mfspr(UGeckoInstruction inst);
	void mtmsr(UGeckoInstruction inst);
	void mfmsr(UGeckoInstruction inst);
	void mftb(UGeckoInstruction inst);

	void reg_imm(UGeckoInstruction inst);

	void ps_sel(UGeckoInstruction inst);
	void ps_mr(UGeckoInstruction inst);
	void ps_sign(UGeckoInstruction inst); //aggregate
	void ps_arith(UGeckoInstruction inst); //aggregate
	void ps_mergeXX(UGeckoInstruction inst);
	void ps_maddXX(UGeckoInstruction inst);
	void ps_rsqrte(UGeckoInstruction inst);

	void fp_arith_s(UGeckoInstruction inst);

	void fcmpx(UGeckoInstruction inst);
	void fmrx(UGeckoInstruction inst);

	void cmpli(UGeckoInstruction inst);
	void cmpi(UGeckoInstruction inst);
	void cmpl(UGeckoInstruction inst);
	void cmp(UGeckoInstruction inst);

	void cntlzwx(UGeckoInstruction inst);

	void lfs(UGeckoInstruction inst);
	void lfd(UGeckoInstruction inst);
	void stfd(UGeckoInstruction inst);
	void stfs(UGeckoInstruction inst);
	void psq_l(UGeckoInstruction inst);
	void psq_st(UGeckoInstruction inst);

	void fmaddXX(UGeckoInstruction inst);
	void stX(UGeckoInstruction inst); //stw sth stb
	void lXz(UGeckoInstruction inst);
	void lha(UGeckoInstruction inst);
	void rlwinmx(UGeckoInstruction inst);
	void rlwimix(UGeckoInstruction inst);
	void rlwnmx(UGeckoInstruction inst);
	void negx(UGeckoInstruction inst);
	void slwx(UGeckoInstruction inst);
	void srwx(UGeckoInstruction inst);
	void dcbz(UGeckoInstruction inst);
	void lfsx(UGeckoInstruction inst);
	void subfic(UGeckoInstruction inst);
	void subfcx(UGeckoInstruction inst);
	void subfx(UGeckoInstruction inst);
	void lbzx(UGeckoInstruction inst);

	void lmw(UGeckoInstruction inst);
	void stmw(UGeckoInstruction inst);
}

#endif

