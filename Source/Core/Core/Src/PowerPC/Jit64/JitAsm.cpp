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

#include "ABI.h"
#include "Jit.h"
#include "JitCache.h"

#include "../../HW/CPUCompare.h"
#include "../../HW/GPFifo.h"
#include "../../Core.h"

using namespace Gen;
int blocksExecuted;

namespace Jit64
{

namespace Asm
{
const u8 *enterCode;
const u8 *testExceptions;
const u8 *fpException;
const u8 *doTiming;
const u8 *dispatcher;
const u8 *dispatcherNoCheck;
const u8 *dispatcherPcInEAX;
const u8 *computeRc;
const u8 *computeRcFp;

const u8 *fifoDirectWrite8;
const u8 *fifoDirectWrite16;
const u8 *fifoDirectWrite32;
const u8 *fifoDirectWriteFloat;

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


// PLAN: no more block numbers - crazy opcodes just contain offset within
// dynarec buffer
// At this offset - 4, there is an int specifying the block number.


void GenerateCommon();

#ifdef _M_IX86
void Generate()
{
	enterCode = AlignCode16();
	PUSH(EBP);
	PUSH(EBX);
	PUSH(ESI);
	PUSH(EDI);

	//MOV(32, R(EBX), Imm32((u32)&Memory::base));
	const u8 *outerLoop = GetCodePtr();
		CALL(reinterpret_cast<void *>(&CoreTiming::Advance));
		FixupBranch skipToRealDispatch = J(); //skip the sync and compare first time
	
		dispatcher = GetCodePtr();
			//This is the place for CPUCompare!

			//The result of slice decrementation should be in flags if somebody jumped here
			//Jump on negative, not carry!!!
			FixupBranch bail = J_CC(CC_BE);
			if (Core::bReadTrace || Core::bWriteTrace)
			{
				CALL(reinterpret_cast<void *>(&Core::SyncTrace));
				//			CMP(32, R(EAX),Imm32(0));
				//			bail2 = J_CC();
			}
			SetJumpTarget(skipToRealDispatch);
			//TEST(32,M(&PowerPC::ppcState.Exceptions), Imm32(0xFFFFFFFF));
			//FixupBranch bail2 = J_CC(CC_NZ);
			dispatcherNoCheck = GetCodePtr();
			MOV(32, R(EAX), M(&PowerPC::ppcState.pc));
			dispatcherPcInEAX = GetCodePtr();
			
			AND(32, R(EAX), Imm32(Memory::MEMVIEW32_MASK));
			MOV(32, R(EBX), Imm32((u32)Memory::base));
			MOV(32, R(EAX), MComplex(EBX, EAX, SCALE_1, 0));
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
				//INT3();
				MOV(32, R(EDX), ImmPtr(GetCodePointers()));
				JMPptr(MComplex(EDX, EAX, 4, 0));
			SetJumpTarget(notfound);

			//Ok, no block, let's jit
			ABI_AlignStack(4);
			PUSH(32, M(&PowerPC::ppcState.pc));
			CALL(reinterpret_cast<void *>(&Jit));
			ABI_RestoreStack(4);
			JMP(dispatcherNoCheck); // no point in special casing this

			//FP blocks test for FPU available, jump here if false
			fpException = AlignCode4(); 
			MOV(32, R(EAX), M(&PC));
			MOV(32, M(&NPC), R(EAX));
			OR(32, M(&PowerPC::ppcState.Exceptions), Imm32(EXCEPTION_FPU_UNAVAILABLE));
			CALL(reinterpret_cast<void *>(&PowerPC::CheckExceptions));
			MOV(32, R(EAX), M(&NPC));
			MOV(32, M(&PC), R(EAX));
			JMP(dispatcher);

		SetJumpTarget(bail);
		doTiming = GetCodePtr();

		CALL(reinterpret_cast<void *>(&CoreTiming::Advance));
		
		testExceptions = GetCodePtr();
		TEST(32, M(&PowerPC::ppcState.Exceptions), Imm32(0xFFFFFFFF));
		FixupBranch skipExceptions = J_CC(CC_Z);
			MOV(32, R(EAX), M(&PC));
			MOV(32, M(&NPC), R(EAX));
			CALL(reinterpret_cast<void *>(&PowerPC::CheckExceptions));
			MOV(32, R(EAX), M(&NPC));
			MOV(32, M(&PC), R(EAX));
		SetJumpTarget(skipExceptions);
		
		TEST(32, M((void*)&PowerPC::state), Imm32(0xFFFFFFFF));
		J_CC(CC_Z, outerLoop, true);
	
	POP(EDI);
	POP(ESI);
	POP(EBX);
	POP(EBP);
	RET(); 

	GenerateCommon();
}

#elif defined(_M_X64)

void Generate()
{
	enterCode = AlignCode16();

	ABI_PushAllCalleeSavedRegsAndAdjustStack();

	MOV(64, R(RBX), Imm64((u64)Memory::base));
	MOV(64, R(R15), Imm64((u64)GetCodePointers())); //It's below 2GB so 32 bits are good enough
	const u8 *outerLoop = GetCodePtr();

		CALL((void *)&CoreTiming::Advance);
		FixupBranch skipToRealDispatch = J(); //skip the sync and compare first time

		dispatcher = GetCodePtr();
			//The result of slice decrementation should be in flags if somebody jumped here
			//Jump on negative, not carry!!!
			FixupBranch bail = J_CC(CC_BE);
			SetJumpTarget(skipToRealDispatch);

			dispatcherNoCheck = GetCodePtr();
			MOV(32, R(EAX), M(&PowerPC::ppcState.pc));
			dispatcherPcInEAX = GetCodePtr();
			MOV(32, R(EAX), MComplex(RBX, RAX, SCALE_1, 0));
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
				JMPptr(MComplex(R15, RAX, 8, 0));
			SetJumpTarget(notfound);

			//Ok, no block, let's jit
			MOV(32, R(ABI_PARAM1), M(&PowerPC::ppcState.pc));
			CALL((void *)&Jit);
			JMP(dispatcherNoCheck); // no point in special casing this, not the "fast path"

			//FP blocks test for FPU available, jump here if false
			fpException = AlignCode4(); 
			MOV(32, R(EAX), M(&PC));
			MOV(32, M(&NPC), R(EAX));
			OR(32, M(&PowerPC::ppcState.Exceptions), Imm32(EXCEPTION_FPU_UNAVAILABLE));
			CALL((void *)&PowerPC::CheckExceptions);
			MOV(32, R(EAX), M(&NPC));
			MOV(32, M(&PC), R(EAX));
			JMP(dispatcherNoCheck);

		SetJumpTarget(bail);
		doTiming = GetCodePtr();
		TEST(32, M(&PC), Imm32(0xFFFFFFFF));
		FixupBranch mojs = J_CC(CC_NZ);
		INT3(); // if you hit this, PC == 0 - no good
		SetJumpTarget(mojs);
		CALL((void *)&CoreTiming::Advance);
		
		testExceptions = GetCodePtr();
		TEST(32, M(&PowerPC::ppcState.Exceptions), Imm32(0xFFFFFFFF));
		FixupBranch skipExceptions = J_CC(CC_Z);
			MOV(32, R(EAX), M(&PC));
			MOV(32, M(&NPC), R(EAX));
			CALL((void *)&PowerPC::CheckExceptions);
			MOV(32, R(EAX), M(&NPC));
			MOV(32, M(&PC), R(EAX));
		SetJumpTarget(skipExceptions);
		
		TEST(32, M((void*)&PowerPC::state), Imm32(0xFFFFFFFF));
		J_CC(CC_Z, outerLoop, true);
	
	//Landing pad for drec space
	ABI_PopAllCalleeSavedRegsAndAdjustStack();
	RET();

	GenerateCommon();
}
#endif

void GenFifoWrite(int size) 
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

static int temp32;
void GenFifoFloatWrite() 
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

void GenerateCommon()
{
	computeRc = AlignCode16();
	AND(32, M(&CR), Imm32(0x0FFFFFFF));
	CMP(32, R(EAX), Imm8(0));
	FixupBranch pLesser  = J_CC(CC_L);
	FixupBranch pGreater = J_CC(CC_G);
	OR(32, M(&CR), Imm32(0x20000000)); // _x86Reg == 0
	RET();
	SetJumpTarget(pGreater);
	OR(32, M(&CR), Imm32(0x40000000)); // _x86Reg > 0
	RET();
	SetJumpTarget(pLesser);
	OR(32, M(&CR), Imm32(0x80000000));	// _x86Reg < 0
	RET();

	fifoDirectWrite8 = AlignCode4();
	GenFifoWrite(8);
	fifoDirectWrite16 = AlignCode4();
	GenFifoWrite(16);
	fifoDirectWrite32 = AlignCode4();
	GenFifoWrite(32);
	fifoDirectWriteFloat = AlignCode4();
	GenFifoFloatWrite();

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

}  // namespace Asm

}  // namespace Jit64

