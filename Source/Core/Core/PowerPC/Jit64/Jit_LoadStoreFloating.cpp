// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "Common/CPUDetect.h"

#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/JitAsm.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"

// TODO: Add peephole optimizations for multiple consecutive lfd/lfs/stfd/stfs since they are so common,
// and pshufb could help a lot.

void Jit64::lfs(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff);

	int d = inst.RD;
	int a = inst.RA;
	FALLBACK_IF(!a);

	s32 offset = (s32)(s16)inst.SIMM_16;

	SafeLoadToReg(EAX, gpr.R(a), 32, offset, RegistersInUse(), false);

	fpr.Lock(d);
	fpr.BindToRegister(d, js.memcheck);

	MEMCHECK_START
	ConvertSingleToDouble(fpr.RX(d), EAX, true);
	MEMCHECK_END

	fpr.UnlockAll();
}


void Jit64::lfd(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff);
	FALLBACK_IF(!inst.RA);

	int d = inst.RD;
	int a = inst.RA;

	s32 offset = (s32)(s16)inst.SIMM_16;

	SafeLoadToReg(RAX, gpr.R(a), 64, offset, RegistersInUse(), false);

	fpr.Lock(d);
	fpr.BindToRegister(d, true);

	MEMCHECK_START
	MOVQ_xmm(XMM0, R(RAX));
	MOVSD(fpr.RX(d), R(XMM0));
	MEMCHECK_END

	fpr.UnlockAll();
}


void Jit64::stfd(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff);
	FALLBACK_IF(!inst.RA);

	int s = inst.RS;
	int a = inst.RA;

	gpr.FlushLockX(ABI_PARAM1);
	MOV(32, R(ABI_PARAM1), gpr.R(a));

	if (fpr.R(s).IsSimpleReg())
		MOVQ_xmm(R(RAX), fpr.RX(s));
	else
		MOV(64, R(RAX), fpr.R(s));

	s32 offset = (s32)(s16)inst.SIMM_16;
	SafeWriteRegToReg(RAX, ABI_PARAM1, 64, offset, RegistersInUse());

	gpr.UnlockAllX();
}

void Jit64::stfs(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff);
	FALLBACK_IF(!inst.RA);

	int s = inst.RS;
	int a = inst.RA;
	s32 offset = (s32)(s16)inst.SIMM_16;

	fpr.BindToRegister(s, true, false);
	ConvertDoubleToSingle(XMM0, fpr.RX(s));
	gpr.FlushLockX(ABI_PARAM1);
	MOV(32, R(ABI_PARAM1), gpr.R(a));
	SafeWriteF32ToReg(XMM0, ABI_PARAM1, offset, RegistersInUse());
	fpr.UnlockAll();
	gpr.UnlockAllX();
}

void Jit64::stfsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff);

	gpr.FlushLockX(ABI_PARAM1);
	MOV(32, R(ABI_PARAM1), gpr.R(inst.RB));
	if (inst.RA)
		ADD(32, R(ABI_PARAM1), gpr.R(inst.RA));

	int s = inst.RS;
	fpr.Lock(s);
	fpr.BindToRegister(s, true, false);
	ConvertDoubleToSingle(XMM0, fpr.RX(s));
	SafeWriteF32ToReg(XMM0, ABI_PARAM1, 0, RegistersInUse());
	fpr.UnlockAll();
	gpr.UnlockAllX();
}

void Jit64::lfsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff);

	MOV(32, R(EAX), gpr.R(inst.RB));
	if (inst.RA)
		ADD(32, R(EAX), gpr.R(inst.RA));

	SafeLoadToReg(EAX, R(EAX), 32, 0, RegistersInUse(), false);

	fpr.Lock(inst.RS);
	fpr.BindToRegister(inst.RS, js.memcheck);

	MEMCHECK_START
	ConvertSingleToDouble(fpr.RX(inst.RS), EAX, true);
	MEMCHECK_END

	fpr.UnlockAll();
	gpr.UnlockAllX();
}
