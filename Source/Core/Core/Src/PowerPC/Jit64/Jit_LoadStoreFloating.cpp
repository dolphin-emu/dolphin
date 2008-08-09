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
#include "../../Core.h"
#include "../../HW/GPFifo.h"
#include "../../HW/CommandProcessor.h"
#include "../../HW/PixelEngine.h"
#include "../../HW/Memmap.h"
#include "../PPCTables.h"
#include "x64Emitter.h"
#include "ABI.h"

#include "Jit.h"
#include "JitCache.h"
#include "JitAsm.h"
#include "JitRegCache.h"

// #define INSTRUCTION_START Default(inst); return;
#define INSTRUCTION_START

#ifdef _M_IX86
#define DISABLE_32BIT Default(inst); return;
#else
#define DISABLE_32BIT ;
#endif

namespace Jit64
{

static u64 GC_ALIGNED16(temp64);
static u32 GC_ALIGNED16(temp32);

// TODO: Add peephole optimizations for multiple consecutive lfd/lfs/stfd/stfs since they are so common,
// and pshufb could help a lot.
// Also add hacks for things like lfs/stfs the same reg consecutively, that is, simple memory moves.

void lfs(UGeckoInstruction inst)
{
	INSTRUCTION_START;
	int d = inst.RD;
	int a = inst.RA;
	if (!a) 
	{
		Default(inst);
		return;
	}
	s32 offset = (s32)(s16)inst.SIMM_16;

	gpr.Flush(FLUSH_VOLATILE);
	gpr.Lock(d, a);
	
	MOV(32, R(ABI_PARAM1), gpr.R(a));
	if (!jo.noAssumeFPLoadFromMem)
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
	fpr.UnlockAll();
}

void lfd(UGeckoInstruction inst)
{
	INSTRUCTION_START;
	DISABLE_32BIT;
	int d = inst.RD;
	int a = inst.RA;
	if (!a) 
	{
		Default(inst);
		return;
	}
	s32 offset = (s32)(s16)inst.SIMM_16;
	gpr.Lock(a);
	MOV(32, R(ABI_PARAM1), gpr.R(a));
	MOV(64, R(EAX), MComplex(RBX, ABI_PARAM1, SCALE_1, offset));
	BSWAP(64, EAX);
	MOV(64, M(&temp64), R(EAX));
	fpr.Lock(d);
	fpr.LoadToX64(d, false);
	MOVDDUP(fpr.RX(d), M(&temp64));
	gpr.UnlockAll();
	fpr.UnlockAll();
}

void stfd(UGeckoInstruction inst)
{
	INSTRUCTION_START;
	DISABLE_32BIT;
	int s = inst.RS;
	int a = inst.RA;
	if (!a)
	{
		Default(inst);
		return;
	}
	s32 offset = (s32)(s16)inst.SIMM_16;
	gpr.Lock(a);
	fpr.Lock(s);
	fpr.LoadToX64(s, true, false);
	MOVSD(M(&temp64), fpr.RX(s));
	MOV(32, R(ABI_PARAM1), gpr.R(a));
	MOV(64, R(EAX), M(&temp64));
	BSWAP(64, EAX);
	MOV(64, MComplex(RBX, ABI_PARAM1, SCALE_1, offset), R(EAX));
	gpr.UnlockAll();
	fpr.UnlockAll();
}

void stfs(UGeckoInstruction inst)
{
	INSTRUCTION_START;
	bool update = inst.OPCD & 1;
	int s = inst.RS;
	int a = inst.RA;
	s32 offset = (s32)(s16)inst.SIMM_16;

	if (a && !update)
	{
		gpr.Flush(FLUSH_VOLATILE);
		gpr.Lock(a);
		fpr.Lock(s);
		gpr.LockX(ABI_PARAM1, ABI_PARAM2);
		MOV(32, R(ABI_PARAM2), gpr.R(a));
		if (update && offset)
		{
			MOV(32, gpr.R(a), R(ABI_PARAM2));
		}
		CVTSD2SS(XMM0, fpr.R(s));
		MOVSS(M(&temp32), XMM0);
		MOV(32, R(ABI_PARAM1), M(&temp32));

		SafeWriteRegToReg(ABI_PARAM1, ABI_PARAM2, 32, offset);
		gpr.UnlockAll();
		gpr.UnlockAllX();
		fpr.UnlockAll();
	}
	else
	{
		Default(inst);
	}
}

void lfsx(UGeckoInstruction inst)
{
	INSTRUCTION_START;
	fpr.Lock(inst.RS);
	fpr.LoadToX64(inst.RS, false, true);
	MOV(32, R(EAX), gpr.R(inst.RB));
	if (inst.RA)
		ADD(32, R(EAX), gpr.R(inst.RA));
	UnsafeLoadRegToReg(EAX, EAX, 32, false);
	MOV(32, M(&temp32), R(EAX));
	CVTSS2SD(XMM0, M(&temp32));
	MOVDDUP(fpr.R(inst.RS).GetSimpleReg(), R(XMM0));
	fpr.UnlockAll();
}

}  // namespace
