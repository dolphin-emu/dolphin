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
#include "x64Emitter.h"
#include "ABI.h"

#include "Jit.h"
#include "JitAsm.h"
#include "JitRegCache.h"

void Jit64::lbzx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)
	if (Core::g_CoreStartupParameter.bJITLoadStorelbzxOff)
	{ Default(inst); return; }

	int a = inst.RA, b = inst.RB, d = inst.RD;
	gpr.Lock(a, b, d);
	gpr.FlushLockX(ABI_PARAM1);
	if (b == d || a == d)
		gpr.LoadToX64(d, true, true);
	else 
		gpr.LoadToX64(d, false, true);
	MOV(32, R(ABI_PARAM1), gpr.R(b));
	if (a)
		ADD(32, R(ABI_PARAM1), gpr.R(a));
#if 0
	SafeLoadRegToEAX(ABI_PARAM1, 8, 0);
	MOV(32, gpr.R(d), R(EAX));
#else
	UnsafeLoadRegToReg(ABI_PARAM1, gpr.RX(d), 8, 0, false);
#endif
	gpr.UnlockAll();
	gpr.UnlockAllX();
}

void Jit64::lwzx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)

	int a = inst.RA, b = inst.RB, d = inst.RD;
	gpr.Lock(a, b, d);
	gpr.FlushLockX(ABI_PARAM1);
	if (b == d || a == d)
		gpr.LoadToX64(d, true, true);
	else 
		gpr.LoadToX64(d, false, true);
	MOV(32, R(ABI_PARAM1), gpr.R(b));
	if (a)
		ADD(32, R(ABI_PARAM1), gpr.R(a));
#if 1
	SafeLoadRegToEAX(ABI_PARAM1, 32, 0);
	MOV(32, gpr.R(d), R(EAX));
#else
	UnsafeLoadRegToReg(ABI_PARAM1, gpr.RX(d), 32, 0, false);
#endif
	gpr.UnlockAll();
	gpr.UnlockAllX();
}

void Jit64::lhax(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)

	int a = inst.RA, b = inst.RB, d = inst.RD;
	gpr.Lock(a, b, d);
	gpr.FlushLockX(ABI_PARAM1);
	if (b == d || a == d)
		gpr.LoadToX64(d, true, true);
	else 
		gpr.LoadToX64(d, false, true);
	MOV(32, R(ABI_PARAM1), gpr.R(b));
	if (a)
		ADD(32, R(ABI_PARAM1), gpr.R(a));

	// Some homebrew actually loads from a hw reg with this instruction
	SafeLoadRegToEAX(ABI_PARAM1, 16, 0, true);
	MOV(32, gpr.R(d), R(EAX));

	gpr.UnlockAll();
	gpr.UnlockAllX();
}

