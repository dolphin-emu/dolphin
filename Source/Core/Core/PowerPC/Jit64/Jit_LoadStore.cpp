// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// TODO(ector): Tons of pshufb optimization of the loads/stores, for SSSE3+, possibly SSE4, only.
// Should give a very noticable speed boost to paired single heavy code.

#include "Common/CommonTypes.h"

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
	FALLBACK_IF(SConfig::GetInstance().m_LocalCoreStartupParameter.bJITLoadStorelbzxOff && (inst.OPCD == 31) && (inst.SUBOP10 == 87));
	FALLBACK_IF(SConfig::GetInstance().m_LocalCoreStartupParameter.bJITLoadStorelXzOff && ((inst.OPCD == 34) || (inst.OPCD == 40) || (inst.OPCD == 32)));
	FALLBACK_IF(SConfig::GetInstance().m_LocalCoreStartupParameter.bJITLoadStorelwzOff && (inst.OPCD == 32));

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
	if (accessSize == 8 && js.next_inst.OPCD == 31 && js.next_inst.SUBOP10 == 954 &&
	    js.next_inst.RS == inst.RD && js.next_inst.RA == inst.RD && !js.next_inst.Rc)
	{
		if (PowerPC::GetState() != PowerPC::CPU_STEPPING)
		{
			js.downcountAmount++;
			js.skipnext = true;
			signExtend = true;
		}
	}

	// TODO(ector): Make it dynamically enable/disable idle skipping where appropriate
	// Will give nice boost to dual core mode
	// (mb2): I agree,
	// IMHO those Idles should always be skipped and replaced by a more controllable "native" Idle methode
	// ... maybe the throttle one already do that :p
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bSkipIdle &&
	    PowerPC::GetState() != PowerPC::CPU_STEPPING &&
	    inst.OPCD == 32 &&
	    (inst.hex & 0xFFFF0000) == 0x800D0000 &&
	    (Memory::ReadUnchecked_U32(js.compilerPC + 4) == 0x28000000 ||
	    (SConfig::GetInstance().m_LocalCoreStartupParameter.bWii && Memory::ReadUnchecked_U32(js.compilerPC + 4) == 0x2C000000)) &&
	    Memory::ReadUnchecked_U32(js.compilerPC + 8) == 0x4182fff8)
	{
		// TODO(LinesPrower):
		// - Rewrite this!
		// It seems to be ugly and inefficient, but I don't know JIT stuff enough to make it right
		// It only demonstrates the idea

		// do our job at first
		s32 offset = (s32)(s16)inst.SIMM_16;
		gpr.BindToRegister(a, true, false);
		gpr.BindToRegister(d, false, true);
		SafeLoadToReg(gpr.RX(d), gpr.R(a), accessSize, offset, CallerSavedRegistersInUse(), signExtend);

		// if it's still 0, we can wait until the next event
		TEST(32, gpr.R(d), gpr.R(d));
		FixupBranch noIdle = J_CC(CC_NZ);

		BitSet32 registersInUse = CallerSavedRegistersInUse();
		ABI_PushRegistersAndAdjustStack(registersInUse, 0);

		ABI_CallFunctionC((void *)&PowerPC::OnIdle, PowerPC::ppcState.gpr[a] + (s32)(s16)inst.SIMM_16);

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
		update = ((inst.SUBOP10 & 0x20) != 0) && (!gpr.R(b).IsImm() || gpr.R(b).offset != 0);
	else
		update = ((inst.OPCD & 1) != 0) && inst.SIMM_16 != 0;

	bool storeAddress = false;
	s32 loadOffset = 0;

	// Prepare address operand
	Gen::OpArg opAddress;
	if (!update && !a)
	{
		if (inst.OPCD == 31)
		{
			gpr.Lock(b);
			opAddress = gpr.R(b);
		}
		else
		{
			opAddress = Imm32((u32)(s32)inst.SIMM_16);
		}
	}
	else if (update && ((a == 0) || (d == a)))
	{
		PanicAlert("Invalid instruction");
	}
	else
	{
		if ((inst.OPCD != 31) && gpr.R(a).IsImm() && !js.memcheck)
		{
			u32 val = (u32)gpr.R(a).offset + (s32)inst.SIMM_16;
			opAddress = Imm32(val);
			if (update)
				gpr.SetImmediate32(a, val);
		}
		else if ((inst.OPCD == 31) && gpr.R(a).IsImm() && gpr.R(b).IsImm() && !js.memcheck)
		{
			u32 val = (u32)gpr.R(a).offset + (u32)gpr.R(b).offset;
			opAddress = Imm32(val);
			if (update)
				gpr.SetImmediate32(a, val);
		}
		else
		{
			// If we're using reg+reg mode and b is an immediate, pretend we're using constant offset mode
			bool use_constant_offset = inst.OPCD != 31 || gpr.R(b).IsImm();
			s32 offset = inst.OPCD == 31 ? (s32)gpr.R(b).offset : (s32)inst.SIMM_16;
			// Depending on whether we have an immediate and/or update, find the optimum way to calculate
			// the load address.
			if ((update || use_constant_offset) && !js.memcheck)
			{
				gpr.BindToRegister(a, true, update);
				opAddress = gpr.R(a);
				if (!use_constant_offset)
					ADD(32, opAddress, gpr.R(b));
				else if (update)
					ADD(32, opAddress, Imm32((u32)offset));
				else
					loadOffset = offset;
			}
			else
			{
				// In this case we need an extra temporary register.
				opAddress = R(RSCRATCH2);
				storeAddress = true;
				if (use_constant_offset)
				{
					if (gpr.R(a).IsSimpleReg() && offset != 0)
					{
						LEA(32, RSCRATCH2, MDisp(gpr.RX(a), offset));
					}
					else
					{
						MOV(32, opAddress, gpr.R(a));
						if (offset != 0)
							ADD(32, opAddress, Imm32((u32)offset));
					}
				}
				else if (gpr.R(a).IsSimpleReg() && gpr.R(b).IsSimpleReg())
				{
					LEA(32, RSCRATCH2, MComplex(gpr.RX(a), gpr.RX(b), SCALE_1, 0));
				}
				else
				{
					MOV(32, opAddress, gpr.R(a));
					ADD(32, opAddress, gpr.R(b));
				}
			}
		}
	}

	gpr.Lock(a, b, d);
	gpr.BindToRegister(d, js.memcheck, true);
	BitSet32 registersInUse = CallerSavedRegistersInUse();
	if (update && storeAddress)
	{
		// We need to save the (usually scratch) address register for the update.
		registersInUse[RSCRATCH2] = true;
	}
	SafeLoadToReg(gpr.RX(d), opAddress, accessSize, loadOffset, registersInUse, signExtend);

	if (update && storeAddress)
	{
		gpr.BindToRegister(a, true, true);
		MEMCHECK_START(false)
		MOV(32, gpr.R(a), opAddress);
		MEMCHECK_END
	}

	// TODO: support no-swap in SafeLoadToReg instead
	if (byte_reversed)
	{
		MEMCHECK_START(false)
		BSWAP(accessSize, gpr.RX(d));
		MEMCHECK_END
	}

	gpr.UnlockAll();
	gpr.UnlockAllX();
}

