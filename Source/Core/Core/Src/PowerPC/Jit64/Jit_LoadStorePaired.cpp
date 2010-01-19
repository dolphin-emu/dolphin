// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

// TODO(ector): Tons of pshufb optimization of the loads/stores, for SSSE3+, possibly SSE4, only.
// Should give a very noticable speed boost to paired single heavy code.

#include "Common.h"
 
#include "Thunk.h"
#include "../PowerPC.h"
#include "../../Core.h"
#include "../../HW/GPFifo.h"
#include "../../HW/Memmap.h"
#include "../PPCTables.h"
#include "CPUDetect.h"
#include "x64Emitter.h"
#include "ABI.h"

#include "Jit.h"
#include "JitAsm.h"
#include "JitRegCache.h"

const u8 GC_ALIGNED16(pbswapShuffle2x4[16]) = {3, 2, 1, 0, 7, 6, 5, 4, 8, 9, 10, 11, 12, 13, 14, 15};

static u64 GC_ALIGNED16(temp64);
 
// TODO(ector): Improve 64-bit version
static void WriteDual32(u64 value, u32 address)
{
	Memory::Write_U32((u32)(value >> 32), address);
	Memory::Write_U32((u32)value, address + 4);
}

// The big problem is likely instructions that set the quantizers in the same block.
// We will have to break block after quantizers are written to.
void Jit64::psq_st(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStorePaired)

	if (!inst.RA)
	{
		// TODO: Support these cases if it becomes necessary.
		Default(inst);
		return;
	}

	bool update = inst.OPCD == 61;

	int offset = inst.SIMM_12;
	int a = inst.RA;
	int s = inst.RS; // Fp numbers

	const UGQR gqr(rSPR(SPR_GQR0 + inst.I));
	u16 store_gqr = gqr.Hex & 0xFFFF;

	const EQuantizeType stType = static_cast<EQuantizeType>(gqr.ST_TYPE);
	int stScale = gqr.ST_SCALE;

#if 0
	// Is this specialization still worth it? Let's keep it for now. It's probably
	// not very risky since a game most likely wouldn't use the same code to process
	// floats as integers (but you never know....).
	if (stType == QUANTIZE_FLOAT)
	{
		if (gpr.R(a).IsImm() && !update && cpu_info.bSSSE3)
		{
			u32 addr = (u32)(gpr.R(a).offset + offset);
			if (addr == 0xCC008000) {
				// Writing to FIFO. Let's do fast method.
				CVTPD2PS(XMM0, fpr.R(s));
				PSHUFB(XMM0, M((void*)&pbswapShuffle2x4));
				CALL((void*)asm_routines.fifoDirectWriteXmm64);
				js.fifoBytesThisBlock += 8;
				return;
			}
		}
	}
#endif

	gpr.FlushLockX(EAX, EDX);
	gpr.FlushLockX(ECX);
	if (update)
		gpr.LoadToX64(inst.RA, true, true);
	fpr.LoadToX64(inst.RS, true);
	MOV(32, R(ECX), gpr.R(inst.RA));
	if (offset)
		ADD(32, R(ECX), Imm32((u32)offset));
	if (update && offset)
		MOV(32, gpr.R(a), R(ECX));
	MOVZX(32, 16, EAX, M(&PowerPC::ppcState.spr[SPR_GQR0 + inst.I]));
	MOVZX(32, 8, EDX, R(AL));
	// FIXME: Fix ModR/M encoding to allow [EDX*4+disp32] without a base register!
#ifdef _M_IX86
	int addr_shift = 2;
#else
	int addr_shift = 3;
#endif
	SHL(32, R(EDX), Imm8(addr_shift));
	if (inst.W) {
		// One value
		XORPS(XMM0, R(XMM0));  // TODO: See if we can get rid of this cheaply by tweaking the code in the singleStore* functions.
		CVTSD2SS(XMM0, fpr.R(s));
		CALLptr(MDisp(EDX, (u32)(u64)asm_routines.singleStoreQuantized));
	} else {
		// Pair of values
		CVTPD2PS(XMM0, fpr.R(s));
		CALLptr(MDisp(EDX, (u32)(u64)asm_routines.pairedStoreQuantized));
	}
	gpr.UnlockAll();
	gpr.UnlockAllX();
}

void Jit64::psq_l(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStorePaired)

	if (!inst.RA)
	{
		Default(inst);
		return;
	}

	const UGQR gqr(rSPR(SPR_GQR0 + inst.I));

	if (inst.W) {
		// PanicAlert("Single ps load: %i %i", gqr.ST_TYPE, gqr.ST_SCALE);
		Default(inst);
		return;
	}

	bool update = inst.OPCD == 57;
	int offset = inst.SIMM_12;

	gpr.FlushLockX(EAX, EDX);
	gpr.FlushLockX(ECX);
	gpr.LoadToX64(inst.RA, true, true);
	fpr.LoadToX64(inst.RS, false, true);
	if (offset)
		LEA(32, ECX, MDisp(gpr.RX(inst.RA), offset));
	else
		MOV(32, R(ECX), gpr.R(inst.RA));
	if (update && offset)
		MOV(32, gpr.R(inst.RA), R(ECX));
	MOVZX(32, 16, EAX, M(((char *)&GQR(inst.I)) + 2));
	MOVZX(32, 8, EDX, R(AL));
	// FIXME: Fix ModR/M encoding to allow [EDX*4+disp32]! (MComplex can do this, no?)
#ifdef _M_IX86
	SHL(32, R(EDX), Imm8(2));
#else
	SHL(32, R(EDX), Imm8(3));
#endif
	CALLptr(MDisp(EDX, (u32)(u64)asm_routines.pairedLoadQuantized));
	CVTPS2PD(fpr.RX(inst.RS), R(XMM0));
	gpr.UnlockAll();
	gpr.UnlockAllX();
}
