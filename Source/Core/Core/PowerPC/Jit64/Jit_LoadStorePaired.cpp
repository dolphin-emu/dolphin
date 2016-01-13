// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// TODO(ector): Tons of pshufb optimization of the loads/stores, for SSSE3+, possibly SSE4, only.
// Should give a very noticeable speed boost to paired single heavy code.

#include "Common/CommonTypes.h"
#include "Common/CPUDetect.h"

#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/JitAsm.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"
#include "Core/PowerPC/JitCommon/JitAsmCommon.h"

using namespace Gen;

// The big problem is likely instructions that set the quantizers in the same block.
// We will have to break block after quantizers are written to.
void Jit64::psq_stXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStorePairedOff);

	s32 offset = inst.SIMM_12;
	bool indexed = inst.OPCD == 4;
	bool update = (inst.OPCD == 61 && offset) || (inst.OPCD == 4 && !!(inst.SUBOP6 & 32));
	int a = inst.RA;
	int b = indexed ? inst.RB : a;
	int s = inst.FS;
	int i = indexed ? inst.Ix : inst.I;
	int w = indexed ? inst.Wx : inst.W;
	FALLBACK_IF(!a);

	auto it = js.constantGqr.find(i);
	bool gqrIsConstant = it != js.constantGqr.end();
	u32 gqrValue = gqrIsConstant ? it->second & 0xffff : 0;

	auto ra = regs.gpr.Lock(a);
	auto rb = indexed ? regs.gpr.Lock(b) : Imm32((u32)offset);
	auto rs = regs.fpu.Lock(s);

	auto scratch_extra = regs.gpr.Borrow(RSCRATCH_EXTRA);

	MOV_sum(32, scratch_extra, ra, rb);

	if (update)
	{
		auto xa = ra.Bind(Jit64Reg::WriteTransaction);
		MOV(32, xa, scratch_extra);
	}

	if (w)
		CVTSD2SS(XMM0, rs); // one
	else
		CVTPD2PS(XMM0, rs); // pair

	if (gqrIsConstant)
	{
		int type = gqrValue & 0x7;

		// Paired stores (other than w/type zero) don't yield any real change in
		// performance right now, but if we can improve fastmem support this might change
		if (gqrValue == 0)
		{
			if (w)
				GenQuantizedStore(true, (EQuantizeType)type, (gqrValue & 0x3F00) >> 8);
			else
				GenQuantizedStore(false, (EQuantizeType)type, (gqrValue & 0x3F00) >> 8);
		}
		else
		{
			auto scratch2 = regs.gpr.Borrow(RSCRATCH2);

			// We know what GQR is here, so we can load RSCRATCH2 and call into the store method directly
			// with just the scale bits.
			MOV(32, scratch2, Imm32(gqrValue & 0x3F00));

			if (w)
				CALL(asm_routines.singleStoreQuantized[type]);
			else
				CALL(asm_routines.pairedStoreQuantized[type]);
		}
	}
	else
	{
		auto scratch = regs.gpr.Borrow();
		auto scratch2 = regs.gpr.Borrow(RSCRATCH2);

		// Some games (e.g. Dirt 2) incorrectly set the unused bits which breaks the lookup table code.
		// Hence, we need to mask out the unused bits. The layout of the GQR register is
		// UU[SCALE]UUUUU[TYPE] where SCALE is 6 bits and TYPE is 3 bits, so we have to AND with
		// 0b0011111100000111, or 0x3F07.
		MOV(32, scratch2, Imm32(0x3F07));
		AND(32, scratch2, PPCSTATE(spr[SPR_GQR0 + i]));
		MOVZX(32, 8, scratch, scratch2);

		if (w)
			CALLptr(MScaled(scratch, SCALE_8, (u32)(u64)asm_routines.singleStoreQuantized));
		else
			CALLptr(MScaled(scratch, SCALE_8, (u32)(u64)asm_routines.pairedStoreQuantized));
	}
}

void Jit64::psq_lXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStorePairedOff);

	s32 offset = inst.SIMM_12;
	bool indexed = inst.OPCD == 4;
	bool update = (inst.OPCD == 57 && offset) || (inst.OPCD == 4 && !!(inst.SUBOP6 & 32));
	int a = inst.RA;
	int b = indexed ? inst.RB : a;
	int s = inst.FS;
	int i = indexed ? inst.Ix : inst.I;
	int w = indexed ? inst.Wx : inst.W;
	FALLBACK_IF(!a);

	auto it = js.constantGqr.find(i);
	bool gqrIsConstant = it != js.constantGqr.end();
	u32 gqrValue = gqrIsConstant ? it->second >> 16 : 0;

	auto ra = regs.gpr.Lock(a);
	auto rb = indexed ? regs.gpr.Lock(b) : Imm32((u32)offset);

	auto scratch_extra = regs.gpr.Borrow(RSCRATCH_EXTRA);

	MOV_sum(32, scratch_extra, ra, rb);

	if (update)
	{
		auto xa = ra.Bind(Jit64Reg::WriteTransaction);
		MOV(32, xa, scratch_extra);
	}

	if (gqrIsConstant)
	{
		// TODO: pass it a scratch register it can use
		GenQuantizedLoad(w == 1, (EQuantizeType)(gqrValue & 0x7), (gqrValue & 0x3F00) >> 8);
	}
	else
	{
		auto scratch = regs.gpr.Borrow();
		auto scratch2 = regs.gpr.Borrow(RSCRATCH2);

		MOV(32, scratch2, Imm32(0x3F07));

		// Get the high part of the GQR register
		OpArg gqr = PPCSTATE(spr[SPR_GQR0 + i]);
		gqr.AddMemOffset(2);

		AND(32, scratch2, gqr);
		MOVZX(32, 8, scratch, scratch2);

		// Explicitly uses RSCRATCH2 and RSCRATCH_EXTRA
		CALLptr(MScaled(scratch, SCALE_8, (u32)(u64)(&asm_routines.pairedLoadQuantized[w * 8])));
	}

	auto rs = regs.fpu.Lock(s).Bind(Jit64Reg::Write);
	CVTPS2PD(rs, R(XMM0));
}
