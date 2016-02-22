// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/CPUDetect.h"
#include "Common/x64Emitter.h"
#include "Core/ConfigManager.h"
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/JitCommon/Jit_Util.h"

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
	int accessSize = single ? 32 : 64;

	auto ra = a ? regs.gpr.Lock(a) : regs.gpr.Zero();
	auto rb = indexed ? regs.gpr.Lock(b) : regs.gpr.Imm32((u32)(s32)(s16)inst.SIMM_16);
	auto rd = regs.fpu.Lock(d);

	auto val = regs.gpr.Borrow();
	SafeLoad(val, ra, rb, accessSize, false, true, update);

	auto xd = rd.BindWriteAndReadIf(!single);
	if (single)
	{
		ConvertSingleToDouble(xd, val, regs.gpr.Borrow(), true);
	}
	else
	{
		auto fscratch = regs.fpu.Borrow();
		MOVQ_xmm((X64Reg)fscratch, R(val));
		MOVSD(xd, R(fscratch));
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

	auto val = regs.gpr.Borrow();

	auto ra = a ? regs.gpr.Lock(a) : regs.gpr.Zero();
	auto rb = indexed ? regs.gpr.Lock(b) : regs.gpr.Imm32((u32)(s32)(s16)inst.SIMM_16);
	auto rs = regs.fpu.Lock(s);

	if (single)
	{
		auto fscratch = regs.fpu.Borrow();
		if (jit->js.op->fprIsStoreSafe[s])
		{
			CVTSD2SS(fscratch, rs);
		}
		else
		{
			auto xs = rs.Bind(BindMode::Read);
			ConvertDoubleToSingle(fscratch, xs);
		}
		MOVD_xmm(R(val), fscratch);
	}
	else
	{
		if (rs.IsRegBound())
		{
			auto xs = rs.Bind(BindMode::Reuse);
			MOVQ_xmm(R(val), xs);
		}
		else
		{
			MOV(64, val, rs);
		}
	}

	SafeWrite(val, ra, rb, accessSize, true, update);
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

	// We could optimize this to avoid using a temp
	auto addr = regs.gpr.Borrow();
	auto val = regs.gpr.Borrow();

	MOV_sum(32, addr, ra, rb);

	if (rs.IsRegBound())
	{
		auto xs = rs.Bind(BindMode::Reuse);
		MOVD_xmm(R(val), xs);
	}
	else
	{
		MOV(32, val, rs);
	}

	SafeWriteRegToReg(R(val), addr, 32, 0, CallerSavedRegistersInUse());
}
