// Copyright (C) 2003-2008 Dolphin Project.

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

#include "../PowerPC.h"
#include "../../Core.h" // include "Common.h", "CoreParameter.h"
#include "../../HW/GPFifo.h"
#include "../../HW/CommandProcessor.h"
#include "../../HW/PixelEngine.h"
#include "../../HW/Memmap.h"
#include "../PPCTables.h"
#include "CPUDetect.h"
#include "x64Emitter.h"
#include "ABI.h"

#include "Jit.h"
#include "JitCache.h"
#include "JitAsm.h"
#include "JitRegCache.h"
#include "Jit_Util.h"

// #define INSTRUCTION_START Default(inst); return;
#define INSTRUCTION_START

namespace Jit64
{

// pshufb todo: MOVQ
const u8 GC_ALIGNED16(bswapShuffle1x4[16]) = {3, 2, 1, 0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
const u8 GC_ALIGNED16(bswapShuffle2x4[16]) = {3, 2, 1, 0, 7, 6, 5, 4, 8, 9, 10, 11, 12, 13, 14, 15};
const u8 GC_ALIGNED16(bswapShuffle1x8[16]) = {7, 6, 5, 4, 3, 2, 1, 0, 8, 9, 10, 11, 12, 13, 14, 15};
const u8 GC_ALIGNED16(bswapShuffle1x8Dupe[16]) = {7, 6, 5, 4, 3, 2, 1, 0, 7, 6, 5, 4, 3, 2, 1, 0};
const u8 GC_ALIGNED16(bswapShuffle2x8[16]) = {7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8};

namespace {
u64 GC_ALIGNED16(temp64);
u32 GC_ALIGNED16(temp32);
}
// TODO: Add peephole optimizations for multiple consecutive lfd/lfs/stfd/stfs since they are so common,
// and pshufb could help a lot.
// Also add hacks for things like lfs/stfs the same reg consecutively, that is, simple memory moves.

void lfs(UGeckoInstruction inst)
{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITLoadStoreFloatingOff)
			{Default(inst); return;} // turn off from debugger	
#endif
	INSTRUCTION_START;
	int d = inst.RD;
	int a = inst.RA;
	if (!a) 
	{
		Default(inst);
		return;
	}
	s32 offset = (s32)(s16)inst.SIMM_16;
	gpr.FlushLockX(ABI_PARAM1);
	gpr.Lock(a);
	MOV(32, R(ABI_PARAM1), gpr.R(a));
	if (jo.assumeFPLoadFromMem)
	{
		UnsafeLoadRegToReg(ABI_PARAM1, EAX, 32, offset, false);
	}
	else
	{
		SafeLoadRegToEAX(ABI_PARAM1, 32, offset);
	}

	MOV(32, M(&temp32), R(EAX));
	fpr.Lock(d);
	fpr.LoadToX64(d, false);
	CVTSS2SD(fpr.RX(d), M(&temp32));
	MOVDDUP(fpr.RX(d), fpr.R(d));
	gpr.UnlockAll();
	gpr.UnlockAllX();
	fpr.UnlockAll();
}


void lfd(UGeckoInstruction inst)
{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITLoadStoreFloatingOff)
			{Default(inst); return;} // turn off from debugger	
#endif
	INSTRUCTION_START;
	int d = inst.RD;
	int a = inst.RA;
	if (!a) 
	{
		Default(inst);
		return;
	}
	s32 offset = (s32)(s16)inst.SIMM_16;
	gpr.FlushLockX(ABI_PARAM1);
	gpr.Lock(a);
	MOV(32, R(ABI_PARAM1), gpr.R(a));
	// TODO - optimize. This has to load the previous value - upper double should stay unmodified.
	fpr.LoadToX64(d, true);
	fpr.Lock(d);
	X64Reg xd = fpr.RX(d);
	if (cpu_info.bSSSE3) {
#ifdef _M_X64
		MOVQ_xmm(XMM0, MComplex(RBX, ABI_PARAM1, SCALE_1, offset));
#else
		AND(32, R(ABI_PARAM1), Imm32(Memory::MEMVIEW32_MASK));
		MOVQ_xmm(XMM0, MDisp(ABI_PARAM1, (u32)Memory::base + offset));
#endif
		PSHUFB(XMM0, M((void *)bswapShuffle1x8Dupe));
		MOVSD(xd, R(XMM0));
	} else {
#ifdef _M_X64
		MOV(64, R(EAX), MComplex(RBX, ABI_PARAM1, SCALE_1, offset));
		BSWAP(64, EAX);
		MOV(64, M(&temp64), R(EAX));
		MOVSD(XMM0, M(&temp64));
		MOVSD(xd, R(XMM0));
#else
		AND(32, R(ABI_PARAM1), Imm32(Memory::MEMVIEW32_MASK));
		MOV(32, R(EAX), MDisp(ABI_PARAM1, (u32)Memory::base + offset));
		BSWAP(32, EAX);
		MOV(32, M((void*)((u32)&temp64+4)), R(EAX));
		MOV(32, R(EAX), MDisp(ABI_PARAM1, (u32)Memory::base + offset + 4));
		BSWAP(32, EAX);
		MOV(32, M(&temp64), R(EAX));
		MOVSD(XMM0, M(&temp64));
		MOVSD(xd, R(XMM0));
#if 0
		// Alternate implementation; possibly faster
		AND(32, R(ABI_PARAM1), Imm32(Memory::MEMVIEW32_MASK));
		MOVQ_xmm(XMM0, MDisp(ABI_PARAM1, (u32)Memory::base + offset));
		PSHUFLW(XMM0, R(XMM0), 0x1B);
		PSRLW(XMM0, 8);
		MOVSD(xd, R(XMM0));
		MOVQ_xmm(XMM0, MDisp(ABI_PARAM1, (u32)Memory::base + offset));
		PSHUFLW(XMM0, R(XMM0), 0x1B);
		PSLLW(XMM0, 8);
		POR(xd, R(XMM0));
#endif
#endif
	}
	gpr.UnlockAll();
	gpr.UnlockAllX();
	fpr.UnlockAll();
}


