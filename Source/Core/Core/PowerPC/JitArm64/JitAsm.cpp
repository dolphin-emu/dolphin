// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Arm64Emitter.h"

#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/JitAsm.h"
#include "Core/PowerPC/JitCommon/JitCache.h"

using namespace Arm64Gen;

void JitArm64AsmRoutineManager::Generate()
{
	// This value is all of the callee saved registers that we are required to save.
	// According to the AACPS64 we need to save R19 ~ R30.
	const u32 ALL_CALLEE_SAVED = 0x7FF80000;
	BitSet32 regs_to_save(ALL_CALLEE_SAVED);
	enterCode = GetCodePtr();

	ABI_PushRegisters(regs_to_save);

	MOVI2R(X29, (u64)&PowerPC::ppcState);
	FixupBranch to_dispatcher = B();

	// If we align the dispatcher to a page then we can load its location with one ADRP instruction
	AlignCodePage();
	dispatcher = GetCodePtr();
	WARN_LOG(DYNA_REC, "Dispatcher is %p\n", dispatcher);

	SetJumpTarget(to_dispatcher);

	// Downcount Check
	// The result of slice decrementation should be in flags if somebody jumped here
	// IMPORTANT - We jump on negative, not carry!!!
	FixupBranch bail = B(CC_MI);

		dispatcherNoCheck = GetCodePtr();

		// This block of code gets the address of the compiled block of code
		// It runs though to the compiling portion if it isn't found
		LDR(INDEX_UNSIGNED, W28, X29, PPCSTATE_OFF(pc)); // Load the current PC into W28
		BFM(W28, WSP, 3, 2); // Wipe the top 3 bits. Same as PC & JIT_ICACHE_MASK

		MOVI2R(X27, (u64)jit->GetBlockCache()->iCache.data());
		LDR(W27, X27, X28);

		FixupBranch JitBlock = TBNZ(W27, 7); // Test the 7th bit
			// Success, it is our Jitblock.
			MOVI2R(X30, (u64)jit->GetBlockCache()->GetCodePointers());
			UBFM(X27, X27, 61, 60); // Same as X27 << 3
			LDR(X30, X30, X27); // Load the block address in to R14
			BR(X30);
			// No need to jump anywhere after here, the block will go back to dispatcher start

		SetJumpTarget(JitBlock);

		MOVI2R(X30, (u64)&Jit);
		BLR(X30);

		B(dispatcherNoCheck);

	SetJumpTarget(bail);
	doTiming = GetCodePtr();
		MOVI2R(X30, (u64)&CoreTiming::Advance);
		BLR(X30);

		// Does exception checking
		LDR(INDEX_UNSIGNED, W0, X29, PPCSTATE_OFF(pc));
		STR(INDEX_UNSIGNED, W0, X29, PPCSTATE_OFF(npc));
			MOVI2R(X30, (u64)&PowerPC::CheckExceptions);
			BLR(X30);
		LDR(INDEX_UNSIGNED, W0, X29, PPCSTATE_OFF(npc));
		STR(INDEX_UNSIGNED, W0, X29, PPCSTATE_OFF(pc));

		// Check the state pointer to see if we are exiting
		// Gets checked on every exception check
		MOVI2R(X0, (u64)PowerPC::GetStatePtr());
		LDR(INDEX_UNSIGNED, W0, X0, 0);

		CMP(W0, 0);
		FixupBranch Exit = B(CC_NEQ);

	B(dispatcher);

	SetJumpTarget(Exit);

	ABI_PopRegisters(regs_to_save);
	RET(X30);

	FlushIcache();
}

void JitArm64AsmRoutineManager::GenerateCommon()
{
}
