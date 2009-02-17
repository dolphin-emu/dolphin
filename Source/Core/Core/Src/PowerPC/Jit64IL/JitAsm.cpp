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

#include "ABI.h"
#include "x64Emitter.h"

#include "../../HW/Memmap.h"

#include "../PowerPC.h"
#include "../../CoreTiming.h"
#include "MemoryUtil.h"
#include "CPUDetect.h"

#include "ABI.h"
#include "Jit.h"
#include "JitCache.h"
#include "Thunk.h"

#include "../../HW/CPUCompare.h"
#include "../../HW/GPFifo.h"
#include "../../Core.h"
#include "JitAsm.h"

using namespace Gen;
int blocksExecuted;

static int temp32;

bool compareEnabled = false;

//TODO - make an option
//#if _DEBUG
static bool enableDebug = false; 
//#else
//		bool enableDebug = false; 
//#endif

static bool enableStatistics = false;

//GLOBAL STATIC ALLOCATIONS x86
//EAX - ubiquitous scratch register - EVERYBODY scratches this

//GLOBAL STATIC ALLOCATIONS x64
//EAX - ubiquitous scratch register - EVERYBODY scratches this
//RBX - Base pointer of memory
//R15 - Pointer to array of block pointers 

AsmRoutineManager asm_routines;

// PLAN: no more block numbers - crazy opcodes just contain offset within
// dynarec buffer
// At this offset - 4, there is an int specifying the block number.

void AsmRoutineManager::Generate()
{
	enterCode = AlignCode16();
	ABI_PushAllCalleeSavedRegsAndAdjustStack();
#ifndef _M_IX86
	// Two statically allocated registers.
	MOV(64, R(RBX), Imm64((u64)Memory::base));
	MOV(64, R(R15), Imm64((u64)jit.GetBlockCache()->GetCodePointers())); //It's below 2GB so 32 bits are good enough
#endif
//	INT3();

	const u8 *outerLoop = GetCodePtr();
		ABI_CallFunction(reinterpret_cast<void *>(&CoreTiming::Advance));
		FixupBranch skipToRealDispatch = J(); //skip the sync and compare first time
	
		dispatcher = GetCodePtr();
			//This is the place for CPUCompare!

			//The result of slice decrementation should be in flags if somebody jumped here
			FixupBranch bail = J_CC(CC_S);
			SetJumpTarget(skipToRealDispatch);

			dispatcherNoCheck = GetCodePtr();
			MOV(32, R(EAX), M(&PowerPC::ppcState.pc));
			dispatcherPcInEAX = GetCodePtr();
#ifdef _M_IX86
			AND(32, R(EAX), Imm32(Memory::MEMVIEW32_MASK));
			MOV(32, R(EBX), Imm32((u32)Memory::base));
			MOV(32, R(EAX), MComplex(EBX, EAX, SCALE_1, 0));
#else
			MOV(32, R(EAX), MComplex(RBX, RAX, SCALE_1, 0));
#endif
			TEST(32, R(EAX), Imm32(0xFC));
			FixupBranch notfound = J_CC(CC_NZ);
				BSWAP(32, EAX);
				//IDEA - we have 26 bits, why not just use offsets from base of code?
				if (enableStatistics)
				{
					ADD(32, M(&blocksExecuted), Imm8(1));
				}
				if (enableDebug)
				{
					ADD(32, M(&PowerPC::ppcState.DebugCount), Imm8(1));
				}
				//grab from list and jump to it
#ifdef _M_IX86
				MOV(32, R(EDX), ImmPtr(jit.GetBlockCache()->GetCodePointers()));
				JMPptr(MComplex(EDX, EAX, 4, 0));
#else
				JMPptr(MComplex(R15, RAX, 8, 0));
#endif
			SetJumpTarget(notfound);

			//Ok, no block, let's jit
#ifdef _M_IX86
			ABI_AlignStack(4);
			PUSH(32, M(&PowerPC::ppcState.pc));
			CALL(reinterpret_cast<void *>(&Jit));
			ABI_RestoreStack(4);
#else
			MOV(32, R(ABI_PARAM1), M(&PowerPC::ppcState.pc));
			CALL((void *)&Jit);
#endif
			JMP(dispatcherNoCheck); // no point in special casing this

			//FP blocks test for FPU available, jump here if false
			fpException = AlignCode4(); 
			MOV(32, R(EAX), M(&PC));
			MOV(32, M(&NPC), R(EAX));
			OR(32, M(&PowerPC::ppcState.Exceptions), Imm32(EXCEPTION_FPU_UNAVAILABLE));
			ABI_CallFunction(reinterpret_cast<void *>(&PowerPC::CheckExceptions));
			MOV(32, R(EAX), M(&NPC));
			MOV(32, M(&PC), R(EAX));
			JMP(dispatcher);

		SetJumpTarget(bail);
		doTiming = GetCodePtr();

		ABI_CallFunction(reinterpret_cast<void *>(&CoreTiming::Advance));
		
		testExceptions = GetCodePtr();
		TEST(32, M(&PowerPC::ppcState.Exceptions), Imm32(0xFFFFFFFF));
		FixupBranch skipExceptions = J_CC(CC_Z);
			MOV(32, R(EAX), M(&PC));
			MOV(32, M(&NPC), R(EAX));
			ABI_CallFunction(reinterpret_cast<void *>(&PowerPC::CheckExceptions));
			MOV(32, R(EAX), M(&NPC));
			MOV(32, M(&PC), R(EAX));
		SetJumpTarget(skipExceptions);
		
		TEST(32, M((void*)PowerPC::GetStatePtr()), Imm32(0xFFFFFFFF));
		J_CC(CC_Z, outerLoop, true);
	//Landing pad for drec space
	ABI_PopAllCalleeSavedRegsAndAdjustStack();
	RET();

	breakpointBailout = GetCodePtr();
	//Landing pad for drec space
	ABI_PopAllCalleeSavedRegsAndAdjustStack();
	RET();

	GenerateCommon();
}

