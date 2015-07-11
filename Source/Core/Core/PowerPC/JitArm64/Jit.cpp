// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Arm64Emitter.h"
#include "Common/Common.h"
#include "Common/PerformanceCounter.h"

#include "Core/PatchEngine.h"
#include "Core/PowerPC/Profiler.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"
#include "Core/PowerPC/JitArm64/JitArm64_Tables.h"

using namespace Arm64Gen;

void JitArm64::Init()
{
	AllocCodeSpace(CODE_SIZE);
	farcode.Init(SConfig::GetInstance().bMMU ? FARCODE_SIZE_MMU : FARCODE_SIZE);
	jo.enableBlocklink = true;
	jo.optimizeGatherPipe = true;
	UpdateMemoryOptions();
	gpr.Init(this);
	fpr.Init(this);

	blocks.Init();
	asm_routines.Init();

	code_block.m_stats = &js.st;
	code_block.m_gpa = &js.gpa;
	code_block.m_fpa = &js.fpa;
	InitBackpatch();
	analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE);
}

void JitArm64::ClearCache()
{
	ClearCodeSpace();
	farcode.ClearCodeSpace();
	blocks.Clear();
	UpdateMemoryOptions();
}

void JitArm64::Shutdown()
{
	FreeCodeSpace();
	farcode.Shutdown();
	blocks.Shutdown();
	asm_routines.Shutdown();
}

void JitArm64::FallBackToInterpreter(UGeckoInstruction inst)
{
	gpr.Flush(FlushMode::FLUSH_INTERPRETER, js.op);
	fpr.Flush(FlushMode::FLUSH_INTERPRETER, js.op);

	if (js.op->opinfo->flags & FL_ENDBLOCK)
	{
		// also flush the program counter
		ARM64Reg WA = gpr.GetReg();
		MOVI2R(WA, js.compilerPC);
		STR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(pc));
		ADD(WA, WA, 4);
		STR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(npc));
		gpr.Unlock(WA);
	}

	Interpreter::_interpreterInstruction instr = GetInterpreterOp(inst);
	MOVI2R(W0, inst.hex);
	MOVI2R(X30, (u64)instr);
	BLR(X30);

	if (js.op->opinfo->flags & FL_ENDBLOCK)
	{
		if (js.isLastInstruction)
		{
			ARM64Reg WA = gpr.GetReg();
			LDR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(npc));
			WriteExceptionExit(WA);
		}
		else
		{
			// only exit if ppcstate.npc was changed
			ARM64Reg WA = gpr.GetReg();
			LDR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(npc));
			ARM64Reg WB = gpr.GetReg();
			MOVI2R(WB, js.compilerPC + 4);
			CMP(WB, WA);
			gpr.Unlock(WB);
			FixupBranch c = B(CC_EQ);
			WriteExceptionExit(WA);
			SetJumpTarget(c);
		}
	}
}

void JitArm64::HLEFunction(UGeckoInstruction inst)
{
	gpr.Flush(FlushMode::FLUSH_ALL);
	fpr.Flush(FlushMode::FLUSH_ALL);

	MOVI2R(W0, js.compilerPC);
	MOVI2R(W1, inst.hex);
	MOVI2R(X30, (u64)&HLE::Execute);
	BLR(X30);

	ARM64Reg WA = gpr.GetReg();
	LDR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(npc));
	WriteExitDestInR(WA);
}

void JitArm64::DoNothing(UGeckoInstruction inst)
{
	// Yup, just don't do anything.
}

void JitArm64::Break(UGeckoInstruction inst)
{
	WARN_LOG(DYNA_REC, "Breaking! %08x - Fix me ;)", inst.hex);
	exit(0);
}

void JitArm64::DoDownCount()
{
	ARM64Reg WA = gpr.GetReg();
	LDR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(downcount));
	if (js.downcountAmount < 4096) // We can enlarge this if we used rotations
	{
		SUBS(WA, WA, js.downcountAmount);
		STR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(downcount));
	}
	else
	{
		ARM64Reg WB = gpr.GetReg();
		MOVI2R(WB, js.downcountAmount);
		SUBS(WA, WA, WB);
		STR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(downcount));
		gpr.Unlock(WB);
	}
	gpr.Unlock(WA);
}

