// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// TODO(ector): Tons of pshufb optimization of the loads/stores, for SSSE3+, possibly SSE4, only.
// Should give a very noticable speed boost to paired single heavy code.

#include "Common/CommonTypes.h"

#include "Core/HW/DSP.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/JitAsm.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"

using namespace Gen;

void Jit64::lXXx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);

	int a = inst.RA, b = inst.RB, d = inst.RD;

	// Skip disabled JIT instructions
	FALLBACK_IF(SConfig::GetInstance().bJITLoadStorelbzxOff && (inst.OPCD == 31) && (inst.SUBOP10 == 87));
	FALLBACK_IF(SConfig::GetInstance().bJITLoadStorelXzOff && ((inst.OPCD == 34) || (inst.OPCD == 40) || (inst.OPCD == 32)));
	FALLBACK_IF(SConfig::GetInstance().bJITLoadStorelwzOff && (inst.OPCD == 32));

	auto ra = a ? regs.gpr.Lock(a) : regs.gpr.Zero();
	auto rb = inst.OPCD == 31 ? regs.gpr.Imm32((u32)(s32)(s16)inst.SIMM_16) : regs.gpr.Lock(b);
	auto rd = regs.gpr.Lock(d);

	// Determine memory access size and sign extend
	int accessSize = 0;
	bool signExtend = false;
	bool byte_reversed = false;
	switch (inst.OPCD)
	{
	case 32: // lwz
	case 33: // lwzu
		accessSize = 32;
		signExtend = false;
		break;

	case 34: // lbz
	case 35: // lbzu
		accessSize = 8;
		signExtend = false;
		break;

	case 40: // lhz
	case 41: // lhzu
		accessSize = 16;
		signExtend = false;
		break;

	case 42: // lha
	case 43: // lhau
		accessSize = 16;
		signExtend = true;
		break;

	case 31:
		switch (inst.SUBOP10)
		{
		case 534: // lwbrx
			byte_reversed = true;
		case 23: // lwzx
		case 55: // lwzux
			accessSize = 32;
			signExtend = false;
			break;

		case 87:  // lbzx
		case 119: // lbzux
			accessSize = 8;
			signExtend = false;
			break;
		case 790: // lhbrx
			byte_reversed = true;
		case 279: // lhzx
		case 311: // lhzux
			accessSize = 16;
			signExtend = false;
			break;

		case 343: // lhax
		case 375: // lhaux
			accessSize = 16;
			signExtend = true;
			break;

		default:
			PanicAlert("Invalid instruction");
		}
		break;

	default:
		PanicAlert("Invalid instruction");
	}

	// PowerPC has no 8-bit sign extended load, but x86 does, so merge extsb with the load if we find it.
	if (MergeAllowedNextInstructions(1) && accessSize == 8 && js.op[1].inst.OPCD == 31 && js.op[1].inst.SUBOP10 == 954 &&
	    js.op[1].inst.RS == inst.RD && js.op[1].inst.RA == inst.RD && !js.op[1].inst.Rc)
	{
		js.downcountAmount++;
		js.skipInstructions = 1;
		signExtend = true;
	}

	// TODO(ector): Make it dynamically enable/disable idle skipping where appropriate
	// Will give nice boost to dual core mode
	// (mb2): I agree,
	// IMHO those Idles should always be skipped and replaced by a more controllable "native" Idle methode
	// ... maybe the throttle one already do that :p
	// TODO: We shouldn't use a debug read here.  It should be possible to get
	// the following instructions out of the JIT state.
	if (SConfig::GetInstance().bSkipIdle &&
	    PowerPC::GetState() != PowerPC::CPU_STEPPING &&
	    inst.OPCD == 32 &&
	    MergeAllowedNextInstructions(2) &&
	    (inst.hex & 0xFFFF0000) == 0x800D0000 &&
	    (js.op[1].inst.hex == 0x28000000 ||
	    (SConfig::GetInstance().bWii && js.op[1].inst.hex == 0x2C000000)) &&
	    js.op[2].inst.hex == 0x4182fff8)
	{
		// TODO(LinesPrower):
		// - Rewrite this!
		// It seems to be ugly and inefficient, but I don't know JIT stuff enough to make it right
		// It only demonstrates the idea

		// do our job at first
		s32 offset = (s32)(s16)inst.SIMM_16;
		auto xd = rd.Bind(Jit64Reg::Write);
		SafeLoadToReg(xd, ra, accessSize, offset, CallerSavedRegistersInUse(), signExtend);

		// if it's still 0, we can wait until the next event
		TEST(32, xd, xd);
		FixupBranch noIdle = J_CC(CC_NZ);

		BitSet32 registersInUse = CallerSavedRegistersInUse();
		ABI_PushRegistersAndAdjustStack(registersInUse, 0);
		ABI_CallFunction((void *)&CoreTiming::Idle);
		ABI_PopRegistersAndAdjustStack(registersInUse, 0);

		// ! we must continue executing of the loop after exception handling, maybe there is still 0 in r0
		//MOV(32, PPCSTATE(pc), Imm32(js.compilerPC));
		WriteExceptionExit();

		SetJumpTarget(noIdle);

		//js.compilerPC += 8;
		return;
	}

	// Determine whether this instruction updates inst.RA
	bool update;
	if (inst.OPCD == 31)
		update = ((inst.SUBOP10 & 0x20) != 0) && (!rb.IsImm() || rb.Imm32() != 0);
	else
		update = ((inst.OPCD & 1) != 0) && inst.SIMM_16 != 0;

	auto xd = rd.Bind(Jit64Reg::Write);
	SafeLoad(xd, ra, rb, accessSize, signExtend, byte_reversed, update);
}

