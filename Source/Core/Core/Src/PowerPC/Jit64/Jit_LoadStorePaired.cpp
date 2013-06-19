// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// TODO(ector): Tons of pshufb optimization of the loads/stores, for SSSE3+, possibly SSE4, only.
// Should give a very noticeable speed boost to paired single heavy code.

#include "Common.h"
 
#include "Thunk.h"
#include "../PowerPC.h"
#include "../../Core.h"
#include "../../HW/GPFifo.h"
#include "../../HW/Memmap.h"
#include "../PPCTables.h"
#include "CPUDetect.h"
#include "x64Emitter.h"
#include "x64ABI.h"

#include "Jit.h"
#include "JitAsm.h"
#include "JitRegCache.h"

const u8 GC_ALIGNED16(pbswapShuffle2x4[16]) = {3, 2, 1, 0, 7, 6, 5, 4, 8, 9, 10, 11, 12, 13, 14, 15};

//static u64 GC_ALIGNED16(temp64); // unused?
 
// TODO(ector): Improve 64-bit version
#if 0
static void WriteDual32(u64 value, u32 address)
{
	MOV(32, M(&PC), Imm32(jit->js.compilerPC)); // Helps external systems know which instruction triggered the write
	Memory::Write_U32((u32)(value >> 32), address);
	Memory::Write_U32((u32)value, address + 4);
}
#endif

// The big problem is likely instructions that set the quantizers in the same block.
// We will have to break block after quantizers are written to.
void Jit64::psq_st(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStorePaired)

	if (js.memcheck) { Default(inst); return; }

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
#if 0
	u16 store_gqr = gqr.Hex & 0xFFFF;

	const EQuantizeType stType = static_cast<EQuantizeType>(gqr.ST_TYPE);
	int stScale = gqr.ST_SCALE;

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
#ifdef _M_IX86
	int addr_scale = SCALE_4;
#else
	int addr_scale = SCALE_8;
#endif
	if (inst.W) {
		// One value
		XORPS(XMM0, R(XMM0));  // TODO: See if we can get rid of this cheaply by tweaking the code in the singleStore* functions.
		CVTSD2SS(XMM0, fpr.R(s));
		ABI_AlignStack(0);
		CALLptr(MScaled(EDX, addr_scale, (u32)(u64)asm_routines.singleStoreQuantized));
		ABI_RestoreStack(0);
	} else {
		// Pair of values
		CVTPD2PS(XMM0, fpr.R(s));
		ABI_AlignStack(0);
		CALLptr(MScaled(EDX, addr_scale, (u32)(u64)asm_routines.pairedStoreQuantized));
		ABI_RestoreStack(0);
	}
	gpr.UnlockAll();
	gpr.UnlockAllX();
}

void Jit64::psq_l(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStorePaired)

	if (js.memcheck) { Default(inst); return; }

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
#ifdef _M_IX86
	int addr_scale = SCALE_4;
#else
	int addr_scale = SCALE_8;
#endif
	ABI_AlignStack(0);
	CALLptr(MScaled(EDX, addr_scale, (u32)(u64)asm_routines.pairedLoadQuantized));
	ABI_RestoreStack(0);

//	MEMCHECK_START // FIXME: MMU does not work here because of unsafe memory access

	CVTPS2PD(fpr.RX(inst.RS), R(XMM0));

//	MEMCHECK_END

	gpr.UnlockAll();
	gpr.UnlockAllX();
}