// Exits
void JitArm64::WriteExit(u32 destination)
{
	DoDownCount();

	if (Profiler::g_ProfileBlocks)
		EndTimeProfile(js.curBlock);

	//If nobody has taken care of this yet (this can be removed when all branches are done)
	JitBlock *b = js.curBlock;
	JitBlock::LinkData linkData;
	linkData.exitAddress = destination;
	linkData.exitPtrs = GetWritableCodePtr();
	linkData.linkStatus = false;

	// Link opportunity!
	int block;
	if (jo.enableBlocklink && (block = blocks.GetBlockNumberFromStartAddress(destination)) >= 0)
	{
		// It exists! Joy of joy!
		B(blocks.GetBlock(block)->checkedEntry);
		linkData.linkStatus = true;
	}
	else
	{
		ARM64Reg WA = gpr.GetReg();
		ARM64Reg XA = EncodeRegTo64(WA);
		MOVI2R(WA, destination);
		STR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(pc));
		MOVI2R(XA, (u64)asm_routines.dispatcher);
		BR(XA);
		gpr.Unlock(WA);
	}

	b->linkData.push_back(linkData);
}
void JitArm64::WriteExceptionExit(ARM64Reg dest)
{
	STR(INDEX_UNSIGNED, dest, X29, PPCSTATE_OFF(pc));
	STR(INDEX_UNSIGNED, dest, X29, PPCSTATE_OFF(npc));
	gpr.Unlock(dest);
		DoDownCount();

		if (Profiler::g_ProfileBlocks)
			EndTimeProfile(js.curBlock);

		MOVI2R(EncodeRegTo64(dest), (u64)&PowerPC::CheckExceptions);
		BLR(EncodeRegTo64(dest));
	LDR(INDEX_UNSIGNED, dest, X29, PPCSTATE_OFF(npc));
	STR(INDEX_UNSIGNED, dest, X29, PPCSTATE_OFF(pc));

	MOVI2R(EncodeRegTo64(dest), (u64)asm_routines.dispatcher);
	BR(EncodeRegTo64(dest));
}

void JitArm64::WriteExceptionExit()
{
	DoDownCount();

	if (Profiler::g_ProfileBlocks)
		EndTimeProfile(js.curBlock);

	ARM64Reg WA = gpr.GetReg();
	ARM64Reg XA = EncodeRegTo64(WA);
	LDR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(pc));
	STR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(npc));
		MOVI2R(XA, (u64)&PowerPC::CheckExceptions);
		BLR(XA);
	LDR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(npc));
	STR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(pc));

	MOVI2R(XA, (u64)asm_routines.dispatcher);
	BR(XA);

	gpr.Unlock(WA);
}

void JitArm64::WriteExitDestInR(ARM64Reg Reg)
{
	STR(INDEX_UNSIGNED, Reg, X29, PPCSTATE_OFF(pc));
	gpr.Unlock(Reg);
	DoDownCount();

	if (Profiler::g_ProfileBlocks)
		EndTimeProfile(js.curBlock);

	MOVI2R(EncodeRegTo64(Reg), (u64)asm_routines.dispatcher);
	BR(EncodeRegTo64(Reg));
}

void JitArm64::DumpCode(const u8* start, const u8* end)
{
	std::string output = "";
	for (u8* code = (u8*)start; code < end; code += 4)
		output += StringFromFormat("%08x", Common::swap32(*(u32*)code));
	WARN_LOG(DYNA_REC, "Code dump from %p to %p:\n%s", start, end, output.c_str());
}

void JitArm64::BeginTimeProfile(JitBlock* b)
{
	b->ticCounter = 0;
	b->ticStart = 0;
	b->ticStop = 0;

	MOVI2R(X1, (u64)QueryPerformanceCounter);
	MOVI2R(X0, (u64)&b->ticStart);
	BLR(X1);
}

void JitArm64::EndTimeProfile(JitBlock* b)
{
	MOVI2R(X1, (u64)QueryPerformanceCounter);
	MOVI2R(X0, (u64)&b->ticStop);
	BLR(X1);

	MOVI2R(X0, (u64)&b->ticStart);
	LDR(INDEX_UNSIGNED, X1, X0, 0); // Start
	LDR(INDEX_UNSIGNED, X2, X0, 8); // Stop
	LDR(INDEX_UNSIGNED, X3, X0, 16); // Counter
	SUB(X2, X2, X1);
	ADD(X3, X3, X2);
	STR(INDEX_UNSIGNED, X3, X0, 16);
}

void JitArm64::Run()
{
	CompiledCode pExecAddr = (CompiledCode)asm_routines.enterCode;
	pExecAddr();
}

void JitArm64::SingleStep()
{
	CompiledCode pExecAddr = (CompiledCode)asm_routines.enterCode;
	pExecAddr();
}

void JitArm64::Jit(u32)
{
	if (GetSpaceLeft() < 0x10000 || farcode.GetSpaceLeft() < 0x10000 || blocks.IsFull() ||
		SConfig::GetInstance().bJITNoBlockCache)
	{
		ClearCache();
	}
	int block_num = blocks.AllocateBlock(PowerPC::ppcState.pc);
	JitBlock *b = blocks.GetBlock(block_num);
	const u8* BlockPtr = DoJit(PowerPC::ppcState.pc, &code_buffer, b);
	blocks.FinalizeBlock(block_num, jo.enableBlocklink, BlockPtr);
}