void Jit64::dcbx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);

	auto addr = regs.gpr.Borrow();
	auto value = regs.gpr.Borrow();
	auto tmp = regs.gpr.Borrow();

	auto ra = inst.RA ? regs.gpr.Lock(inst.RA) : regs.gpr.Zero();
	auto rb = regs.gpr.Lock(inst.RB);

	MOV_sum(32, addr, ra, rb);

	// Check whether a JIT cache line needs to be invalidated.
	LEA(32, value, MScaled(addr, SCALE_8, 0)); // addr << 3 (masks the first 3 bits)
	SHR(32, R(value), Imm8(3 + 5 + 5));        // >> 5 for cache line size, >> 5 for width of bitset
	MOV(64, R(tmp), ImmPtr(jit->GetBlockCache()->GetBlockBitSet()));
	MOV(32, R(value), MComplex(tmp, value, SCALE_4, 0));
	SHR(32, R(addr), Imm8(5));
	BT(32, R(value), R(addr));

	FixupBranch c = J_CC(CC_C, true);
	SwitchToFarCode();
	SetJumpTarget(c);
	BitSet32 registersInUse = CallerSavedRegistersInUse();
	ABI_PushRegistersAndAdjustStack(registersInUse, 0);
	// We might have ended up with the parameter in the right spot
	if ((X64Reg)addr != ABI_PARAM1)
		MOV(32, R(ABI_PARAM1), R(addr));
	SHL(32, R(ABI_PARAM1), Imm8(5));
	MOV(32, R(ABI_PARAM2), Imm32(32));
	XOR(32, R(ABI_PARAM3), R(ABI_PARAM3));
	ABI_CallFunction((void*)JitInterface::InvalidateICache);
	ABI_PopRegistersAndAdjustStack(registersInUse, 0);
	c = J(true);
	SwitchToNearCode();
	SetJumpTarget(c);

	// dcbi
	if (inst.SUBOP10 == 470)
	{
		// Flush DSP DMA if DMAState bit is set
		TEST(16, M(&DSP::g_dspState), Imm16(1 << 9));
		c = J_CC(CC_NZ, true);
		SwitchToFarCode();
		SetJumpTarget(c);
		ABI_PushRegistersAndAdjustStack(registersInUse, 0);
		SHL(32, R(addr), Imm8(5));
		ABI_CallFunctionR((void*)DSP::FlushInstantDMA, addr);
		ABI_PopRegistersAndAdjustStack(registersInUse, 0);
		c = J(true);
		SwitchToNearCode();
		SetJumpTarget(c);
	}
}

void Jit64::dcbt(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);

	// Prefetch. Since we don't emulate the data cache, we don't need to do anything.

	// If a dcbst follows a dcbt, it probably isn't a case of dynamic code
	// modification, so don't bother invalidating the jit block cache.
	// This is important because invalidating the block cache when we don't
	// need to is terrible for performance.
	// (Invalidating the jit block cache on dcbst is a heuristic.)
	if (MergeAllowedNextInstructions(1) &&
	    js.op[1].inst.OPCD == 31 && js.op[1].inst.SUBOP10 == 54 &&
	    js.op[1].inst.RA == inst.RA && js.op[1].inst.RB == inst.RB)
	{
		js.skipInstructions = 1;
	}
}

