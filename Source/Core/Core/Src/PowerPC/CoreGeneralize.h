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
#ifndef _CORE_GENERALIZE_H
#define _CORE_GENERALIZE_H
#include "PPCAnalyst.h"
#include "JitCommon/JitCache.h"
#include "Jit64/JitRegCache.h" // These two Jit Includes NEED to be dropped
#include "x64Emitter.h"
#include "x64Analyzer.h"
#include "Jit64IL/IR.h"

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

class TrampolineCache : public Gen::XCodeBlock
{
public:
	void Init();
	void Shutdown();

	const u8 *GetReadTrampoline(const InstructionInfo &info);
	const u8 *GetWriteTrampoline(const InstructionInfo &info);
};


class cCore : public Gen::XCodeBlock
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
		u8* rewriteStart;

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
	#if !(defined JITTEST && JITTEST)
	GPRRegCache gpr;
	FPURegCache fpr;
	#endif

	// The default code buffer. We keep it around to not have to alloc/dealloc a
	// large chunk of memory for each recompiled block.
	PPCAnalyst::CodeBuffer code_buffer;
	
public:
	cCore() : code_buffer(32000){}

	~cCore(){}


	JitState js;
	JitOptions jo;
	IREmitter::IRBuilder ibuild;

	// Initialization, etc

	virtual void Init() = 0;
	virtual void Shutdown() = 0;

	// Jit!

	virtual void Jit(u32 em_address) = 0;
	virtual const u8* DoJit(u32 em_address, PPCAnalyst::CodeBuffer *code_buffer, JitBlock *b) = 0;

	virtual JitBlockCache *GetBlockCache() { return &blocks; }

	virtual void NotifyBreakpoint(u32 em_address, bool set) = 0;

	virtual void ClearCache() = 0;

	// Run!

	virtual void Run() = 0;
	virtual void SingleStep() = 0;

	const u8 *BackPatch(u8 *codePtr, int accessType, u32 em_address, CONTEXT *ctx);

