// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/JitRegister.h"
#include "Common/MemoryUtil.h"

#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/JitAsm.h"

using namespace Gen;

// Not PowerPC state.  Can't put in 'this' because it's out of range...
static void* s_saved_rsp;

// PLAN: no more block numbers - crazy opcodes just contain offset within
// dynarec buffer
// At this offset - 4, there is an int specifying the block number.

static void DispatchSlow()
{
	jit->GetBlockCache()->MoveBlockIntoFastCache(PC);
}

void Jit64AsmRoutineManager::Generate()
{
	enterCode = AlignCode16();
	// We need to own the beginning of RSP, so we do an extra stack adjustment
	// for the shadow region before calls in this function.  This call will
	// waste a bit of space for a second shadow, but whatever.
	ABI_PushRegistersAndAdjustStack(ABI_ALL_CALLEE_SAVED, 8, /*frame*/ 16);
	if (m_stack_top)
	{
		// Pivot the stack to our custom one.
		MOV(64, R(RSCRATCH), R(RSP));
		MOV(64, R(RSP), Imm64((u64)m_stack_top - 0x20));
		MOV(64, MDisp(RSP, 0x18), R(RSCRATCH));
	}
	else
	{
		MOV(64, M(&s_saved_rsp), R(RSP));
	}
	// something that can't pass the BLR test
	MOV(64, MDisp(RSP, 8), Imm32((u32)-1));

	// Two statically allocated registers.
	//MOV(64, R(RMEM), Imm64((u64)Memory::physical_base));
	MOV(64, R(RPPCSTATE), Imm64((u64)&PowerPC::ppcState + 0x80));

	const u8* outerLoop = GetCodePtr();
		ABI_PushRegistersAndAdjustStack({}, 0);
		ABI_CallFunction(reinterpret_cast<void *>(&CoreTiming::Advance));
		ABI_PopRegistersAndAdjustStack({}, 0);
		FixupBranch skipToRealDispatch = J(SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableDebugging); //skip the sync and compare first time
		dispatcherMispredictedBLR = GetCodePtr();
		AND(32, PPCSTATE(pc), Imm32(0xFFFFFFFC));

		#if 0 // debug mispredicts
		MOV(32, R(ABI_PARAM1), MDisp(RSP, 8)); // guessed_pc
		ABI_PushRegistersAndAdjustStack(1 << RSCRATCH2, 0);
		CALL(reinterpret_cast<void *>(&ReportMispredict));
		ABI_PopRegistersAndAdjustStack(1 << RSCRATCH2, 0);
		#endif

		ResetStack();

		SUB(32, PPCSTATE(downcount), R(RSCRATCH2));

		dispatcher = GetCodePtr();
			// The result of slice decrementation should be in flags if somebody jumped here
			// IMPORTANT - We jump on negative, not carry!!!
			FixupBranch bail = J_CC(CC_BE, true);

			FixupBranch dbg_exit;

			if (SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableDebugging)
			{
				TEST(32, M(PowerPC::GetStatePtr()), Imm32(PowerPC::CPU_STEPPING));
				FixupBranch notStepping = J_CC(CC_Z);
				ABI_PushRegistersAndAdjustStack({}, 0);
				ABI_CallFunction(reinterpret_cast<void *>(&PowerPC::CheckBreakPoints));
				ABI_PopRegistersAndAdjustStack({}, 0);
				TEST(32, M(PowerPC::GetStatePtr()), Imm32(0xFFFFFFFF));
				dbg_exit = J_CC(CC_NZ, true);
				SetJumpTarget(notStepping);
			}

			SetJumpTarget(skipToRealDispatch);

			dispatcherNoCheck = GetCodePtr();

			// Switch to the correct memory base, in case MSR.DR has changed.
			// TODO: Is there a more efficient place to put this?  We don't
			// need to do this for indirect jumps, just exceptions etc.
			TEST(32, PPCSTATE(msr), Imm32(1 << (31 - 27)));
			FixupBranch physmem = J_CC(CC_NZ);
			MOV(64, R(RMEM), Imm64((u64)Memory::physical_base));
			FixupBranch membaseend = J();
			SetJumpTarget(physmem);
			MOV(64, R(RMEM), Imm64((u64)Memory::logical_base));
			SetJumpTarget(membaseend);

			// Translate PC.
			MOV(32, R(RSCRATCH), PPCSTATE(pc));
			u64 icache = (u64)jit->GetBlockCache()->iCache.data();
			AND(32, R(RSCRATCH), Imm32(JitBaseBlockCache::iCache_Mask << 2));
			if (icache <= INT_MAX)
			{
				MOV(32, R(RSCRATCH), MDisp(RSCRATCH, (s32)icache));
			}
			else
			{
				MOV(64, R(RSCRATCH2), Imm64(icache));
				MOV(32, R(RSCRATCH), MRegSum(RSCRATCH2, RSCRATCH));
			}

			u64 blocks = (u64)jit->GetBlockCache()->GetBlocks();
			IMUL(32, RSCRATCH, R(RSCRATCH), Imm32(sizeof(JitBlock)));
			if (blocks <= INT_MAX)
			{
				ADD(32, R(RSCRATCH), Imm32((s32)blocks));
			}
			else
			{
				MOV(64, R(RSCRATCH2), Imm64(blocks));
				ADD(32, R(RSCRATCH), R(RSCRATCH2));
			}
			MOV(32, R(RSCRATCH2), PPCSTATE(msr));
			AND(32, R(RSCRATCH2), Imm32(0x30));
			SHL(64, R(RSCRATCH2), Imm8(32));
			MOV(32, R(RSCRATCH_EXTRA), PPCSTATE(pc));
			OR(64, R(RSCRATCH2), R(RSCRATCH_EXTRA));
			CMP(64, R(RSCRATCH2), MDisp(RSCRATCH, (s32)offsetof(JitBlock, effectiveAddress)));
			FixupBranch notfound = J_CC(CC_NE);
			JMPptr(MDisp(RSCRATCH, (s32)offsetof(JitBlock, normalEntry)));
			SetJumpTarget(notfound);

			// Slow path.
			ABI_PushRegistersAndAdjustStack({}, 0);
			ABI_CallFunction((void*)DispatchSlow);
			ABI_PopRegistersAndAdjustStack({}, 0);

			// Jit might have cleared the code cache
			ResetStack();

			JMP(dispatcherNoCheck, true); // no point in special casing this

		SetJumpTarget(bail);
		doTiming = GetCodePtr();

		// Test external exceptions.
		TEST(32, PPCSTATE(Exceptions), Imm32(EXCEPTION_EXTERNAL_INT | EXCEPTION_PERFORMANCE_MONITOR | EXCEPTION_DECREMENTER));
		FixupBranch noExtException = J_CC(CC_Z);
		MOV(32, R(RSCRATCH), PPCSTATE(pc));
		MOV(32, PPCSTATE(npc), R(RSCRATCH));
		ABI_PushRegistersAndAdjustStack({}, 0);
		ABI_CallFunction(reinterpret_cast<void *>(&PowerPC::CheckExternalExceptions));
		ABI_PopRegistersAndAdjustStack({}, 0);
		SetJumpTarget(noExtException);

		TEST(32, M(PowerPC::GetStatePtr()), Imm32(0xFFFFFFFF));
		J_CC(CC_Z, outerLoop);

	//Landing pad for drec space
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableDebugging)
		SetJumpTarget(dbg_exit);
	ResetStack();
	if (m_stack_top)
	{
		ADD(64, R(RSP), Imm8(0x18));
		POP(RSP);
	}
	ABI_PopRegistersAndAdjustStack(ABI_ALL_CALLEE_SAVED, 8, 16);
	RET();

	JitRegister::Register(enterCode, GetCodePtr(), "JIT_Loop");

	GenerateCommon();
}

