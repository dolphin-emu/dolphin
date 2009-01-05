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

void Jit64::lfs(UGeckoInstruction inst)
{
	IREmitter::InstLoc addr = ibuild.EmitIntConst(inst.SIMM_16), val;
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	val = ibuild.EmitDupSingleToMReg(ibuild.EmitLoadSingle(addr));
	ibuild.EmitStoreFReg(val, inst.RD);
	return;
}


void Jit64::lfd(UGeckoInstruction inst)
{
	if (Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITLoadStoreFloatingOff)
		{Default(inst); return;} // turn off from debugger	
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


void Jit64::stfd(UGeckoInstruction inst)
{
	if (Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITLoadStoreFloatingOff)
		{Default(inst); return;} // turn off from debugger	
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


void Jit64::stfs(UGeckoInstruction inst)
{
	IREmitter::InstLoc addr = ibuild.EmitIntConst(inst.SIMM_16),
			   val  = ibuild.EmitLoadFReg(inst.RS);
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	if (inst.OPCD & 1)
		ibuild.EmitStoreGReg(addr, inst.RA);
	val = ibuild.EmitDoubleToSingle(val);
	ibuild.EmitStoreSingle(val, addr);
	return;
}


void Jit64::stfsx(UGeckoInstruction inst)
{
	IREmitter::InstLoc addr = ibuild.EmitLoadGReg(inst.RB),
			   val  = ibuild.EmitLoadFReg(inst.RS);
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	val = ibuild.EmitDoubleToSingle(val);
	ibuild.EmitStoreSingle(val, addr);
	return;
}


void Jit64::lfsx(UGeckoInstruction inst)
{
	IREmitter::InstLoc addr = ibuild.EmitLoadGReg(inst.RB), val;
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	val = ibuild.EmitDupSingleToMReg(ibuild.EmitLoadSingle(addr));
	ibuild.EmitStoreFReg(val, inst.RD);
}

