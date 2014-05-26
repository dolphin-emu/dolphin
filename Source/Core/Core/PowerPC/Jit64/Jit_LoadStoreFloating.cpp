// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// TODO(ector): Tons of pshufb optimization of the loads/stores, for SSSE3+, possibly SSE4, only.
// Should give a very noticeable speed boost to paired single heavy code.

#include "Common/Common.h"
#include "Common/CPUDetect.h"

#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/JitAsm.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"

namespace {

// pshufb todo: MOVQ
const u8 GC_ALIGNED16(bswapShuffle1x4[16]) = {3, 2, 1, 0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
const u8 GC_ALIGNED16(bswapShuffle1x8[16]) = {7, 6, 5, 4, 3, 2, 1, 0, 8, 9, 10, 11, 12, 13, 14, 15};
const u8 GC_ALIGNED16(bswapShuffle1x8Dupe[16]) = {7, 6, 5, 4, 3, 2, 1, 0, 7, 6, 5, 4, 3, 2, 1, 0};

u64 GC_ALIGNED16(temp64);

}

// TODO: Add peephole optimizations for multiple consecutive lfd/lfs/stfd/stfs since they are so common,
// and pshufb could help a lot.
// Also add hacks for things like lfs/stfs the same reg consecutively, that is, simple memory moves.

void Jit64::lfs(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff)

	int d = inst.RD;
	int a = inst.RA;
	if (!a)
	{
		FallBackToInterpreter(inst);
		return;
	}

	s32 offset = (s32)(s16)inst.SIMM_16;

	SafeLoadToReg(EAX, gpr.R(a), 32, offset, RegistersInUse(), false);

	MEMCHECK_START

	fpr.Lock(d);
	fpr.BindToRegister(d, false);
	ConvertSingleToDouble(fpr.RX(d), EAX, true);

	MEMCHECK_END

	fpr.UnlockAll();
}


void Jit64::lfd(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff)

	if (js.memcheck)
	{
		FallBackToInterpreter(inst);
		return;
	}

	int d = inst.RD;
	int a = inst.RA;
	if (!a)
	{
		FallBackToInterpreter(inst);
		return;
	}

	s32 offset = (s32)(s16)inst.SIMM_16;
	gpr.FlushLockX(ABI_PARAM1);
	gpr.Lock(a);
	MOV(32, R(ABI_PARAM1), gpr.R(a));
	// TODO - optimize. This has to load the previous value - upper double should stay unmodified.
	fpr.Lock(d);
	fpr.BindToRegister(d, true);
	X64Reg xd = fpr.RX(d);

	if (cpu_info.bSSSE3)
	{
#if _M_X86_64
		MOVQ_xmm(XMM0, MComplex(RBX, ABI_PARAM1, SCALE_1, offset));
#else
		AND(32, R(ABI_PARAM1), Imm32(Memory::MEMVIEW32_MASK));
		MOVQ_xmm(XMM0, MDisp(ABI_PARAM1, (u32)Memory::base + offset));
#endif
		PSHUFB(XMM0, M((void *)bswapShuffle1x8Dupe));
		MOVSD(xd, R(XMM0));
	} else {
#if _M_X86_64
		LoadAndSwap(64, EAX, MComplex(RBX, ABI_PARAM1, SCALE_1, offset));
		MOV(64, M(&temp64), R(EAX));

		MEMCHECK_START

		MOVSD(XMM0, M(&temp64));
		MOVSD(xd, R(XMM0));

		MEMCHECK_END
#else
		AND(32, R(ABI_PARAM1), Imm32(Memory::MEMVIEW32_MASK));
		MOV(32, R(EAX), MDisp(ABI_PARAM1, (u32)Memory::base + offset));
		BSWAP(32, EAX);
		MOV(32, M((void*)((u8 *)&temp64+4)), R(EAX));

		MEMCHECK_START

		MOV(32, R(EAX), MDisp(ABI_PARAM1, (u32)Memory::base + offset + 4));
		BSWAP(32, EAX);
		MOV(32, M(&temp64), R(EAX));
		MOVSD(XMM0, M(&temp64));
		MOVSD(xd, R(XMM0));

		MEMCHECK_END
#endif
	}

	gpr.UnlockAll();
	gpr.UnlockAllX();
	fpr.UnlockAll();
}


void Jit64::stfd(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff)

	if (js.memcheck)
	{
		FallBackToInterpreter(inst);
		return;
	}

	int s = inst.RS;
	int a = inst.RA;
	if (!a)
	{
		FallBackToInterpreter(inst);
		return;
	}

	u32 mem_mask = Memory::ADDR_MASK_HW_ACCESS;
	if (Core::g_CoreStartupParameter.bMMU ||
		Core::g_CoreStartupParameter.bTLBHack) {
			mem_mask |= Memory::ADDR_MASK_MEM1;
	}
#ifdef ENABLE_MEM_CHECK
	if (Core::g_CoreStartupParameter.bEnableDebugging)
	{
		mem_mask |= Memory::EXRAM_MASK;
	}
