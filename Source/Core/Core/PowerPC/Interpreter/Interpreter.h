// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/Atomic.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/CPU.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/CPUCoreBase.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PowerPC.h"

class Interpreter : public CPUCoreBase
{
public:
	void Init() override;
	void Shutdown() override;
	void Reset();
	void SingleStep() override;
	int SingleStepInner();

	void Run() override;
	void ClearCache() override;
	const char *GetName() override;

	typedef void (*_interpreterInstruction)(UGeckoInstruction instCode);

	_interpreterInstruction GetInstruction(UGeckoInstruction instCode);

	void Log();

	static bool m_EndBlock;

	static void unknown_instruction(UGeckoInstruction _inst);

	// Branch Instructions
	static void bx(UGeckoInstruction _inst);
	static void bcx(UGeckoInstruction _inst);
	static void bcctrx(UGeckoInstruction _inst);
	static void bclrx(UGeckoInstruction _inst);
	static void HLEFunction(UGeckoInstruction _inst);
	static void CompiledBlock(UGeckoInstruction _inst);

	// Syscall Instruction
	static void sc(UGeckoInstruction _inst);

	// Floating Point Instructions
	static void faddsx(UGeckoInstruction _inst);
	static void fdivsx(UGeckoInstruction _inst);
	static void fmaddsx(UGeckoInstruction _inst);
	static void fmsubsx(UGeckoInstruction _inst);
	static void fmulsx(UGeckoInstruction _inst);
	static void fnmaddsx(UGeckoInstruction _inst);
	static void fnmsubsx(UGeckoInstruction _inst);
	static void fresx(UGeckoInstruction _inst);
	//static void fsqrtsx(UGeckoInstruction _inst);
	static void fsubsx(UGeckoInstruction _inst);
	static void fabsx(UGeckoInstruction _inst);
	static void fcmpo(UGeckoInstruction _inst);
	static void fcmpu(UGeckoInstruction _inst);
	static void fctiwx(UGeckoInstruction _inst);
	static void fctiwzx(UGeckoInstruction _inst);
	static void fmrx(UGeckoInstruction _inst);
	static void fnabsx(UGeckoInstruction _inst);
	static void fnegx(UGeckoInstruction _inst);
	static void frspx(UGeckoInstruction _inst);
	static void faddx(UGeckoInstruction _inst);
	static void fdivx(UGeckoInstruction _inst);
	static void fmaddx(UGeckoInstruction _inst);
	static void fmsubx(UGeckoInstruction _inst);
	static void fmulx(UGeckoInstruction _inst);
	static void fnmaddx(UGeckoInstruction _inst);
	static void fnmsubx(UGeckoInstruction _inst);
	static void frsqrtex(UGeckoInstruction _inst);
	static void fselx(UGeckoInstruction _inst);
	static void fsqrtx(UGeckoInstruction _inst);
	static void fsubx(UGeckoInstruction _inst);