void Jit64::lXz(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)
	if (Core::g_CoreStartupParameter.bJITLoadStorelXzOff)
	{ Default(inst); return; }

	int d = inst.RD;
	int a = inst.RA;

	// TODO(ector): Make it dynamically enable/disable idle skipping where appropriate
	// Will give nice boost to dual core mode
	// (mb2): I agree, 
	// IMHO those Idles should always be skipped and replaced by a more controllable "native" Idle methode
	// ... maybe the throttle one already do that :p
	// if (CommandProcessor::AllowIdleSkipping() && PixelEngine::AllowIdleSkipping())
	if (Core::GetStartupParameter().bSkipIdle &&
		inst.OPCD == 32 && 
		(inst.hex & 0xFFFF0000) == 0x800D0000 &&
		(Memory::ReadUnchecked_U32(js.compilerPC + 4) == 0x28000000 ||
		(Core::GetStartupParameter().bWii && Memory::ReadUnchecked_U32(js.compilerPC + 4) == 0x2C000000)) &&
		Memory::ReadUnchecked_U32(js.compilerPC + 8) == 0x4182fff8)
	{
		// TODO(LinesPrower): 			
		// - Rewrite this!
		// It seems to be ugly and unefficient, but I don't know JIT stuff enough to make it right
		// It only demonstrates the idea

		// do our job at first
		s32 offset = (s32)(s16)inst.SIMM_16;
		gpr.FlushLockX(ABI_PARAM1);
		gpr.Lock(d, a);
		MOV(32, R(ABI_PARAM1), gpr.R(a));
		SafeLoadRegToEAX(ABI_PARAM1, 32, offset);
		gpr.LoadToX64(d, false, true);
		MOV(32, gpr.R(d), R(EAX));
		gpr.UnlockAll();
		gpr.UnlockAllX();

		gpr.Flush(FLUSH_ALL); 

		// if it's still 0, we can wait until the next event
		CMP(32, R(RAX), Imm32(0));
		FixupBranch noIdle = J_CC(CC_NE);

		gpr.Flush(FLUSH_ALL);
		fpr.Flush(FLUSH_ALL);
		ABI_CallFunctionC((void *)&PowerPC::OnIdle, PowerPC::ppcState.gpr[a] + (s32)(s16)inst.SIMM_16);

		// ! we must continue executing of the loop after exception handling, maybe there is still 0 in r0
		//MOV(32, M(&PowerPC::ppcState.pc), Imm32(js.compilerPC));
		JMP(asm_routines.testExceptions, true);			

		SetJumpTarget(noIdle);

		//js.compilerPC += 8;
		return;
	}

	// R2 always points to the small read-only data area. We could bake R2-relative loads into immediates.
	// R13 always points to the small read/write data area. Not so exciting but at least could drop checks in 32-bit safe mode.

	s32 offset = (s32)(s16)inst.SIMM_16;
	if (!a) 
	{
		Default(inst);
		return;
	}
	int accessSize;
	switch (inst.OPCD)
	{
	case 32:
		accessSize = 32;
		if (Core::g_CoreStartupParameter.bJITLoadStorelwzOff) {Default(inst); return;}
		break; //lwz	
	case 40: accessSize = 16; break; //lhz
	case 34: accessSize = 8;  break; //lbz
	default:
		//_assert_msg_(DYNA_REC, 0, "lXz: invalid access size");
		PanicAlert("lXz: invalid access size");
		return;
	}

	//Still here? Do regular path.
#if defined(_M_X64)
	if (accessSize == 8 || accessSize == 16 || !jo.enableFastMem) {
#else
	if (true) {
#endif
		// Safe and boring
		gpr.FlushLockX(ABI_PARAM1);
		gpr.Lock(d, a);
		MOV(32, R(ABI_PARAM1), gpr.R(a));
		SafeLoadRegToEAX(ABI_PARAM1, accessSize, offset);
		gpr.LoadToX64(d, false, true);
		MOV(32, gpr.R(d), R(EAX));
		gpr.UnlockAll();
		gpr.UnlockAllX();
		return;
	}

	// Fast and daring/failing
	gpr.Lock(a, d);
	gpr.LoadToX64(a, true, false);
	gpr.LoadToX64(d, a == d, true);
	MOV(accessSize, gpr.R(d), MComplex(RBX, gpr.R(a).GetSimpleReg(), SCALE_1, offset));
	switch (accessSize) {
		case 32:
			BSWAP(32, gpr.R(d).GetSimpleReg());
			break;
			// Careful in the backpatch - need to properly nop over first
			//		case 16:
			//			BSWAP(32, gpr.R(d).GetSimpleReg());
			//			SHR(32, gpr.R(d), Imm8(16));
			//			break;
	}
	gpr.UnlockAll();
}

void Jit64::lha(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)

	int d = inst.RD;
	int a = inst.RA;
	s32 offset = (s32)(s16)inst.SIMM_16;
	// Safe and boring
	gpr.FlushLockX(ABI_PARAM1);
	gpr.Lock(d, a);
	MOV(32, R(ABI_PARAM1), gpr.R(a));
	SafeLoadRegToEAX(ABI_PARAM1, 16, offset, true);
	gpr.LoadToX64(d, d == a, true);
	MOV(32, gpr.R(d), R(EAX));
	gpr.UnlockAll();
	gpr.UnlockAllX();
	return;
}

void Jit64::lwzux(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)

	int a = inst.RA, b = inst.RB, d = inst.RD;
	if (!a || a == d || a == b)
	{
		Default(inst);
		return;
	}
	gpr.Lock(a, b, d);

	gpr.LoadToX64(d, b == d, true);
	gpr.LoadToX64(a, true, true);
	ADD(32, gpr.R(a), gpr.R(b));
	MOV(32, R(EAX), gpr.R(a));
	SafeLoadRegToEAX(EAX, 32, 0, false);
	MOV(32, gpr.R(d), R(EAX));

	gpr.UnlockAll();
	return;
}

