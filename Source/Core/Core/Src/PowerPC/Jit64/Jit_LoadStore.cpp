// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// TODO(ector): Tons of pshufb optimization of the loads/stores, for SSSE3+, possibly SSE4, only.
// Should give a very noticable speed boost to paired single heavy code.

#include "Common.h"
#include "Thunk.h"

#include "../PowerPC.h"
#include "../../Core.h"
#include "../../ConfigManager.h"
#include "../../HW/GPFifo.h"
#include "../../HW/Memmap.h"
#include "../PPCTables.h"
#include "x64Emitter.h"
#include "x64ABI.h"

#include "Jit.h"
#include "JitAsm.h"
#include "JitRegCache.h"

void Jit64::lXXx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)

	int a = inst.RA, b = inst.RB, d = inst.RD;

	// Skip disabled JIT instructions
	if (Core::g_CoreStartupParameter.bJITLoadStorelbzxOff && (inst.OPCD == 31) && (inst.SUBOP10 == 87))
	{ Default(inst); return; }
	if (Core::g_CoreStartupParameter.bJITLoadStorelXzOff && ((inst.OPCD == 34) || (inst.OPCD == 40) || (inst.OPCD == 32)))
	{ Default(inst); return; }
	if (Core::g_CoreStartupParameter.bJITLoadStorelwzOff && (inst.OPCD == 32))
	{ Default(inst); return; }

	// Determine memory access size and sign extend
	int accessSize = 0;
	bool signExtend = false;
	switch (inst.OPCD)
	{
	case 32: /* lwz */
	case 33: /* lwzu */
		accessSize = 32;
		signExtend = false;
		break;
		
	case 34: /* lbz */
	case 35: /* lbzu */
		accessSize = 8;
		signExtend = false;
		break;
		
	case 40: /* lhz */
	case 41: /* lhzu */
		accessSize = 16;
		signExtend = false;
		break;
		
	case 42: /* lha */
	case 43: /* lhau */
		accessSize = 16;
		signExtend = true;
		break;
		
	case 31:
		switch (inst.SUBOP10)
		{
		case 23:  /* lwzx */
		case 55:  /* lwzux */
			accessSize = 32;
			signExtend = false;
			break;
			
		case 87:  /* lbzx */
		case 119: /* lbzux */
			accessSize = 8;
			signExtend = false;
			break;
		case 279: /* lhzx */
		case 311: /* lhzux */
			accessSize = 16;
			signExtend = false;
			break;
			
		case 343: /* lhax */
		case 375: /* lhaux */
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

	// TODO(ector): Make it dynamically enable/disable idle skipping where appropriate
	// Will give nice boost to dual core mode
	// (mb2): I agree, 
	// IMHO those Idles should always be skipped and replaced by a more controllable "native" Idle methode
	// ... maybe the throttle one already do that :p
	// if (CommandProcessor::AllowIdleSkipping() && PixelEngine::AllowIdleSkipping())
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bSkipIdle &&
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
		gpr.Lock(d);
		SafeLoadToEAX(gpr.R(a), accessSize, offset, signExtend);
		gpr.KillImmediate(d, false, true);
		MOV(32, gpr.R(d), R(EAX));
		gpr.UnlockAll();
		
		gpr.Flush(FLUSH_ALL);
		fpr.Flush(FLUSH_ALL);

		// if it's still 0, we can wait until the next event
		TEST(32, R(EAX), R(EAX));
		FixupBranch noIdle = J_CC(CC_NZ);
		
		ABI_CallFunctionC((void *)&PowerPC::OnIdle, PowerPC::ppcState.gpr[a] + (s32)(s16)inst.SIMM_16);

		// ! we must continue executing of the loop after exception handling, maybe there is still 0 in r0
		//MOV(32, M(&PowerPC::ppcState.pc), Imm32(js.compilerPC));
		WriteExceptionExit();

		SetJumpTarget(noIdle);

		//js.compilerPC += 8;
		return;
	}
	
	// Determine whether this instruction updates inst.RA
	bool update;
	if (inst.OPCD == 31)
		update = ((inst.SUBOP10 & 0x20) != 0);
	else
		update = ((inst.OPCD & 1) != 0);
	
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
		if ((inst.OPCD != 31) && gpr.R(a).IsImm())
		{
			opAddress = Imm32((u32)gpr.R(a).offset + (s32)inst.SIMM_16);
		}
		else if ((inst.OPCD == 31) && gpr.R(a).IsImm() && gpr.R(b).IsImm())
		{
			opAddress = Imm32((u32)gpr.R(a).offset + (u32)gpr.R(b).offset);
		}
		else
		{
			gpr.FlushLockX(ABI_PARAM1);
			opAddress = R(ABI_PARAM1);
			MOV(32, opAddress, gpr.R(a));
			
			if (inst.OPCD == 31)
				ADD(32, opAddress, gpr.R(b));
			else
				ADD(32, opAddress, Imm32((u32)(s32)inst.SIMM_16));
		}
	}

	SafeLoadToEAX(opAddress, accessSize, 0, signExtend);

	// We must flush immediate values from the following registers because
	// they may change at runtime if no MMU exception has been raised
	gpr.KillImmediate(d, true, true);
	if (update)
	{
		gpr.Lock(a);
		gpr.BindToRegister(a, true, true);
	}
	
	MEMCHECK_START

	if (update)
	{
		if (inst.OPCD == 31)
			ADD(32, gpr.R(a), gpr.R(b));
		else
			ADD(32, gpr.R(a), Imm32((u32)(s32)inst.SIMM_16));
	}
	MOV(32, gpr.R(d), R(EAX));

	MEMCHECK_END
	
	gpr.UnlockAll();
	gpr.UnlockAllX();
}

