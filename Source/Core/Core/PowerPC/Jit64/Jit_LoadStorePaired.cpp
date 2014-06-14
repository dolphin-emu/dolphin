// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// TODO(ector): Tons of pshufb optimization of the loads/stores, for SSSE3+, possibly SSE4, only.
// Should give a very noticeable speed boost to paired single heavy code.

#include "Common/Common.h"
#include "Common/CPUDetect.h"

#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/JitAsm.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"

// The big problem is likely instructions that set the quantizers in the same block.
// We will have to break block after quantizers are written to.
void Jit64::psq_st(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStorePairedOff);
	FALLBACK_IF(js.memcheck || !inst.RA);

	bool update = inst.OPCD == 61;

	int offset = inst.SIMM_12;
	int a = inst.RA;
	int s = inst.RS; // Fp numbers

	gpr.FlushLockX(EAX, EDX);
	gpr.FlushLockX(ECX);
	if (update)
		gpr.BindToRegister(inst.RA, true, true);
	fpr.BindToRegister(inst.RS, true, false);
	MOV(32, R(ECX), gpr.R(inst.RA));
	if (offset)
		ADD(32, R(ECX), Imm32((u32)offset));
	if (update && offset)
		MOV(32, gpr.R(a), R(ECX));
	MOVZX(32, 16, EAX, M(&PowerPC::ppcState.spr[SPR_GQR0 + inst.I]));
	MOVZX(32, 8, EDX, R(AL));
	// FIXME: Fix ModR/M encoding to allow [EDX*4+disp32] without a base register!
#if _M_X86_32
	int addr_scale = SCALE_4;
#else
	int addr_scale = SCALE_8;
#endif
	if (inst.W) {
		// One value
		PXOR(XMM0, R(XMM0));  // TODO: See if we can get rid of this cheaply by tweaking the code in the singleStore* functions.
		CVTSD2SS(XMM0, fpr.R(s));
		CALLptr(MScaled(EDX, addr_scale, (u32)(u64)asm_routines.singleStoreQuantized));
	} else {
		// Pair of values
		CVTPD2PS(XMM0, fpr.R(s));
		CALLptr(MScaled(EDX, addr_scale, (u32)(u64)asm_routines.pairedStoreQuantized));
	}
	gpr.UnlockAll();
	gpr.UnlockAllX();
}

void Jit64::psq_l(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStorePairedOff);
	FALLBACK_IF(js.memcheck || !inst.RA);

	bool update = inst.OPCD == 57;
	int offset = inst.SIMM_12;

	gpr.FlushLockX(EAX, EDX);
	gpr.FlushLockX(ECX);
	gpr.BindToRegister(inst.RA, true, update && offset);
	fpr.BindToRegister(inst.RS, false, true);
	if (offset)
		LEA(32, ECX, MDisp(gpr.RX(inst.RA), offset));
	else
		MOV(32, R(ECX), gpr.R(inst.RA));
	if (update && offset)
		MOV(32, gpr.R(inst.RA), R(ECX));
	MOVZX(32, 16, EAX, M(((char *)&GQR(inst.I)) + 2));
	MOVZX(32, 8, EDX, R(AL));
	if (inst.W)
		OR(32, R(EDX), Imm8(8));
#if _M_X86_32
	int addr_scale = SCALE_4;
#else
	int addr_scale = SCALE_8;
#endif
	ABI_AlignStack(0);
	CALLptr(MScaled(EDX, addr_scale, (u32)(u64)asm_routines.pairedLoadQuantized));
	ABI_RestoreStack(0);

	// MEMCHECK_START // FIXME: MMU does not work here because of unsafe memory access

	CVTPS2PD(fpr.RX(inst.RS), R(XMM0));

	// MEMCHECK_END

	gpr.UnlockAll();
	gpr.UnlockAllX();
}
