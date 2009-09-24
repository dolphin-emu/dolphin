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

#ifndef _INTERPRETER_H
#define _INTERPRETER_H

#include "../Gekko.h"
#include "../PowerPC.h"

namespace Interpreter
{
	void Init();
	void Shutdown();
	void Reset();	
	void SingleStep();	
	void SingleStepInner();	
	void Run();

	typedef void (*_interpreterInstruction)(UGeckoInstruction instCode);

	_interpreterInstruction GetInstruction(UGeckoInstruction instCode);

	void Log();

	// pointer to the CPU-Regs to keep the code cleaner
	extern u32* m_GPR;
	extern bool m_EndBlock;

	void unknown_instruction(UGeckoInstruction _inst);

	// Branch Instructions
	void bx(UGeckoInstruction _inst);
	void bcx(UGeckoInstruction _inst);
	void bcctrx(UGeckoInstruction _inst);
	void bclrx(UGeckoInstruction _inst);
	void HLEFunction(UGeckoInstruction _inst);
	void CompiledBlock(UGeckoInstruction _inst);

	// Syscall Instruction
	void sc(UGeckoInstruction _inst);

	// Floating Point Instructions
	void faddsx(UGeckoInstruction _inst);
	void fdivsx(UGeckoInstruction _inst);
	void fmaddsx(UGeckoInstruction _inst);
	void fmsubsx(UGeckoInstruction _inst);
	void fmulsx(UGeckoInstruction _inst);
	void fnmaddsx(UGeckoInstruction _inst);
	void fnmsubsx(UGeckoInstruction _inst);
	void fresx(UGeckoInstruction _inst);
//	void fsqrtsx(UGeckoInstruction _inst);
	void fsubsx(UGeckoInstruction _inst);
	void fabsx(UGeckoInstruction _inst);
	void fcmpo(UGeckoInstruction _inst);
	void fcmpu(UGeckoInstruction _inst);
	void fctiwx(UGeckoInstruction _inst);
	void fctiwzx(UGeckoInstruction _inst);
	void fmrx(UGeckoInstruction _inst);
	void fnabsx(UGeckoInstruction _inst);
	void fnegx(UGeckoInstruction _inst);
	void frspx(UGeckoInstruction _inst);
	void faddx(UGeckoInstruction _inst);
	void fdivx(UGeckoInstruction _inst);
	void fmaddx(UGeckoInstruction _inst);
	void fmsubx(UGeckoInstruction _inst);
	void fmulx(UGeckoInstruction _inst);
	void fnmaddx(UGeckoInstruction _inst);
	void fnmsubx(UGeckoInstruction _inst);
	void frsqrtex(UGeckoInstruction _inst);
	void fselx(UGeckoInstruction _inst);
	void fsqrtx(UGeckoInstruction _inst);
	void fsubx(UGeckoInstruction _inst);

	// Integer Instructions
	void addi(UGeckoInstruction _inst);
	void addic(UGeckoInstruction _inst);
	void addic_rc(UGeckoInstruction _inst);
	void addis(UGeckoInstruction _inst);
	void andi_rc(UGeckoInstruction _inst);
	void andis_rc(UGeckoInstruction _inst);
	void cmpi(UGeckoInstruction _inst);
	void cmpli(UGeckoInstruction _inst);
	void mulli(UGeckoInstruction _inst);
	void ori(UGeckoInstruction _inst);
	void oris(UGeckoInstruction _inst);
	void subfic(UGeckoInstruction _inst);
	void twi(UGeckoInstruction _inst);
	void xori(UGeckoInstruction _inst);
	void xoris(UGeckoInstruction _inst);
	void rlwimix(UGeckoInstruction _inst);
	void rlwinmx(UGeckoInstruction _inst);
	void rlwnmx(UGeckoInstruction _inst);
	void andx(UGeckoInstruction _inst);
	void andcx(UGeckoInstruction _inst);
	void cmp(UGeckoInstruction _inst);
	void cmpl(UGeckoInstruction _inst);
	void cntlzwx(UGeckoInstruction _inst);
	void eqvx(UGeckoInstruction _inst);
	void extsbx(UGeckoInstruction _inst);
	void extshx(UGeckoInstruction _inst);
	void nandx(UGeckoInstruction _inst);
	void norx(UGeckoInstruction _inst);
	void orx(UGeckoInstruction _inst);
	void orcx(UGeckoInstruction _inst);
	void slwx(UGeckoInstruction _inst);
	void srawx(UGeckoInstruction _inst);
	void srawix(UGeckoInstruction _inst);
	void srwx(UGeckoInstruction _inst);
	void tw(UGeckoInstruction _inst);
	void xorx(UGeckoInstruction _inst);
	void addx(UGeckoInstruction _inst);
	void addcx(UGeckoInstruction _inst);
	void addex(UGeckoInstruction _inst);
	void addmex(UGeckoInstruction _inst);
	void addzex(UGeckoInstruction _inst);
	void divwx(UGeckoInstruction _inst);
	void divwux(UGeckoInstruction _inst);
	void mulhwx(UGeckoInstruction _inst);
	void mulhwux(UGeckoInstruction _inst);
	void mullwx(UGeckoInstruction _inst);
	void negx(UGeckoInstruction _inst);
	void subfx(UGeckoInstruction _inst);
	void subfcx(UGeckoInstruction _inst);
	void subfex(UGeckoInstruction _inst);
	void subfmex(UGeckoInstruction _inst);
	void subfzex(UGeckoInstruction _inst);

