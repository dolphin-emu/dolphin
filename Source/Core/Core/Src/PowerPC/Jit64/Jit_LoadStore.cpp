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

	void UnsafeLoadRegToReg(X64Reg reg_addr, X64Reg reg_value, int accessSize, s32 offset, bool signExtend)
	{
#ifdef _M_IX86
		AND(32, R(reg_addr), Imm32(Memory::MEMVIEW32_MASK));
		MOVZX(32, accessSize, reg_value, MDisp(reg_addr, (u32)Memory::base + offset));
#else
		MOVZX(32, accessSize, reg_value, MComplex(RBX, reg_addr, SCALE_1, offset));
#endif
		if (accessSize == 32)
		{
			BSWAP(32, EAX);
		}
		else if (accessSize == 16)
		{
			BSWAP(32, EAX);
			SHR(32, R(EAX), Imm8(16));
		}
		if (signExtend && accessSize < 32) {
			MOVSX(32, accessSize, EAX, R(EAX));
		}
	}

	void SafeLoadRegToEAX(X64Reg reg, int accessSize, s32 offset, bool signExtend)
	{
		if (offset)
			ADD(32, R(reg), Imm32((u32)offset));
		TEST(32, R(reg), Imm32(0x0C000000));
		FixupBranch argh = J_CC(CC_NZ);
		UnsafeLoadRegToReg(reg, EAX, accessSize, 0, signExtend);
		FixupBranch arg2 = J();
		SetJumpTarget(argh);
		switch (accessSize)
		{
		case 32: ABI_CallFunctionR((void *)&Memory::Read_U32, reg); break;
		case 16: ABI_CallFunctionR((void *)&Memory::Read_U16, reg); break;
		case 8:  ABI_CallFunctionR((void *)&Memory::Read_U8, reg);  break;
		}
		SetJumpTarget(arg2);
	}

	void UnsafeWriteRegToReg(X64Reg reg_value, X64Reg reg_addr, int accessSize, s32 offset)
	{
		if (accessSize != 32) {
			PanicAlert("UnsafeWriteRegToReg can't handle %i byte accesses", accessSize);
		}
		BSWAP(32, reg_value);
#ifdef _M_IX86
		AND(32, R(reg_addr), Imm32(Memory::MEMVIEW32_MASK));
		MOV(accessSize, MDisp(reg_addr, (u32)Memory::base), R(reg_value));
#else
		MOV(accessSize, MComplex(RBX, reg_addr, SCALE_1, 0), R(reg_value));
#endif
	}

	// Destroys both arg registers
	void SafeWriteRegToReg(X64Reg reg_value, X64Reg reg_addr, int accessSize, s32 offset)
	{
		if (offset)
			ADD(32, R(reg_addr), Imm32(offset));
		TEST(32, R(reg_addr), Imm32(0x0C000000));
		FixupBranch argh = J_CC(CC_NZ);	
		UnsafeWriteRegToReg(reg_value, reg_addr, accessSize, 0);
		FixupBranch arg2 = J();
		SetJumpTarget(argh);
		ABI_CallFunctionRR((void *)&Memory::Write_U32, ABI_PARAM1, ABI_PARAM2); 
		SetJumpTarget(arg2);
	}

	void lbzx(UGeckoInstruction inst)
	{
		INSTRUCTION_START;
		gpr.Flush(FLUSH_VOLATILE);
		fpr.Flush(FLUSH_VOLATILE);
		int a = inst.RA, b = inst.RB, d = inst.RD;
		gpr.Lock(a, b, d);
		if (b == d || a == d)
			gpr.LoadToX64(d, true, true);
		else 
			gpr.LoadToX64(d, false, true);
		MOV(32, R(ABI_PARAM1), gpr.R(b));
		if (a)
			ADD(32, R(ABI_PARAM1), gpr.R(a));
		SafeLoadRegToEAX(ABI_PARAM1, 8, 0);
		MOV(32, gpr.R(d), R(EAX));
		gpr.UnlockAll();
	}

	void lXz(UGeckoInstruction inst)
	{
		INSTRUCTION_START;
		int d = inst.RD;
		int a = inst.RA;

		// TODO(ector): Make it dynamically enable/disable idle skipping where appropriate
		// Will give nice boost to dual core mode
		// if (CommandProcessor::AllowIdleSkipping() && PixelEngine::AllowIdleSkipping())

		if (!Core::GetStartupParameter().bUseDualCore && 
			inst.OPCD == 32 && 
			(inst.hex & 0xFFFF0000) == 0x800D0000 &&
			Memory::ReadUnchecked_U32(js.compilerPC+4) == 0x28000000 &&
			Memory::ReadUnchecked_U32(js.compilerPC+8) == 0x4182fff8)
		{
			gpr.Flush(FLUSH_ALL);
			fpr.Flush(FLUSH_ALL);
			ABI_CallFunctionC((void *)&PowerPC::OnIdle, PowerPC::ppcState.gpr[a] + (s32)(s16)inst.SIMM_16);
			MOV(32, M(&PowerPC::ppcState.pc), Imm32(js.compilerPC + 12));
			JMP(Asm::testExceptions, true);
			js.compilerPC += 8;
			return;
		}

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
		case 34: accessSize = 8; break;  //lbz
		default: _assert_msg_(DYNA_REC, 0, "lXz: invalid access size"); return;
		}

		//Still here? Do regular path.
