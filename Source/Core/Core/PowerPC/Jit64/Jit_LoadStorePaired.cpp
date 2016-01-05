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

	gpr.Lock(a, b);
	if (gqrIsConstant && gqrValue == 0)
	{
		int storeOffset = 0;
		gpr.BindToRegister(a, true, update);
		X64Reg addr = gpr.RX(a);
		// TODO: this is kind of ugly :/ we should probably create a universal load/store address calculation
		// function that handles all these weird cases, e.g. how non-fastmem loadstores clobber addresses.
		bool storeAddress = (update && jo.memcheck) || !jo.fastmem;
		if (storeAddress)
		{
			addr = RSCRATCH2;
			MOV(32, R(addr), gpr.R(a));
		}
		if (indexed)
		{
			if (update)
			{
				ADD(32, R(addr), gpr.R(b));
			}
			else
			{
				addr = RSCRATCH2;
				if (a && gpr.R(a).IsSimpleReg() && gpr.R(b).IsSimpleReg())
				{
					LEA(32, addr, MRegSum(gpr.RX(a), gpr.RX(b)));
				}
				else
				{
					MOV(32, R(addr), gpr.R(b));
					if (a)
						ADD(32, R(addr), gpr.R(a));
				}
			}
		}
		else
		{
			if (update)
				ADD(32, R(addr), Imm32(offset));
			else
				storeOffset = offset;
		}

		fpr.Lock(s);
		if (w)
		{
			CVTSD2SS(XMM0, fpr.R(s));
			MOVD_xmm(R(RSCRATCH), XMM0);
		}
		else
		{
			CVTPD2PS(XMM0, fpr.R(s));
			MOVQ_xmm(R(RSCRATCH), XMM0);
			ROL(64, R(RSCRATCH), Imm8(32));
		}

		BitSet32 registersInUse = CallerSavedRegistersInUse();
		if (update && storeAddress)
			registersInUse[addr] = true;
		SafeWriteRegToReg(RSCRATCH, addr, w ? 32 : 64, storeOffset, registersInUse);
		MemoryExceptionCheck();
		if (update && storeAddress)
			MOV(32, gpr.R(a), R(addr));
		gpr.UnlockAll();
		fpr.UnlockAll();
		return;
	}
	gpr.FlushLockX(RSCRATCH_EXTRA);
	if (update)
		gpr.BindToRegister(a, true, true);
	if (gpr.R(a).IsSimpleReg() && gpr.R(b).IsSimpleReg() && (indexed || offset))
	{
		if (indexed)
			LEA(32, RSCRATCH_EXTRA, MRegSum(gpr.RX(a), gpr.RX(b)));
		else
			LEA(32, RSCRATCH_EXTRA, MDisp(gpr.RX(a), offset));
	}
	else
	{
		MOV(32, R(RSCRATCH_EXTRA), gpr.R(a));
		if (indexed)
			ADD(32, R(RSCRATCH_EXTRA), gpr.R(b));
		else if (offset)
			ADD(32, R(RSCRATCH_EXTRA), Imm32((u32)offset));
	}
	// In memcheck mode, don't update the address until the exception check
	if (update && !jo.memcheck)
		MOV(32, gpr.R(a), R(RSCRATCH_EXTRA));

	if (gqrIsConstant)
	{
		// Paired stores don't yield any real change in performance right now, but if we can
		// improve fastmem support this might change
		//#define INLINE_PAIRED_STORES
#ifdef INLINE_PAIRED_STORES
		if (w)
		{
			// One value
			CVTSD2SS(XMM0, fpr.R(s));
			GenQuantizedStore(true, (EQuantizeType)(gqrValue & 0x7), (gqrValue & 0x3F00) >> 8);
		}
		else
		{
			// Pair of values
			CVTPD2PS(XMM0, fpr.R(s));
			GenQuantizedStore(false, (EQuantizeType)(gqrValue & 0x7), (gqrValue & 0x3F00) >> 8);
		}
#else
		// We know what GQR is here, so we can load RSCRATCH2 and call into the store method directly
		// with just the scale bits.
		int type = gqrValue & 0x7;
		MOV(32, R(RSCRATCH2), Imm32(gqrValue & 0x3F00));

		if (w)
		{
			// One value
			CVTSD2SS(XMM0, fpr.R(s));
			CALL(asm_routines.singleStoreQuantized[type]);
		}
		else
		{
			// Pair of values
			CVTPD2PS(XMM0, fpr.R(s));
			CALL(asm_routines.pairedStoreQuantized[type]);
		}
#endif
	}
	else
	{
		// Some games (e.g. Dirt 2) incorrectly set the unused bits which breaks the lookup table code.
		// Hence, we need to mask out the unused bits. The layout of the GQR register is
		// UU[SCALE]UUUUU[TYPE] where SCALE is 6 bits and TYPE is 3 bits, so we have to AND with
		// 0b0011111100000111, or 0x3F07.
		MOV(32, R(RSCRATCH2), Imm32(0x3F07));
		AND(32, R(RSCRATCH2), PPCSTATE(spr[SPR_GQR0 + i]));
		MOVZX(32, 8, RSCRATCH, R(RSCRATCH2));

		if (w)
		{
			// One value
			CVTSD2SS(XMM0, fpr.R(s));
			CALLptr(MScaled(RSCRATCH, SCALE_8, (u32)(u64)asm_routines.singleStoreQuantized));
		}
		else
		{
			// Pair of values
			CVTPD2PS(XMM0, fpr.R(s));
			CALLptr(MScaled(RSCRATCH, SCALE_8, (u32)(u64)asm_routines.pairedStoreQuantized));
		}
	}

	if (update && jo.memcheck)
	{
		MemoryExceptionCheck();
		if (indexed)
			ADD(32, gpr.R(a), gpr.R(b));
		else
			ADD(32, gpr.R(a), Imm32((u32)offset));
	}
	gpr.UnlockAll();
	gpr.UnlockAllX();
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

	gpr.Lock(a, b);

	if (gqrIsConstant && gqrValue == 0)
	{
		s32 loadOffset = 0;
		gpr.BindToRegister(a, true, update);
		X64Reg addr = gpr.RX(a);
		if (update && jo.memcheck)
		{
			addr = RSCRATCH2;
			MOV(32, R(addr), gpr.R(a));
		}
		if (indexed)
		{
			if (update)
			{
				ADD(32, R(addr), gpr.R(b));
			}
			else
			{
				addr = RSCRATCH2;
				if (a && gpr.R(a).IsSimpleReg() && gpr.R(b).IsSimpleReg())
				{
					LEA(32, addr, MRegSum(gpr.RX(a), gpr.RX(b)));
				}
				else
				{
					MOV(32, R(addr), gpr.R(b));
					if (a)
						ADD(32, R(addr), gpr.R(a));
				}
			}
		}
		else
		{
			if (update)
				ADD(32, R(addr), Imm32(offset));
			else
				loadOffset = offset;
		}

		fpr.Lock(s);
		if (jo.memcheck)
		{
			fpr.StoreFromRegister(s);
			js.revertFprLoad = s;
		}
		fpr.BindToRegister(s, false);

		// Let's mirror the JitAsmCommon code and assume all non-MMU loads go to RAM.
		if (!jo.memcheck)
		{
			if (w)
			{
				if (cpu_info.bSSSE3)
				{
					MOVD_xmm(XMM0, MComplex(RMEM, addr, SCALE_1, loadOffset));
					PSHUFB(XMM0, M(pbswapShuffle1x4));
					UNPCKLPS(XMM0, M(m_one));
				}
				else
				{
					LoadAndSwap(32, RSCRATCH, MComplex(RMEM, addr, SCALE_1, loadOffset));
					MOVD_xmm(XMM0, R(RSCRATCH));
					UNPCKLPS(XMM0, M(m_one));
				}
			}
			else
			{
				if (cpu_info.bSSSE3)
				{
					MOVQ_xmm(XMM0, MComplex(RMEM, addr, SCALE_1, loadOffset));
					PSHUFB(XMM0, M(pbswapShuffle2x4));
				}
				else
				{
					LoadAndSwap(64, RSCRATCH, MComplex(RMEM, addr, SCALE_1, loadOffset));
					ROL(64, R(RSCRATCH), Imm8(32));
					MOVQ_xmm(XMM0, R(RSCRATCH));
				}
			}
			CVTPS2PD(fpr.RX(s), R(XMM0));
		}
		else
		{
			BitSet32 registersInUse = CallerSavedRegistersInUse();
			registersInUse[fpr.RX(s) << 16] = false;
			if (update)
				registersInUse[addr] = true;
			SafeLoadToReg(RSCRATCH, R(addr), w ? 32 : 64, loadOffset, registersInUse, false);
			MemoryExceptionCheck();
			if (w)
			{
				MOVD_xmm(XMM0, R(RSCRATCH));
				UNPCKLPS(XMM0, M(m_one));
			}
			else
			{
				ROL(64, R(RSCRATCH), Imm8(32));
				MOVQ_xmm(XMM0, R(RSCRATCH));
			}
			CVTPS2PD(fpr.RX(s), R(XMM0));
			if (update)
				MOV(32, gpr.R(a), R(addr));
		}
		gpr.UnlockAll();
		fpr.UnlockAll();
		return;
	}
	gpr.FlushLockX(RSCRATCH_EXTRA);
	gpr.BindToRegister(a, true, update);
	fpr.BindToRegister(s, false, true);
	if (gpr.R(a).IsSimpleReg() && gpr.R(b).IsSimpleReg() && (indexed || offset))
	{
		if (indexed)
			LEA(32, RSCRATCH_EXTRA, MRegSum(gpr.RX(a), gpr.RX(b)));
		else
			LEA(32, RSCRATCH_EXTRA, MDisp(gpr.RX(a), offset));
	}
	else
	{
		MOV(32, R(RSCRATCH_EXTRA), gpr.R(a));
		if (indexed)
			ADD(32, R(RSCRATCH_EXTRA), gpr.R(b));
		else if (offset)
			ADD(32, R(RSCRATCH_EXTRA), Imm32((u32)offset));
	}
	// In memcheck mode, don't update the address until the exception check
	if (update && !jo.memcheck)
		MOV(32, gpr.R(a), R(RSCRATCH_EXTRA));

	if (gqrIsConstant)
	{
		GenQuantizedLoad(w == 1, (EQuantizeType)(gqrValue & 0x7), (gqrValue & 0x3F00) >> 8);
	}
	else
	{
		MOV(32, R(RSCRATCH2), Imm32(0x3F07));

		// Get the high part of the GQR register
		OpArg gqr = PPCSTATE(spr[SPR_GQR0 + i]);
		gqr.AddMemOffset(2);

		AND(32, R(RSCRATCH2), gqr);
		MOVZX(32, 8, RSCRATCH, R(RSCRATCH2));

		CALLptr(MScaled(RSCRATCH, SCALE_8, (u32)(u64)(&asm_routines.pairedLoadQuantized[w * 8])));
	}

	MemoryExceptionCheck();
	CVTPS2PD(fpr.RX(s), R(XMM0));
	if (update && jo.memcheck)
	{
		if (indexed)
			ADD(32, gpr.R(a), gpr.R(b));
		else
			ADD(32, gpr.R(a), Imm32((u32)offset));
	}

	gpr.UnlockAll();
	gpr.UnlockAllX();
}