const u8 GC_ALIGNED16(pbswapShuffle2x4[16]) = {3, 2, 1, 0, 7, 6, 5, 4, 8, 9, 10, 11, 12, 13, 14, 15};

const float m_quantizeTableS[] =
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

const float m_dequantizeTableS[] =
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

float psTemp[2];

void AsmRoutineManager::GenQuantizedStores() {
	const u8* storePairedIllegal = AlignCode4();
	UD2();
	const u8* storePairedFloat = AlignCode4();
#ifdef _M_X64
	MOVQ_xmm(R(RAX), XMM0);
	ROL(64, R(RAX), Imm8(32));
	TEST(32, R(ECX), Imm32(0x0C000000));
	FixupBranch argh = J_CC(CC_NZ);
	BSWAP(64, RAX);
	MOV(64, MComplex(RBX, RCX, 1, 0), R(RAX));
	FixupBranch arg2 = J();
	SetJumpTarget(argh);
	ABI_CallFunctionRR(thunks.ProtectFunction((void *)&Memory::Write_U64, 2), RAX, RCX); 
	SetJumpTarget(arg2);
#else
	MOVQ_xmm(M(&psTemp[0]), XMM0);
	TEST(32, R(ECX), Imm32(0x0C000000));
	FixupBranch argh = J_CC(CC_NZ);
	MOV(32, R(EAX), M(&psTemp));
	BSWAP(32, EAX);
	AND(32, R(ECX), Imm32(Memory::MEMVIEW32_MASK));
	MOV(32, MDisp(ECX, (u32)Memory::base), R(EAX));
	MOV(32, R(EAX), M(((char*)&psTemp) + 4));
	BSWAP(32, EAX);
	MOV(32, MDisp(ECX, 4+(u32)Memory::base), R(EAX));
	FixupBranch arg2 = J();
	SetJumpTarget(argh);
	MOV(32, R(EAX), M(((char*)&psTemp)));
	ABI_CallFunctionRR(thunks.ProtectFunction((void *)&Memory::Write_U32, 2), EAX, ECX); 
	MOV(32, R(EAX), M(((char*)&psTemp)+4));
	ADD(32, R(ECX), Imm32(4));
	ABI_CallFunctionRR(thunks.ProtectFunction((void *)&Memory::Write_U32, 2), EAX, ECX); 
	SetJumpTarget(arg2);
#endif
	RET();

	const u8* storePairedU8 = AlignCode4();
	SHR(32, R(EAX), Imm8(6));
	MOVSS(XMM1, MDisp(EAX, (u32)(u64)m_quantizeTableS));
	PUNPCKLDQ(XMM1, R(XMM1));
	MULPS(XMM0, R(XMM1));
	CVTPS2DQ(XMM0, R(XMM0));
	PACKSSDW(XMM0, R(XMM0));
	PACKUSWB(XMM0, R(XMM0));
	MOVD_xmm(R(EAX), XMM0);
#ifdef _M_X64
	MOV(16, MComplex(RBX, RCX, 1, 0), R(AX));
#else
	AND(32, R(ECX), Imm32(Memory::MEMVIEW32_MASK));
	MOV(16, MDisp(ECX, (u32)Memory::base), R(AX));
#endif
	RET();

	const u8* storePairedS8 = AlignCode4();
	SHR(32, R(EAX), Imm8(6));
	MOVSS(XMM1, MDisp(EAX, (u32)(u64)m_quantizeTableS));
	PUNPCKLDQ(XMM1, R(XMM1));
	MULPS(XMM0, R(XMM1));
	CVTPS2DQ(XMM0, R(XMM0));
	PACKSSDW(XMM0, R(XMM0));
	PACKSSWB(XMM0, R(XMM0));
	MOVD_xmm(R(EAX), XMM0);
#ifdef _M_X64
	MOV(16, MComplex(RBX, RCX, 1, 0), R(AX));
#else
	AND(32, R(ECX), Imm32(Memory::MEMVIEW32_MASK));
	MOV(16, MDisp(ECX, (u32)Memory::base), R(AX));
#endif
	RET();

	const u8* storePairedU16 = AlignCode4();
	SHR(32, R(EAX), Imm8(6));
	MOVSS(XMM1, MDisp(EAX, (u32)(u64)m_quantizeTableS));
	PUNPCKLDQ(XMM1, R(XMM1));
	MULPS(XMM0, R(XMM1));
	CVTPS2DQ(XMM0, R(XMM0));
	PXOR(XMM1, R(XMM1));
	PCMPGTD(XMM1, R(XMM0));
	PANDN(XMM0, R(XMM1));
	PACKSSDW(XMM0, R(XMM0)); //PACKUSDW(XMM0, R(XMM0)); // FIXME: Wrong!
	MOVD_xmm(R(EAX), XMM0);
	BSWAP(32, EAX);
	ROL(32, R(EAX), Imm8(16));
#ifdef _M_X64
	MOV(32, MComplex(RBX, RCX, 1, 0), R(EAX));
#else
	AND(32, R(ECX), Imm32(Memory::MEMVIEW32_MASK));
	MOV(32, MDisp(ECX, (u32)Memory::base), R(EAX));
#endif
	RET();

	const u8* storePairedS16 = AlignCode4();
	SHR(32, R(EAX), Imm8(6));
	MOVSS(XMM1, MDisp(EAX, (u32)(u64)m_quantizeTableS));
	PUNPCKLDQ(XMM1, R(XMM1));
	MULPS(XMM0, R(XMM1));
	CVTPS2DQ(XMM0, R(XMM0));
	PACKSSDW(XMM0, R(XMM0));
	MOVD_xmm(R(EAX), XMM0);
	BSWAP(32, EAX);
	ROL(32, R(EAX), Imm8(16));
#ifdef _M_X64
	MOV(32, MComplex(RBX, RCX, 1, 0), R(EAX));
#else
	AND(32, R(ECX), Imm32(Memory::MEMVIEW32_MASK));
	MOV(32, MDisp(ECX, (u32)Memory::base), R(EAX));
#endif
	RET();

	pairedStoreQuantized[0] = storePairedFloat;
	pairedStoreQuantized[1] = storePairedIllegal;
	pairedStoreQuantized[2] = storePairedIllegal;
	pairedStoreQuantized[3] = storePairedIllegal;
	pairedStoreQuantized[4] = storePairedU8;
	pairedStoreQuantized[5] = storePairedU16;
	pairedStoreQuantized[6] = storePairedS8;
	pairedStoreQuantized[7] = storePairedS16;
}