// Zero cache line.
void Jit64::dcbz(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)

	Default(inst); return;

	MOV(32, R(EAX), gpr.R(inst.RB));
	if (inst.RA)
		ADD(32, R(EAX), gpr.R(inst.RA));
	AND(32, R(EAX), Imm32(~31));
	XORPD(XMM0, R(XMM0));
#ifdef _M_X64
	MOVAPS(MComplex(EBX, EAX, SCALE_1, 0), XMM0);
	MOVAPS(MComplex(EBX, EAX, SCALE_1, 16), XMM0);
#else
	AND(32, R(EAX), Imm32(Memory::MEMVIEW32_MASK));
	MOVAPS(MDisp(EAX, (u32)Memory::base), XMM0);
	MOVAPS(MDisp(EAX, (u32)Memory::base + 16), XMM0);
#endif
}

void Jit64::stX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)

	int s = inst.RS;
	int a = inst.RA;

	bool update = inst.OPCD & 1;

	s32 offset = (s32)(s16)inst.SIMM_16;
	if (a || update) 
	{
		int accessSize;
		switch (inst.OPCD & ~1)
		{
		case 36: accessSize = 32; break; //stw
		case 44: accessSize = 16; break; //sth
		case 38: accessSize = 8; break;  //stb
		default: _assert_msg_(DYNA_REC, 0, "AWETKLJASDLKF"); return;
		}

		if (gpr.R(a).IsImm())
		{
			// If we already know the address through constant folding, we can do some
			// fun tricks...
			u32 addr = (u32)gpr.R(a).offset;
			addr += offset;
			if ((addr & 0xFFFFF000) == 0xCC008000 && jo.optimizeGatherPipe)
			{
				if (offset && update)
					gpr.SetImmediate32(a, addr);
				gpr.FlushLockX(ABI_PARAM1);
				MOV(32, R(ABI_PARAM1), gpr.R(s));
				switch (accessSize)
				{	
					// No need to protect these, they don't touch any state
					// question - should we inline them instead? Pro: Lose a CALL   Con: Code bloat
				case 8:  CALL((void *)asm_routines.fifoDirectWrite8);  break;
				case 16: CALL((void *)asm_routines.fifoDirectWrite16); break;
				case 32: CALL((void *)asm_routines.fifoDirectWrite32); break;
				}
				js.fifoBytesThisBlock += accessSize >> 3;
				gpr.UnlockAllX();
				return;
			}
			else if (Memory::IsRAMAddress(addr) && accessSize == 32)
			{
				if (offset && update)
					gpr.SetImmediate32(a, addr);
				MOV(accessSize, R(EAX), gpr.R(s));
				BSWAP(accessSize, EAX);
				WriteToConstRamAddress(accessSize, R(EAX), addr); 
				return;
			}
			// Other IO not worth the trouble.
		}

		// Optimized stack access?
		if (accessSize == 32 && !gpr.R(a).IsImm() && a == 1 && js.st.isFirstBlockOfFunction && jo.optimizeStack)
		{
			gpr.FlushLockX(ABI_PARAM1);
			MOV(32, R(ABI_PARAM1), gpr.R(a));
			MOV(32, R(EAX), gpr.R(s));
			BSWAP(32, EAX);
#ifdef _M_X64	
			MOV(accessSize, MComplex(RBX, ABI_PARAM1, SCALE_1, (u32)offset), R(EAX));
#elif _M_IX86
			AND(32, R(ABI_PARAM1), Imm32(Memory::MEMVIEW32_MASK));
			MOV(accessSize, MDisp(ABI_PARAM1, (u32)Memory::base + (u32)offset), R(EAX));
#endif
			if (update)
				ADD(32, gpr.R(a), Imm32(offset));
			gpr.UnlockAllX();
			return;
		}

		/* // TODO - figure out why Beyond Good and Evil hates this
		#ifdef _M_X64
		if (accessSize == 32 && !update && jo.enableFastMem)
		{
		// Fast and daring - requires 64-bit
		MOV(32, R(EAX), gpr.R(s));
		gpr.LoadToX64(a, true, false);
		BSWAP(32, EAX);
		MOV(accessSize, MComplex(RBX, gpr.RX(a), SCALE_1, (u32)offset), R(EAX));
		return;
		}
		#endif*/

		//Still here? Do regular path.
		gpr.Lock(s, a);
		gpr.FlushLockX(ECX, EDX);
		MOV(32, R(EDX), gpr.R(a));
		MOV(32, R(ECX), gpr.R(s));
		if (offset)
			ADD(32, R(EDX), Imm32((u32)offset));
		if (update && offset)
		{
			gpr.LoadToX64(a, true, true);
			MOV(32, gpr.R(a), R(EDX));
		}
		TEST(32, R(EDX), Imm32(0x0C000000));
		FixupBranch unsafe_addr = J_CC(CC_NZ);
		BSWAP(accessSize, ECX);
#ifdef _M_X64
		MOV(accessSize, MComplex(RBX, EDX, SCALE_1, 0), R(ECX));
#else
		AND(32, R(EDX), Imm32(Memory::MEMVIEW32_MASK));
		MOV(accessSize, MDisp(EDX, (u32)Memory::base), R(ECX));
#endif
		FixupBranch skip_call = J();
		SetJumpTarget(unsafe_addr);
		switch (accessSize)
		{
		case 32: ABI_CallFunctionRR(thunks.ProtectFunction((void *)&Memory::Write_U32, 2), ECX, EDX); break;
		case 16: ABI_CallFunctionRR(thunks.ProtectFunction((void *)&Memory::Write_U16, 2), ECX, EDX); break;
		case 8:  ABI_CallFunctionRR(thunks.ProtectFunction((void *)&Memory::Write_U8,  2), ECX, EDX); break;
		}
		SetJumpTarget(skip_call);
		gpr.UnlockAll();
		gpr.UnlockAllX();
	}
	else
	{
		Default(inst);
	}
}