#if defined(_M_X64)
		if (accessSize == 8 || accessSize == 16 || !jo.enableFastMem) {
#else
		if (true) {
#endif
			// Safe and boring
			gpr.Flush(FLUSH_VOLATILE);
			fpr.Flush(FLUSH_VOLATILE);
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
		INSTRUCTION_START;
		int d = inst.RD;
		int a = inst.RA;
		s32 offset = (s32)(s16)inst.SIMM_16;
		// Safe and boring
		gpr.Flush(FLUSH_VOLATILE);
		fpr.Flush(FLUSH_VOLATILE);
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

#ifndef _WIN32
#define INSTRUCTION_START Default(inst); return;
#endif

	void stX(UGeckoInstruction inst)
	{
		INSTRUCTION_START;
		int s = inst.RS;
		int a = inst.RA;

		bool update = false;
		if (inst.OPCD & 1)
		{
			update = true;
		}

		s32 offset = (s32)(s16)inst.SIMM_16;
		if (a || update) 
		{
			gpr.Flush(FLUSH_VOLATILE);
			fpr.Flush(FLUSH_VOLATILE);
			int accessSize;
			switch (inst.OPCD & ~1)
			{
			case 36: accessSize = 32; break; //stw
			case 44: accessSize = 16; break; //sth
			case 38: accessSize = 8; break;  //stb
			default: _assert_msg_(DYNA_REC, 0, "AWETKLJASDLKF"); return;
			}
/*
			if (gpr.R(a).IsImm() && !update)
			{
				u32 addr = (u32)gpr.R(a).offset;
				addr += offset;
				//YAY!
				//Now do something smart
				if ((addr & 0xFFFFF000) == 0xCC008000)
				{
					//MessageBox(0,"FIFO",0,0);
					//Do a direct I/O write
#ifdef _M_X64
					MOV(32, R(EDX), Imm32((u32)gpr.R(a).offset));
					MOV(32, R(ECX), gpr.R(s));
#elif _M_IX86
					PUSH(32, Imm32((u32)gpr.R(a).offset));
					PUSH(32, gpr.R(s));
#endif
					switch (accessSize)
					{	
					case 8:  CALL((void *)&GPFifo::FastWrite8); break;
					case 16: CALL((void *)&GPFifo::FastWrite16); break;
					case 32: CALL((void *)&GPFifo::FastWrite32); break;
					}
					js.fifoBytesThisBlock += accessSize >> 3;
					if (js.fifoBytesThisBlock > 32)
					{
						js.fifoBytesThisBlock -= 32;
						CALL((void *)&GPFifo::CheckGatherPipe);
					}
#ifdef _M_IX86
					ADD(32, R(ESP), Imm8(8));
#endif
					return;
				}
				else if ((addr>>24) == 0xCC && accessSize == 32) //Other I/O
				{
#ifdef _M_X64
					MOV(32, R(EDX), Imm32((u32)gpr.R(a).offset));
					MOV(32, R(ECX), gpr.R(s));
#elif _M_IX86
					PUSH(32, Imm32((u32)gpr.R(a).offset));
					PUSH(32, gpr.R(s));
#endif
					CALL((void *)Memory::GetHWWriteFun32(addr));
#ifdef _M_IX86
					ADD(32, R(ESP), Imm8(8));
#endif
				}
			}
			if (accessSize == 32 && !gpr.R(a).IsImm() && a == 1 && js.st.isFirstBlockOfFunction && jo.optimizeStack) //Zelda does not like this
			{
				//Stack access
				MOV(32, R(ECX), gpr.R(a));
				MOV(32, R(EAX), gpr.R(s));
				BSWAP(32, EAX);
#ifdef _M_X64	
				MOV(accessSize, MComplex(RBX, ECX, SCALE_1, (u32)offset), R(EAX));
#elif _M_IX86
				AND(32, R(ECX), Imm32(Memory::MEMVIEW32_MASK));
				MOV(accessSize, MDisp(ECX, (u32)Memory::base + (u32)offset), R(EAX));
#endif
				if (update)
					ADD(32, gpr.R(a), Imm32(offset));
				return;
			}
*/
			//Still here? Do regular path.
			gpr.Lock(s, a);
			gpr.FlushLockX(ABI_PARAM1, ABI_PARAM2);
			MOV(32, R(ABI_PARAM2), gpr.R(a));
			MOV(32, R(ABI_PARAM1), gpr.R(s));
			if (offset)
				ADD(32, R(ABI_PARAM2), Imm32((u32)offset));
			if (update && offset)
			{
				MOV(32, gpr.R(a), R(ABI_PARAM2));
			}
			TEST(32, R(ABI_PARAM2), Imm32(0x0C000000));
			FixupBranch argh = J_CC(CC_NZ);
			if (accessSize == 32)
				BSWAP(32, ABI_PARAM1);
			else if (accessSize == 16)
			{
				// TODO(ector): xchg ch, cl?
				BSWAP(32, ABI_PARAM1);
				SHR(32, R(ABI_PARAM1), Imm8(16));
			}
#ifdef _M_X64
			MOV(accessSize, MComplex(RBX, ABI_PARAM2, SCALE_1, 0), R(ABI_PARAM1));
#else
			AND(32, R(ABI_PARAM2), Imm32(Memory::MEMVIEW32_MASK));
			MOV(accessSize, MDisp(ABI_PARAM2, (u32)Memory::base), R(ABI_PARAM1));
#endif
			FixupBranch arg2 = J();
			SetJumpTarget(argh);
			switch (accessSize)
			{
			case 32: ABI_CallFunctionRR((void *)&Memory::Write_U32, ABI_PARAM1, ABI_PARAM2); break;
			case 16: ABI_CallFunctionRR((void *)&Memory::Write_U16, ABI_PARAM1, ABI_PARAM2); break;
			case 8:  ABI_CallFunctionRR((void *)&Memory::Write_U8,  ABI_PARAM1, ABI_PARAM2); break;
			}
			SetJumpTarget(arg2);
			gpr.UnlockAll();
			gpr.UnlockAllX();
		}
		else
		{
			Default(inst);
		}
	}

	// A few games use these heavily.
	void lmw(UGeckoInstruction inst)
	{
		INSTRUCTION_START;
		Default(inst);
		return;
		/// BUGGY
		//return _inst.RA ? (m_GPR[_inst.RA] + _inst.SIMM_16) : _inst.SIMM_16;
		gpr.Flush(FLUSH_ALL);
		gpr.LockX(ECX, EDX, ESI);
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
		J_CC(CC_NE, loopPtr, false);
		gpr.UnlockAllX();
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

