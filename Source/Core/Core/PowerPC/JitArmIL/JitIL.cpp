// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <map>

#include "Common/ArmEmitter.h"
#include "Common/Common.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/PatchEngine.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/Profiler.h"
#include "Core/PowerPC/JitArmIL/JitIL.h"
#include "Core/PowerPC/JitArmIL/JitIL_Tables.h"

using namespace ArmGen;
using namespace PowerPC;

static int CODE_SIZE = 1024*1024*32;
namespace CPUCompare
{
	extern u32 m_BlockStart;
}
void JitArmIL::Init()
{
	AllocCodeSpace(CODE_SIZE);
	blocks.Init();
	asm_routines.Init();

	code_block.m_stats = &js.st;
	code_block.m_gpa = &js.gpa;
	code_block.m_fpa = &js.fpa;
}

void JitArmIL::ClearCache()
{
	ClearCodeSpace();
	blocks.Clear();
}

void JitArmIL::Shutdown()
{
	FreeCodeSpace();
	blocks.Shutdown();
	asm_routines.Shutdown();
}
void JitArmIL::unknown_instruction(UGeckoInstruction inst)
{
	// CCPU::Break();
	PanicAlert("unknown_instruction %08x - Fix me ;)", inst.hex);
}

void JitArmIL::FallBackToInterpreter(UGeckoInstruction _inst)
{
	ibuild.EmitFallBackToInterpreter(
		ibuild.EmitIntConst(_inst.hex),
		ibuild.EmitIntConst(js.compilerPC));
}

void JitArmIL::HLEFunction(UGeckoInstruction _inst)
{
	// XXX
}

void JitArmIL::DoNothing(UGeckoInstruction _inst)
{
	// Yup, just don't do anything.
}
void JitArmIL::Break(UGeckoInstruction _inst)
{
	ibuild.EmitINT3();
}

void JitArmIL::DoDownCount()
{
	ARMReg rA = R14;
	ARMReg rB = R12;
	MOVI2R(rA, (u32)&CoreTiming::downcount);
	LDR(rB, rA);
	if (js.downcountAmount < 255) // We can enlarge this if we used rotations
	{
		SUBS(rB, rB, js.downcountAmount);
		STR(rB, rA);
	}
	else
	{
		ARMReg rC = R11;
		MOVI2R(rC, js.downcountAmount);
		SUBS(rB, rB, rC);
		STR(rB, rA);
	}
}

void JitArmIL::WriteExitDestInReg(ARMReg Reg)
{
	STR(Reg, R9, PPCSTATE_OFF(pc));
	DoDownCount();
	MOVI2R(Reg, (u32)asm_routines.dispatcher);
	B(Reg);
}

void JitArmIL::WriteRfiExitDestInR(ARMReg Reg)
{
	STR(Reg, R9, PPCSTATE_OFF(pc));
	DoDownCount();

	LDR(R0, R9, PPCSTATE_OFF(pc));
	STR(R0, R9, PPCSTATE_OFF(npc));
		QuickCallFunction(R0, (void*)&PowerPC::CheckExceptions);
	LDR(R0, R9, PPCSTATE_OFF(npc));
	STR(R0, R9, PPCSTATE_OFF(pc));

	MOVI2R(R0, (u32)asm_routines.dispatcher);
	B(R0);
}
void JitArmIL::WriteExceptionExit()
{
	DoDownCount();

	LDR(R0, R9, PPCSTATE_OFF(pc));
	STR(R0, R9, PPCSTATE_OFF(npc));
		QuickCallFunction(R0, (void*)&PowerPC::CheckExceptions);
	LDR(R0, R9, PPCSTATE_OFF(npc));
	STR(R0, R9, PPCSTATE_OFF(pc));

	MOVI2R(R0, (u32)asm_routines.dispatcher);
	B(R0);
}
void JitArmIL::WriteExit(u32 destination)
{
	DoDownCount();
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
		MOVI2R(R14, destination);
		STR(R14, R9, PPCSTATE_OFF(pc));
		MOVI2R(R14, (u32)asm_routines.dispatcher);
		B(R14);
	}

	b->linkData.push_back(linkData);
}
void JitArmIL::PrintDebug(UGeckoInstruction inst, u32 level)
{
	if (level > 0)
		printf("Start: %08x OP '%s' Info\n", (u32)GetCodePtr(),  PPCTables::GetInstructionName(inst));
	if (level > 1)
	{
		GekkoOPInfo* Info = GetOpInfo(inst.hex);
		printf("\tOuts\n");
		if (Info->flags & FL_OUT_A)
			printf("\t-OUT_A: %x\n", inst.RA);
		if (Info->flags & FL_OUT_D)
			printf("\t-OUT_D: %x\n", inst.RD);
		printf("\tIns\n");
		// A, AO, B, C, S
		if (Info->flags & FL_IN_A)
			printf("\t-IN_A: %x\n", inst.RA);
		if (Info->flags & FL_IN_A0)
			printf("\t-IN_A0: %x\n", inst.RA);
		if (Info->flags & FL_IN_B)
			printf("\t-IN_B: %x\n", inst.RB);
		if (Info->flags & FL_IN_C)
			printf("\t-IN_C: %x\n", inst.RC);
		if (Info->flags & FL_IN_S)
			printf("\t-IN_S: %x\n", inst.RS);
	}
}

