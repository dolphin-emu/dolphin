// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Arm64Emitter.h"
#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
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
	FALLBACK_IF(jo.memcheck || !jo.fastmem);

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
	ARM64Reg VS;

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

	if (update)
	{
		gpr.BindToRegister(inst.RA, REG_REG);
		MOV(arm_addr, addr_reg);
	}

	if (js.assumeNoPairedQuantize)
	{
		if (!inst.W)
		{
			VS = fpr.RW(inst.RS, REG_REG_SINGLE);
			ADD(EncodeRegTo64(addr_reg), EncodeRegTo64(addr_reg), X28);
			m_float_emit.LD1(32, 1, EncodeRegToDouble(VS), EncodeRegTo64(addr_reg));
			m_float_emit.REV32(8, VS, VS);
		}
		else
		{
			VS = fpr.RW(inst.RS, REG_REG);
			m_float_emit.LDR(32, VS, EncodeRegTo64(addr_reg), X28);
			m_float_emit.REV32(8, VS, VS);
			m_float_emit.FCVT(64, 32, EncodeRegToDouble(VS), EncodeRegToDouble(VS));
		}
	}
	else
	{
		LDR(INDEX_UNSIGNED, scale_reg, X29, PPCSTATE_OFF(spr[SPR_GQR0 + inst.I]));
		UBFM(type_reg, scale_reg, 16, 18); // Type
		UBFM(scale_reg, scale_reg, 24, 29); // Scale

		MOVI2R(X30, (u64)&asm_routines.pairedLoadQuantized[inst.W * 8]);
		LDR(X30, X30, ArithOption(EncodeRegTo64(type_reg), true));
		BLR(X30);

		if (!inst.W)
		{
			VS = fpr.RW(inst.RS, REG_REG_SINGLE);
			m_float_emit.ORR(VS, D0, D0);
		}
		else
		{
			VS = fpr.RW(inst.RS, REG_REG);
			m_float_emit.FCVT(64, 32, EncodeRegToDouble(VS), D0);
		}
	}

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
	FALLBACK_IF(jo.memcheck || !jo.fastmem);

	// X30 is LR
	// X0 contains the scale
	// X1 is the address
	// Q0 is the store register

	bool update = inst.OPCD == 61;
	s32 offset = inst.SIMM_12;

	gpr.Lock(W0, W1, W2, W30);
	fpr.Lock(Q0, Q1);

	bool single = fpr.IsSingle(inst.RS);

	ARM64Reg arm_addr = gpr.R(inst.RA);
	ARM64Reg VS = fpr.R(inst.RS, single ? REG_REG_SINGLE : REG_REG);

	ARM64Reg scale_reg = W0;
	ARM64Reg addr_reg = W1;
	ARM64Reg type_reg = W2;

	BitSet32 gprs_in_use = gpr.GetCallerSavedUsed();
	BitSet32 fprs_in_use = fpr.GetCallerSavedUsed();

	// Wipe the registers we are using as temporaries
	gprs_in_use &= BitSet32(~7);
	fprs_in_use &= BitSet32(~3);

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

	if (update)
	{
		gpr.BindToRegister(inst.RA, REG_REG);
		MOV(arm_addr, addr_reg);
	}

	if (js.assumeNoPairedQuantize)
	{
		u32 flags = BackPatchInfo::FLAG_STORE;

		if (single)
			flags |= (inst.W ? BackPatchInfo::FLAG_SIZE_F32I : BackPatchInfo::FLAG_SIZE_F32X2I);
		else
			flags |= (inst.W ? BackPatchInfo::FLAG_SIZE_F32 : BackPatchInfo::FLAG_SIZE_F32X2);

		EmitBackpatchRoutine(flags,
			jo.fastmem,
			jo.fastmem,
			VS, EncodeRegTo64(addr_reg),
			gprs_in_use,
			fprs_in_use);
	}
	else
	{
		if (single)
		{
			m_float_emit.ORR(D0, VS, VS);
		}
		else
		{
			if (inst.W)
				m_float_emit.FCVT(32, 64, D0, VS);
			else
				m_float_emit.FCVTN(32, D0, VS);
		}

		LDR(INDEX_UNSIGNED, scale_reg, X29, PPCSTATE_OFF(spr[SPR_GQR0 + inst.I]));
		UBFM(type_reg, scale_reg, 0, 2); // Type
		UBFM(scale_reg, scale_reg, 8, 13); // Scale

		// Inline address check
		TST(addr_reg, 6, 1);
		FixupBranch pass = B(CC_EQ);
		FixupBranch fail = B();

		SwitchToFarCode();
			SetJumpTarget(fail);
			// Slow
			MOVI2R(X30, (u64)&asm_routines.pairedStoreQuantized[16 + inst.W * 8]);
			LDR(EncodeRegTo64(type_reg), X30, ArithOption(EncodeRegTo64(type_reg), true));

			ABI_PushRegisters(gprs_in_use);
			m_float_emit.ABI_PushRegisters(fprs_in_use, X30);
			BLR(EncodeRegTo64(type_reg));
			m_float_emit.ABI_PopRegisters(fprs_in_use, X30);
			ABI_PopRegisters(gprs_in_use);
			FixupBranch continue1 = B();
		SwitchToNearCode();
		SetJumpTarget(pass);

		// Fast
		MOVI2R(X30, (u64)&asm_routines.pairedStoreQuantized[inst.W * 8]);
		LDR(EncodeRegTo64(type_reg), X30, ArithOption(EncodeRegTo64(type_reg), true));
		BLR(EncodeRegTo64(type_reg));

		SetJumpTarget(continue1);
	}

	gpr.Unlock(W0, W1, W2, W30);
	fpr.Unlock(Q0, Q1);
}

