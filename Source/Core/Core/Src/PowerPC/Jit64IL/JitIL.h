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

#ifndef _JITIL_H
#define _JITIL_H

#include "../PPCAnalyst.h"
#include "../JitCommon/JitBase.h"
#include "../JitCommon/JitCache.h"
#include "../JitCommon/JitBackpatch.h"
#include "../JitCommon/Jit_Util.h"
#include "x64Emitter.h"
#include "x64Analyzer.h"
#include "IR.h"
#include "../JitCommon/JitBase.h"
#include "JitILAsm.h"

// #define INSTRUCTION_START Default(inst); return;
// #define INSTRUCTION_START PPCTables::CountInstruction(inst);
#define INSTRUCTION_START

#define JITDISABLE(type) \
	if (Core::g_CoreStartupParameter.bJITOff || \
		Core::g_CoreStartupParameter.bJIT##type##Off) \
		{Default(inst); return;}

#ifdef _M_X64
#define DISABLE64 \
	{Default(inst); return;}
#else
#define DISABLE64
#endif

class JitIL : public Jitx86Base
{
private:


	// The default code buffer. We keep it around to not have to alloc/dealloc a
	// large chunk of memory for each recompiled block.
	PPCAnalyst::CodeBuffer code_buffer;

public:
	JitILAsmRoutineManager asm_routines;

	JitIL() : code_buffer(32000) {}
	~JitIL() {}

	IREmitter::IRBuilder ibuild;

	// Initialization, etc

	void Init();
	void Shutdown();

	// Jit!

	void Jit(u32 em_address);
	const u8* DoJit(u32 em_address, PPCAnalyst::CodeBuffer *code_buffer, JitBlock *b);

	void Trace();

	void ClearCache();
	const u8 *GetDispatcher() {
		return asm_routines.dispatcher;  // asm_routines.dispatcher
	}
	const CommonAsmRoutines *GetAsmRoutines() {
		return &asm_routines;
	}

	const char *GetName() {
#ifdef _M_X64
		return "JIT64IL";
#else
		return "JIT32IL";
#endif
	}

	// Run!

	void Run();
	void SingleStep();

	// Utilities for use by opcodes

	void WriteExit(u32 destination, int exit_num);
	void WriteExitDestInOpArg(const Gen::OpArg& arg);
	void WriteExceptionExit();
	void WriteRfiExitDestInOpArg(const Gen::OpArg& arg);
	void WriteCallInterpreter(UGeckoInstruction _inst);
	void Cleanup();
	
	void WriteToConstRamAddress(int accessSize, const Gen::OpArg& arg, u32 address);
	void WriteFloatToConstRamAddress(const Gen::X64Reg& xmm_reg, u32 address);
	void GenerateCarry(Gen::X64Reg temp_reg);

	void tri_op(int d, int a, int b, bool reversible, void (Gen::XEmitter::*op)(Gen::X64Reg, Gen::OpArg));
	typedef u32 (*Operation)(u32 a, u32 b);
	void regimmop(int d, int a, bool binary, u32 value, Operation doop, void (Gen::XEmitter::*op)(int, const Gen::OpArg&, const Gen::OpArg&), bool Rc = false, bool carry = false);
	void fp_tri_op(int d, int a, int b, bool reversible, bool dupe, void (Gen::XEmitter::*op)(Gen::X64Reg, Gen::OpArg));

	void WriteCode();

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
	void boolX(UGeckoInstruction inst);
	void mulli(UGeckoInstruction inst);
	void mulhwux(UGeckoInstruction inst);
	void mullwx(UGeckoInstruction inst);
	void divwux(UGeckoInstruction inst);
	void srawix(UGeckoInstruction inst);
	void srawx(UGeckoInstruction inst);
	void addex(UGeckoInstruction inst);
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
	void crXX(UGeckoInstruction inst);

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
	void lbzu(UGeckoInstruction inst);
	void lha(UGeckoInstruction inst);
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

	void lXzx(UGeckoInstruction inst);
	void lhax(UGeckoInstruction inst);

	void stXx(UGeckoInstruction inst);

	void lmw(UGeckoInstruction inst);
	void stmw(UGeckoInstruction inst);

	void icbi(UGeckoInstruction inst);
};

void Jit(u32 em_address);

void ProfiledReJit();

#endif  // _JITIL_H
