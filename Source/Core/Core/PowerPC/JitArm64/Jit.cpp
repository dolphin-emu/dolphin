// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Arm64Emitter.h"
#include "Common/Common.h"

#include "Core/PatchEngine.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"
#include "Core/PowerPC/JitArm64/JitArm64_Tables.h"

using namespace Arm64Gen;

static int CODE_SIZE = 1024*1024*32;

void JitArm64::Init()
{
	AllocCodeSpace(CODE_SIZE);
	jo.enableBlocklink = true;
	gpr.Init(this);
	fpr.Init(this);

	blocks.Init();
	asm_routines.Init();

	code_block.m_stats = &js.st;
	code_block.m_gpa = &js.gpa;
	code_block.m_fpa = &js.fpa;
}

void JitArm64::ClearCache()
{
	ClearCodeSpace();
	blocks.Clear();
}

void JitArm64::Shutdown()
{
	FreeCodeSpace();
	blocks.Shutdown();
	asm_routines.Shutdown();
}

void JitArm64::unknown_instruction(UGeckoInstruction inst)
{
	WARN_LOG(DYNA_REC, "unknown_instruction %08x - Fix me ;)", inst.hex);
}

void JitArm64::FallBackToInterpreter(UGeckoInstruction inst)
{
	gpr.Flush(FlushMode::FLUSH_ALL);
	fpr.Flush(FlushMode::FLUSH_ALL);
	Interpreter::_interpreterInstruction instr = GetInterpreterOp(inst);
	MOVI2R(W0, inst.hex);
	MOVI2R(X30, (u64)instr);
	BLR(X30);
}

void JitArm64::HLEFunction(UGeckoInstruction inst)
{
	WARN_LOG(DYNA_REC, "HLEFunction %08x - Fix me ;)", inst.hex);
	exit(0);
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
	//If nobody has taken care of this yet (this can be removed when all branches are done)
	JitBlock *b = js.curBlock;
	JitBlock::LinkData linkData;
	linkData.exitAddress = destination;
	linkData.exitPtrs = GetWritableCodePtr();
	linkData.linkStatus = false;

	DoDownCount();

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
		MOVI2R(EncodeRegTo64(dest), (u64)&PowerPC::CheckExceptions);
		BLR(EncodeRegTo64(dest));
	LDR(INDEX_UNSIGNED, dest, X29, PPCSTATE_OFF(npc));
	STR(INDEX_UNSIGNED, dest, X29, PPCSTATE_OFF(pc));

	MOVI2R(EncodeRegTo64(dest), (u64)asm_routines.dispatcher);
	BR(EncodeRegTo64(dest));
}

void JitArm64::WriteExitDestInR(ARM64Reg Reg)
{
	STR(INDEX_UNSIGNED, Reg, X29, PPCSTATE_OFF(pc));
	gpr.Unlock(Reg);
	DoDownCount();
	MOVI2R(EncodeRegTo64(Reg), (u64)asm_routines.dispatcher);
	BR(EncodeRegTo64(Reg));
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

void JitArm64::Jit(u32 em_address)
{
	if (GetSpaceLeft() < 0x10000 || blocks.IsFull() || SConfig::GetInstance().m_LocalCoreStartupParameter.bJITNoBlockCache)
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

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableDebugging)
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
	js.blockStart = em_address;
	js.fifoBytesThisBlock = 0;
	js.downcountAmount = 0;
	js.skipnext = false;
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

	const u8 *normalEntry = GetCodePtr();
	b->normalEntry = normalEntry;

	gpr.Start(js.gpa);
	fpr.Start(js.fpa);

	if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableDebugging)
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
			js.next_inst = 0;
		}
		else
		{
			// help peephole optimizations
			js.next_inst = ops[i + 1].inst;
			js.next_compilerPC = ops[i + 1].address;
		}
		if (!ops[i].skip)
		{
			if (js.memcheck && (opinfo->flags & FL_USE_FPU))
			{
				// Don't do this yet
				BRK(0x7777);
			}

			JitArm64Tables::CompileInstruction(ops[i]);

			if (js.memcheck && (opinfo->flags & FL_LOADSTORE))
			{
				// Don't do this yet
				BRK(0x666);
			}
		}
	}

	if (code_block.m_memory_exception)
		BRK(0x500);

	if (code_block.m_broken)
	{
		printf("Broken Block going to 0x%08x\n", nextPC);
		WriteExit(nextPC);
	}

	b->codeSize = (u32)(GetCodePtr() - normalEntry);
	b->originalSize = code_block.m_num_instructions;
	FlushIcache();
	return start;
}
