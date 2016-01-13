// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Common/CPUDetect.h"

#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"

using namespace Gen;

void Jit64::ps_mr(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	int d = inst.FD;
	int b = inst.FB;
	if (d == b)
		return;

	auto rb = regs.fpu.Lock(b);
	auto rd = regs.fpu.Lock(d);

	auto xd = rd.Bind(Jit64Reg::Write);
	MOVAPD(xd, rb);
}

void Jit64::ps_sum(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	int d = inst.FD;
	int a = inst.FA;
	int b = inst.FB;
	int c = inst.FC;

	auto rd = regs.fpu.Lock(d);
	auto ra = regs.fpu.Lock(a);
	auto rb = regs.fpu.Lock(b);
	auto rc = regs.fpu.Lock(c);

	auto xd = rd.BindWriteAndReadIf(d == b || d == c);
	auto tmp = regs.fpu.Borrow();
	MOVDDUP(tmp, ra);   // {a.ps0, a.ps0}
	ADDPD(tmp, rb); // {a.ps0 + b.ps0, a.ps0 + b.ps1}
	switch (inst.SUBOP5)
	{
	case 10: // ps_sum0: {a.ps0 + b.ps1, c.ps1}
		UNPCKHPD(tmp, rc);
		break;
	case 11: // ps_sum1: {c.ps0, a.ps0 + b.ps1}
		if (rc.IsRegBound())
		{
			auto xc = rc.Bind(Jit64Reg::Reuse);
			if (cpu_info.bSSE4_1)
			{
				BLENDPD(tmp, xc, 1);
			}
			else
			{
				auto tmp2 = regs.fpu.Borrow();
				MOVAPD((X64Reg)tmp2, xc);
				SHUFPD(tmp2, tmp, 2);
				// tmp can get released here
				tmp = tmp2;
			}
		}
		else
		{
			MOVLPD(tmp, rc);
		}
		break;
	default:
		PanicAlert("ps_sum WTF!!!");
	}
	HandleNaNs(inst, xd, tmp);
	ForceSinglePrecision(xd, xd);
	SetFPRFIfNeeded(xd);
}


void Jit64::ps_muls(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	int d = inst.FD;
	int a = inst.FA;
	int c = inst.FC;
	bool round_input = !jit->js.op->fprIsSingle[c];
	auto rd = regs.fpu.Lock(d);
	auto ra = regs.fpu.Lock(a);
	auto rc = regs.fpu.Lock(c);
	switch (inst.SUBOP5)
	{
	case 12: // ps_muls0
		MOVDDUP(XMM1, rc);
		break;
	case 13: // ps_muls1
		avx_op(&XEmitter::VSHUFPD, &XEmitter::SHUFPD, XMM1, rc, rc, 3);
		break;
	default:
		PanicAlert("ps_muls WTF!!!");
	}
	if (round_input)
		Force25BitPrecision(XMM1, R(XMM1), XMM0);
	MULPD(XMM1, ra);
	auto xd = rd.Bind(Jit64Reg::Write);
	HandleNaNs(inst, xd, XMM1);
	ForceSinglePrecision(xd, rd);
	SetFPRFIfNeeded(xd);
}


void Jit64::ps_mergeXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	int d = inst.FD;
	int a = inst.FA;
	int b = inst.FB;
	auto rd = regs.fpu.Lock(d);
	auto ra = regs.fpu.Lock(a);
	auto rb = regs.fpu.Lock(b);
	auto xd = rd.BindWriteAndReadIf(d == a || d == b);

	switch (inst.SUBOP10)
	{
	case 528:
		avx_op(&XEmitter::VUNPCKLPD, &XEmitter::UNPCKLPD, xd, ra, rb);
		break; //00
	case 560:
		avx_op(&XEmitter::VSHUFPD, &XEmitter::SHUFPD, xd, ra, rb, 2);
		break; //01
	case 592:
		avx_op(&XEmitter::VSHUFPD, &XEmitter::SHUFPD, xd, ra, rb, 1);
		break; //10
	case 624:
		avx_op(&XEmitter::VUNPCKHPD, &XEmitter::UNPCKHPD, xd, ra, rb);
		break; //11
	default:
		_assert_msg_(DYNA_REC, 0, "ps_merge - invalid op");
	}
}

void Jit64::ps_rsqrte(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);
	int b = inst.FB;
	int d = inst.FD;
	auto rd = regs.fpu.Lock(d);
	auto rb = regs.fpu.Lock(b);
	auto scratch_extra = regs.gpr.Borrow(RSCRATCH_EXTRA);
	
	auto xb = rb.Bind(Jit64Reg::Read);
	auto xd = rd.Bind(Jit64Reg::Write);
	auto tmp = regs.fpu.Borrow(XMM0);

	MOVSD((X64Reg)tmp, xb);
	CALL(asm_routines.frsqrte);
	MOVSD(rd, tmp);

	MOVHLPS(tmp, xb);
	CALL(asm_routines.frsqrte);
	MOVLHPS(xd, tmp);

	ForceSinglePrecision(xd, xd);
	SetFPRFIfNeeded(xd);
}

void Jit64::ps_res(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);
	int b = inst.FB;
	int d = inst.FD;
	auto rd = regs.fpu.Lock(d);
	auto rb = regs.fpu.Lock(b);
	auto scratch_extra = regs.gpr.Borrow(RSCRATCH_EXTRA);

	auto xb = rb.Bind(Jit64Reg::Read);
	auto xd = rd.Bind(Jit64Reg::Write);
	auto tmp = regs.fpu.Borrow(XMM0);

	MOVSD((X64Reg)tmp, xb);
	CALL(asm_routines.fres);
	MOVSD((X64Reg)xd, tmp);

	MOVHLPS(tmp, xb);
	CALL(asm_routines.fres);
	MOVLHPS(xd, tmp);

	ForceSinglePrecision(xd, xd);
	SetFPRFIfNeeded(xd);
}

void Jit64::ps_cmpXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);

	FloatCompare(inst, !!(inst.SUBOP10 & 64));
}
