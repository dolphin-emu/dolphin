// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
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

	auto scratch = regs.gpr.Borrow();

	auto ra = a ? regs.gpr.Lock(a) : regs.gpr.Zero();
	auto rb = indexed ? regs.gpr.Lock(b) : regs.gpr.Imm32((u32)(s32)(s16)inst.SIMM_16);
	auto rd = regs.fpu.Lock(d);

	SafeLoad(scratch, ra, rb, 32, false, false, update);

	auto xd = rd.Bind(Jit64Reg::Write);
	if (single)
	{
		ConvertSingleToDouble(xd, scratch, true);
	}
	else
	{
		MOVQ_xmm(XMM0, scratch);
		MOVSD(xd, R(XMM0));
	}
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
	int accessSize = single ? 32 : 64;

	auto scratch = regs.gpr.Borrow();

	auto ra = a ? regs.gpr.Lock(a) : regs.gpr.Zero();
	auto rb = indexed ? regs.gpr.Lock(b) : regs.gpr.Imm32((u32)(s32)(s16)inst.SIMM_16);
	auto rs = regs.fpu.Lock(s);

	if (single)
	{
		if (jit->js.op->fprIsStoreSafe[s])
		{
			CVTSD2SS(XMM0, rs);
		}
		else
		{
			auto xs = rs.Bind(Jit64Reg::Read);
			ConvertDoubleToSingle(XMM0, xs);
		}
		MOVD_xmm(scratch, XMM0);
	}
	else
	{
		if (rs.IsRegBound())
		{
			auto xs = rs.Bind(Jit64Reg::Reuse);
			MOVQ_xmm(scratch, (X64Reg)xs);
		}
		else
			MOV(64, scratch, rs);
	}

	SafeWrite(scratch, ra, rb, accessSize, false, update);
}

// This one is a little bit weird; it stores the low 32 bits of a double without converting it
void Jit64::stfiwx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff);

	int s = inst.RS;
	int a = inst.RA;
	int b = inst.RB;

	auto ra = a ? regs.gpr.Lock(a) : regs.gpr.Zero();
	auto rb = regs.gpr.Lock(b);
	auto rs = regs.fpu.Lock(s);

	MOV_sum(32, RSCRATCH2, ra, rb);

	if (rs.IsRegBound())
	{
		auto xs = rs.Bind(Jit64Reg::Reuse);
		MOVD_xmm(R(RSCRATCH), xs);
	}
	else
	{
		MOV(32, R(RSCRATCH), rs);
	}

	SafeWriteRegToReg(RSCRATCH, RSCRATCH2, 32, 0, CallerSavedRegistersInUse());
}