	// Integer Instructions
	static void addi(UGeckoInstruction _inst);
	static void addic(UGeckoInstruction _inst);
	static void addic_rc(UGeckoInstruction _inst);
	static void addis(UGeckoInstruction _inst);
	static void andi_rc(UGeckoInstruction _inst);
	static void andis_rc(UGeckoInstruction _inst);
	static void cmpi(UGeckoInstruction _inst);
	static void cmpli(UGeckoInstruction _inst);
	static void mulli(UGeckoInstruction _inst);
	static void ori(UGeckoInstruction _inst);
	static void oris(UGeckoInstruction _inst);
	static void subfic(UGeckoInstruction _inst);
	static void twi(UGeckoInstruction _inst);
	static void xori(UGeckoInstruction _inst);
	static void xoris(UGeckoInstruction _inst);
	static void rlwimix(UGeckoInstruction _inst);
	static void rlwinmx(UGeckoInstruction _inst);
	static void rlwnmx(UGeckoInstruction _inst);
	static void andx(UGeckoInstruction _inst);
	static void andcx(UGeckoInstruction _inst);
	static void cmp(UGeckoInstruction _inst);
	static void cmpl(UGeckoInstruction _inst);
	static void cntlzwx(UGeckoInstruction _inst);
	static void eqvx(UGeckoInstruction _inst);
	static void extsbx(UGeckoInstruction _inst);
	static void extshx(UGeckoInstruction _inst);
	static void nandx(UGeckoInstruction _inst);
	static void norx(UGeckoInstruction _inst);
	static void orx(UGeckoInstruction _inst);
	static void orcx(UGeckoInstruction _inst);
	static void slwx(UGeckoInstruction _inst);
	static void srawx(UGeckoInstruction _inst);
	static void srawix(UGeckoInstruction _inst);
	static void srwx(UGeckoInstruction _inst);
	static void tw(UGeckoInstruction _inst);
	static void xorx(UGeckoInstruction _inst);
	static void addx(UGeckoInstruction _inst);
	static void addcx(UGeckoInstruction _inst);
	static void addex(UGeckoInstruction _inst);
	static void addmex(UGeckoInstruction _inst);
	static void addzex(UGeckoInstruction _inst);
	static void divwx(UGeckoInstruction _inst);
	static void divwux(UGeckoInstruction _inst);
	static void mulhwx(UGeckoInstruction _inst);
	static void mulhwux(UGeckoInstruction _inst);
	static void mullwx(UGeckoInstruction _inst);
	static void negx(UGeckoInstruction _inst);
	static void subfx(UGeckoInstruction _inst);
	static void subfcx(UGeckoInstruction _inst);
	static void subfex(UGeckoInstruction _inst);
	static void subfmex(UGeckoInstruction _inst);
	static void subfzex(UGeckoInstruction _inst);

	// Load/Store Instructions
	static void lbz(UGeckoInstruction _inst);
	static void lbzu(UGeckoInstruction _inst);
	static void lfd(UGeckoInstruction _inst);
	static void lfdu(UGeckoInstruction _inst);
	static void lfs(UGeckoInstruction _inst);
	static void lfsu(UGeckoInstruction _inst);
	static void lha(UGeckoInstruction _inst);
	static void lhau(UGeckoInstruction _inst);
	static void lhz(UGeckoInstruction _inst);
	static void lhzu(UGeckoInstruction _inst);
	static void lmw(UGeckoInstruction _inst);
	static void lwz(UGeckoInstruction _inst);
	static void lwzu(UGeckoInstruction _inst);
	static void stb(UGeckoInstruction _inst);
	static void stbu(UGeckoInstruction _inst);
	static void stfd(UGeckoInstruction _inst);
	static void stfdu(UGeckoInstruction _inst);
	static void stfs(UGeckoInstruction _inst);
	static void stfsu(UGeckoInstruction _inst);
	static void sth(UGeckoInstruction _inst);
	static void sthu(UGeckoInstruction _inst);
	static void stmw(UGeckoInstruction _inst);
	static void stw(UGeckoInstruction _inst);
	static void stwu(UGeckoInstruction _inst);
	static void dcba(UGeckoInstruction _inst);
	static void dcbf(UGeckoInstruction _inst);
	static void dcbi(UGeckoInstruction _inst);
	static void dcbst(UGeckoInstruction _inst);
	static void dcbt(UGeckoInstruction _inst);
	static void dcbtst(UGeckoInstruction _inst);
	static void dcbz(UGeckoInstruction _inst);
	static void eciwx(UGeckoInstruction _inst);
	static void ecowx(UGeckoInstruction _inst);
	static void eieio(UGeckoInstruction _inst);
	static void icbi(UGeckoInstruction _inst);
	static void lbzux(UGeckoInstruction _inst);
	static void lbzx(UGeckoInstruction _inst);
	static void lfdux(UGeckoInstruction _inst);
	static void lfdx(UGeckoInstruction _inst);
	static void lfsux(UGeckoInstruction _inst);
	static void lfsx(UGeckoInstruction _inst);
	static void lhaux(UGeckoInstruction _inst);
	static void lhax(UGeckoInstruction _inst);
	static void lhbrx(UGeckoInstruction _inst);
	static void lhzux(UGeckoInstruction _inst);
	static void lhzx(UGeckoInstruction _inst);
	static void lswi(UGeckoInstruction _inst);
	static void lswx(UGeckoInstruction _inst);
	static void lwarx(UGeckoInstruction _inst);
	static void lwbrx(UGeckoInstruction _inst);
	static void lwzux(UGeckoInstruction _inst);
	static void lwzx(UGeckoInstruction _inst);
	static void stbux(UGeckoInstruction _inst);
	static void stbx(UGeckoInstruction _inst);
	static void stfdux(UGeckoInstruction _inst);
	static void stfdx(UGeckoInstruction _inst);
	static void stfiwx(UGeckoInstruction _inst);
	static void stfsux(UGeckoInstruction _inst);
	static void stfsx(UGeckoInstruction _inst);
	static void sthbrx(UGeckoInstruction _inst);
	static void sthux(UGeckoInstruction _inst);
	static void sthx(UGeckoInstruction _inst);
	static void stswi(UGeckoInstruction _inst);
	static void stswx(UGeckoInstruction _inst);
	static void stwbrx(UGeckoInstruction _inst);
	static void stwcxd(UGeckoInstruction _inst);
	static void stwux(UGeckoInstruction _inst);
	static void stwx(UGeckoInstruction _inst);
	static void tlbia(UGeckoInstruction _inst);
	static void tlbie(UGeckoInstruction _inst);
	static void tlbsync(UGeckoInstruction _inst);