#define JIT_OPCODE 0
	// Utilities for use by opcodes

	virtual void WriteExit(u32 destination, int exit_num) = 0;
	virtual void WriteExitDestInEAX(int exit_num) = 0;
	virtual void WriteExceptionExit(u32 exception) = 0;
	virtual void WriteRfiExitDestInEAX() = 0;
	virtual void WriteCallInterpreter(UGeckoInstruction _inst) = 0;
	virtual void Cleanup() = 0;
	
	virtual void UnsafeLoadRegToReg(Gen::X64Reg reg_addr, Gen::X64Reg reg_value, int accessSize, s32 offset = 0, bool signExtend = false) = 0;
	virtual void UnsafeWriteRegToReg(Gen::X64Reg reg_value, Gen::X64Reg reg_addr, int accessSize, s32 offset = 0) = 0;
	virtual void SafeLoadRegToEAX(Gen::X64Reg reg, int accessSize, s32 offset, bool signExtend = false) = 0;
	virtual void SafeWriteRegToReg(Gen::X64Reg reg_value, Gen::X64Reg reg_addr, int accessSize, s32 offset) = 0;

	virtual void WriteToConstRamAddress(int accessSize, const Gen::OpArg& arg, u32 address) = 0;
	virtual void WriteFloatToConstRamAddress(const Gen::X64Reg& xmm_reg, u32 address) = 0;
	virtual void GenerateCarry(Gen::X64Reg temp_reg) = 0;

	virtual void ForceSinglePrecisionS(Gen::X64Reg xmm) = 0;
	virtual void ForceSinglePrecisionP(Gen::X64Reg xmm) = 0;
	virtual void JitClearCA() = 0;
	virtual void JitSetCA() = 0;
	virtual void tri_op(int d, int a, int b, bool reversible, void (XEmitter::*op)(Gen::X64Reg, Gen::OpArg)) = 0;
	typedef u32 (*Operation)(u32 a, u32 b);
	virtual void regimmop(int d, int a, bool binary, u32 value, Operation doop, void (XEmitter::*op)(int, const Gen::OpArg&, const Gen::OpArg&), bool Rc = false, bool carry = false) = 0;
	virtual void fp_tri_op(int d, int a, int b, bool reversible, bool dupe, void (XEmitter::*op)(Gen::X64Reg, Gen::OpArg)) = 0;

	void WriteCode();

	// OPCODES
	virtual void unknown_instruction(UGeckoInstruction _inst) = 0;
	virtual void Default(UGeckoInstruction _inst) = 0;
	virtual void DoNothing(UGeckoInstruction _inst) = 0;
	virtual void HLEFunction(UGeckoInstruction _inst) = 0;

	void DynaRunTable4(UGeckoInstruction _inst);
	void DynaRunTable19(UGeckoInstruction _inst);
	void DynaRunTable31(UGeckoInstruction _inst);
	void DynaRunTable59(UGeckoInstruction _inst);
	void DynaRunTable63(UGeckoInstruction _inst);

	virtual void addx(UGeckoInstruction inst) = 0;
	virtual void orx(UGeckoInstruction inst) = 0;
	virtual void xorx(UGeckoInstruction inst) = 0;
	virtual void andx(UGeckoInstruction inst) = 0;
	virtual void mulli(UGeckoInstruction inst) = 0;
	virtual void mulhwux(UGeckoInstruction inst) = 0;
	virtual void mullwx(UGeckoInstruction inst) = 0;
	virtual void divwux(UGeckoInstruction inst) = 0;
	virtual void srawix(UGeckoInstruction inst) = 0;
	virtual void srawx(UGeckoInstruction inst) = 0;
	virtual void addex(UGeckoInstruction inst) = 0;
	virtual void addzex(UGeckoInstruction inst) = 0;

	virtual void extsbx(UGeckoInstruction inst) = 0;
	virtual void extshx(UGeckoInstruction inst) = 0;

	virtual void sc(UGeckoInstruction _inst) = 0;
	virtual void rfi(UGeckoInstruction _inst) = 0;

	virtual void bx(UGeckoInstruction inst) = 0;
	virtual void bclrx(UGeckoInstruction _inst) = 0;
	virtual void bcctrx(UGeckoInstruction _inst) = 0;
	virtual void bcx(UGeckoInstruction inst) = 0;

	virtual void mtspr(UGeckoInstruction inst) = 0;
	virtual void mfspr(UGeckoInstruction inst) = 0;
	virtual void mtmsr(UGeckoInstruction inst) = 0;
	virtual void mfmsr(UGeckoInstruction inst) = 0;
	virtual void mftb(UGeckoInstruction inst) = 0;
	virtual void mtcrf(UGeckoInstruction inst) = 0;
	virtual void mfcr(UGeckoInstruction inst) = 0;

	virtual void reg_imm(UGeckoInstruction inst) = 0;

	virtual void ps_sel(UGeckoInstruction inst) = 0;
	virtual void ps_mr(UGeckoInstruction inst) = 0;
	virtual void ps_sign(UGeckoInstruction inst) = 0; //aggregate
	virtual void ps_arith(UGeckoInstruction inst) = 0; //aggregate
	virtual void ps_mergeXX(UGeckoInstruction inst) = 0;
	virtual void ps_maddXX(UGeckoInstruction inst) = 0;
	virtual void ps_rsqrte(UGeckoInstruction inst) = 0;
	virtual void ps_sum(UGeckoInstruction inst) = 0;
	virtual void ps_muls(UGeckoInstruction inst) = 0;

	virtual void fp_arith_s(UGeckoInstruction inst) = 0;

	virtual void fcmpx(UGeckoInstruction inst) = 0;
	virtual void fmrx(UGeckoInstruction inst) = 0;

	virtual void cmpXX(UGeckoInstruction inst) = 0;

	virtual void cntlzwx(UGeckoInstruction inst) = 0;

	virtual void lfs(UGeckoInstruction inst) = 0;
	virtual void lfd(UGeckoInstruction inst) = 0;
	virtual void stfd(UGeckoInstruction inst) = 0;
	virtual void stfs(UGeckoInstruction inst) = 0;
	virtual void stfsx(UGeckoInstruction inst) = 0;
	virtual void psq_l(UGeckoInstruction inst) = 0;
	virtual void psq_st(UGeckoInstruction inst) = 0;

	virtual void fmaddXX(UGeckoInstruction inst) = 0;
	virtual void stX(UGeckoInstruction inst) = 0; //stw sth stb
	virtual void lXz(UGeckoInstruction inst) = 0;
	virtual void lha(UGeckoInstruction inst) = 0;
	virtual void rlwinmx(UGeckoInstruction inst) = 0;
	virtual void rlwimix(UGeckoInstruction inst) = 0;
	virtual void rlwnmx(UGeckoInstruction inst) = 0;
	virtual void negx(UGeckoInstruction inst) = 0;
	virtual void slwx(UGeckoInstruction inst) = 0;
	virtual void srwx(UGeckoInstruction inst) = 0;
	virtual void dcbz(UGeckoInstruction inst) = 0;
	virtual void lfsx(UGeckoInstruction inst) = 0;

	virtual void subfic(UGeckoInstruction inst) = 0;
	virtual void subfcx(UGeckoInstruction inst) = 0;
	virtual void subfx(UGeckoInstruction inst) = 0;
	virtual void subfex(UGeckoInstruction inst) = 0;

	virtual void lXzx(UGeckoInstruction inst) = 0;
	//virtual void lbzx(UGeckoInstruction inst) = 0;
	//virtual void lwzx(UGeckoInstruction inst) = 0;
	virtual void lhax(UGeckoInstruction inst) = 0;
	
	//virtual void lwzux(UGeckoInstruction inst) = 0;

	virtual void stXx(UGeckoInstruction inst) = 0;

	virtual void lmw(UGeckoInstruction inst) = 0;
	virtual void stmw(UGeckoInstruction inst) = 0;
};
extern cCore *jit; // jit to retain backwards compatibility
#endif
