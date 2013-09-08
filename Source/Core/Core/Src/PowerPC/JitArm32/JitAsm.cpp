// Copyright (C) 2003 Dolphin Project.

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


#include "../../HW/Memmap.h"

#include "../PowerPC.h"
#include "../../CoreTiming.h"
#include "MemoryUtil.h"

#include "Jit.h"
#include "../JitCommon/JitCache.h"

#include "../../HW/GPFifo.h"
#include "../../Core.h"
#include "JitAsm.h"
#include "ArmEmitter.h"

using namespace ArmGen;

//TODO - make an option
//#if _DEBUG
//	bool enableDebug = false; 
//#else
//	bool enableDebug = false; 
//#endif

JitArmAsmRoutineManager asm_routines;

void JitArmAsmRoutineManager::Generate()
{
	enterCode = GetCodePtr();
	PUSH(9, R4, R5, R6, R7, R8, R9, R10, R11, _LR);
	// Take care to 8-byte align stack for function calls.
	// We are misaligned here because of an odd number of args for PUSH.
	// It's not like x86 where you need to account for an extra 4 bytes
	// consumed by CALL.
	SUB(_SP, _SP, 4);

	MOVI2R(R0, (u32)&CoreTiming::downcount);
	MOVI2R(R9, (u32)&PowerPC::ppcState.spr[0]);

	FixupBranch skipToRealDispatcher = B();
	dispatcher = GetCodePtr();	
		printf("Dispatcher is %p\n", dispatcher);

		// Downcount Check	
		// The result of slice decrementation should be in flags if somebody jumped here
		// IMPORTANT - We jump on negative, not carry!!!
		FixupBranch bail = B_CC(CC_MI);

		SetJumpTarget(skipToRealDispatcher); 
		dispatcherNoCheck = GetCodePtr();

		// This block of code gets the address of the compiled block of code
		// It runs though to the compiling portion if it isn't found
			LDR(R12, R9, PPCSTATE_OFF(pc));// Load the current PC into R12

			Operand2 iCacheMask = Operand2(0xE, 2); // JIT_ICACHE_MASK
			BIC(R12, R12, iCacheMask); // R12 contains PC & JIT_ICACHE_MASK here.

			MOVI2R(R14, (u32)jit->GetBlockCache()->GetICache());

			LDR(R12, R14, R12); // R12 contains iCache[PC & JIT_ICACHE_MASK] here
			// R12 Confirmed this is the correct iCache Location loaded.
			TST(R12, 0xFC); // Test  to see if it is a JIT block.

			SetCC(CC_EQ);
				// Success, it is our Jitblock.
				MOVI2R(R14, (u32)jit->GetBlockCache()->GetCodePointers());
				// LDR R14 right here to get CodePointers()[0] pointer.
				REV(R12, R12); // Reversing this gives us our JITblock.
				LSL(R12, R12, 2); // Multiply by four because address locations are u32 in size 
				LDR(R14, R14, R12); // Load the block address in to R14 

				B(R14);
				// No need to jump anywhere after here, the block will go back to dispatcher start
			SetCC();

		// If we get to this point, that means that we don't have the block cached to execute
		// So call ArmJit to compile the block and then execute it.
		MOVI2R(R14, (u32)&Jit);	
		BL(R14);
			
		B(dispatcherNoCheck);

		// fpException()
		// Floating Point Exception Check, Jumped to if false
		fpException = GetCodePtr();
			LDR(R0, R9, PPCSTATE_OFF(Exceptions));
			ORR(R0, R0, EXCEPTION_FPU_UNAVAILABLE);
			STR(R0, R9, PPCSTATE_OFF(Exceptions));
				QuickCallFunction(R14, (void*)&PowerPC::CheckExceptions);
			LDR(R0, R9, PPCSTATE_OFF(npc));
			STR(R0, R9, PPCSTATE_OFF(pc));
		B(dispatcher);

		SetJumpTarget(bail);
		doTiming = GetCodePtr();			
		// XXX: In JIT64, Advance() gets called /after/ the exception checking
		// once it jumps back to the start of outerLoop 
		QuickCallFunction(R14, (void*)&CoreTiming::Advance);

		// Does exception checking 
		testExceptions = GetCodePtr();
			LDR(R0, R9, PPCSTATE_OFF(pc));
			STR(R0, R9, PPCSTATE_OFF(npc));
				QuickCallFunction(R14, (void*)&PowerPC::CheckExceptions);
			LDR(R0, R9, PPCSTATE_OFF(npc));
			STR(R0, R9, PPCSTATE_OFF(pc));
		// Check the state pointer to see if we are exiting
		// Gets checked on every exception check
			MOVI2R(R0, (u32)PowerPC::GetStatePtr());
			MVN(R1, 0);
			LDR(R0, R0);
			TST(R0, R1);
			FixupBranch Exit = B_CC(CC_NEQ);

	B(dispatcher);
	
	SetJumpTarget(Exit);

	ADD(_SP, _SP, 4);

	POP(9, R4, R5, R6, R7, R8, R9, R10, R11, _PC);  // Returns
	
	GenerateCommon();

	FlushIcache();
}

