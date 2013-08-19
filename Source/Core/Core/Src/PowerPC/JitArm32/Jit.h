// Copyright (C) 2003 Dolphin Project.

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

// Settings
// ----------
#ifndef _JITARM_H
#define _JITARM_H
#include "../CPUCoreBase.h"
#include "../PPCAnalyst.h"
#include "JitArmCache.h"
#include "JitRegCache.h"
#include "JitFPRCache.h"
#include "JitAsm.h"
#include "../JitCommon/JitBase.h"

// Use these to control the instruction selection
// #define INSTRUCTION_START Default(inst); return;
// #define INSTRUCTION_START PPCTables::CountInstruction(inst);
#define INSTRUCTION_START
#define JITDISABLE(type) \
	if (Core::g_CoreStartupParameter.bJITOff || \
	Core::g_CoreStartupParameter.bJIT##type##Off) \
	{Default(inst); return;}
#define PPCSTATE_OFF(elem) ((s32)STRUCT_OFF(PowerPC::ppcState, elem) - (s32)STRUCT_OFF(PowerPC::ppcState, spr[0])) 
class JitArm : public JitBase, public ArmGen::ARMXCodeBlock 
{
private:
	JitArmBlockCache blocks;

	JitArmAsmRoutineManager asm_routines;

	// TODO: Make arm specific versions of these, shouldn't be too hard to
	// make it so we allocate some space at the start(?) of code generation
	// and keep the registers in a cache. Will burn this bridge when we get to
	// it.
	ArmRegCache gpr;
	ArmFPRCache fpr;

	PPCAnalyst::CodeBuffer code_buffer;
	void DoDownCount();

	void PrintDebug(UGeckoInstruction inst, u32 level);

	void Helper_UpdateCR1(ARMReg value);
public:
	JitArm() : code_buffer(32000) {}
	~JitArm() {}

	void Init();
	void Shutdown();

	// Jit!

	void Jit(u32 em_address);
	const u8* DoJit(u32 em_address, PPCAnalyst::CodeBuffer *code_buf, JitBlock *b);
	
	JitBaseBlockCache *GetBlockCache() { return &blocks; }

	const u8 *BackPatch(u8 *codePtr, int accessType, u32 em_address, void *ctx);

	bool IsInCodeSpace(u8 *ptr) { return IsInSpace(ptr); }

	void Trace();

	void ClearCache();

	const u8 *GetDispatcher() {
		return asm_routines.dispatcher; 
	}
	CommonAsmRoutinesBase *GetAsmRoutines() {
		return &asm_routines; 
	}

	const char *GetName() {
		return "JITARM";
	}
	// Run!

	void Run();
	void SingleStep();

	// Utilities for use by opcodes

	void WriteExit(u32 destination, int exit_num);
	void WriteExitDestInR(ARMReg Reg);
	void WriteRfiExitDestInR(ARMReg Reg);
	void WriteExceptionExit();
	void WriteCallInterpreter(UGeckoInstruction _inst);
	void Cleanup();

	void GenerateRC(int cr = 0);
	void ComputeRC(int cr = 0);
	void ComputeRC(s32 value, int cr);

	void ComputeCarry();
	void GetCarryAndClear(ARMReg reg);
	void FinalizeCarry(ARMReg reg);

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

	// Breakin shit
	void Break(UGeckoInstruction _inst);
	// Branch
	void bx(UGeckoInstruction _inst);
	void bcx(UGeckoInstruction _inst);
	void bclrx(UGeckoInstruction _inst);
	void sc(UGeckoInstruction _inst);
	void rfi(UGeckoInstruction _inst);
	void bcctrx(UGeckoInstruction _inst);
		
	// Integer
	void addi(UGeckoInstruction _inst);
	void addis(UGeckoInstruction _inst);
	void addx(UGeckoInstruction _inst);
	void addcx(UGeckoInstruction _inst);
	void addex(UGeckoInstruction _inst);
	void cmp (UGeckoInstruction _inst);
	void cmpi(UGeckoInstruction _inst);
	void cmpl(UGeckoInstruction _inst);
	void cmpli(UGeckoInstruction _inst);
	void negx(UGeckoInstruction _inst);
	void mulli(UGeckoInstruction _inst);
	void mullwx(UGeckoInstruction _inst);
	void mulhwux(UGeckoInstruction _inst);
	void ori(UGeckoInstruction _inst);	
	void oris(UGeckoInstruction _inst);	
	void orx(UGeckoInstruction _inst);
	void xorx(UGeckoInstruction _inst);
	void andx(UGeckoInstruction _inst);
	void andi_rc(UGeckoInstruction _inst);
	void andis_rc(UGeckoInstruction _inst);
	void rlwimix(UGeckoInstruction _inst);
	void rlwinmx(UGeckoInstruction _inst);
	void subfx(UGeckoInstruction _inst);
	void srawix(UGeckoInstruction _inst);
	void extshx(UGeckoInstruction inst);
	void extsbx(UGeckoInstruction inst);

	// System Registers
	void mtmsr(UGeckoInstruction _inst);
	void mfmsr(UGeckoInstruction _inst);
	void mtspr(UGeckoInstruction _inst);
	void mfspr(UGeckoInstruction _inst);
	void mftb(UGeckoInstruction _inst);

	// LoadStore
	void icbi(UGeckoInstruction _inst);
	void dcbst(UGeckoInstruction _inst);
	void lbz(UGeckoInstruction _inst);
	void lhz(UGeckoInstruction _inst);
	void lha(UGeckoInstruction _inst);
	void lwz(UGeckoInstruction _inst);
	void lwzx(UGeckoInstruction _inst);
	void stb(UGeckoInstruction _inst);
	void stbu(UGeckoInstruction _inst);
	void sth(UGeckoInstruction _inst);
	void sthu(UGeckoInstruction _inst);
	void stw(UGeckoInstruction _inst);
	void stwu(UGeckoInstruction _inst);

	// Floating point
	void fabsx(UGeckoInstruction _inst);
	void faddsx(UGeckoInstruction _inst);
	void faddx(UGeckoInstruction _inst);
	void fsubsx(UGeckoInstruction _inst);
	void fsubx(UGeckoInstruction _inst);
	void fmulsx(UGeckoInstruction _inst);
	void fmulx(UGeckoInstruction _inst);
	void fmrx(UGeckoInstruction _inst);

	// Floating point loadStore
	void lfs(UGeckoInstruction _inst);
	void lfd(UGeckoInstruction _inst);

	// Paired Singles
	void ps_add(UGeckoInstruction _inst);
	void ps_sum0(UGeckoInstruction _inst);
	void ps_madd(UGeckoInstruction _inst);
	void ps_sub(UGeckoInstruction _inst);
	void ps_mul(UGeckoInstruction _inst);
};

#endif // _JIT64_H