void Jit64::dcbst(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);

	// If the dcbst instruction is preceded by dcbt, it is flushing a prefetched
	// memory location.  Do not invalidate the JIT cache in this case as the memory
	// will be the same.
	// dcbt = 0x7c00022c
	FALLBACK_IF((Memory::ReadUnchecked_U32(js.compilerPC - 4) & 0x7c00022c) != 0x7c00022c);
}

// Zero cache line.
void Jit64::dcbz(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bDCBZOFF)
		return;

	int a = inst.RA;
	int b = inst.RB;

	u32 mem_mask = Memory::ADDR_MASK_HW_ACCESS;

	// The following masks the region used by the GC/Wii virtual memory lib
	mem_mask |= Memory::ADDR_MASK_MEM1;

	MOV(32, R(RSCRATCH), gpr.R(b));
	if (a)
		ADD(32, R(RSCRATCH), gpr.R(a));
	AND(32, R(RSCRATCH), Imm32(~31));
	TEST(32, R(RSCRATCH), Imm32(mem_mask));
	FixupBranch slow = J_CC(CC_NZ, true);

	// Should this code ever run? I can't find any games that use DCBZ on non-physical addresses, but
	// supposedly there are, at least for some MMU titles. Let's be careful and support it to be sure.
	SwitchToFarCode();
	SetJumpTarget(slow);
	MOV(32, M(&PC), Imm32(jit->js.compilerPC));
	BitSet32 registersInUse = CallerSavedRegistersInUse();
	ABI_PushRegistersAndAdjustStack(registersInUse, 0);
	ABI_CallFunctionR((void *)&Memory::ClearCacheLine, RSCRATCH);
	ABI_PopRegistersAndAdjustStack(registersInUse, 0);
	FixupBranch exit = J(true);

	SwitchToNearCode();
	PXOR(XMM0, R(XMM0));
	MOVAPS(MComplex(RMEM, RSCRATCH, SCALE_1, 0), XMM0);
	MOVAPS(MComplex(RMEM, RSCRATCH, SCALE_1, 16), XMM0);
	SetJumpTarget(exit);
}