void Jit64AsmRoutineManager::ResetStack()
{
	if (m_stack_top)
		MOV(64, R(RSP), Imm64((u64)m_stack_top - 0x20));
	else
		MOV(64, R(RSP), M(&s_saved_rsp));
}


void Jit64AsmRoutineManager::GenerateCommon()
{
	fifoDirectWrite8 = AlignCode4();
	GenFifoWrite(8);
	fifoDirectWrite16 = AlignCode4();
	GenFifoWrite(16);
	fifoDirectWrite32 = AlignCode4();
	GenFifoWrite(32);
	fifoDirectWrite64 = AlignCode4();
	GenFifoWrite(64);
	frsqrte = AlignCode4();
	GenFrsqrte();
	fres = AlignCode4();
	GenFres();
	mfcr = AlignCode4();
	GenMfcr();

	GenQuantizedLoads();
	GenQuantizedStores();
	GenQuantizedSingleStores();

	//CMPSD(R(XMM0), M(&zero),
	// TODO

	// Fast write routines - special case the most common hardware write
	// TODO: use this.
	// Even in x86, the param values will be in the right registers.
	/*
	const u8 *fastMemWrite8 = AlignCode16();
	CMP(32, R(ABI_PARAM2), Imm32(0xCC008000));
	FixupBranch skip_fast_write = J_CC(CC_NE, false);
	MOV(32, RSCRATCH, M(&m_gatherPipeCount));
	MOV(8, MDisp(RSCRATCH, (u32)&m_gatherPipe), ABI_PARAM1);
	ADD(32, 1, M(&m_gatherPipeCount));
	RET();
	SetJumpTarget(skip_fast_write);
	CALL((void *)&PowerPC::Write_U8);*/
}