#endif

	gpr.FlushLockX(ABI_PARAM1);
	gpr.Lock(a);
	fpr.Lock(s);
	gpr.BindToRegister(a, true, false);

	s32 offset = (s32)(s16)inst.SIMM_16;
	LEA(32, ABI_PARAM1, MDisp(gpr.R(a).GetSimpleReg(), offset));
	TEST(32, R(ABI_PARAM1), Imm32(mem_mask));
	FixupBranch safe = J_CC(CC_NZ);

	// Fast routine
	if (cpu_info.bSSSE3) {
		MOVAPD(XMM0, fpr.R(s));
		PSHUFB(XMM0, M((void*)bswapShuffle1x8));
#if _M_X86_64
		MOVQ_xmm(MComplex(RBX, ABI_PARAM1, SCALE_1, 0), XMM0);
#else
		AND(32, R(ECX), Imm32(Memory::MEMVIEW32_MASK));
		MOVQ_xmm(MDisp(ABI_PARAM1, (u32)Memory::base), XMM0);
#endif
	} else {
		MOVAPD(XMM0, fpr.R(s));
		MOVD_xmm(R(EAX), XMM0);
		UnsafeWriteRegToReg(EAX, ABI_PARAM1, 32, 4);

		PSRLQ(XMM0, 32);
		MOVD_xmm(R(EAX), XMM0);
		UnsafeWriteRegToReg(EAX, ABI_PARAM1, 32, 0);
	}
	FixupBranch exit = J(true);
	SetJumpTarget(safe);

	// Safe but slow routine
	MOVAPD(XMM0, fpr.R(s));
	PSRLQ(XMM0, 32);
	MOVD_xmm(R(EAX), XMM0);
	SafeWriteRegToReg(EAX, ABI_PARAM1, 32, 0, RegistersInUse() | (1 << (16 + XMM0)));

	MOVAPD(XMM0, fpr.R(s));
	MOVD_xmm(R(EAX), XMM0);
	LEA(32, ABI_PARAM1, MDisp(gpr.R(a).GetSimpleReg(), offset));
	SafeWriteRegToReg(EAX, ABI_PARAM1, 32, 4, RegistersInUse());

	SetJumpTarget(exit);

	gpr.UnlockAll();
	gpr.UnlockAllX();
	fpr.UnlockAll();
}

// In Release on 32bit build,
// this seemed to cause a problem with PokePark2
// at start after talking to first pokemon,
// you run and smash a box, then he goes on about
// following him and then you cant do anything.
// I have enabled interpreter for this function
// in the mean time.
// Parlane
void Jit64::stfs(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff)

	bool update = inst.OPCD & 1;
	int s = inst.RS;
	int a = inst.RA;
	s32 offset = (s32)(s16)inst.SIMM_16;

	if (!a || update)
	{
		FallBackToInterpreter(inst);
		return;
	}

	fpr.BindToRegister(s, true, false);
	ConvertDoubleToSingle(XMM0, fpr.RX(s));

	if (gpr.R(a).IsImm())
	{
		u32 addr = (u32)(gpr.R(a).offset + offset);
		if (Memory::IsRAMAddress(addr))
		{
			if (cpu_info.bSSSE3) {
				PSHUFB(XMM0, M((void *)bswapShuffle1x4));
				WriteFloatToConstRamAddress(XMM0, addr);
				return;
			}
		}
		else if (addr == 0xCC008000)
		{
			// Float directly to write gather pipe! Fun!
			CALL((void*)asm_routines.fifoDirectWriteFloat);
			// TODO
			js.fifoBytesThisBlock += 4;
			return;
		}
	}

	gpr.FlushLockX(ABI_PARAM1, ABI_PARAM2);
	gpr.Lock(a);
	MOV(32, R(ABI_PARAM2), gpr.R(a));
	ADD(32, R(ABI_PARAM2), Imm32(offset));
	if (update && offset)
	{
		// We must flush immediate values from the following register because
		// it may take another value at runtime if no MMU exception has been raised
		gpr.KillImmediate(a, true, true);

		MEMCHECK_START

		MOV(32, gpr.R(a), R(ABI_PARAM2));

		MEMCHECK_END
	}
	SafeWriteFloatToReg(XMM0, ABI_PARAM2, RegistersInUse());
	gpr.UnlockAll();
	gpr.UnlockAllX();
	fpr.UnlockAll();
}


void Jit64::stfsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff)

	// We can take a shortcut here - it's not likely that a hardware access would use this instruction.
	gpr.FlushLockX(ABI_PARAM1);
	MOV(32, R(ABI_PARAM1), gpr.R(inst.RB));
	if (inst.RA)
		ADD(32, R(ABI_PARAM1), gpr.R(inst.RA));

	int s = inst.RS;
	fpr.Lock(s);
	fpr.BindToRegister(s, true, false);
	ConvertDoubleToSingle(XMM0, fpr.RX(s));
	MOVD_xmm(R(EAX), XMM0);
	SafeWriteRegToReg(EAX, ABI_PARAM1, 32, 0, RegistersInUse());

	gpr.UnlockAllX();
	fpr.UnlockAll();
}


void Jit64::lfsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff)

	MOV(32, R(EAX), gpr.R(inst.RB));
	if (inst.RA)
	{
		ADD(32, R(EAX), gpr.R(inst.RA));
	}
	fpr.Lock(inst.RS);
	fpr.BindToRegister(inst.RS, false);
	X64Reg s = fpr.RX(inst.RS);
	if (cpu_info.bSSSE3 && !js.memcheck) {
#if _M_X86_32
		AND(32, R(EAX), Imm32(Memory::MEMVIEW32_MASK));
		MOVD_xmm(XMM0, MDisp(EAX, (u32)Memory::base));
#else
		MOVD_xmm(XMM0, MComplex(RBX, EAX, SCALE_1, 0));
#endif
		PSHUFB(XMM0, M((void *)bswapShuffle1x4));
		ConvertSingleToDouble(s, XMM0);
	} else {
		SafeLoadToReg(EAX, R(EAX), 32, 0, RegistersInUse(), false);

		MEMCHECK_START

		ConvertSingleToDouble(s, EAX, true);

		MEMCHECK_END
	}
	fpr.UnlockAll();
}

