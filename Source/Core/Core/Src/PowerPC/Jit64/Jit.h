// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// ========================
// See comments in Jit.cpp.
// ========================

// Mystery: Capcom vs SNK 800aa278

// CR flags approach:
//   * Store that "N+Z flag contains CR0" or "S+Z flag contains CR3".
//   * All flag altering instructions flush this
//   * A flush simply does a conditional write to the appropriate CRx.
//   * If flag available, branch code can become absolutely trivial.

// Settings
// ----------
#ifndef _JIT64_H
#define _JIT64_H

#include "../PPCAnalyst.h"
#include "../JitCommon/JitCache.h"
#include "../JitCommon/Jit_Util.h"
#include "JitRegCache.h"
#include "x64Emitter.h"
#include "x64Analyzer.h"
#include "../JitCommon/JitBackpatch.h"
#include "../JitCommon/JitBase.h"
#include "JitAsm.h"

// Use these to control the instruction selection
// #define INSTRUCTION_START Default(inst); return;
// #define INSTRUCTION_START PPCTables::CountInstruction(inst);
#define INSTRUCTION_START

#define JITDISABLE(type) \
	if (Core::g_CoreStartupParameter.bJITOff || \
	Core::g_CoreStartupParameter.bJIT##type##Off) \
	{Default(inst); return;}

#define MEMCHECK_START \
	FixupBranch memException; \
	if (js.memcheck) \
	{ TEST(32, M((void *)&PowerPC::ppcState.Exceptions), Imm32(EXCEPTION_DSI)); \
	memException = J_CC(CC_NZ); }

#define MEMCHECK_END \
	if (js.memcheck) \
		SetJumpTarget(memException);

class Jit64 : public Jitx86Base
{
private:
	GPRRegCache gpr;
	FPURegCache fpr;

	// The default code buffer. We keep it around to not have to alloc/dealloc a
	// large chunk of memory for each recompiled block.
	PPCAnalyst::CodeBuffer code_buffer;
	Jit64AsmRoutineManager asm_routines;

public:
	Jit64() : code_buffer(32000) {}
	~Jit64() {}

	void Init();
	void Shutdown();

	// Jit!

	void Jit(u32 em_address);
	const u8* DoJit(u32 em_address, PPCAnalyst::CodeBuffer *code_buffer, JitBlock *b);

	JitBlockCache *GetBlockCache() { return &blocks; }

	void Trace();

	void ClearCache();

	const u8 *GetDispatcher() {
		return asm_routines.dispatcher;
	}
	const CommonAsmRoutines *GetAsmRoutines() {
		return &asm_routines;
	}

	const char *GetName() {
#ifdef _M_X64
		return "JIT64";
#else
		return "JIT32";
#endif
	}
	// Run!

	void Run();
	void SingleStep();

	// Utilities for use by opcodes

	void WriteExit(u32 destination, int exit_num);
	void WriteExitDestInEAX();
	void WriteExceptionExit();
	void WriteExternalExceptionExit();
	void WriteRfiExitDestInEAX();
	void WriteCallInterpreter(UGeckoInstruction _inst);
	void Cleanup();

	void GenerateConstantOverflow(bool overflow);
	void GenerateOverflow();
	void FinalizeCarryOverflow(bool oe, bool inv = false);
	void GetCarryEAXAndClear();
	void FinalizeCarryGenerateOverflowEAX(bool oe, bool inv = false);
	void GenerateCarry();
	void GenerateRC();
	void ComputeRC(const Gen::OpArg & arg);

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
	void addcx(UGeckoInstruction inst);
	void mulli(UGeckoInstruction inst);
	void mulhwux(UGeckoInstruction inst);
	void mullwx(UGeckoInstruction inst);
	void divwux(UGeckoInstruction inst);
	void divwx(UGeckoInstruction inst);
	void srawix(UGeckoInstruction inst);
	void srawx(UGeckoInstruction inst);
	void addex(UGeckoInstruction inst);
	void addmex(UGeckoInstruction inst);
	void addzex(UGeckoInstruction inst);

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
	void mcrf(UGeckoInstruction inst);
	void mcrxr(UGeckoInstruction inst);

	void boolX(UGeckoInstruction inst);
	void crXXX(UGeckoInstruction inst);

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
	void frsqrtex(UGeckoInstruction inst);

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
	void rlwinmx(UGeckoInstruction inst);
	void rlwimix(UGeckoInstruction inst);
	void rlwnmx(UGeckoInstruction inst);
	void negx(UGeckoInstruction inst);
	void slwx(UGeckoInstruction inst);
	void srwx(UGeckoInstruction inst);
	void dcbst(UGeckoInstruction inst);
	void dcbz(UGeckoInstruction inst);
	void lfsx(UGeckoInstruction inst);

	void subfic(UGeckoInstruction inst);
	void subfcx(UGeckoInstruction inst);
	void subfx(UGeckoInstruction inst);
	void subfex(UGeckoInstruction inst);
	void subfmex(UGeckoInstruction inst);
	void subfzex(UGeckoInstruction inst);

	void twx(UGeckoInstruction inst);

	void lXXx(UGeckoInstruction inst);
	
	void stXx(UGeckoInstruction inst);

	void lmw(UGeckoInstruction inst);
	void stmw(UGeckoInstruction inst);

	void icbi(UGeckoInstruction inst);
};

void ProfiledReJit();

#endif // _JIT64_H