void STACKALIGN JitArmIL::Run()
{
	CompiledCode pExecAddr = (CompiledCode)asm_routines.enterCode;
	pExecAddr();
}

void JitArmIL::SingleStep()
{
	CompiledCode pExecAddr = (CompiledCode)asm_routines.enterCode;
	pExecAddr();
}
void STACKALIGN JitArmIL::Jit(u32 em_address)
{
	if (GetSpaceLeft() < 0x10000 || blocks.IsFull() || Core::g_CoreStartupParameter.bJITNoBlockCache)
	{
		ClearCache();
	}

	int block_num = blocks.AllocateBlock(PowerPC::ppcState.pc);
	JitBlock *b = blocks.GetBlock(block_num);
	const u8* BlockPtr = DoJit(PowerPC::ppcState.pc, &code_buffer, b);
	blocks.FinalizeBlock(block_num, jo.enableBlocklink, BlockPtr);
}

const u8* JitArmIL::DoJit(u32 em_address, PPCAnalyst::CodeBuffer *code_buf, JitBlock *b)
{
	int blockSize = code_buf->GetSize();

	if (Core::g_CoreStartupParameter.bEnableDebugging)
	{
		// Comment out the following to disable breakpoints (speed-up)
		blockSize = 1;
	}

	if (em_address == 0)
	{
		Core::SetState(Core::CORE_PAUSE);
		PanicAlert("ERROR: Compiling at 0. LR=%08x CTR=%08x", LR, CTR);
	}

	js.isLastInstruction = false;
	js.blockStart = em_address;
	js.fifoBytesThisBlock = 0;
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
		// XXX
	}

	const u8 *normalEntry = GetCodePtr();
	b->normalEntry = normalEntry;

	if (js.fpa.any)
	{
		// XXX
		// This block uses FPU - needs to add FP exception bailout
	}
	js.rewriteStart = (u8*)GetCodePtr();

	u64 codeHash = -1;
	{
		// For profiling and IR Writer
		for (u32 i = 0; i < code_block.m_num_instructions; i++)
		{
			const u64 inst = ops[i].inst.hex;
			// Ported from boost::hash
			codeHash ^= inst + (codeHash << 6) + (codeHash >> 2);
		}
	}

	// Conditionally add profiling code.
	if (Profiler::g_ProfileBlocks) {
		// XXX
	}
	// Start up IR builder (structure that collects the
	// instruction processed by the JIT routines)
	ibuild.Reset();

	js.downcountAmount = 0;
	if (!Core::g_CoreStartupParameter.bEnableDebugging)
		js.downcountAmount += PatchEngine::GetSpeedhackCycles(em_address);

	js.skipnext = false;
	js.compilerPC = nextPC;
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
			if (Profiler::g_ProfileBlocks) {
				// CAUTION!!! push on stack regs you use, do your stuff, then pop
				PROFILER_VPUSH;
				// get end tic
				PROFILER_QUERY_PERFORMANCE_COUNTER(&b->ticStop);
				// tic counter += (end tic - start tic)
				PROFILER_ADD_DIFF_LARGE_INTEGER(&b->ticCounter, &b->ticStop, &b->ticStart);
				PROFILER_VPOP;
			}
		}
		else
		{
			// help peephole optimizations
			js.next_inst = ops[i + 1].inst;
			js.next_compilerPC = ops[i + 1].address;
		}
		if (!ops[i].skip)
		{
				PrintDebug(ops[i].inst, 0);
				if (js.memcheck && (opinfo->flags & FL_USE_FPU))
				{
					// Don't do this yet
					BKPT(0x7777);
				}
				JitArmILTables::CompileInstruction(ops[i]);
				if (js.memcheck && (opinfo->flags & FL_LOADSTORE))
				{
					// Don't do this yet
					BKPT(0x666);
				}
		}
	}
	if (code_block.m_memory_exception)
		BKPT(0x500);

	if (code_block.m_broken)
	{
		printf("Broken Block going to 0x%08x\n", nextPC);
		WriteExit(nextPC);
	}

	// Perform actual code generation
	WriteCode(nextPC);
	b->codeSize = (u32)(GetCodePtr() - normalEntry);
	b->originalSize = code_block.m_num_instructions;;

	FlushIcache();
	return start;

}