void AsmRoutineManager::GenQuantizedLoads() {
	const u8* loadPairedIllegal = AlignCode4();
	UD2();
	const u8* loadPairedFloat = AlignCode4();
	if (cpu_info.bSSSE3) {
#ifdef _M_X64
		MOVQ_xmm(XMM0, MComplex(RBX, RCX, 1, 0));
#else
		AND(32, R(ECX), Imm32(Memory::MEMVIEW32_MASK));
		MOVQ_xmm(XMM0, MDisp(ECX, (u32)Memory::base));
#endif
		PSHUFB(XMM0, M((void *)pbswapShuffle2x4));
	} else {
#ifdef _M_X64
		MOV(64, R(RCX), MComplex(RBX, RCX, 1, 0));
		BSWAP(64, RCX);
		ROL(64, R(RCX), Imm8(32));
		MOVQ_xmm(XMM0, R(RCX));
#else
#if 0
		AND(32, R(ECX), Imm32(Memory::MEMVIEW32_MASK));
		MOVQ_xmm(XMM0, MDisp(ECX, (u32)Memory::base));
		PXOR(XMM1, R(XMM1));
		PSHUFLW(XMM0, R(XMM0), 0xB1);
		MOVAPD(XMM1, R(XMM0));
		PSRLW(XMM0, 8);
		PSLLW(XMM1, 8);
		POR(XMM0, R(XMM1));
#else
		AND(32, R(ECX), Imm32(Memory::MEMVIEW32_MASK));
		MOV(32, R(EAX), MDisp(ECX, (u32)Memory::base));
		BSWAP(32, EAX);
		MOV(32, M(&psTemp[0]), R(RAX));
		MOV(32, R(EAX), MDisp(ECX, (u32)Memory::base + 4));
		BSWAP(32, EAX);
		MOV(32, M(((float *)&psTemp[0]) + 1), R(RAX));
		MOVQ_xmm(XMM0, M(&psTemp[0]));
#endif
#endif
	}
	RET();

	const u8* loadPairedU8 = AlignCode4();
#ifdef _M_X64
	MOVZX(32, 16, ECX, MComplex(RBX, RCX, 1, 0));
#else
	AND(32, R(ECX), Imm32(Memory::MEMVIEW32_MASK));
	MOVZX(32, 16, ECX, MDisp(ECX, (u32)Memory::base));
#endif
	MOVD_xmm(XMM0, R(ECX));
	PXOR(XMM1, R(XMM1));
	PUNPCKLBW(XMM0, R(XMM1));
	PUNPCKLWD(XMM0, R(XMM1));
	CVTDQ2PS(XMM0, R(XMM0));
	SHR(32, R(EAX), Imm8(6));
	MOVSS(XMM1, MDisp(EAX, (u32)(u64)m_dequantizeTableS));
	PUNPCKLDQ(XMM1, R(XMM1));
	MULPS(XMM0, R(XMM1));
	RET();

	const u8* loadPairedS8 = AlignCode4();
#ifdef _M_X64
	MOVZX(32, 16, ECX, MComplex(RBX, RCX, 1, 0));
#else
	AND(32, R(ECX), Imm32(Memory::MEMVIEW32_MASK));
	MOVZX(32, 16, ECX, MDisp(ECX, (u32)Memory::base));
#endif
	MOVD_xmm(XMM0, R(ECX));
	PUNPCKLBW(XMM0, R(XMM0));
	PUNPCKLWD(XMM0, R(XMM0));
	PSRAD(XMM0, 24);
	CVTDQ2PS(XMM0, R(XMM0));
	SHR(32, R(EAX), Imm8(6));
	MOVSS(XMM1, MDisp(EAX, (u32)(u64)m_dequantizeTableS));
	PUNPCKLDQ(XMM1, R(XMM1));
	MULPS(XMM0, R(XMM1));
	RET();

	const u8* loadPairedU16 = AlignCode4();
#ifdef _M_X64
	MOV(32, R(ECX), MComplex(RBX, RCX, 1, 0));
#else
	AND(32, R(ECX), Imm32(Memory::MEMVIEW32_MASK));
	MOV(32, R(ECX), MDisp(ECX, (u32)Memory::base));
#endif
	BSWAP(32, ECX);
	ROL(32, R(ECX), Imm8(16));
	MOVD_xmm(XMM0, R(ECX));
	PXOR(XMM1, R(XMM1));
	PUNPCKLWD(XMM0, R(XMM1));
	CVTDQ2PS(XMM0, R(XMM0));
	SHR(32, R(EAX), Imm8(6));
	MOVSS(XMM1, MDisp(EAX, (u32)(u64)m_dequantizeTableS));
	PUNPCKLDQ(XMM1, R(XMM1));
	MULPS(XMM0, R(XMM1));
	RET();

	const u8* loadPairedS16 = AlignCode4();
#ifdef _M_X64
	MOV(32, R(ECX), MComplex(RBX, RCX, 1, 0));
#else
	AND(32, R(ECX), Imm32(Memory::MEMVIEW32_MASK));
	MOV(32, R(ECX), MDisp(ECX, (u32)Memory::base));
#endif
	BSWAP(32, ECX);
	ROL(32, R(ECX), Imm8(16));
	MOVD_xmm(XMM0, R(ECX));
	PUNPCKLWD(XMM0, R(XMM0));
	PSRAD(XMM0, 16);
	CVTDQ2PS(XMM0, R(XMM0));
	SHR(32, R(EAX), Imm8(6));
	AND(32, R(EAX), Imm32(0xFC));
	MOVSS(XMM1, MDisp(EAX, (u32)(u64)m_dequantizeTableS));
	PUNPCKLDQ(XMM1, R(XMM1));
	MULPS(XMM0, R(XMM1));
	RET();

	pairedLoadQuantized[0] = loadPairedFloat;
	pairedLoadQuantized[1] = loadPairedIllegal;
	pairedLoadQuantized[2] = loadPairedIllegal;
	pairedLoadQuantized[3] = loadPairedIllegal;
	pairedLoadQuantized[4] = loadPairedU8;
	pairedLoadQuantized[5] = loadPairedU16;
	pairedLoadQuantized[6] = loadPairedS8;
	pairedLoadQuantized[7] = loadPairedS16;
}