const u8* JitArm64::DoJit(u32 em_address, PPCAnalyst::CodeBuffer *code_buf, JitBlock *b)
{
	int blockSize = code_buf->GetSize();

	if (SConfig::GetInstance().bEnableDebugging)
	{
		// Comment out the following to disable breakpoints (speed-up)
		blockSize = 1;
	}

	if (em_address == 0)
	{
		Core::SetState(Core::CORE_PAUSE);
		WARN_LOG(DYNA_REC, "ERROR: Compiling at 0. LR=%08x CTR=%08x", LR, CTR);
	}

	js.isLastInstruction = false;
	js.firstFPInstructionFound = false;
	js.blockStart = em_address;
	js.fifoBytesThisBlock = 0;
	js.downcountAmount = 0;
	js.skipInstructions = 0;
	js.curBlock = b;

	u32 nextPC = em_address;
	// Analyze the block, collect all instructions it is made of (including inlining,
	// if that is enabled), reorder instructions for optimal performance, and join joinable instructions.
	nextPC = analyzer.Analyze(em_address, &code_block, code_buf, blockSize);

	PPCAnalyst::CodeOp *ops = code_buf->codebuffer;

	const u8 *start = GetCodePtr();
	b->checkedEntry = start;
	b->runCount = 0;

	// Downcount flag check, Only valid for linked blocks
	{
		FixupBranch bail = B(CC_PL);
		ARM64Reg WA = gpr.GetReg();
		ARM64Reg XA = EncodeRegTo64(WA);
		MOVI2R(WA, js.blockStart);
		STR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(pc));
		MOVI2R(XA, (u64)asm_routines.doTiming);
		BR(XA);
		gpr.Unlock(WA);
		SetJumpTarget(bail);
	}

	// Conditionally add profiling code.
	if (Profiler::g_ProfileBlocks)
	{
		ARM64Reg WA = gpr.GetReg();
		ARM64Reg WB = gpr.GetReg();
		ARM64Reg XA = EncodeRegTo64(WA);
		ARM64Reg XB = EncodeRegTo64(WB);
		MOVI2R(XA, (u64)&b->runCount);
		LDR(INDEX_UNSIGNED, XB, XA, 0);
		ADD(XB, XB, 1);
		STR(INDEX_UNSIGNED, XB, XA, 0);
		gpr.Unlock(WA, WB);
		// get start tic
		BeginTimeProfile(b);
	}
	const u8 *normalEntry = GetCodePtr();
	b->normalEntry = normalEntry;

	gpr.Start(js.gpa);
	fpr.Start(js.fpa);

	if (!SConfig::GetInstance().bEnableDebugging)
		js.downcountAmount += PatchEngine::GetSpeedhackCycles(em_address);

	// Translate instructions
	for (u32 i = 0; i < code_block.m_num_instructions; i++)
	{
		js.compilerPC = ops[i].address;
		js.op = &ops[i];
		js.instructionNumber = i;
		const GekkoOPInfo *opinfo = ops[i].opinfo;
		js.downcountAmount += opinfo->numCycles;

		if (i == (code_block.m_num_instructions - 1))
		{
			// WARNING - cmp->branch merging will screw this up.
			js.isLastInstruction = true;
		}

		if (jo.optimizeGatherPipe && js.fifoBytesThisBlock >= 32)
		{
			js.fifoBytesThisBlock -= 32;

			gpr.Lock(W30);
			BitSet32 regs_in_use = gpr.GetCallerSavedUsed();
			regs_in_use[W30] = 0;

			ABI_PushRegisters(regs_in_use);
			MOVI2R(X30, (u64)&GPFifo::FastCheckGatherPipe);
			BLR(X30);
			ABI_PopRegisters(regs_in_use);
			gpr.Unlock(W30);
		}

		if (!ops[i].skip)
		{
			if ((opinfo->flags & FL_USE_FPU) && !js.firstFPInstructionFound)
			{
				//This instruction uses FPU - needs to add FP exception bailout
				ARM64Reg WA = gpr.GetReg();
				LDR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(msr));
				FixupBranch b1 = TBNZ(WA, 13); // Test FP enabled bit

				gpr.Flush(FLUSH_MAINTAIN_STATE);
				fpr.Flush(FLUSH_MAINTAIN_STATE);

				LDR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(Exceptions));
				ORR(WA, WA, 26, 0); // EXCEPTION_FPU_UNAVAILABLE
				STR(INDEX_UNSIGNED, WA, X29, PPCSTATE_OFF(Exceptions));

				MOVI2R(WA, js.compilerPC);
				WriteExceptionExit(WA);

				SetJumpTarget(b1);

				js.firstFPInstructionFound = true;
			}

			if (jo.memcheck && (opinfo->flags & FL_USE_FPU))
			{
				// Don't do this yet
				BRK(0x7777);
			}

			JitArm64Tables::CompileInstruction(ops[i]);

			// If we have a register that will never be used again, flush it.
			for (int j : ~ops[i].gprInUse)
				gpr.StoreRegister(j);
			for (int j : ~ops[i].fprInUse)
				fpr.StoreRegister(j);

			if (jo.memcheck && (opinfo->flags & FL_LOADSTORE))
			{
				// Don't do this yet
				BRK(0x666);
			}
		}

		i += js.skipInstructions;
		js.skipInstructions = 0;
	}

	if (code_block.m_memory_exception)
		BRK(0x500);

	if (code_block.m_broken)
	{
		printf("Broken Block going to 0x%08x\n", nextPC);
		WriteExit(nextPC);
	}

	b->codeSize = (u32)(GetCodePtr() - start);
	b->originalSize = code_block.m_num_instructions;

	FlushIcache();
	farcode.FlushIcache();
	return start;
}