	// Load/Store Instructions
	void lbz(UGeckoInstruction _inst);
	void lbzu(UGeckoInstruction _inst);
	void lfd(UGeckoInstruction _inst);
	void lfdu(UGeckoInstruction _inst);
	void lfs(UGeckoInstruction _inst);
	void lfsu(UGeckoInstruction _inst);
	void lha(UGeckoInstruction _inst);
	void lhau(UGeckoInstruction _inst);
	void lhz(UGeckoInstruction _inst);
	void lhzu(UGeckoInstruction _inst);
	void lmw(UGeckoInstruction _inst);
	void lwz(UGeckoInstruction _inst);
	void lwzu(UGeckoInstruction _inst);
	void stb(UGeckoInstruction _inst);
	void stbu(UGeckoInstruction _inst);
	void stfd(UGeckoInstruction _inst);
	void stfdu(UGeckoInstruction _inst);
	void stfs(UGeckoInstruction _inst);
	void stfsu(UGeckoInstruction _inst);
	void sth(UGeckoInstruction _inst);
	void sthu(UGeckoInstruction _inst);
	void stmw(UGeckoInstruction _inst);
	void stw(UGeckoInstruction _inst);
	void stwu(UGeckoInstruction _inst);
	void dcba(UGeckoInstruction _inst);
	void dcbf(UGeckoInstruction _inst);
	void dcbi(UGeckoInstruction _inst);
	void dcbst(UGeckoInstruction _inst);
	void dcbt(UGeckoInstruction _inst);
	void dcbtst(UGeckoInstruction _inst);
	void dcbz(UGeckoInstruction _inst);
	void eciwx(UGeckoInstruction _inst);
	void ecowx(UGeckoInstruction _inst);
	void eieio(UGeckoInstruction _inst);
	void icbi(UGeckoInstruction _inst);
	void lbzux(UGeckoInstruction _inst);
	void lbzx(UGeckoInstruction _inst);
	void lfdux(UGeckoInstruction _inst);
	void lfdx(UGeckoInstruction _inst);
	void lfsux(UGeckoInstruction _inst);
	void lfsx(UGeckoInstruction _inst);
	void lhaux(UGeckoInstruction _inst);
	void lhax(UGeckoInstruction _inst);
	void lhbrx(UGeckoInstruction _inst);
	void lhzux(UGeckoInstruction _inst);
	void lhzx(UGeckoInstruction _inst);
	void lswi(UGeckoInstruction _inst);
	void lswx(UGeckoInstruction _inst);
	void lwarx(UGeckoInstruction _inst);
	void lwbrx(UGeckoInstruction _inst);
	void lwzux(UGeckoInstruction _inst);
	void lwzx(UGeckoInstruction _inst);
	void stbux(UGeckoInstruction _inst);
	void stbx(UGeckoInstruction _inst);
	void stfdux(UGeckoInstruction _inst);
	void stfdx(UGeckoInstruction _inst);
	void stfiwx(UGeckoInstruction _inst);
	void stfsux(UGeckoInstruction _inst);
	void stfsx(UGeckoInstruction _inst);
	void sthbrx(UGeckoInstruction _inst);
	void sthux(UGeckoInstruction _inst);
	void sthx(UGeckoInstruction _inst);
	void stswi(UGeckoInstruction _inst);
	void stswx(UGeckoInstruction _inst);
	void stwbrx(UGeckoInstruction _inst);
	void stwcxd(UGeckoInstruction _inst);
	void stwux(UGeckoInstruction _inst);
	void stwx(UGeckoInstruction _inst);
	void sync(UGeckoInstruction _inst);
	void tlbia(UGeckoInstruction _inst);
	void tlbie(UGeckoInstruction _inst);
	void tlbsync(UGeckoInstruction _inst);