void AsmRoutineManager::GenFifoWrite(int size) 
{
	// Assume value in ABI_PARAM1
	PUSH(ESI);
	if (size != 32)
		PUSH(EDX);
	BSWAP(size, ABI_PARAM1);
	MOV(32, R(EAX), Imm32((u32)(u64)GPFifo::m_gatherPipe));
	MOV(32, R(ESI), M(&GPFifo::m_gatherPipeCount));
	if (size != 32) {
		MOV(32, R(EDX), R(ABI_PARAM1));
		MOV(size, MComplex(RAX, RSI, 1, 0), R(EDX));
	} else {
		MOV(size, MComplex(RAX, RSI, 1, 0), R(ABI_PARAM1));
	}
	ADD(32, R(ESI), Imm8(size >> 3));
	MOV(32, M(&GPFifo::m_gatherPipeCount), R(ESI));
	if (size != 32)
		POP(EDX);
	POP(ESI);
	RET();
}

void AsmRoutineManager::GenFifoFloatWrite() 
{
	// Assume value in XMM0
	PUSH(ESI);
	PUSH(EDX);
	MOVSS(M(&temp32), XMM0);
	MOV(32, R(EDX), M(&temp32));
	BSWAP(32, EDX);
	MOV(32, R(EAX), Imm32((u32)(u64)GPFifo::m_gatherPipe));
	MOV(32, R(ESI), M(&GPFifo::m_gatherPipeCount));
	MOV(32, MComplex(RAX, RSI, 1, 0), R(EDX));
	ADD(32, R(ESI), Imm8(4));
	MOV(32, M(&GPFifo::m_gatherPipeCount), R(ESI));
	POP(EDX);
	POP(ESI);
	RET();
}