	// Paired Instructions
	static void psq_l(UGeckoInstruction _inst);
	static void psq_lu(UGeckoInstruction _inst);
	static void psq_st(UGeckoInstruction _inst);
	static void psq_stu(UGeckoInstruction _inst);
	static void psq_lx(UGeckoInstruction _inst);
	static void psq_stx(UGeckoInstruction _inst);
	static void psq_lux(UGeckoInstruction _inst);
	static void psq_stux(UGeckoInstruction _inst);
	static void ps_div(UGeckoInstruction _inst);
	static void ps_sub(UGeckoInstruction _inst);
	static void ps_add(UGeckoInstruction _inst);
	static void ps_sel(UGeckoInstruction _inst);
	static void ps_res(UGeckoInstruction _inst);
	static void ps_mul(UGeckoInstruction _inst);
	static void ps_rsqrte(UGeckoInstruction _inst);
	static void ps_msub(UGeckoInstruction _inst);
	static void ps_madd(UGeckoInstruction _inst);
	static void ps_nmsub(UGeckoInstruction _inst);
	static void ps_nmadd(UGeckoInstruction _inst);
	static void ps_neg(UGeckoInstruction _inst);
	static void ps_mr(UGeckoInstruction _inst);
	static void ps_nabs(UGeckoInstruction _inst);
	static void ps_abs(UGeckoInstruction _inst);
	static void ps_sum0(UGeckoInstruction _inst);
	static void ps_sum1(UGeckoInstruction _inst);
	static void ps_muls0(UGeckoInstruction _inst);
	static void ps_muls1(UGeckoInstruction _inst);
	static void ps_madds0(UGeckoInstruction _inst);
	static void ps_madds1(UGeckoInstruction _inst);
	static void ps_cmpu0(UGeckoInstruction _inst);
	static void ps_cmpo0(UGeckoInstruction _inst);
	static void ps_cmpu1(UGeckoInstruction _inst);
	static void ps_cmpo1(UGeckoInstruction _inst);
	static void ps_merge00(UGeckoInstruction _inst);
	static void ps_merge01(UGeckoInstruction _inst);
	static void ps_merge10(UGeckoInstruction _inst);
	static void ps_merge11(UGeckoInstruction _inst);
	static void dcbz_l(UGeckoInstruction _inst);