void stfd(UGeckoInstruction inst)
{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITLoadStoreFloatingOff)
			{Default(inst); return;} // turn off from debugger	
#endif
	INSTRUCTION_START;
	int s = inst.RS;
	int a = inst.RA;
	if (!a)
	{
		Default(inst);
		return;
	}
	s32 offset = (s32)(s16)inst.SIMM_16;
	gpr.FlushLockX(ABI_PARAM1);
	gpr.Lock(a);
	fpr.Lock(s);
	MOV(32, R(ABI_PARAM1), gpr.R(a));
#ifdef _M_IX86
	AND(32, R(ABI_PARAM1), Imm32(Memory::MEMVIEW32_MASK));
#endif
	if (cpu_info.bSSSE3) {
		MOVAPD(XMM0, fpr.R(s));
		PSHUFB(XMM0, M((void *)bswapShuffle1x8));
#ifdef _M_X64
		MOVQ_xmm(MComplex(RBX, ABI_PARAM1, SCALE_1, offset), XMM0);
#else
		MOVQ_xmm(MDisp(ABI_PARAM1, (u32)Memory::base + offset), XMM0);
#endif
	} else {
#ifdef _M_X64
		fpr.LoadToX64(s, true, false);
		MOVSD(M(&temp64), fpr.RX(s));
		MOV(64, R(EAX), M(&temp64));
		BSWAP(64, EAX);
		MOV(64, MComplex(RBX, ABI_PARAM1, SCALE_1, offset), R(EAX));
#else
		fpr.LoadToX64(s, true, false);
		MOVSD(M(&temp64), fpr.RX(s));
		MOV(32, R(EAX), M(&temp64));
		BSWAP(32, EAX);
		MOV(32, MDisp(ABI_PARAM1, (u32)Memory::base + offset + 4), R(EAX));
		MOV(32, R(EAX), M((void*)((u32)&temp64 + 4)));
		BSWAP(32, EAX);
		MOV(32, MDisp(ABI_PARAM1, (u32)Memory::base + offset), R(EAX));
#endif
	}
	gpr.UnlockAll();
	gpr.UnlockAllX();
	fpr.UnlockAll();
}


void stfs(UGeckoInstruction inst)
{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITLoadStoreFloatingOff)
			{Default(inst); return;} // turn off from debugger	
