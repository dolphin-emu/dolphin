// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/MemoryUtil.h"

#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/JitAsm.h"

using namespace Gen;

// PLAN: no more block numbers - crazy opcodes just contain offset within
// dynarec buffer
// At this offset - 4, there is an int specifying the block number.

void Jit64AsmRoutineManager::Generate()
{
	enterCode = AlignCode16();
	ABI_PushRegistersAndAdjustStack(ABI_ALL_CALLEE_SAVED, 8);

	// Two statically allocated registers.
	MOV(64, R(RMEM), Imm64((u64)Memory::base));
	MOV(64, R(RCODE_POINTERS), Imm64((u64)jit->GetBlockCache()->GetCodePointers())); //It's below 2GB so 32 bits are good enough
	MOV(64, R(RPPCSTATE), Imm64((u64)&PowerPC::ppcState + 0x80));

	const u8* outerLoop = GetCodePtr();
		ABI_CallFunction(reinterpret_cast<void *>(&CoreTiming::Advance));
		FixupBranch skipToRealDispatch = J(Core::g_CoreStartupParameter.bEnableDebugging); //skip the sync and compare first time

		dispatcher = GetCodePtr();
			// The result of slice decrementation should be in flags if somebody jumped here
			// IMPORTANT - We jump on negative, not carry!!!
			FixupBranch bail = J_CC(CC_BE, true);

			if (Core::g_CoreStartupParameter.bEnableDebugging)
			{
				TEST(32, M((void*)PowerPC::GetStatePtr()), Imm32(PowerPC::CPU_STEPPING));
				FixupBranch notStepping = J_CC(CC_Z);
				ABI_CallFunction(reinterpret_cast<void *>(&PowerPC::CheckBreakPoints));
				TEST(32, M((void*)PowerPC::GetStatePtr()), Imm32(0xFFFFFFFF));
				FixupBranch noBreakpoint = J_CC(CC_Z);
				ABI_PopRegistersAndAdjustStack(ABI_ALL_CALLEE_SAVED, 8);
				RET();
				SetJumpTarget(noBreakpoint);
				SetJumpTarget(notStepping);
			}

			SetJumpTarget(skipToRealDispatch);

			dispatcherNoCheck = GetCodePtr();
			MOV(32, R(RSCRATCH), PPCSTATE(pc));
			dispatcherPcInRSCRATCH = GetCodePtr();

			u32 mask = 0;
			FixupBranch no_mem;
			FixupBranch exit_mem;
			FixupBranch exit_vmem;
			if (Core::g_CoreStartupParameter.bWii)
				mask = JIT_ICACHE_EXRAM_BIT;
			if (Core::g_CoreStartupParameter.bMMU || Core::g_CoreStartupParameter.bTLBHack)
				mask |= JIT_ICACHE_VMEM_BIT;
			if (Core::g_CoreStartupParameter.bWii || Core::g_CoreStartupParameter.bMMU || Core::g_CoreStartupParameter.bTLBHack)
			{
				TEST(32, R(RSCRATCH), Imm32(mask));
				no_mem = J_CC(CC_NZ);
			}
			AND(32, R(RSCRATCH), Imm32(JIT_ICACHE_MASK));
			MOV(64, R(RSCRATCH2), Imm64((u64)jit->GetBlockCache()->iCache));
			MOV(32, R(RSCRATCH), MComplex(RSCRATCH2, RSCRATCH, SCALE_1, 0));

			if (Core::g_CoreStartupParameter.bWii || Core::g_CoreStartupParameter.bMMU || Core::g_CoreStartupParameter.bTLBHack)
			{
				exit_mem = J();
				SetJumpTarget(no_mem);
			}
			if (Core::g_CoreStartupParameter.bMMU || Core::g_CoreStartupParameter.bTLBHack)
			{
				TEST(32, R(RSCRATCH), Imm32(JIT_ICACHE_VMEM_BIT));
				FixupBranch no_vmem = J_CC(CC_Z);
				AND(32, R(RSCRATCH), Imm32(JIT_ICACHE_MASK));
				MOV(64, R(RSCRATCH2), Imm64((u64)jit->GetBlockCache()->iCacheVMEM));
				MOV(32, R(RSCRATCH), MComplex(RSCRATCH2, RSCRATCH, SCALE_1, 0));

				if (Core::g_CoreStartupParameter.bWii) exit_vmem = J();
				SetJumpTarget(no_vmem);
			}
			if (Core::g_CoreStartupParameter.bWii)
			{
				TEST(32, R(RSCRATCH), Imm32(JIT_ICACHE_EXRAM_BIT));
				FixupBranch no_exram = J_CC(CC_Z);
				AND(32, R(RSCRATCH), Imm32(JIT_ICACHEEX_MASK));
				MOV(64, R(RSCRATCH2), Imm64((u64)jit->GetBlockCache()->iCacheEx));
				MOV(32, R(RSCRATCH), MComplex(RSCRATCH2, RSCRATCH, SCALE_1, 0));

				SetJumpTarget(no_exram);
			}
			if (Core::g_CoreStartupParameter.bWii || Core::g_CoreStartupParameter.bMMU || Core::g_CoreStartupParameter.bTLBHack)
				SetJumpTarget(exit_mem);
			if (Core::g_CoreStartupParameter.bWii && (Core::g_CoreStartupParameter.bMMU || Core::g_CoreStartupParameter.bTLBHack))
				SetJumpTarget(exit_vmem);

			TEST(32, R(RSCRATCH), R(RSCRATCH));
			FixupBranch notfound = J_CC(CC_L);
				//grab from list and jump to it
				JMPptr(MComplex(RCODE_POINTERS, RSCRATCH, 8, 0));
			SetJumpTarget(notfound);

			//Ok, no block, let's jit
			MOV(32, R(ABI_PARAM1), PPCSTATE(pc));
			CALL((void *)&Jit);

			JMP(dispatcherNoCheck); // no point in special casing this

		SetJumpTarget(bail);
		doTiming = GetCodePtr();

		// Test external exceptions.
		TEST(32, PPCSTATE(Exceptions), Imm32(EXCEPTION_EXTERNAL_INT | EXCEPTION_PERFORMANCE_MONITOR | EXCEPTION_DECREMENTER));
		FixupBranch noExtException = J_CC(CC_Z);
		MOV(32, R(RSCRATCH), PPCSTATE(pc));
		MOV(32, PPCSTATE(npc), R(RSCRATCH));
		ABI_CallFunction(reinterpret_cast<void *>(&PowerPC::CheckExternalExceptions));
		SetJumpTarget(noExtException);

		TEST(32, M((void*)PowerPC::GetStatePtr()), Imm32(0xFFFFFFFF));
		J_CC(CC_Z, outerLoop);

	//Landing pad for drec space
	ABI_PopRegistersAndAdjustStack(ABI_ALL_CALLEE_SAVED, 8);
	RET();

	GenerateCommon();
}

void Jit64AsmRoutineManager::GenerateCommon()
{
	fifoDirectWrite8 = AlignCode4();
	GenFifoWrite(8);
	fifoDirectWrite16 = AlignCode4();
	GenFifoWrite(16);
	fifoDirectWrite32 = AlignCode4();
	GenFifoWrite(32);
	fifoDirectWriteFloat = AlignCode4();
	GenFifoFloatWrite();
	frsqrte = AlignCode4();
	GenFrsqrte();
	fres = AlignCode4();
	GenFres();

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
	CALL((void *)&Memory::Write_U8);*/
}