void Jit64::stX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);

	int s = inst.RS;
	int a = inst.RA;

	bool update = inst.OPCD & 1;

	s32 offset = (s32)(s16)inst.SIMM_16;
	if (a || !update)
	{
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

		if ((a == 0) || gpr.R(a).IsImm())
		{
			// If we already know the address through constant folding, we can do some
			// fun tricks...
			u32 addr = ((a == 0) ? 0 : (u32)gpr.R(a).offset);
			addr += offset;
			if ((addr & 0xFFFFF000) == 0xCC008000 && jo.optimizeGatherPipe)
			{
				// Helps external systems know which instruction triggered the write
				MOV(32, PPCSTATE(pc), Imm32(jit->js.compilerPC));

				MOV(32, R(RSCRATCH2), gpr.R(s));
				if (update)
					gpr.SetImmediate32(a, addr);

				// No need to protect these, they don't touch any state
				// question - should we inline them instead? Pro: Lose a CALL   Con: Code bloat
				switch (accessSize)
				{
				case 8:
					CALL((void *)asm_routines.fifoDirectWrite8);
					break;
				case 16:
					CALL((void *)asm_routines.fifoDirectWrite16);
					break;
				case 32:
					CALL((void *)asm_routines.fifoDirectWrite32);
					break;
				}
				js.fifoBytesThisBlock += accessSize >> 3;
				gpr.UnlockAllX();
				return;
			}
			else if (Memory::IsRAMAddress(addr))
			{
				MOV(32, R(RSCRATCH), gpr.R(s));
				WriteToConstRamAddress(accessSize, RSCRATCH, addr, true);
				if (update)
					gpr.SetImmediate32(a, addr);
				return;
			}
			else
			{
				// Helps external systems know which instruction triggered the write
				MOV(32, PPCSTATE(pc), Imm32(jit->js.compilerPC));

				BitSet32 registersInUse = CallerSavedRegistersInUse();
				ABI_PushRegistersAndAdjustStack(registersInUse, 0);
				switch (accessSize)
				{
				case 32:
					ABI_CallFunctionAC(true ? ((void *)&Memory::Write_U32) : ((void *)&Memory::Write_U32_Swap), gpr.R(s), addr);
					break;
				case 16:
					ABI_CallFunctionAC(true ? ((void *)&Memory::Write_U16) : ((void *)&Memory::Write_U16_Swap), gpr.R(s), addr);
					break;
				case 8:
					ABI_CallFunctionAC((void *)&Memory::Write_U8, gpr.R(s), addr);
					break;
				}
				ABI_PopRegistersAndAdjustStack(registersInUse, 0);
				if (update)
					gpr.SetImmediate32(a, addr);
				return;
			}
		}

		gpr.Lock(a, s);
		gpr.BindToRegister(a, true, false);
		if (gpr.R(s).IsImm())
		{
			SafeWriteRegToReg(gpr.R(s), gpr.RX(a), accessSize, offset, CallerSavedRegistersInUse(), SAFE_LOADSTORE_CLOBBER_RSCRATCH_INSTEAD_OF_ADDR);
		}
		else
		{
			X64Reg reg_value;
			if (WriteClobbersRegValue(accessSize, /* swap */ true))
			{
				MOV(32, R(RSCRATCH2), gpr.R(s));
				reg_value = RSCRATCH2;
			}
			else
			{
				gpr.BindToRegister(s, true, false);
				reg_value = gpr.RX(s);
			}
			SafeWriteRegToReg(reg_value, gpr.RX(a), accessSize, offset, CallerSavedRegistersInUse(), SAFE_LOADSTORE_CLOBBER_RSCRATCH_INSTEAD_OF_ADDR);
		}

		if (update && offset)
		{
			MEMCHECK_START(false)
			gpr.KillImmediate(a, true, true);

			ADD(32, gpr.R(a), Imm32((u32)offset));

			MEMCHECK_END
		}
		gpr.UnlockAll();
	}
	else
	{
		PanicAlert("Invalid stX");
	}
}

