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

#include "../PowerPC.h"
#include "../../Core.h"
#include "../../HW/GPFifo.h"
#include "../../HW/CommandProcessor.h"
#include "../../HW/PixelEngine.h"
#include "../../HW/Memmap.h"
#include "../PPCTables.h"
#include "x64Emitter.h"

#include "Jit.h"
#include "JitCache.h"
#include "JitAsm.h"
#include "JitRegCache.h"

#define OLD
//#define OLD Default(inst); return;

#ifdef _M_IX86
#define BIT32OLD Default(inst); return;
#else
#define BIT32OLD ;
#endif

namespace Jit64
{
#ifdef _M_X64
	void SafeLoadECXtoEAX(int accessSize, int offset)
	{
		if (offset)
			ADD(32,R(ECX),Imm32((u32)offset));
		TEST(32,R(ECX),Imm32(0x0C000000));
		FixupBranch argh = J_CC(CC_NZ);
		if (accessSize != 32)
			XOR(32, R(EAX), R(EAX));
		MOV(accessSize, R(EAX), MComplex(RBX, ECX, SCALE_1, 0));
		if (accessSize == 32)
			BSWAP(32,EAX);
		else if (accessSize == 16)
		{
			BSWAP(32,EAX);
			SHR(32,R(EAX),Imm8(16));
		}
		FixupBranch arg2 = J();
		SetJumpTarget(argh);
		switch (accessSize)
		{
		case 32: CALL((void *)&Memory::Read_U32); break;
		case 16: CALL((void *)&Memory::Read_U16);break;
		case 8:  CALL((void *)&Memory::Read_U8);break;
		}
		SetJumpTarget(arg2);
	}
#elif _M_IX86
	void SafeLoadECXtoEAX(int accessSize, int offset)
	{
		if (offset)
			ADD(32, R(ECX), Imm32((u32)offset));
		TEST(32, R(ECX), Imm32(0x0C000000));
		FixupBranch argh = J_CC(CC_NZ);
		if (accessSize != 32)
			XOR(32, R(EAX), R(EAX));
		AND(32, R(ECX), Imm32(Memory::MEMVIEW32_MASK));

		MOV(accessSize, R(EAX), MDisp(ECX, (u32)Memory::base));
		if (accessSize == 32)
			BSWAP(32,EAX);
		else if (accessSize == 16)
		{
			BSWAP(32,EAX);
			SHR(32, R(EAX), Imm8(16));
		}
		FixupBranch arg2 = J();
		SetJumpTarget(argh);
		PUSH(ECX);
		switch (accessSize)
		{
		case 32: CALL(&Memory::Read_U32); break;
		case 16: CALL(&Memory::Read_U16); break;
		case 8:  CALL(&Memory::Read_U8);  break;
		}
		ADD(32, R(ESP), Imm8(4));
		SetJumpTarget(arg2);
	}
#endif

	void lbzx(UGeckoInstruction inst)
	{
		int a = inst.RA, b = inst.RB, d = inst.RD;
		gpr.Lock(a, b, d);
		if (b == d || a == d)
			gpr.LoadToX64(d, true, true);
		else 
			gpr.LoadToX64(d, false, true);
		MOV(32, R(ECX), gpr.R(b));
		if (a)
			ADD(32, R(ECX), gpr.R(a));
		SafeLoadECXtoEAX(8, 0);
		MOV(32, gpr.R(d), R(EAX));
		gpr.UnlockAll();
	}

	void SafeStoreECXtoEDX(int accessSize, int offset)
	{

	}

