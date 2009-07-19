// Copyright (C) 2003-2009 Dolphin Project.

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

// ========================
// See comments in Jit.cpp.
// ========================

// Mystery: Capcom vs SNK 800aa278

// CR flags approach:
//   * Store that "N+Z flag contains CR0" or "S+Z flag contains CR3".
//   * All flag altering instructions flush this
//   * A flush simply does a conditional write to the appropriate CRx.
//   * If flag available, branch code can become absolutely trivial.


//////////////////////////////////////////////////////////////////////////////////////////
// Settings
// ¯¯¯¯¯¯¯¯¯¯
#define JIT_OFF_OPTIONS // Compile with JIT off options
////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Include
// ¯¯¯¯¯¯¯¯¯¯
#if JITTEST
	#include "../Jit64IL/Jit.h"
#else

#ifndef _JIT_H
#define _JIT_H

#include "../PPCAnalyst.h"
#include "../JitCommon/JitCache.h"
#include "JitRegCache.h"
#include "x64Emitter.h"
#include "x64Analyzer.h"

#ifdef _WIN32
	#include <windows.h>
#endif
///////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Declarations and definitions
// ¯¯¯¯¯¯¯¯¯¯

void Jit(u32 em_address);

#ifndef _WIN32

	// A bit of a hack to get things building under linux. We manually fill in this structure as needed
	// from the real context.
	struct CONTEXT
	{
	#ifdef _M_X64
		u64 Rip;
		u64 Rax;
	#else
		u32 Eip;
		u32 Eax;
	#endif 
	};

#endif

// Use these to control the instruction selection
// #define INSTRUCTION_START Default(inst); return;
// #define INSTRUCTION_START PPCTables::CountInstruction(inst);
#define INSTRUCTION_START
///////////////////////////////////


class TrampolineCache : public Gen::XCodeBlock
{
public:
	void Init();
	void Shutdown();

	const u8 *GetReadTrampoline(const InstructionInfo &info);
	const u8 *GetWriteTrampoline(const InstructionInfo &info);
};


class Jit64 : public Gen::XCodeBlock
{
private:
	struct JitState
	{
		u32 compilerPC;
		u32 next_compilerPC;
		u32 blockStart;
		bool cancel;
		UGeckoInstruction next_inst;  // for easy peephole opt.
		int blockSize;
		int instructionNumber;
		int downcountAmount;
		int block_flags;

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
		bool fastInterrupts;
		bool accurateSinglePrecision;
	};

	JitBlockCache blocks;
	TrampolineCache trampolines;
	GPRRegCache gpr;
	FPURegCache fpr;

	// The default code buffer. We keep it around to not have to alloc/dealloc a
	// large chunk of memory for each recompiled block.
	PPCAnalyst::CodeBuffer code_buffer;

public:
	Jit64() : code_buffer(32000) {}
	~Jit64() {}

	JitState js;
	JitOptions jo;

	// Initialization, etc

	void Init();
	void Shutdown();

	// Jit!

	void Jit(u32 em_address);
	const u8* DoJit(u32 em_address, PPCAnalyst::CodeBuffer *code_buffer, JitBlock *b);

	JitBlockCache *GetBlockCache() { return &blocks; }

	void NotifyBreakpoint(u32 em_address, bool set);

	void ClearCache();

	// Run!

	void Run();
	void SingleStep();

	const u8 *BackPatch(u8 *codePtr, int accessType, u32 em_address, CONTEXT *ctx);

#define JIT_OPCODE 0

	// Utilities for use by opcodes

	void WriteExit(u32 destination, int exit_num);
	void WriteExitDestInEAX(int exit_num);
	void WriteExceptionExit(u32 exception);
	void WriteRfiExitDestInEAX();
	void WriteCallInterpreter(UGeckoInstruction _inst);
	void Cleanup();
	
	void UnsafeLoadRegToReg(Gen::X64Reg reg_addr, Gen::X64Reg reg_value, int accessSize, s32 offset = 0, bool signExtend = false);
	void UnsafeWriteRegToReg(Gen::X64Reg reg_value, Gen::X64Reg reg_addr, int accessSize, s32 offset = 0);
	void SafeLoadRegToEAX(Gen::X64Reg reg, int accessSize, s32 offset, bool signExtend = false);
	void SafeWriteRegToReg(Gen::X64Reg reg_value, Gen::X64Reg reg_addr, int accessSize, s32 offset);

