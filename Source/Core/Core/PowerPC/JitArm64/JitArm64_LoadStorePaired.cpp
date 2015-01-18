// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Arm64Emitter.h"
#include "Common/Common.h"
#include "Common/StringUtil.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"
#include "Core/PowerPC/JitArm64/JitAsm.h"

using namespace Arm64Gen;

void JitArm64::psq_l(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStorePairedOff);
	FALLBACK_IF(js.memcheck || !SConfig::GetInstance().m_LocalCoreStartupParameter.bFastmem);

	// X30 is LR
	// X0 contains the scale
	// X1 is the address
	// X2 is a temporary
	// Q0 is the return register
	// Q1 is a temporary
	bool update = inst.OPCD == 57;
	s32 offset = inst.SIMM_12;

	gpr.Lock(W0, W1, W2, W30);
	fpr.Lock(Q0, Q1);

	ARM64Reg arm_addr = gpr.R(inst.RA);
	ARM64Reg scale_reg = W0;
	ARM64Reg addr_reg = W1;
	ARM64Reg type_reg = W2;

	LDR(INDEX_UNSIGNED, scale_reg, X29, PPCSTATE_OFF(spr[SPR_GQR0 + inst.I]));

	if (inst.RA || update) // Always uses the register on update
	{
		if (offset >= 0)
			ADD(addr_reg, gpr.R(inst.RA), offset);
		else
			SUB(addr_reg, gpr.R(inst.RA), std::abs(offset));
	}
	else
	{
		MOVI2R(addr_reg, (u32)offset);
	}

	UBFM(type_reg, scale_reg, 16, 18); // Type
	UBFM(scale_reg, scale_reg, 24, 29); // Scale

	if (update)
		MOV(arm_addr, addr_reg);

	MOVI2R(X30, (u64)&asm_routines.pairedLoadQuantized[inst.W * 8]);
	LDR(X30, X30, ArithOption(EncodeRegTo64(type_reg), true));
	BLR(X30);

	fpr.BindToRegister(inst.RS, false);
	ARM64Reg VS = fpr.R(inst.RS);
	m_float_emit.FCVTL(64, EncodeRegToDouble(VS), D0);
	if (inst.W)
	{
		m_float_emit.FMOV(D0, 0x70); // 1.0 as a Double
		m_float_emit.INS(64, VS, 1, Q0, 0);
	}

	gpr.Unlock(W0, W1, W2, W30);
	fpr.Unlock(Q0, Q1);
}

void JitArm64::psq_st(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStorePairedOff);
	FALLBACK_IF(js.memcheck || !SConfig::GetInstance().m_LocalCoreStartupParameter.bFastmem);

	// X30 is LR
	// X0 contains the scale
	// X1 is the address
	// Q0 is the store register

	bool update = inst.OPCD == 61;
	s32 offset = inst.SIMM_12;

	gpr.Lock(W0, W1, W2, W30);
	fpr.Lock(Q0, Q1);

	ARM64Reg arm_addr = gpr.R(inst.RA);
	ARM64Reg scale_reg = W0;
	ARM64Reg addr_reg = W1;
	ARM64Reg type_reg = gpr.GetReg();

	LDR(INDEX_UNSIGNED, scale_reg, X29, PPCSTATE_OFF(spr[SPR_GQR0 + inst.I]));

	if (inst.RA || update) // Always uses the register on update
	{
		if (offset >= 0)
			ADD(addr_reg, gpr.R(inst.RA), offset);
		else
			SUB(addr_reg, gpr.R(inst.RA), std::abs(offset));
	}
	else
	{
		MOVI2R(addr_reg, (u32)offset);
	}

	UBFM(type_reg, scale_reg, 0, 2); // Type
	UBFM(scale_reg, scale_reg, 8, 13); // Scale

	if (update)
		MOV(arm_addr, addr_reg);

	ARM64Reg VS = fpr.R(inst.RS);
	m_float_emit.FCVTN(32, D0, VS);
	MOVI2R(X30, (u64)&asm_routines.pairedStoreQuantized[inst.W * 8]);
	LDR(X30, X30, ArithOption(EncodeRegTo64(type_reg), true));
	BLR(X30);

	gpr.Unlock(W0, W1, W2, W30, type_reg);
	fpr.Unlock(Q0, Q1);
}