	// Paired Instructions
	void psq_l(UGeckoInstruction _inst);
	void psq_lu(UGeckoInstruction _inst);
	void psq_st(UGeckoInstruction _inst);
	void psq_stu(UGeckoInstruction _inst);
	void psq_lx(UGeckoInstruction _inst);
	void psq_stx(UGeckoInstruction _inst);
	void psq_lux(UGeckoInstruction _inst);
	void psq_stux(UGeckoInstruction _inst);
	void ps_div(UGeckoInstruction _inst);
	void ps_sub(UGeckoInstruction _inst);
	void ps_add(UGeckoInstruction _inst);
	void ps_sel(UGeckoInstruction _inst);
	void ps_res(UGeckoInstruction _inst);
	void ps_mul(UGeckoInstruction _inst);
	void ps_rsqrte(UGeckoInstruction _inst);
	void ps_msub(UGeckoInstruction _inst);
	void ps_madd(UGeckoInstruction _inst);
	void ps_nmsub(UGeckoInstruction _inst);
	void ps_nmadd(UGeckoInstruction _inst);
	void ps_neg(UGeckoInstruction _inst);
	void ps_mr(UGeckoInstruction _inst);
	void ps_nabs(UGeckoInstruction _inst);
	void ps_abs(UGeckoInstruction _inst);
	void ps_sum0(UGeckoInstruction _inst);
	void ps_sum1(UGeckoInstruction _inst);
	void ps_muls0(UGeckoInstruction _inst);
	void ps_muls1(UGeckoInstruction _inst);
	void ps_madds0(UGeckoInstruction _inst);
	void ps_madds1(UGeckoInstruction _inst);
	void ps_cmpu0(UGeckoInstruction _inst);
	void ps_cmpo0(UGeckoInstruction _inst);
	void ps_cmpu1(UGeckoInstruction _inst);
	void ps_cmpo1(UGeckoInstruction _inst);
	void ps_merge00(UGeckoInstruction _inst);
	void ps_merge01(UGeckoInstruction _inst);
	void ps_merge10(UGeckoInstruction _inst);
	void ps_merge11(UGeckoInstruction _inst);
	void dcbz_l(UGeckoInstruction _inst);

	// System Registers Instructions
	void mcrfs(UGeckoInstruction _inst);
	void mffsx(UGeckoInstruction _inst);
	void mtfsb0x(UGeckoInstruction _inst);
	void mtfsb1x(UGeckoInstruction _inst);
	void mtfsfix(UGeckoInstruction _inst);
	void mtfsfx(UGeckoInstruction _inst);
	void mcrxr(UGeckoInstruction _inst);
	void mfcr(UGeckoInstruction _inst);
	void mfmsr(UGeckoInstruction _inst);
	void mfsr(UGeckoInstruction _inst);
	void mfsrin(UGeckoInstruction _inst);
	void mtmsr(UGeckoInstruction _inst);
	void mtsr(UGeckoInstruction _inst);
	void mtsrin(UGeckoInstruction _inst);
	void mfspr(UGeckoInstruction _inst);
	void mftb(UGeckoInstruction _inst);
	void mtcrf(UGeckoInstruction _inst);
	void mtspr(UGeckoInstruction _inst);
	void crand(UGeckoInstruction _inst);
	void crandc(UGeckoInstruction _inst);
	void creqv(UGeckoInstruction _inst);
	void crnand(UGeckoInstruction _inst);
	void crnor(UGeckoInstruction _inst);
	void cror(UGeckoInstruction _inst);
	void crorc(UGeckoInstruction _inst);
	void crxor(UGeckoInstruction _inst);
	void mcrf(UGeckoInstruction _inst);
	void rfi(UGeckoInstruction _inst);
	void rfid(UGeckoInstruction _inst);	
//   void sync(UGeckoInstruction _inst);
	void isync(UGeckoInstruction _inst);

	void RunTable4(UGeckoInstruction _instCode);
	void RunTable19(UGeckoInstruction _instCode);
	void RunTable31(UGeckoInstruction _instCode);
	void RunTable59(UGeckoInstruction _instCode);
	void RunTable63(UGeckoInstruction _instCode);

	// flag helper
	void Helper_UpdateCR0(u32 _uValue);
	void Helper_UpdateCR1(double _fValue);
	void Helper_UpdateCR1(float _fValue);
	void Helper_UpdateCRx(int _x, u32 _uValue);
	u32 Helper_Carry(u32 _uValue1, u32 _uValue2);

	// address helper
	u32 Helper_Get_EA   (const UGeckoInstruction _inst);
	u32 Helper_Get_EA_U (const UGeckoInstruction _inst);
	u32 Helper_Get_EA_X (const UGeckoInstruction _inst);
	u32 Helper_Get_EA_UX(const UGeckoInstruction _inst);

	// paired helper
	float Helper_Dequantize(const u32 _Addr, const EQuantizeType _quantizeType, const unsigned int _uScale);
	void  Helper_Quantize  (const u32 _Addr, const double _fValue, const EQuantizeType _quantizeType, const unsigned _uScale);

	// other helper
	u32 Helper_Mask(int mb, int me);

	extern _interpreterInstruction m_opTable[64];
	extern _interpreterInstruction m_opTable4[1024];
	extern _interpreterInstruction m_opTable19[1024];
	extern _interpreterInstruction m_opTable31[1024];
	extern _interpreterInstruction m_opTable59[32];
	extern _interpreterInstruction m_opTable63[1024];
};

#endif