	// System Registers Instructions
	static void mcrfs(UGeckoInstruction _inst);
	static void mffsx(UGeckoInstruction _inst);
	static void mtfsb0x(UGeckoInstruction _inst);
	static void mtfsb1x(UGeckoInstruction _inst);
	static void mtfsfix(UGeckoInstruction _inst);
	static void mtfsfx(UGeckoInstruction _inst);
	static void mcrxr(UGeckoInstruction _inst);
	static void mfcr(UGeckoInstruction _inst);
	static void mfmsr(UGeckoInstruction _inst);
	static void mfsr(UGeckoInstruction _inst);
	static void mfsrin(UGeckoInstruction _inst);
	static void mtmsr(UGeckoInstruction _inst);
	static void mtsr(UGeckoInstruction _inst);
	static void mtsrin(UGeckoInstruction _inst);
	static void mfspr(UGeckoInstruction _inst);
	static void mftb(UGeckoInstruction _inst);
	static void mtcrf(UGeckoInstruction _inst);
	static void mtspr(UGeckoInstruction _inst);
	static void crand(UGeckoInstruction _inst);
	static void crandc(UGeckoInstruction _inst);
	static void creqv(UGeckoInstruction _inst);
	static void crnand(UGeckoInstruction _inst);
	static void crnor(UGeckoInstruction _inst);
	static void cror(UGeckoInstruction _inst);
	static void crorc(UGeckoInstruction _inst);
	static void crxor(UGeckoInstruction _inst);
	static void mcrf(UGeckoInstruction _inst);
	static void rfi(UGeckoInstruction _inst);
	static void rfid(UGeckoInstruction _inst);
	static void sync(UGeckoInstruction _inst);
	static void isync(UGeckoInstruction _inst);

	static _interpreterInstruction m_opTable[64];
	static _interpreterInstruction m_opTable4[1024];
	static _interpreterInstruction m_opTable19[1024];
	static _interpreterInstruction m_opTable31[1024];
	static _interpreterInstruction m_opTable59[32];
	static _interpreterInstruction m_opTable63[1024];

	// singleton
	static Interpreter* getInstance();

	static void RunTable4(UGeckoInstruction _instCode);
	static void RunTable19(UGeckoInstruction _instCode);
	static void RunTable31(UGeckoInstruction _instCode);
	static void RunTable59(UGeckoInstruction _instCode);
	static void RunTable63(UGeckoInstruction _instCode);

	static u32 Helper_Carry(u32 _uValue1, u32 _uValue2);

private:
	// flag helper
	static void Helper_UpdateCR0(u32 _uValue);
	static void Helper_UpdateCR1();
	static void Helper_UpdateCRx(int _x, u32 _uValue);

	// address helper
	static u32 Helper_Get_EA   (const UGeckoInstruction _inst);
	static u32 Helper_Get_EA_U (const UGeckoInstruction _inst);
	static u32 Helper_Get_EA_X (const UGeckoInstruction _inst);
	static u32 Helper_Get_EA_UX(const UGeckoInstruction _inst);

	// paired helper
	static float Helper_Dequantize(const u32 _Addr, const EQuantizeType _quantizeType, const unsigned int _uScale);
	static void  Helper_Quantize  (const u32 _Addr, const double _fValue, const EQuantizeType _quantizeType, const unsigned _uScale);

	// other helper
	static u32 Helper_Mask(int mb, int me);

	static void Helper_FloatCompareOrdered(UGeckoInstruction _inst, double a, double b);
	static void Helper_FloatCompareUnordered(UGeckoInstruction _inst, double a, double b);

	// TODO: These should really be in the save state, although it's unlikely to matter much.
	// They are for lwarx and its friend stwcxd.
	static bool g_bReserve;
	static u32  g_reserveAddr;
};
