// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Common/CPUDetect.h"

#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/JitAsm.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"

using namespace Gen;

// TODO: Add peephole optimizations for multiple consecutive lfd/lfs/stfd/stfs since they are so common,
// and pshufb could help a lot.

void Jit64::lfXXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff);
	bool indexed = inst.OPCD == 31;
	bool update = indexed ? !!(inst.SUBOP10 & 0x20) : !!(inst.OPCD & 1);
	bool single = indexed ? !(inst.SUBOP10 & 0x40) : !(inst.OPCD & 2);
	update &= indexed || inst.SIMM_16;

	int d = inst.RD;
	int a = inst.RA;
	int b = inst.RB;

	FALLBACK_IF(!indexed && !a);

	gpr.BindToRegister(a, true, update);

	s32 offset = 0;
	OpArg addr = gpr.R(a);
	if (update && js.memcheck)
	{
		addr = R(RSCRATCH2);
		MOV(32, addr, gpr.R(a));
	}
	if (indexed)
	{
		if (update)
		{
			ADD(32, addr, gpr.R(b));
		}
		else
		{
			addr = R(RSCRATCH);
			if (a && gpr.R(a).IsSimpleReg() && gpr.R(b).IsSimpleReg())
				LEA(32, RSCRATCH, MComplex(gpr.RX(a), gpr.RX(b), SCALE_1, 0));
			else
			{
				MOV(32, addr, gpr.R(b));
				if (a)
					ADD(32, addr, gpr.R(a));
			}
		}
	}
	else
	{
		if (update)
			ADD(32, addr, Imm32((s32)(s16)inst.SIMM_16));
		else
			offset = (s16)inst.SIMM_16;
	}

	BitSet32 registersInUse = CallerSavedRegistersInUse();
	if (update && js.memcheck)
		registersInUse[RSCRATCH2] = true;
	SafeLoadToReg(RSCRATCH, addr, single ? 32 : 64, offset, registersInUse, false);
	fpr.Lock(d);
	fpr.BindToRegister(d, js.memcheck || !single);

	MEMCHECK_START(false)
	if (single)
	{
		ConvertSingleToDouble(fpr.RX(d), RSCRATCH, true);
	}
	else
	{
		MOVQ_xmm(XMM0, R(RSCRATCH));
		MOVSD(fpr.RX(d), R(XMM0));
	}
	if (update && js.memcheck)
		MOV(32, gpr.R(a), addr);
	MEMCHECK_END
	fpr.UnlockAll();
	gpr.UnlockAll();
}

void Jit64::stfXXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff);
	bool indexed = inst.OPCD == 31;
	bool update = indexed ? !!(inst.SUBOP10&0x20) : !!(inst.OPCD&1);
	bool single = indexed ? !(inst.SUBOP10&0x40) : !(inst.OPCD&2);
	update &= indexed || inst.SIMM_16;

	int s = inst.RS;
	int a = inst.RA;
	int b = inst.RB;

	FALLBACK_IF((!indexed && !a) || (update && js.memcheck && a == b));

	s32 offset = 0;
	s32 imm = (s16)inst.SIMM_16;
	if (indexed)
	{
		if (update)
		{
			gpr.BindToRegister(a, true, true);
			ADD(32, gpr.R(a), gpr.R(b));
			MOV(32, R(RSCRATCH2), gpr.R(a));
		}
		else
		{
			if (a && gpr.R(a).IsSimpleReg() && gpr.R(b).IsSimpleReg())
				LEA(32, RSCRATCH2, MComplex(gpr.RX(a), gpr.RX(b), SCALE_1, 0));
			else
			{
				MOV(32, R(RSCRATCH2), gpr.R(b));
				if (a)
					ADD(32, R(RSCRATCH2), gpr.R(a));
			}
		}
	}
	else
	{
		if (update)
		{
			gpr.BindToRegister(a, true, true);
			ADD(32, gpr.R(a), Imm32(imm));
		}
		else
		{
			offset = imm;
		}
		MOV(32, R(RSCRATCH2), gpr.R(a));
	}

	if (single)
	{
		fpr.BindToRegister(s, true, false);
		ConvertDoubleToSingle(XMM0, fpr.RX(s));
		SafeWriteF32ToReg(XMM0, RSCRATCH2, offset, CallerSavedRegistersInUse());
		fpr.UnlockAll();
	}
	else
	{
		if (fpr.R(s).IsSimpleReg())
			MOVQ_xmm(R(RSCRATCH), fpr.RX(s));
		else
			MOV(64, R(RSCRATCH), fpr.R(s));
		SafeWriteRegToReg(RSCRATCH, RSCRATCH2, 64, offset, CallerSavedRegistersInUse());
	}
	if (js.memcheck && update)
	{
		// revert the address change if an exception occurred
		MEMCHECK_START(true)
		SUB(32, gpr.R(a), indexed ? gpr.R(b) : Imm32(imm));
		MEMCHECK_END
	}
	gpr.UnlockAll();
	gpr.UnlockAllX();
}

// This one is a little bit weird; it stores the low 32 bits of a double without converting it
void Jit64::stfiwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff);

	int s = inst.RS;
	int a = inst.RA;
	int b = inst.RB;

	MOV(32, R(RSCRATCH2), gpr.R(b));
	if (a)
		ADD(32, R(RSCRATCH2), gpr.R(a));

	if (fpr.R(s).IsSimpleReg())
		MOVD_xmm(R(RSCRATCH), fpr.RX(s));
	else
		MOV(32, R(RSCRATCH), fpr.R(s));
	SafeWriteRegToReg(RSCRATCH, RSCRATCH2, 32, 0, CallerSavedRegistersInUse());
	gpr.UnlockAllX();
}
