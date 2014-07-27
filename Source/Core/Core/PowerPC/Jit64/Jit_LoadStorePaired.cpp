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
void Jit64::psq_st(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStorePairedOff);
	FALLBACK_IF(!inst.RA);

	s32 offset = inst.SIMM_12;
	bool update = inst.OPCD == 61 && offset;
	int a = inst.RA;
	int s = inst.RS;

	gpr.FlushLockX(RSCRATCH_EXTRA);
	if (update)
		gpr.BindToRegister(a, true, true);
	fpr.BindToRegister(s, true, false);
	if (offset && gpr.R(a).IsSimpleReg())
	{
		LEA(32, RSCRATCH_EXTRA, MDisp(gpr.RX(a), offset));
	}
	else
	{
		MOV(32, R(RSCRATCH_EXTRA), gpr.R(a));
		if (offset)
			ADD(32, R(RSCRATCH_EXTRA), Imm32((u32)offset));
	}
	// In memcheck mode, don't update the address until the exception check
	if (update && offset && !js.memcheck)
		MOV(32, gpr.R(a), R(RSCRATCH_EXTRA));
	// Some games (e.g. Dirt 2) incorrectly set the unused bits which breaks the lookup table code.
	// Hence, we need to mask out the unused bits. The layout of the GQR register is
	// UU[SCALE]UUUUU[TYPE] where SCALE is 6 bits and TYPE is 3 bits, so we have to AND with
	// 0b0011111100000111, or 0x3F07.
	MOV(32, R(RSCRATCH2), Imm32(0x3F07));
	AND(32, R(RSCRATCH2), PPCSTATE(spr[SPR_GQR0 + inst.I]));
	MOVZX(32, 8, RSCRATCH, R(RSCRATCH2));

	// FIXME: Fix ModR/M encoding to allow [RSCRATCH2*8+disp32] without a base register!
	if (inst.W)
	{
		// One value
		PXOR(XMM0, R(XMM0));  // TODO: See if we can get rid of this cheaply by tweaking the code in the singleStore* functions.
		CVTSD2SS(XMM0, fpr.R(s));
		CALLptr(MScaled(RSCRATCH, SCALE_8, (u32)(u64)asm_routines.singleStoreQuantized));
	}
	else
	{
		// Pair of values
		CVTPD2PS(XMM0, fpr.R(s));
		CALLptr(MScaled(RSCRATCH, SCALE_8, (u32)(u64)asm_routines.pairedStoreQuantized));
	}

	if (update && offset && js.memcheck)
	{
		MEMCHECK_START(false)
		ADD(32, gpr.R(a), Imm32((u32)offset));
		MEMCHECK_END
	}
	gpr.UnlockAll();
	gpr.UnlockAllX();
}

void Jit64::psq_l(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStorePairedOff);
	FALLBACK_IF(!inst.RA);

	s32 offset = inst.SIMM_12;
	bool update = inst.OPCD == 57 && offset;
	int a = inst.RA;
	int s = inst.RS;

	gpr.FlushLockX(RSCRATCH_EXTRA);
	gpr.BindToRegister(a, true, update && offset);
	fpr.BindToRegister(s, false, true);
	if (offset && gpr.R(a).IsSimpleReg())
	{
		LEA(32, RSCRATCH_EXTRA, MDisp(gpr.RX(a), offset));
	}
	else
	{
		MOV(32, R(RSCRATCH_EXTRA), gpr.R(a));
		if (offset)
			ADD(32, R(RSCRATCH_EXTRA), Imm32((u32)offset));
	}
	// In memcheck mode, don't update the address until the exception check
	if (update && offset && !js.memcheck)
		MOV(32, gpr.R(a), R(RSCRATCH_EXTRA));
	MOV(32, R(RSCRATCH2), Imm32(0x3F07));
	AND(32, R(RSCRATCH2), M(((char *)&GQR(inst.I)) + 2));
	MOVZX(32, 8, RSCRATCH, R(RSCRATCH2));
	if (inst.W)
		OR(32, R(RSCRATCH), Imm8(8));

	CALLptr(MScaled(RSCRATCH, SCALE_8, (u32)(u64)asm_routines.pairedLoadQuantized));

	MEMCHECK_START(false)
	CVTPS2PD(fpr.RX(s), R(XMM0));
	if (update && offset && js.memcheck)
	{
		ADD(32, gpr.R(a), Imm32((u32)offset));
	}
	MEMCHECK_END

	gpr.UnlockAll();
	gpr.UnlockAllX();
}
