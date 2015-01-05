// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// TODO(ector): Tons of pshufb optimization of the loads/stores, for SSSE3+, possibly SSE4, only.
// Should give a very noticeable speed boost to paired single heavy code.

#include "Common/CommonTypes.h"
#include "Common/CPUDetect.h"

#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/JitAsm.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"

using namespace Gen;

// The big problem is likely instructions that set the quantizers in the same block.
// We will have to break block after quantizers are written to.
void Jit64::psq_stXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStorePairedOff);
	FALLBACK_IF(!inst.RA);

	s32 offset = inst.SIMM_12;
	bool indexed = inst.OPCD == 4;
	bool update = (inst.OPCD == 61 && offset) || (inst.OPCD == 4 && !!(inst.SUBOP6 & 32));
	int a = inst.RA;
	int b = indexed ? inst.RB : a;
	int s = inst.FS;
	int i = indexed ? inst.Ix : inst.I;
	int w = indexed ? inst.Wx : inst.W;

	gpr.Lock(a, b);
	if (update)
		gpr.BindToRegister(a, true, true);
	fpr.BindToRegister(s, true, false);
	if (gpr.R(a).IsSimpleReg() && gpr.R(b).IsSimpleReg() && (indexed || offset))
	{
		if (indexed)
			LEA(32, RSCRATCH2, MComplex(gpr.RX(a), gpr.RX(b), SCALE_1, 0));
		else
			LEA(32, RSCRATCH2, MDisp(gpr.RX(a), offset));
	}
	else
	{
		MOV(32, R(RSCRATCH2), gpr.R(a));
		if (indexed)
			ADD(32, R(RSCRATCH2), gpr.R(b));
		else if (offset)
			ADD(32, R(RSCRATCH2), Imm32((u32)offset));
	}
	// In memcheck mode, don't update the address until the exception check
	if (update && !js.memcheck)
		MOV(32, gpr.R(a), R(RSCRATCH2));
	MOVZX(32, 8, RSCRATCH, PPCSTATE(gqrquant[i][0]));

	if (w)
		CVTSD2SS(XMM0, fpr.R(s));
	else
		CVTPD2PS(XMM0, fpr.R(s));
	CALLptr(M(&asm_routines.pairedStoreQuantizeFunc[i + w * 8]));

	if (update && js.memcheck)
	{
		MEMCHECK_START(false)
		if (indexed)
			ADD(32, gpr.R(a), gpr.R(b));
		else
			ADD(32, gpr.R(a), Imm32((u32)offset));
		MEMCHECK_END
	}
	gpr.UnlockAll();
}

void Jit64::psq_lXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStorePairedOff);
	FALLBACK_IF(!inst.RA);

	s32 offset = inst.SIMM_12;
	bool indexed = inst.OPCD == 4;
	bool update = (inst.OPCD == 57 && offset) || (inst.OPCD == 4 && !!(inst.SUBOP6 & 32));
	int a = inst.RA;
	int b = indexed ? inst.RB : a;
	int s = inst.FS;
	int i = indexed ? inst.Ix : inst.I;
	int w = indexed ? inst.Wx : inst.W;

	gpr.Lock(a, b);
	gpr.BindToRegister(a, true, update);
	fpr.BindToRegister(s, false, true);
	if (gpr.R(a).IsSimpleReg() && gpr.R(b).IsSimpleReg() && (indexed || offset))
	{
		if (indexed)
			LEA(32, RSCRATCH2, MComplex(gpr.RX(a), gpr.RX(b), SCALE_1, 0));
		else
			LEA(32, RSCRATCH2, MDisp(gpr.RX(a), offset));
	}
	else
	{
		MOV(32, R(RSCRATCH2), gpr.R(a));
		if (indexed)
			ADD(32, R(RSCRATCH2), gpr.R(b));
		else if (offset)
			ADD(32, R(RSCRATCH2), Imm32((u32)offset));
	}
	// In memcheck mode, don't update the address until the exception check
	if (update && !js.memcheck)
		MOV(32, gpr.R(a), R(RSCRATCH2));

	MOVZX(32, 8, RSCRATCH, PPCSTATE(gqrquant[i][1]));
	CALLptr(M(&asm_routines.pairedLoadQuantizeFunc[i + w * 8]));

	MEMCHECK_START(false)
	CVTPS2PD(fpr.RX(s), R(XMM0));
	if (update && js.memcheck)
	{
		if (indexed)
			ADD(32, gpr.R(a), gpr.R(b));
		else
			ADD(32, gpr.R(a), Imm32((u32)offset));
	}
	MEMCHECK_END

	gpr.UnlockAll();
}
