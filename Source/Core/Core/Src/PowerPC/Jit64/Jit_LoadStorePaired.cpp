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

namespace Jit64 {

static double GC_ALIGNED16(psTemp[2]) = {1.0, 1.0};
static u64 GC_ALIGNED16(temp64);
static u32 GC_ALIGNED16(temp32);
static u32 temp;

// TODO(ector): Improve 64-bit version
void WriteDual32(u64 value, u32 address)
{
	Memory::Write_U32((u32)(value >> 32), address);
	Memory::Write_U32((u32)value, address + 4);
}

static const double m_quantizeTableD[] =
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

static const double m_dequantizeTableD[] =
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

// The big problem is likely instructions that set the quantizers in the same block.
// We will have to break block after quantizers are written to.
void psq_st(UGeckoInstruction inst)
{
	INSTRUCTION_START;
	DISABLE_32BIT;
	if (js.blockSetsQuantizers || !Core::GetStartupParameter().bOptimizeQuantizers)
	{
		Default(inst);
		return;
	}
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
		MOV(32, R(ABI_PARAM2), gpr.R(a));
		if (offset)
			ADD(32, R(ABI_PARAM2), Imm32((u32)offset));
		TEST(32, R(ABI_PARAM2), Imm32(0x0C000000));
		if (update && offset)
			MOV(32, gpr.R(a), R(ABI_PARAM2));
		CVTPD2PS(XMM0, fpr.R(s));
		SHUFPS(XMM0, R(XMM0), 1);
		MOVAPS(M(&temp64), XMM0);
		MOV(64, R(ABI_PARAM1), M(&temp64));
		FixupBranch argh = J_CC(CC_NZ);
		BSWAP(64, ABI_PARAM1);
		MOV(64, MComplex(RBX, ABI_PARAM2, SCALE_1, 0), R(ABI_PARAM1));
		FixupBranch arg2 = J();
		SetJumpTarget(argh);
		CALL((void *)&WriteDual32); 
		SetJumpTarget(arg2);
		if (update)
			MOV(32, gpr.R(a), R(ABI_PARAM2));
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
		MOV(32, R(ABI_PARAM2), gpr.R(a));
		if (offset)
			ADD(32, R(ABI_PARAM2), Imm32((u32)offset));
		MOVAPS(XMM0, fpr.R(s));
		MOVDDUP(XMM1, M((void*)&m_quantizeTableD[stScale]));
		MULPD(XMM0, R(XMM1));
		CVTPD2DQ(XMM0, R(XMM0));
		PACKSSDW(XMM0, R(XMM0));
		PACKUSWB(XMM0, R(XMM0));
		MOVAPS(M(&temp64), XMM0);
		MOV(16, R(ABI_PARAM1), M(&temp64));
#ifdef _M_X64
		MOV(16, MComplex(RBX, ABI_PARAM2, SCALE_1, 0), R(ABI_PARAM1));
#else
		BSWAP(32, ABI_PARAM1);
		SHR(32, R(ABI_PARAM1), Imm8(16));
		CALL(&Memory::Write_U16);
#endif
		if (update)
			MOV(32, gpr.R(a), R(ABI_PARAM2));
		gpr.UnlockAll();
		fpr.UnlockAll();
	} 
	else if (stType == QUANTIZE_S16)
	{
		gpr.Lock(a);
		fpr.Lock(s);
		if (update)
			gpr.LoadToX64(a, true, update);
		MOV(32, R(ABI_PARAM2), gpr.R(a));
		if (offset)
			ADD(32, R(ABI_PARAM2), Imm32((u32)offset));
		MOVAPS(XMM0, fpr.R(s));
		MOVDDUP(XMM1, M((void*)&m_quantizeTableD[stScale]));
		MULPD(XMM0, R(XMM1));
		SHUFPD(XMM0, R(XMM0), 1);
		CVTPD2DQ(XMM0, R(XMM0));
		PACKSSDW(XMM0, R(XMM0));
		MOVD_xmm(M(&temp64), XMM0);
		MOV(32, R(ABI_PARAM1), M(&temp64));
#ifdef _M_X64
		BSWAP(32, ABI_PARAM1);
		MOV(32, MComplex(RBX, ABI_PARAM2, SCALE_1, 0), R(ABI_PARAM1));
#else
		BSWAP(32, ABI_PARAM1);
		PUSH(32, R(ABI_PARAM1));
		CALL(&Memory::Write_U32);
#endif
		if (update)
			MOV(32, gpr.R(a), R(ABI_PARAM2));
		gpr.UnlockAll();
		fpr.UnlockAll();
	}
	else {
		// Dodger uses this.
        // mario tennis
		//PanicAlert("st %i:%i", stType, inst.W);
		Default(inst);
	}
}


void psq_l(UGeckoInstruction inst)
{
	INSTRUCTION_START;
	DISABLE_32BIT;
	if (js.blockSetsQuantizers || !Core::GetStartupParameter().bOptimizeQuantizers)
	{
		Default(inst);
		return;
	}
	const UGQR gqr(rSPR(SPR_GQR0 + inst.I));
	const EQuantizeType ldType = static_cast<EQuantizeType>(gqr.LD_TYPE);
	int ldScale = gqr.LD_SCALE;
	bool update = inst.OPCD == 57;
	if (!inst.RA || inst.W)
	{
		// 0 1 during load
		//PanicAlert("ld:%i %i", ldType, (int)inst.W);
		Default(inst);
		return;
	}
	int offset = inst.SIMM_12;
	//INT3();
	switch (ldType) {
#ifdef _M_X64
		case QUANTIZE_FLOAT:
			{
			gpr.LoadToX64(inst.RA);
			MOV(64, R(RAX), MComplex(RBX, gpr.R(inst.RA).GetSimpleReg(), 1, offset));
			BSWAP(64, RAX);
			MOV(64, M(&psTemp[0]), R(RAX));
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

			/*
			Dynamic quantizer. Todo when we have a test set.
			MOVZX(32, 8, EAX, M(((char *)&PowerPC::ppcState.spr[SPR_GQR0 + inst.I]) + 3));  // it's in the high byte.
			AND(32, R(EAX), Imm8(0x3F));
			MOV(32, R(ECX), Imm32((u32)&m_dequantizeTableD));
			MOVDDUP(r, MComplex(RCX, EAX, 8, 0));
			*/
#endif	
		default:
			// 4 0
			// 6 0 //power tennis
			// 5 0 
			//PanicAlert("ld:%i %i", ldType, (int)inst.W);
			Default(inst);
			return;
	}

	//u32 EA = (m_GPR[_inst.RA] + _inst.SIMM_12) : _inst.SIMM_12;
}

}  // namespace