void JitArmAsmRoutineManager::GenerateCommon()
{
	const u8* loadPairedIllegal = GetCodePtr();
	BKPT(0x10);

	const u8* loadPairedFloatTwo = GetCodePtr();
	PUSH(2, R12, _LR);
	// R12, R14 is scratch
	// R10 is the address
	MOVI2R(R14, Memory::MEMVIEW32_MASK); 
	AND(R10, R10, R14);
	MOVI2R(R14, (u32)Memory::base);
	ADD(R10, R10, R14);

	NEONXEmitter nemit(this);
	nemit.VLD1(I_32, D0, R10);
	nemit.VREV32(I_8, D0, D0);

	POP(2, R12, _PC);
	const u8* loadPairedFloatOne = GetCodePtr();
	BKPT(0x12);
	const u8* loadPairedU8Two = GetCodePtr();
	BKPT(0x13);
	const u8* loadPairedU8One = GetCodePtr();
	BKPT(0x14);
	const u8* loadPairedS8Two = GetCodePtr();
	BKPT(0x15);
	const u8* loadPairedS8One = GetCodePtr();
	BKPT(0x16);
	const u8* loadPairedU16Two = GetCodePtr();
	BKPT(0x17);
	const u8* loadPairedU16One = GetCodePtr();
	BKPT(0x18);
	const u8* loadPairedS16Two = GetCodePtr();
	BKPT(0x19);
	const u8* loadPairedS16One = GetCodePtr();
	BKPT(0x20);

	pairedLoadQuantized = reinterpret_cast<const u8**>(const_cast<u8*>(AlignCode16()));
	ReserveCodeSpace(16 * sizeof(u8*));

	pairedLoadQuantized[0] = loadPairedFloatTwo;
	pairedLoadQuantized[1] = loadPairedIllegal;
	pairedLoadQuantized[2] = loadPairedIllegal;
	pairedLoadQuantized[3] = loadPairedIllegal;
	pairedLoadQuantized[4] = loadPairedU8Two;
	pairedLoadQuantized[5] = loadPairedU16Two;
	pairedLoadQuantized[6] = loadPairedS8Two;
	pairedLoadQuantized[7] = loadPairedS16Two;

	pairedLoadQuantized[8] = loadPairedFloatOne;
	pairedLoadQuantized[9] = loadPairedIllegal;
	pairedLoadQuantized[10] = loadPairedIllegal;
	pairedLoadQuantized[11] = loadPairedIllegal;
	pairedLoadQuantized[12] = loadPairedU8One;
	pairedLoadQuantized[13] = loadPairedU16One;
	pairedLoadQuantized[14] = loadPairedS8One;
	pairedLoadQuantized[15] = loadPairedS16One;

}