	void WriteToConstRamAddress(int accessSize, const Gen::OpArg& arg, u32 address);
	void WriteFloatToConstRamAddress(const Gen::X64Reg& xmm_reg, u32 address);
	void GenerateCarry(Gen::X64Reg temp_reg);

	void ForceSinglePrecisionS(Gen::X64Reg xmm);
	void ForceSinglePrecisionP(Gen::X64Reg xmm);
	void JitClearCA();
	void JitSetCA();
	void tri_op(int d, int a, int b, bool reversible, void (XEmitter::*op)(Gen::X64Reg, Gen::OpArg));
	typedef u32 (*Operation)(u32 a, u32 b);
	void regimmop(int d, int a, bool binary, u32 value, Operation doop, void (XEmitter::*op)(int, const Gen::OpArg&, const Gen::OpArg&), bool Rc = false, bool carry = false);
	void fp_tri_op(int d, int a, int b, bool reversible, bool dupe, void (XEmitter::*op)(Gen::X64Reg, Gen::OpArg));


	// OPCODES
	void unknown_instruction(UGeckoInstruction _inst);
	void Default(UGeckoInstruction _inst);
	void DoNothing(UGeckoInstruction _inst);
	void HLEFunction(UGeckoInstruction _inst);

	void DynaRunTable4(UGeckoInstruction _inst);
	void DynaRunTable19(UGeckoInstruction _inst);
	void DynaRunTable31(UGeckoInstruction _inst);
	void DynaRunTable59(UGeckoInstruction _inst);
	void DynaRunTable63(UGeckoInstruction _inst);

	void addx(UGeckoInstruction inst);
	void orx(UGeckoInstruction inst);
	void xorx(UGeckoInstruction inst);
	void andx(UGeckoInstruction inst);
	void mulli(UGeckoInstruction inst);
	void mulhwux(UGeckoInstruction inst);
	void mullwx(UGeckoInstruction inst);
	void divwux(UGeckoInstruction inst);
	void srawix(UGeckoInstruction inst);
	void srawx(UGeckoInstruction inst);
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
	void mtcrf(UGeckoInstruction inst);
	void mfcr(UGeckoInstruction inst);

	void reg_imm(UGeckoInstruction inst);

	void ps_sel(UGeckoInstruction inst);
	void ps_mr(UGeckoInstruction inst);
	void ps_sign(UGeckoInstruction inst); //aggregate
	void ps_arith(UGeckoInstruction inst); //aggregate
	void ps_mergeXX(UGeckoInstruction inst);
	void ps_maddXX(UGeckoInstruction inst);
	void ps_rsqrte(UGeckoInstruction inst);
	void ps_sum(UGeckoInstruction inst);
	void ps_muls(UGeckoInstruction inst);

	void fp_arith_s(UGeckoInstruction inst);

	void fcmpx(UGeckoInstruction inst);
	void fmrx(UGeckoInstruction inst);

	void cmpXX(UGeckoInstruction inst);

	void cntlzwx(UGeckoInstruction inst);

	void lfs(UGeckoInstruction inst);
	void lfd(UGeckoInstruction inst);
	void stfd(UGeckoInstruction inst);
	void stfs(UGeckoInstruction inst);
	void stfsx(UGeckoInstruction inst);
	void psq_l(UGeckoInstruction inst);
	void psq_st(UGeckoInstruction inst);

	void fmaddXX(UGeckoInstruction inst);
	void fsign(UGeckoInstruction inst);
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
	void subfex(UGeckoInstruction inst);

	void lbzx(UGeckoInstruction inst);
	void lwzx(UGeckoInstruction inst);
	void lhax(UGeckoInstruction inst);
	
	void lwzux(UGeckoInstruction inst);

	void stXx(UGeckoInstruction inst);

	void lmw(UGeckoInstruction inst);
	void stmw(UGeckoInstruction inst);
};

extern Jit64 jit;

#endif // _JIT_H
#endif // JITTEST