void Jit64::stXx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)

	int a = inst.RA, b = inst.RB, s = inst.RS;
	if (!a || a == s || a == b)
	{
		Default(inst);
		return;
	}
	gpr.Lock(a, b, s);
	gpr.FlushLockX(ECX, EDX);

	if (inst.SUBOP10 & 32) {
		gpr.LoadToX64(a, true, true);
		ADD(32, gpr.R(a), gpr.R(b));
		MOV(32, R(EDX), gpr.R(a));
	} else {
		MOV(32, R(EDX), gpr.R(a));
		ADD(32, R(EDX), gpr.R(b));
	}
	unsigned accessSize;
	switch (inst.SUBOP10 & ~32) {
		case 151: accessSize = 32; break;
		case 407: accessSize = 16; break;
		case 215: accessSize = 8; break;
	}

	MOV(32, R(ECX), gpr.R(s));
	SafeWriteRegToReg(ECX, EDX, accessSize, 0);

	gpr.UnlockAll();
	gpr.UnlockAllX();
	return;
}

// A few games use these heavily in video codecs.
void Jit64::lmw(UGeckoInstruction inst)
{
#ifdef _M_IX86
	Default(inst); return;
#else
	gpr.FlushLockX(ECX);
	MOV(32, R(EAX), Imm32((u32)(s32)inst.SIMM_16));
	if (inst.RA)
		ADD(32, R(EAX), gpr.R(inst.RA));
	for (int i = inst.RD; i < 32; i++)
	{
		MOV(32, R(ECX), MComplex(EBX, EAX, SCALE_1, (i - inst.RD) * 4));
		BSWAP(32, ECX);
		gpr.LoadToX64(i, false, true);
		MOV(32, gpr.R(i), R(ECX));
	}
	gpr.UnlockAllX();
#endif
}

void Jit64::stmw(UGeckoInstruction inst)
{
#ifdef _M_IX86
	Default(inst); return;
#else
	gpr.FlushLockX(ECX);
	MOV(32, R(EAX), Imm32((u32)(s32)inst.SIMM_16));
	if (inst.RA)
		ADD(32, R(EAX), gpr.R(inst.RA));
	for (int i = inst.RD; i < 32; i++)
	{
		MOV(32, R(ECX), gpr.R(i));
		BSWAP(32, ECX);
		MOV(32, MComplex(EBX, EAX, SCALE_1, (i - inst.RD) * 4), R(ECX));
	}
	gpr.UnlockAllX();
#endif
}

void Jit64::icbi(UGeckoInstruction inst)
{
	Default(inst);
	WriteExit(js.compilerPC + 4, 0);
}
