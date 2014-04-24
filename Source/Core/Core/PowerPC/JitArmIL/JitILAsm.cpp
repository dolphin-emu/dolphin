// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/ArmEmitter.h"
#include "Common/MemoryUtil.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/JitArmIL/JitIL.h"
#include "Core/PowerPC/JitArmIL/JitILAsm.h"
#include "Core/PowerPC/JitCommon/JitCache.h"

using namespace ArmGen;

JitArmILAsmRoutineManager armil_asm_routines;
void JitArmILAsmRoutineManager::Generate()
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
		printf("ILDispatcher is %p\n", dispatcher);

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

			MOVI2R(R14, (u32)jit->GetBlockCache()->iCache);

			LDR(R12, R14, R12); // R12 contains iCache[PC & JIT_ICACHE_MASK] here
			// R12 Confirmed this is the correct iCache Location loaded.
			TST(R12, 0x80); // Test  to see if it is a JIT block.

			SetCC(CC_EQ);
				// Success, it is our Jitblock.
				MOVI2R(R14, (u32)jit->GetBlockCache()->GetCodePointers());
				// LDR R14 right here to get CodePointers()[0] pointer.
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

		SetJumpTarget(bail);
		doTiming = GetCodePtr();
		// XXX: In JIT64, Advance() gets called /after/ the exception checking
		// once it jumps back to the start of outerLoop
		QuickCallFunction(R14, (void*)&CoreTiming::Advance);

		// Does exception checking
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