void Jit64::dcbst(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)

	// If the dcbst instruction is preceded by dcbt, it is flushing a prefetched
	// memory location.  Do not invalidate the JIT cache in this case as the memory
	// will be the same.
	// dcbt = 0x7c00022c
	if ((Memory::ReadUnchecked_U32(js.compilerPC - 4) & 0x7c00022c) != 0x7c00022c)
	{
		Default(inst); return;
	}
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
	if (a || !update) 
	{
		int accessSize;
		switch (inst.OPCD & ~1)
		{
		case 36: accessSize = 32; break; //stw
		case 44: accessSize = 16; break; //sth
		case 38: accessSize = 8; break;  //stb
		default: _assert_msg_(DYNA_REC, 0, "AWETKLJASDLKF"); return;
		}

		if ((a == 0) || gpr.R(a).IsImm())
		{
			// If we already know the address through constant folding, we can do some
			// fun tricks...
			u32 addr = ((a == 0) ? 0 : (u32)gpr.R(a).offset);
			addr += offset;
			if ((addr & 0xFFFFF000) == 0xCC008000 && jo.optimizeGatherPipe)
			{
				MOV(32, M(&PC), Imm32(jit->js.compilerPC)); // Helps external systems know which instruction triggered the write
				gpr.FlushLockX(ABI_PARAM1);
				MOV(32, R(ABI_PARAM1), gpr.R(s));
				if (update)
					gpr.SetImmediate32(a, addr);
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
			else if (Memory::IsRAMAddress(addr))
			{
				MOV(32, R(EAX), gpr.R(s));
				BSWAP(accessSize, EAX);
				WriteToConstRamAddress(accessSize, R(EAX), addr);
				if (update)
					gpr.SetImmediate32(a, addr);
				return;
			}
			else
			{
				MOV(32, M(&PC), Imm32(jit->js.compilerPC)); // Helps external systems know which instruction triggered the write
				switch (accessSize)
				{
				case 32: ABI_CallFunctionAC(thunks.ProtectFunction(true ? ((void *)&Memory::Write_U32) : ((void *)&Memory::Write_U32_Swap), 2), gpr.R(s), addr); break;
				case 16: ABI_CallFunctionAC(thunks.ProtectFunction(true ? ((void *)&Memory::Write_U16) : ((void *)&Memory::Write_U16_Swap), 2), gpr.R(s), addr); break;
				case 8:  ABI_CallFunctionAC(thunks.ProtectFunction((void *)&Memory::Write_U8, 2), gpr.R(s), addr);  break;
				}
				if (update)
					gpr.SetImmediate32(a, addr);
				return;
			}
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
#else
			AND(32, R(ABI_PARAM1), Imm32(Memory::MEMVIEW32_MASK));
			MOV(accessSize, MDisp(ABI_PARAM1, (u32)Memory::base + (u32)offset), R(EAX));
#endif
			if (update && offset)
			{
				gpr.Lock(a);
				gpr.KillImmediate(a, true, true);
				ADD(32, gpr.R(a), Imm32(offset));
				gpr.UnlockAll();
			}
			gpr.UnlockAllX();
			return;
		}

		/* // TODO - figure out why Beyond Good and Evil hates this
		#if defined(_WIN32) && defined(_M_X64)
		if (accessSize == 32 && !update)
		{
		// Fast and daring - requires 64-bit
		MOV(32, R(EAX), gpr.R(s));
		gpr.BindToRegister(a, true, false);
		BSWAP(32, EAX);
		MOV(accessSize, MComplex(RBX, gpr.RX(a), SCALE_1, (u32)offset), R(EAX));
		return;
		}
		#endif*/

		//Still here? Do regular path.

		gpr.FlushLockX(ECX, EDX);
		gpr.Lock(s, a);
		MOV(32, R(EDX), gpr.R(a));
		MOV(32, R(ECX), gpr.R(s));
		SafeWriteRegToReg(ECX, EDX, accessSize, offset);

		if (update && offset)
		{
			gpr.KillImmediate(a, true, true);
			MEMCHECK_START
			
			ADD(32, gpr.R(a), Imm32((u32)offset));

			MEMCHECK_END
		}

		gpr.UnlockAll();
		gpr.UnlockAllX();
	}
	else
	{
		PanicAlert("Invalid stX");
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

	if (inst.SUBOP10 & 32)
	{
		MEMCHECK_START
		gpr.BindToRegister(a, true, true);
		ADD(32, gpr.R(a), gpr.R(b));
		MOV(32, R(EDX), gpr.R(a));
		MEMCHECK_END
	} else {
		MOV(32, R(EDX), gpr.R(a));
		ADD(32, R(EDX), gpr.R(b));
	}
	int accessSize;
	switch (inst.SUBOP10 & ~32) {
		case 151: accessSize = 32; break;
		case 407: accessSize = 16; break;
		case 215: accessSize = 8; break;
		default: PanicAlert("stXx: invalid access size"); 
			accessSize = 0; break;
	}

	MOV(32, R(ECX), gpr.R(s));
	SafeWriteRegToReg(ECX, EDX, accessSize, 0);

	gpr.UnlockAll();
	gpr.UnlockAllX();
}

// A few games use these heavily in video codecs.
void Jit64::lmw(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)

#ifdef _M_X64
	gpr.FlushLockX(ECX);
	MOV(32, R(EAX), Imm32((u32)(s32)inst.SIMM_16));
	if (inst.RA)
		ADD(32, R(EAX), gpr.R(inst.RA));
	for (int i = inst.RD; i < 32; i++)
	{
		MOV(32, R(ECX), MComplex(EBX, EAX, SCALE_1, (i - inst.RD) * 4));
		BSWAP(32, ECX);
		gpr.BindToRegister(i, false, true);
		MOV(32, gpr.R(i), R(ECX));
	}
	gpr.UnlockAllX();
#else
	Default(inst); return;
#endif
}

void Jit64::stmw(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStore)

#ifdef _M_X64
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
#else
	Default(inst); return;
#endif
}

void Jit64::icbi(UGeckoInstruction inst)
{
	Default(inst);
	WriteExit(js.compilerPC + 4, 0);
}