void Jit64::stXx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);

	int a = inst.RA, b = inst.RB, s = inst.RS;
	bool update = !!(inst.SUBOP10 & 32);
	bool byte_reverse = !!(inst.SUBOP10 & 512);
	FALLBACK_IF(!a || (update && a == s) || (update && js.memcheck && a == b));

	gpr.Lock(a, b, s);

	if (update)
	{
		gpr.BindToRegister(a, true, true);
		ADD(32, gpr.R(a), gpr.R(b));
		MOV(32, R(RSCRATCH2), gpr.R(a));
	}
	else if (gpr.R(a).IsSimpleReg() && gpr.R(b).IsSimpleReg())
	{
		LEA(32, RSCRATCH2, MComplex(gpr.RX(a), gpr.RX(b), SCALE_1, 0));
	}
	else
	{
		MOV(32, R(RSCRATCH2), gpr.R(a));
		ADD(32, R(RSCRATCH2), gpr.R(b));
	}

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

	if (gpr.R(s).IsImm())
	{
		SafeWriteRegToReg(gpr.R(s), RSCRATCH2, accessSize, 0, CallerSavedRegistersInUse(), byte_reverse ? SAFE_LOADSTORE_NO_SWAP : 0);
	}
	else
	{
		X64Reg reg_value;
		if (WriteClobbersRegValue(accessSize, /* swap */ !byte_reverse))
		{
			MOV(32, R(RSCRATCH), gpr.R(s));
			reg_value = RSCRATCH;
		}
		else
		{
			gpr.BindToRegister(s, true, false);
			reg_value = gpr.RX(s);
		}
		SafeWriteRegToReg(reg_value, RSCRATCH2, accessSize, 0, CallerSavedRegistersInUse(), byte_reverse ? SAFE_LOADSTORE_NO_SWAP : 0);
	}

	if (update && js.memcheck)
	{
		// revert the address change if an exception occurred
		MEMCHECK_START(true)
		SUB(32, gpr.R(a), gpr.R(b));
		MEMCHECK_END;
	}

	gpr.UnlockAll();
	gpr.UnlockAllX();
}

// A few games use these heavily in video codecs.
void Jit64::lmw(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);

	// TODO: This doesn't handle rollback on DSI correctly
	MOV(32, R(RSCRATCH2), Imm32((u32)(s32)inst.SIMM_16));
	if (inst.RA)
		ADD(32, R(RSCRATCH2), gpr.R(inst.RA));
	for (int i = inst.RD; i < 32; i++)
	{
		SafeLoadToReg(RSCRATCH, R(RSCRATCH2), 32, (i - inst.RD) * 4, CallerSavedRegistersInUse() | BitSet32 { RSCRATCH_EXTRA }, false);
		gpr.BindToRegister(i, false, true);
		MOV(32, gpr.R(i), R(RSCRATCH));
	}
	gpr.UnlockAllX();
}

void Jit64::stmw(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreOff);

	// TODO: This doesn't handle rollback on DSI correctly
	for (int i = inst.RD; i < 32; i++)
	{
		if (inst.RA)
			MOV(32, R(RSCRATCH), gpr.R(inst.RA));
		else
			XOR(32, R(RSCRATCH), R(RSCRATCH));
		if (gpr.R(i).IsImm())
		{
			SafeWriteRegToReg(gpr.R(i), RSCRATCH, 32, (i - inst.RD) * 4 + (u32)(s32)inst.SIMM_16, CallerSavedRegistersInUse());
		}
		else
		{
			MOV(32, R(RSCRATCH2), gpr.R(i));
			SafeWriteRegToReg(RSCRATCH2, RSCRATCH, 32, (i - inst.RD) * 4 + (u32)(s32)inst.SIMM_16, CallerSavedRegistersInUse());
		}
	}
	gpr.UnlockAllX();
}

void Jit64::icbi(UGeckoInstruction inst)
{
	FallBackToInterpreter(inst);
	WriteExit(js.compilerPC + 4);
}
