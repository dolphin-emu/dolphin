// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/JitILCommon/IR.h"

class JitILBase : public Jitx86Base
{
protected:
	// The default code buffer. We keep it around to not have to alloc/dealloc a
	// large chunk of memory for each recompiled block.
	PPCAnalyst::CodeBuffer code_buffer;
public:
	JitILBase() : code_buffer(32000) {}
	~JitILBase() {}

	IREmitter::IRBuilder ibuild;

	virtual void Jit(u32 em_address) = 0;

	virtual const CommonAsmRoutinesBase *GetAsmRoutines() = 0;

	// OPCODES
	virtual void FallBackToInterpreter(UGeckoInstruction inst) = 0;
	virtual void DoNothing(UGeckoInstruction inst) = 0;
	virtual void HLEFunction(UGeckoInstruction inst) = 0;

	virtual void DynaRunTable4(UGeckoInstruction _inst) = 0;
	virtual void DynaRunTable19(UGeckoInstruction _inst) = 0;
	virtual void DynaRunTable31(UGeckoInstruction _inst) = 0;
	virtual void DynaRunTable59(UGeckoInstruction _inst) = 0;
	virtual void DynaRunTable63(UGeckoInstruction _inst) = 0;

	// Branches
	void sc(UGeckoInstruction inst);
	void rfi(UGeckoInstruction inst);
	void bx(UGeckoInstruction inst);
	void bcx(UGeckoInstruction inst);
	void bcctrx(UGeckoInstruction inst);
	void bclrx(UGeckoInstruction inst);

	// LoadStore
	void lXzx(UGeckoInstruction inst);
	void lhax(UGeckoInstruction inst);
	void lhaux(UGeckoInstruction inst);
	void stXx(UGeckoInstruction inst);
	void lmw(UGeckoInstruction inst);
	void stmw(UGeckoInstruction inst);
	void stX(UGeckoInstruction inst); //stw sth stb
	void lXz(UGeckoInstruction inst);
	void lbzu(UGeckoInstruction inst);
	void lha(UGeckoInstruction inst);
	void lhau(UGeckoInstruction inst);

	// System Registers
	void mtspr(UGeckoInstruction inst);
	void mfspr(UGeckoInstruction inst);
	void mtmsr(UGeckoInstruction inst);
	void mfmsr(UGeckoInstruction inst);
	void mftb(UGeckoInstruction inst);
	void mtcrf(UGeckoInstruction inst);
	void mfcr(UGeckoInstruction inst);
	void mcrf(UGeckoInstruction inst);
	void crXX(UGeckoInstruction inst);

	void dcbst(UGeckoInstruction inst);
	void dcbz(UGeckoInstruction inst);
	void icbi(UGeckoInstruction inst);

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

	void reg_imm(UGeckoInstruction inst);

	void ps_arith(UGeckoInstruction inst); //aggregate
	void ps_mergeXX(UGeckoInstruction inst);
	void ps_maddXX(UGeckoInstruction inst);
	void ps_sum(UGeckoInstruction inst);
	void ps_muls(UGeckoInstruction inst);

	void fp_arith_s(UGeckoInstruction inst);

	void fcmpX(UGeckoInstruction inst);
	void fmrx(UGeckoInstruction inst);

	void cmpXX(UGeckoInstruction inst);

	void cntlzwx(UGeckoInstruction inst);

	void lfs(UGeckoInstruction inst);
	void lfsu(UGeckoInstruction inst);
	void lfd(UGeckoInstruction inst);
	void lfdu(UGeckoInstruction inst);
	void stfd(UGeckoInstruction inst);
	void stfs(UGeckoInstruction inst);
	void stfsx(UGeckoInstruction inst);
	void psq_l(UGeckoInstruction inst);
	void psq_st(UGeckoInstruction inst);

	void fmaddXX(UGeckoInstruction inst);
	void fsign(UGeckoInstruction inst);
	void rlwinmx(UGeckoInstruction inst);
	void rlwimix(UGeckoInstruction inst);
	void rlwnmx(UGeckoInstruction inst);
	void negx(UGeckoInstruction inst);
	void slwx(UGeckoInstruction inst);
	void srwx(UGeckoInstruction inst);
	void lfsx(UGeckoInstruction inst);

	void subfic(UGeckoInstruction inst);
	void subfcx(UGeckoInstruction inst);
	void subfx(UGeckoInstruction inst);
	void subfex(UGeckoInstruction inst);

};