void AsmRoutineManager::GenFifoXmm64Write() 
{
	// Assume value in XMM0. Assume pre-byteswapped (unlike the others here!)
	PUSH(ESI);
	MOV(32, R(EAX), Imm32((u32)(u64)GPFifo::m_gatherPipe));
	MOV(32, R(ESI), M(&GPFifo::m_gatherPipeCount));
	MOVQ_xmm(MComplex(RAX, RSI, 1, 0), XMM0);
	ADD(32, R(ESI), Imm8(8));
	MOV(32, M(&GPFifo::m_gatherPipeCount), R(ESI));
	POP(ESI);
	RET();
}

void AsmRoutineManager::GenerateCommon()
{
	// USES_CR
	computeRc = AlignCode16();
	CMP(32, R(EAX), Imm8(0));
	FixupBranch pLesser  = J_CC(CC_L);
	FixupBranch pGreater = J_CC(CC_G);
	MOV(8, M(&PowerPC::ppcState.cr_fast[0]), Imm8(0x2)); // _x86Reg == 0
	RET();
	SetJumpTarget(pGreater);
	MOV(8, M(&PowerPC::ppcState.cr_fast[0]), Imm8(0x4)); // _x86Reg > 0
	RET();
	SetJumpTarget(pLesser);
	MOV(8, M(&PowerPC::ppcState.cr_fast[0]), Imm8(0x8)); // _x86Reg < 0
	RET();
	
	fifoDirectWrite8 = AlignCode4();
	GenFifoWrite(8);
	fifoDirectWrite16 = AlignCode4();
	GenFifoWrite(16);
	fifoDirectWrite32 = AlignCode4();
	GenFifoWrite(32);
	fifoDirectWriteFloat = AlignCode4();
	GenFifoFloatWrite();
	fifoDirectWriteXmm64 = AlignCode4();
	GenFifoXmm64Write();

	doReJit = AlignCode4();
	ABI_AlignStack(0);
	CALL(reinterpret_cast<void *>(&ProfiledReJit));
	ABI_RestoreStack(0);
	SUB(32, M(&CoreTiming::downcount), Imm8(0));
	JMP(dispatcher, true);

	GenQuantizedLoads();
	GenQuantizedStores();

	computeRcFp = AlignCode16();
	//CMPSD(R(XMM0), M(&zero), 
	// TODO

	// Fast write routines - special case the most common hardware write
	// TODO: use this.
	// Even in x86, the param values will be in the right registers.
	/*
	const u8 *fastMemWrite8 = AlignCode16();
	CMP(32, R(ABI_PARAM2), Imm32(0xCC008000));
	FixupBranch skip_fast_write = J_CC(CC_NE, false);
	MOV(32, EAX, M(&m_gatherPipeCount));
	MOV(8, MDisp(EAX, (u32)&m_gatherPipe), ABI_PARAM1);
	ADD(32, 1, M(&m_gatherPipeCount));
	RET();
	SetJumpTarget(skip_fast_write);
	CALL((void *)&Memory::Write_U8);*/
}