// Zero cache line.
void Jit64::dcbz(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);
	if (SConfig::GetInstance().bDCBZOFF)
		return;

	int a = inst.RA;
	int b = inst.RB;

	auto ra = regs.gpr.Lock(a);
	auto rb = regs.gpr.Lock(b);
	auto scratch = regs.gpr.Borrow();

	u32 mem_mask = Memory::ADDR_MASK_HW_ACCESS;

	// The following masks the region used by the GC/Wii virtual memory lib
	mem_mask |= Memory::ADDR_MASK_MEM1;

	MOV_sum(32, scratch, ra, rb);
	AND(32, scratch, Imm32(~31));
	TEST(32, scratch, Imm32(mem_mask));
	FixupBranch slow = J_CC(CC_NZ, true);

	// Should this code ever run? I can't find any games that use DCBZ on non-physical addresses, but
	// supposedly there are, at least for some MMU titles. Let's be careful and support it to be sure.
	SwitchToFarCode();
		SetJumpTarget(slow);
		MOV(32, M(&PC), Imm32(jit->js.compilerPC));
		BitSet32 registersInUse = CallerSavedRegistersInUse();
		ABI_PushRegistersAndAdjustStack(registersInUse, 0);
		ABI_CallFunctionR((void *)&PowerPC::ClearCacheLine, scratch);
		ABI_PopRegistersAndAdjustStack(registersInUse, 0);
		FixupBranch exit = J(true);
	SwitchToNearCode();

	// Mask out the address so we don't write to MEM1 out of bounds
	// FIXME: Work out why the AGP disc writes out of bounds
	if (!SConfig::GetInstance().bWii)
		AND(32, scratch, Imm32(Memory::RAM_MASK));
	PXOR(XMM0, R(XMM0));
	MOVAPS(MComplex(RMEM, scratch, SCALE_1, 0), XMM0);
	MOVAPS(MComplex(RMEM, scratch, SCALE_1, 16), XMM0);
	SetJumpTarget(exit);
}

void Jit64::stX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);

	int s = inst.RS;
	int a = inst.RA;
	s32 offset = (s32)(s16)inst.SIMM_16;
	bool update = (inst.OPCD & 1) && offset;

	if (!a && update)
		PanicAlert("Invalid stX");

	int accessSize;
	switch (inst.OPCD & ~1)
	{
	case 36: // stw
		accessSize = 32;
		break;
	case 44: // sth
		accessSize = 16;
		break;
	case 38: // stb
		accessSize = 8;
		break;
	default:
		_assert_msg_(DYNA_REC, 0, "stX: Invalid access size.");
		return;
	}

	auto rs = regs.gpr.Lock(s);
	auto ra = a ? regs.gpr.Lock(a) : regs.gpr.Zero();
	auto ofs = regs.gpr.Imm32(offset);
	SafeWrite(rs, ra, ofs, accessSize, false, update);
}

void Jit64::stXx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);

	int a = inst.RA, b = inst.RB, s = inst.RS;
	bool update = !!(inst.SUBOP10 & 32);
	bool byte_reverse = !!(inst.SUBOP10 & 512);

	auto ra = regs.gpr.Lock(a);
	auto rb = regs.gpr.Lock(b);
	auto rs = regs.gpr.Lock(s);

	int accessSize;
	switch (inst.SUBOP10 & ~32)
	{
		case 151:
		case 662:
			accessSize = 32;
			break;
		case 407:
		case 918:
			accessSize = 16;
			break;
		case 215:
			accessSize = 8;
			break;
		default:
			PanicAlert("stXx: invalid access size");
			accessSize = 0;
			break;
	}

	SafeWrite(rs, ra, rb, accessSize, byte_reverse, update);
}

// A few games use these heavily in video codecs.
void Jit64::lmw(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);

	auto ra = inst.RA ? regs.gpr.Lock(inst.RA) : regs.gpr.Zero();

	// (this needs some thoughts on perf)

	// TODO: This doesn't handle rollback on DSI correctly
	for (int i = inst.RD; i < 32; i++)
	{
		auto reg = regs.gpr.Lock(i).Bind(Jit64Reg::Write);
		u32 offset = (i - inst.RD) * 4 + (u32)(s32)inst.SIMM_16;
		auto ofs = regs.gpr.Imm32(offset);

		SafeLoad(reg, ra, ofs, 32, false, false, false);
	}
}

void Jit64::stmw(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);

	auto ra = inst.RA ? regs.gpr.Lock(inst.RA) : regs.gpr.Zero();

	// TODO: This doesn't handle rollback on DSI correctly
	for (int i = inst.RD; i < 32; i++)
	{
		auto reg = regs.gpr.Lock(i);
		u32 offset = (i - inst.RD) * 4 + (u32)(s32)inst.SIMM_16;
		auto ofs = regs.gpr.Imm32(offset);

		SafeWrite(reg, ra, ofs, 32, false, false);
	}
}