	void lXz(UGeckoInstruction inst)
	{
		int d = inst.RD;
		int a = inst.RA;

		// TODO(ector): Make it dynamically enable/disable idle skipping where appropriate
		// Will give nice boost to dual core mode
		// if (CommandProcessor::AllowIdleSkipping() && PixelEngine::AllowIdleSkipping())

		if (!Core::GetStartupParameter().bUseDualCore && 
			inst.OPCD == 32 && 
			(inst.hex & 0xFFFF0000)==0x800D0000 &&
			Memory::ReadUnchecked_U32(js.compilerPC+4)==0x28000000 &&
			Memory::ReadUnchecked_U32(js.compilerPC+8)==0x4182fff8)
		{
			//PowerPC::downcount -= PowerPC::OnIdle(uAddress);
			gpr.Flush(FLUSH_ALL);
			fpr.Flush(FLUSH_ALL);
#ifdef _M_IX86
			MOV(32, R(ECX), Imm32(PowerPC::ppcState.gpr[a] + (s32)(s16)inst.SIMM_16));
			PUSH(ECX);
			CALL((void *)&PowerPC::OnIdle);
			ADD(32, R(ESP), Imm32(4));
#elif defined(_M_X64)
			//INT3();
			MOV(32, R(ECX), Imm32(PowerPC::ppcState.gpr[a] + (s32)(s16)inst.SIMM_16));
			CALL((void *)&PowerPC::OnIdle);
#endif
			MOV(32,M(&PowerPC::ppcState.pc), Imm32(js.compilerPC+12));
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
#ifdef _M_IX86
		if (true) {
#elif defined(_M_X64)
		if (accessSize == 8 || accessSize == 16 || !jo.enableFastMem) {
#endif
			// Safe and boring
			gpr.Flush(FLUSH_VOLATILE);
			gpr.Lock(d, a);
			MOV(32,R(ECX), gpr.R(a));
			SafeLoadECXtoEAX(accessSize, offset);
			gpr.LoadToX64(d, false, true);
			MOV(32,gpr.R(d), R(EAX));
			gpr.UnlockAll();
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

 	u32 GC_ALIGNED16(temp32);
	u64 GC_ALIGNED16(temp64);

	void lfs(UGeckoInstruction inst)
	{
//		BIT32OLD;
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
		
		MOV(32,R(ECX),gpr.R(a));
#ifdef _M_X64
		if (!jo.noAssumeFPLoadFromMem)
		{
			MOV(32, R(EAX), MComplex(RBX,ECX,SCALE_1,offset));
//#else
//			MOV(32, R(EAX), MDisp(ECX, (u32)Memory::GetMainRAMPtr() + (u32)offset));
//#endif
			BSWAP(32,EAX);
		}
		else
#endif
		{
			SafeLoadECXtoEAX(32, offset);
		}

		MOV(32,M(&temp32), R(EAX));
		fpr.Lock(d);
		fpr.LoadToX64(d, false);
		CVTSS2SD(fpr.RX(d), M(&temp32));
		MOVDDUP(fpr.RX(d), fpr.R(d));
		gpr.UnlockAll();
		fpr.UnlockAll();
	}


	void lfd(UGeckoInstruction inst)
	{
		BIT32OLD;
		int d = inst.RD;
		int a = inst.RA;
		if (!a) 
		{
			Default(inst);
			return;
		}
		s32 offset = (s32)(s16)inst.SIMM_16;
		gpr.Lock(a);
		MOV(32, R(ECX), gpr.R(a));
		MOV(64, R(EAX), MComplex(RBX, ECX, SCALE_1, offset));
		BSWAP(64,EAX);
		MOV(64, M(&temp64), R(EAX));
		fpr.Lock(d);
		fpr.LoadToX64(d, false);
		MOVSD(fpr.RX(d), M(&temp64));
		MOVDDUP(fpr.RX(d), fpr.R(d));
		gpr.UnlockAll();
		fpr.UnlockAll();
	}

	void stfd(UGeckoInstruction inst)
	{
		BIT32OLD;
		OLD;
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
		MOV(32, R(ECX), gpr.R(a));
		MOV(64, R(EAX), M(&temp64));
		BSWAP(64, EAX);
		MOV(64, MComplex(RBX, ECX, SCALE_1, offset), R(EAX));
		gpr.UnlockAll();
		fpr.UnlockAll();
	}

	double GC_ALIGNED16(psTemp[2])  = {1.0, 1.0};
	
	void stfs(UGeckoInstruction inst)
	{
		BIT32OLD;
		OLD;
		bool update = inst.OPCD & 1;
		int s = inst.RS;
		int a = inst.RA;
		s32 offset = (s32)(s16)inst.SIMM_16;

		if (a && !update)
		{
			gpr.Flush(FLUSH_VOLATILE);
			gpr.Lock(a);
			fpr.Lock(s);
			MOV(32,R(EDX),gpr.R(a));
			if (offset)
				ADD(32,R(EDX),Imm32((u32)offset));
			if (update && offset)
			{
				MOV(32,gpr.R(a),R(EDX));
			}
			CVTSD2SS(XMM0,fpr.R(s));
			MOVSS(M(&temp32),XMM0);
			MOV(32,R(ECX),M(&temp32));

			TEST(32,R(EDX),Imm32(0x0C000000));
			FixupBranch argh = J_CC(CC_NZ);
			BSWAP(32,ECX);
			MOV(32, MComplex(RBX, EDX, SCALE_1, 0), R(ECX));
			FixupBranch arg2 = J();
			SetJumpTarget(argh);
			CALL((void *)&Memory::Write_U32); 
			SetJumpTarget(arg2);
			gpr.UnlockAll();
			fpr.UnlockAll();
		}
		else
		{
			Default(inst);
		}
	}

	void lfsx(UGeckoInstruction inst)
	{
#ifdef _M_IX86
		Default(inst);
		return;
#endif
		fpr.Lock(inst.RS);
		fpr.LoadToX64(inst.RS, false, true);
		MOV(32, R(EAX), gpr.R(inst.RB));
		if (inst.RA)
			ADD(32, R(EAX), gpr.R(inst.RA));
		MOV(32, R(EAX), MComplex(RBX, EAX, SCALE_1, 0));
		BSWAP(32, EAX);
		MOV(32, M(&temp32), R(EAX));
		CVTSS2SD(XMM0, M(&temp32));
		MOVDDUP(fpr.R(inst.RS).GetSimpleReg(), R(XMM0));
		fpr.UnlockAll();
	}


	// TODO(ector): Improve 64-bit version
	void WriteDual32(u64 value, u32 address)
	{
		Memory::Write_U32((u32)(value>>32), address);
		Memory::Write_U32((u32)value, address+4);
	}

	const double m_quantizeTableD[] =
	{
		(1 <<  0),	(1 <<  1),	(1 <<  2),	(1 <<  3),
		(1 <<  4),	(1 <<  5),	(1 <<  6),	(1 <<  7),
		(1 <<  8),	(1 <<  9),	(1 << 10),	(1 << 11),
		(1 << 12),	(1 << 13),	(1 << 14),	(1 << 15),
		(1 << 16),	(1 << 17),	(1 << 18),	(1 << 19),
		(1 << 20),	(1 << 21),	(1 << 22),	(1 << 23),
		(1 << 24),	(1 << 25),	(1 << 26),	(1 << 27),
		(1 << 28),	(1 << 29),	(1 << 30),	(1 << 31),
		1.0 / (1ULL << 32),	1.0 / (1 << 31),	1.0 / (1 << 30),	1.0 / (1 << 29),
		1.0 / (1 << 28),	1.0 / (1 << 27),	1.0 / (1 << 26),	1.0 / (1 << 25),
		1.0 / (1 << 24),	1.0 / (1 << 23),	1.0 / (1 << 22),	1.0 / (1 << 21),
		1.0 / (1 << 20),	1.0 / (1 << 19),	1.0 / (1 << 18),	1.0 / (1 << 17),
		1.0 / (1 << 16),	1.0 / (1 << 15),	1.0 / (1 << 14),	1.0 / (1 << 13),
		1.0 / (1 << 12),	1.0 / (1 << 11),	1.0 / (1 << 10),	1.0 / (1 <<  9),
		1.0 / (1 <<  8),	1.0 / (1 <<  7),	1.0 / (1 <<  6),	1.0 / (1 <<  5),
		1.0 / (1 <<  4),	1.0 / (1 <<  3),	1.0 / (1 <<  2),	1.0 / (1 <<  1),
	}; 

	const double m_dequantizeTableD[] =
	{
		1.0 / (1 <<  0),	1.0 / (1 <<  1),	1.0 / (1 <<  2),	1.0 / (1 <<  3),
		1.0 / (1 <<  4),	1.0 / (1 <<  5),	1.0 / (1 <<  6),	1.0 / (1 <<  7),
		1.0 / (1 <<  8),	1.0 / (1 <<  9),	1.0 / (1 << 10),	1.0 / (1 << 11),
		1.0 / (1 << 12),	1.0 / (1 << 13),	1.0 / (1 << 14),	1.0 / (1 << 15),
		1.0 / (1 << 16),	1.0 / (1 << 17),	1.0 / (1 << 18),	1.0 / (1 << 19),
		1.0 / (1 << 20),	1.0 / (1 << 21),	1.0 / (1 << 22),	1.0 / (1 << 23),
		1.0 / (1 << 24),	1.0 / (1 << 25),	1.0 / (1 << 26),	1.0 / (1 << 27),
		1.0 / (1 << 28),	1.0 / (1 << 29),	1.0 / (1 << 30),	1.0 / (1 << 31),
		(1ULL << 32),	(1 << 31),		(1 << 30),		(1 << 29),
		(1 << 28),		(1 << 27),		(1 << 26),		(1 << 25),
		(1 << 24),		(1 << 23),		(1 << 22),		(1 << 21),
		(1 << 20),		(1 << 19),		(1 << 18),		(1 << 17),
		(1 << 16),		(1 << 15),		(1 << 14),		(1 << 13),
		(1 << 12),		(1 << 11),		(1 << 10),		(1 <<  9),
		(1 <<  8),		(1 <<  7),		(1 <<  6),		(1 <<  5),
		(1 <<  4),		(1 <<  3),		(1 <<  2),		(1 <<  1),
	};  

	u32 temp;
	void psq_st(UGeckoInstruction inst)
	{
		BIT32OLD;
		OLD;
		const UGQR gqr(rSPR(SPR_GQR0 + inst.I));
		const EQuantizeType stType = static_cast<EQuantizeType>(gqr.ST_TYPE);
		int stScale = gqr.ST_SCALE;
		bool update = inst.OPCD == 61;
		if (!inst.RA || inst.W)
		{
		//	PanicAlert(inst.RA ? "W" : "inst");
			Default(inst);
			return;
		}

		int offset = inst.SIMM_12;
		int a = inst.RA;
		int s = inst.RS; // Fp numbers

		if (stType == QUANTIZE_FLOAT)
		{
			gpr.Flush(FLUSH_VOLATILE);
			gpr.Lock(a);
			fpr.Lock(s);
			if (update)
				gpr.LoadToX64(a, true, true);
			MOV(32, R(EDX), gpr.R(a));
			if (offset)
				ADD(32, R(EDX), Imm32((u32)offset));
			TEST(32, R(EDX), Imm32(0x0C000000));
			if (update && offset)
				MOV(32, gpr.R(a), R(EDX));
			CVTPD2PS(XMM0, fpr.R(s));
			SHUFPS(XMM0, R(XMM0), 1);
			MOVAPS(M(&temp64), XMM0);
			MOV(64, R(ECX), M(&temp64));
			FixupBranch argh = J_CC(CC_NZ);
			BSWAP(64, ECX);
			MOV(64, MComplex(RBX, EDX, SCALE_1, 0), R(ECX));
			FixupBranch arg2 = J();
			SetJumpTarget(argh);
			CALL((void *)&WriteDual32); 
			SetJumpTarget(arg2);
			if (update)
				MOV(32, gpr.R(a), R(EDX));
			gpr.UnlockAll();
			fpr.UnlockAll();
		}
		else if (stType == QUANTIZE_U8)
		{
			gpr.Flush(FLUSH_VOLATILE);
			gpr.Lock(a);
			fpr.Lock(s);
			if (update)
				gpr.LoadToX64(a, true, update);
			MOV(32, R(EDX), gpr.R(a));
			if (offset)
				ADD(32,R(EDX),Imm32((u32)offset));
			MOVAPS(XMM0, fpr.R(s));
			MOVDDUP(XMM1, M((void*)&m_quantizeTableD[stScale]));
			MULPD(XMM0, R(XMM1));
			CVTPD2DQ(XMM0, R(XMM0));
			PACKSSDW(XMM0, R(XMM0));
			PACKUSWB(XMM0, R(XMM0));
			MOVAPS(M(&temp64), XMM0);
			MOV(16, R(ECX), M(&temp64));
#ifdef _M_X64
			MOV(16, MComplex(RBX, RDX, SCALE_1, 0), R(ECX));
#else
			BSWAP(32, ECX);
			SHR(32, R(ECX), Imm8(16));
			CALL(&Memory::Write_U16);
#endif
			if (update)
				MOV(32, gpr.R(a), R(EDX));
			gpr.UnlockAll();
			fpr.UnlockAll();
		} 
		else if (stType == QUANTIZE_S16)
		{
			gpr.Lock(a);
			fpr.Lock(s);
			if (update)
				gpr.LoadToX64(a, true, update);
			MOV(32, R(EDX), gpr.R(a));
			if (offset)
				ADD(32,R(EDX),Imm32((u32)offset));
			MOVAPS(XMM0, fpr.R(s));
			MOVDDUP(XMM1, M((void*)&m_quantizeTableD[stScale]));
			MULPD(XMM0, R(XMM1));
			SHUFPD(XMM0, R(XMM0), 1);
			CVTPD2DQ(XMM0, R(XMM0));
			PACKSSDW(XMM0, R(XMM0));
			MOVD_xmm(M(&temp64), XMM0);
			MOV(32, R(ECX), M(&temp64));
#ifdef _M_X64
			BSWAP(32, ECX);
			MOV(32, MComplex(RBX, RDX, SCALE_1, 0), R(ECX));
#else
			BSWAP(32, ECX);
			CALL(&Memory::Write_U32);
#endif
			if (update)
				MOV(32, gpr.R(a), R(EDX));
			gpr.UnlockAll();
			fpr.UnlockAll();
		}
		else {
			// Dodger uses this.
			PanicAlert("st %i:%i", stType, inst.W);
			Default(inst);
		}
	}

	
	void psq_l(UGeckoInstruction inst)
	{
		BIT32OLD;
		OLD;
		const UGQR gqr(rSPR(SPR_GQR0 + inst.I));
		const EQuantizeType ldType = static_cast<EQuantizeType>(gqr.LD_TYPE);
		int ldScale = gqr.LD_SCALE;
		if (!inst.RA || inst.W)
		{
			// 0 1 during load
			//PanicAlert("ld:%i %i", ldType, (int)inst.W);
			Default(inst);
			return;
		}
		bool update = inst.OPCD == 57;
		int offset = inst.SIMM_12;
		//INT3();
		switch (ldType) {
#ifdef _M_X64
			case QUANTIZE_FLOAT:
				{
				gpr.LoadToX64(inst.RA);
				MOV(64, R(RAX), MComplex(RBX, gpr.R(inst.RA).GetSimpleReg(), 1, offset));
				BSWAP(64, RAX);
				MOV(64, M(&psTemp[0]),R(RAX));
				fpr.LoadToX64(inst.RS, false);
				X64Reg r = fpr.R(inst.RS).GetSimpleReg();
				CVTPS2PD(r, M(&psTemp[0]));
				SHUFPD(r, R(r),1);
				if (update)
					ADD(32, gpr.R(inst.RA), Imm32(offset));
				break;
				}

			case QUANTIZE_U8:
				{
				gpr.LoadToX64(inst.RA);
				XOR(32, R(EAX), R(EAX));
				MOV(16, R(EAX), MComplex(RBX, gpr.R(inst.RA).GetSimpleReg(), 1, offset));
				MOV(32, M(&temp64), R(EAX));
				MOVD_xmm(XMM0, M(&temp64));
				// SSE4 optimization opportunity here.
				PXOR(XMM1, R(XMM1));
				PUNPCKLBW(XMM0, R(XMM1));
				PUNPCKLWD(XMM0, R(XMM1));
				CVTDQ2PD(XMM0, R(XMM0));
				fpr.LoadToX64(inst.RS, false);
				X64Reg r = fpr.R(inst.RS).GetSimpleReg();
				MOVDDUP(r, M((void *)&m_dequantizeTableD[ldScale]));
				MULPD(r, R(XMM0));
				if (update)
					ADD(32, gpr.R(inst.RA), Imm32(offset));
				}
				break;
			case QUANTIZE_S16:
				{
				gpr.LoadToX64(inst.RA);
				MOV(32, R(EAX), MComplex(RBX, gpr.R(inst.RA).GetSimpleReg(), 1, offset));
				BSWAP(32, EAX);
				MOV(32, M(&temp64), R(EAX));
				//INT3();
				fpr.LoadToX64(inst.RS, false);
				X64Reg r = fpr.R(inst.RS).GetSimpleReg();
				MOVD_xmm(XMM0, M(&temp64));
				PUNPCKLWD(XMM0, R(XMM0)); // unpack to higher word in each dword..
				PSRAD(XMM0, 16);          // then use this signed shift to sign extend. clever eh? :P
				CVTDQ2PD(XMM0, R(XMM0));
				MOVDDUP(r, M((void*)&m_dequantizeTableD[ldScale]));
				MULPD(r, R(XMM0));
				SHUFPD(r, R(r), 1);
				if (update)
					ADD(32, gpr.R(inst.RA), Imm32(offset));
				}
				break;
#endif	
			default:
				// 4 0
				PanicAlert("ld:%i %i", ldType, (int)inst.W);
				Default(inst);
				return;
		}

		//u32 EA = (m_GPR[_inst.RA] + _inst.SIMM_12) : _inst.SIMM_12;
	}

	void dcbz(UGeckoInstruction inst)
	{
#ifdef _M_IX86
		Default(inst);
		return;
#endif
		MOV(32, R(EAX), gpr.R(inst.RB));
		if (inst.RA)
			ADD(32, R(EAX), gpr.R(inst.RA));
		AND(32, R(EAX), Imm32(~31));
		XORPD(XMM0, R(XMM0));
		MOVAPS(MComplex(EBX, EAX, SCALE_1, 0), XMM0);
		MOVAPS(MComplex(EBX, EAX, SCALE_1, 16), XMM0);
	}

	void stX(UGeckoInstruction inst)
	{
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

			int accessSize;
			switch (inst.OPCD & ~1)
			{
			case 36: accessSize = 32; break; //stw
			case 44: accessSize = 16; break; //sth
			case 38: accessSize = 8; break;  //stb
			default: _assert_msg_(DYNA_REC,0,"AWETKLJASDLKF"); return;
			}

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

			//Still here? Do regular path.
			gpr.Lock(s, a);
			MOV(32, R(EDX), gpr.R(a));
			MOV(32, R(ECX), gpr.R(s));
			if (offset)
				ADD(32, R(EDX), Imm32((u32)offset));
			if (update && offset)
			{
				MOV(32, gpr.R(a), R(EDX));
			}
			TEST(32, R(EDX), Imm32(0x0C000000));
			FixupBranch argh = J_CC(CC_NZ);
			if (accessSize == 32)
				BSWAP(32, ECX);
			else if (accessSize == 16)
			{
				// TODO(ector): xchg ch, cl?
				BSWAP(32, ECX);
				SHR(32, R(ECX), Imm8(16));
			}
#ifdef _M_X64
			MOV(accessSize, MComplex(RBX, EDX, SCALE_1, 0), R(ECX));
#else
			AND(32, R(EDX), Imm32(Memory::MEMVIEW32_MASK));
			MOV(accessSize, MDisp(EDX, (u32)Memory::base), R(ECX));
#endif
			FixupBranch arg2 = J();
			SetJumpTarget(argh);
#ifdef _M_IX86
			PUSH(EDX);
			PUSH(ECX);
#endif
			switch (accessSize)
			{
			case 32: CALL((void *)&Memory::Write_U32); break;
			case 16: CALL((void *)&Memory::Write_U16); break;
			case 8:  CALL((void *)&Memory::Write_U8); break;
			}
#ifdef _M_IX86
			ADD(32, R(ESP), Imm8(8));
#endif
			SetJumpTarget(arg2);
			gpr.UnlockAll();
		}
		else
		{
			Default(inst);
		}
	}

	// A few games use these heavily.
	void lmw(UGeckoInstruction inst)
	{
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

