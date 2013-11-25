// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <map>

#include "Common.h"
#include "../../HLE/HLE.h"
#include "../../Core.h"
#include "../../PatchEngine.h"
#include "../../CoreTiming.h"
#include "../../ConfigManager.h"
#include "../PowerPC.h"
#include "../Profiler.h"
#include "../PPCTables.h"
#include "../PPCAnalyst.h"
#include "../../HW/Memmap.h"
#include "../../HW/GPFifo.h"
#include "JitIL.h"
#include "JitIL_Tables.h"
#include "ArmEmitter.h"
#include "../JitInterface.h"

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
	//	CCPU::Break();
	PanicAlert("unknown_instruction %08x - Fix me ;)", inst.hex);
}

void JitArmIL::Default(UGeckoInstruction _inst)
{
	ibuild.EmitInterpreterFallback(
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
	if(js.downcountAmount < 255) // We can enlarge this if we used rotations
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
	MOVI2R(Reg, (u32)asm_routines.testExceptions);
	B(Reg);
}
void JitArmIL::WriteExceptionExit()
{
	DoDownCount();

	MOVI2R(R14, (u32)asm_routines.testExceptions);
	B(R14);
}
void JitArmIL::WriteExit(u32 destination, int exit_num)
{
	DoDownCount();
	//If nobody has taken care of this yet (this can be removed when all branches are done)
	JitBlock *b = js.curBlock;
	b->exitAddress[exit_num] = destination;
	b->exitPtrs[exit_num] = GetWritableCodePtr();

	// Link opportunity!
	int block = blocks.GetBlockNumberFromStartAddress(destination);
	if (block >= 0 && jo.enableBlocklink)
	{
		// It exists! Joy of joy!
		B(blocks.GetBlock(block)->checkedEntry);
		b->linkStatus[exit_num] = true;
	}
	else
	{
		MOVI2R(R14, destination);
		STR(R14, R9, PPCSTATE_OFF(pc));
		MOVI2R(R14, (u32)asm_routines.dispatcher);
		B(R14);
	}
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
		if(Info->flags & FL_OUT_D)
			printf("\t-OUT_D: %x\n", inst.RD);
		printf("\tIns\n");
		// A, AO, B, C, S
		if(Info->flags & FL_IN_A)
			printf("\t-IN_A: %x\n", inst.RA);
		if(Info->flags & FL_IN_A0)
			printf("\t-IN_A0: %x\n", inst.RA);
		if(Info->flags & FL_IN_B)
			printf("\t-IN_B: %x\n", inst.RB);
		if(Info->flags & FL_IN_C)
			printf("\t-IN_C: %x\n", inst.RC);
		if(Info->flags & FL_IN_S)
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
	// Memory exception on instruction fetch
	bool memory_exception = false;

	// A broken block is a block that does not end in a branch
	bool broken_block = false;

	if (Core::g_CoreStartupParameter.bEnableDebugging)
	{
		// Comment out the following to disable breakpoints (speed-up)
		blockSize = 1;
		broken_block = true;
	}

	if (em_address == 0)
	{
		Core::SetState(Core::CORE_PAUSE);
		PanicAlert("ERROR: Compiling at 0. LR=%08x CTR=%08x", LR, CTR);
	}

	if (Core::g_CoreStartupParameter.bMMU && (em_address & JIT_ICACHE_VMEM_BIT))
	{
		if (!Memory::TranslateAddress(em_address, Memory::FLAG_OPCODE))
		{
			// Memory exception occurred during instruction fetch
			memory_exception = true;
		}
	}


	int size = 0;
	js.isLastInstruction = false;
	js.blockStart = em_address;
	js.fifoBytesThisBlock = 0;
	js.curBlock = b;
	js.block_flags = 0;
	js.cancel = false;

	// Analyze the block, collect all instructions it is made of (including inlining,
	// if that is enabled), reorder instructions for optimal performance, and join joinable instructions.
	u32 nextPC = em_address;
	u32 merged_addresses[32];
	const int capacity_of_merged_addresses = sizeof(merged_addresses) / sizeof(merged_addresses[0]);
	int size_of_merged_addresses = 0;
	if (!memory_exception)
	{
		// If there is a memory exception inside a block (broken_block==true), compile up to that instruction.
		nextPC = PPCAnalyst::Flatten(em_address, &size, &js.st, &js.gpa, &js.fpa, broken_block, code_buf, blockSize, merged_addresses, capacity_of_merged_addresses, size_of_merged_addresses);
	}
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
		for (int i = 0; i < (int)size; i++)
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
	{
		for (int i = 0; i < size_of_merged_addresses; ++i)
		{
			const u32 address = merged_addresses[i];
			js.downcountAmount += PatchEngine::GetSpeedhackCycles(address);
		}
	}

	js.skipnext = false;
	js.blockSize = size;
	js.compilerPC = nextPC;
	// Translate instructions
	for (int i = 0; i < (int)size; i++)
	{
		js.compilerPC = ops[i].address;
		js.op = &ops[i];
		js.instructionNumber = i;
		const GekkoOPInfo *opinfo = ops[i].opinfo;
		js.downcountAmount += (opinfo->numCyclesMinusOne + 1);

		if (i == (int)size - 1)
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
	if (memory_exception)
		BKPT(0x500);
	if	(broken_block)
	{
		printf("Broken Block going to 0x%08x\n", nextPC);
		WriteExit(nextPC, 0);
	}

	// Perform actual code generation

	WriteCode();
	b->flags = js.block_flags;
	b->codeSize = (u32)(GetCodePtr() - normalEntry);
	b->originalSize = size;

	{
	}
	FlushIcache();
	return start;

}
