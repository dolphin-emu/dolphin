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
#include "Thunk.h"

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
#include "Jit_Util.h"

// #define INSTRUCTION_START Default(inst); return;
#define INSTRUCTION_START

#ifdef _M_IX86
#define DISABLE_32BIT Default(inst); return;
#else
#define DISABLE_32BIT ;
#endif

namespace Jit64
{
	namespace {
          //	u64 GC_ALIGNED16(temp64);
          //	u32 GC_ALIGNED16(temp32);
	}
	void lbzx(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		//if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITLoadStoreOff)
		//	{Default(inst); return;} // turn off from debugger	
#endif
		INSTRUCTION_START;

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
		//SafeLoadRegToEAX(ABI_PARAM1, 8, 0);
		UnsafeLoadRegToReg(ABI_PARAM1, gpr.RX(d), 8, 0, false);
		//MOV(32, gpr.R(d), R(EAX));
		gpr.UnlockAll();
		gpr.UnlockAllX();
	}

	void lXz(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		//if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITLoadStoreOff)
		//	{Default(inst); return;} // turn off from debugger	
#endif
		INSTRUCTION_START;

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
			gpr.Flush(FLUSH_ALL);
			fpr.Flush(FLUSH_ALL);
			if (Core::GetStartupParameter().bUseDualCore) 
				CALL((void *)&PowerPC::OnIdleDC);
			else 
				ABI_CallFunctionC((void *)&PowerPC::OnIdle, PowerPC::ppcState.gpr[a] + (s32)(s16)inst.SIMM_16);
			MOV(32, M(&PowerPC::ppcState.pc), Imm32(js.compilerPC + 12));
			JMP(Asm::testExceptions, true);
			js.compilerPC += 8;
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
		case 32: accessSize = 32; break; //lwz
		case 40: accessSize = 16; break; //lhz
		case 34: accessSize = 8;  break; //lbz
		default: _assert_msg_(DYNA_REC, 0, "lXz: invalid access size"); return;
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

		// Fast and daring
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

	void lha(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		//if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITLoadStoreOff)
		//	{Default(inst); return;} // turn off from debugger	
#endif
		INSTRUCTION_START;

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

	// Zero cache line.
	void dcbz(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		//if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITLoadStoreOff)
		//	{Default(inst); return;} // turn off from debugger	
#endif
		INSTRUCTION_START;

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

	void stX(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		//if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITLoadStoreOff)
		//	{Default(inst); return;} // turn off from debugger	
#endif
		INSTRUCTION_START;

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
					// INT3();
					switch (accessSize)
					{	
					// No need to protect these, they don't touch any state
					// question - should we inline them instead? Pro: Lose a CALL   Con: Code bloat
					case 8:  CALL((void *)Asm::fifoDirectWrite8);  break;
					case 16: CALL((void *)Asm::fifoDirectWrite16); break;
					case 32: CALL((void *)Asm::fifoDirectWrite32); break;
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
					// PanicAlert("yum yum");
					// This may be quite beneficial.
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
			#ifndef _WIN32
			if(accessSize == 8)
			{
				Default(inst);
				return;
			}
			#endif
			gpr.Lock(s, a);
			gpr.FlushLockX(ABI_PARAM1, ABI_PARAM2);
			MOV(32, R(ABI_PARAM2), gpr.R(a));
			MOV(32, R(ABI_PARAM1), gpr.R(s));
			if (offset)
				ADD(32, R(ABI_PARAM2), Imm32((u32)offset));
			if (update && offset)
			{
				gpr.LoadToX64(a, true, true);
				MOV(32, gpr.R(a), R(ABI_PARAM2));
			}
			TEST(32, R(ABI_PARAM2), Imm32(0x0C000000));
			FixupBranch unsafe_addr = J_CC(CC_NZ);
			BSWAP(accessSize, ABI_PARAM1);
#ifdef _M_X64
			MOV(accessSize, MComplex(RBX, ABI_PARAM2, SCALE_1, 0), R(ABI_PARAM1));
#else
			AND(32, R(ABI_PARAM2), Imm32(Memory::MEMVIEW32_MASK));
			MOV(accessSize, MDisp(ABI_PARAM2, (u32)Memory::base), R(ABI_PARAM1));
#endif
			FixupBranch skip_call = J();
			SetJumpTarget(unsafe_addr);
			switch (accessSize)
			{
			case 32: ABI_CallFunctionRR(ProtectFunction((void *)&Memory::Write_U32, 2), ABI_PARAM1, ABI_PARAM2); break;
			case 16: ABI_CallFunctionRR(ProtectFunction((void *)&Memory::Write_U16, 2), ABI_PARAM1, ABI_PARAM2); break;
			case 8:  ABI_CallFunctionRR(ProtectFunction((void *)&Memory::Write_U8,  2), ABI_PARAM1, ABI_PARAM2); break;
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

	// A few games use these heavily in video codecs.
	void lmw(UGeckoInstruction inst)
	{
		INSTRUCTION_START;
		Default(inst);
		return;

		/*
		/// BUGGY
		//return _inst.RA ? (m_GPR[_inst.RA] + _inst.SIMM_16) : _inst.SIMM_16;
		gpr.FlushLockX(ECX, EDX);
		gpr.FlushLockX(ESI);
		//INT3();
		MOV(32, R(EAX), Imm32((u32)(s32)inst.SIMM_16));
		if (inst.RA)
			ADD(32, R(EAX), gpr.R(inst.RA));
		MOV(32, R(ECX), Imm32(inst.RD));
		MOV(32, R(ESI), Imm32(static_cast<u32>((u64)&PowerPC::ppcState.gpr[0])));
		const u8 *loopPtr = GetCodePtr();
		MOV(32, R(EDX), MComplex(RBX, EAX, SCALE_1, 0));
		MOV(32, MComplex(ESI, ECX, SCALE_4, 0), R(EDX));
		ADD(32, R(EAX), Imm8(4));
		ADD(32, R(ESI), Imm8(4));
		ADD(32, R(ECX), Imm8(1));
		CMP(32, R(ECX), Imm8(32));
		gpr.UnlockAllX();*/
	}

	void stmw(UGeckoInstruction inst)
	{
		INSTRUCTION_START;
		Default(inst);
		return;
		/*
		u32 uAddress = Helper_Get_EA(_inst);
		for (int iReg = _inst.RS; iReg <= 31; iReg++, uAddress+=4)
		{		
			Memory::Write_U32(m_GPR[iReg], uAddress);
			if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
				return;
		}*/
	}
}