#endif
	INSTRUCTION_START;
	bool update = inst.OPCD & 1;
	int s = inst.RS;
	int a = inst.RA;
	s32 offset = (s32)(s16)inst.SIMM_16;
	if (!a || update) {
		Default(inst);
		return;
	}

	if (gpr.R(a).IsImm())
	{
		u32 addr = (u32)(gpr.R(a).offset + offset);
		if (Memory::IsRAMAddress(addr))
		{
			if (cpu_info.bSSSE3) {
				CVTSD2SS(XMM0, fpr.R(s));
				PSHUFB(XMM0, M((void *)bswapShuffle1x4));
				WriteFloatToConstRamAddress(XMM0, addr);
				return;
			}
		}
		else if (addr == 0xCC008000)
		{
			// Float directly to write gather pipe! Fun!
			CVTSD2SS(XMM0, fpr.R(s));
			CALL((void*)Asm::fifoDirectWriteFloat);
			// TODO
			js.fifoBytesThisBlock += 4;
			return;
		}
	}

	gpr.FlushLockX(ABI_PARAM1, ABI_PARAM2);
	gpr.Lock(a);
	fpr.Lock(s);
	MOV(32, R(ABI_PARAM2), gpr.R(a));
	ADD(32, R(ABI_PARAM2), Imm32(offset));
	if (update && offset)
	{
		MOV(32, gpr.R(a), R(ABI_PARAM2));
	}
	CVTSD2SS(XMM0, fpr.R(s));
	MOVSS(M(&temp32), XMM0);
	MOV(32, R(ABI_PARAM1), M(&temp32));
	SafeWriteRegToReg(ABI_PARAM1, ABI_PARAM2, 32, 0);
	gpr.UnlockAll();
	gpr.UnlockAllX();
	fpr.UnlockAll();
}


void stfsx(UGeckoInstruction inst)
{
	// We can take a shortcut here - it's not likely that a hardware access would use this instruction.
	INSTRUCTION_START;
#ifdef _M_X64
	Default(inst); return;
#endif
	gpr.FlushLockX(ABI_PARAM1);
	fpr.Lock(inst.RS);
	MOV(32, R(ABI_PARAM1), gpr.R(inst.RB));
	if (inst.RA)
		ADD(32, R(ABI_PARAM1), gpr.R(inst.RA));
	AND(32, R(ABI_PARAM1), Imm32(Memory::MEMVIEW32_MASK));
	CVTSD2SS(XMM0, fpr.R(inst.RS));
	MOVD_xmm(R(EAX), XMM0);
	BSWAP(32, EAX);
	MOV(32, MDisp(ABI_PARAM1, (u32)Memory::base), R(EAX));
	gpr.UnlockAllX();
	fpr.UnlockAll();
	return;
}


void lfsx(UGeckoInstruction inst)
{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITLoadStoreFloatingOff)
			{Default(inst); return;} // turn off from debugger	
#endif
	INSTRUCTION_START;
	fpr.Lock(inst.RS);
	fpr.LoadToX64(inst.RS, false, true);
	MOV(32, R(EAX), gpr.R(inst.RB));
	if (inst.RA)
		ADD(32, R(EAX), gpr.R(inst.RA));
	if (cpu_info.bSSSE3) {
		X64Reg r = fpr.R(inst.RS).GetSimpleReg();
#ifdef _M_IX86
		AND(32, R(EAX), Imm32(Memory::MEMVIEW32_MASK));
		MOVD_xmm(r, MDisp(EAX, (u32)Memory::base));
#else
		MOVD_xmm(r, MComplex(RBX, EAX, SCALE_1, 0));
#endif
		PSHUFB(r, M((void *)bswapShuffle1x4));
		CVTSS2SD(r, R(r));
		MOVDDUP(r, R(r));
	} else {
		UnsafeLoadRegToReg(EAX, EAX, 32, false);
		MOV(32, M(&temp32), R(EAX));
		CVTSS2SD(XMM0, M(&temp32));
		MOVDDUP(fpr.R(inst.RS).GetSimpleReg(), R(XMM0));
	}
	fpr.UnlockAll();
}

}  // namespace
