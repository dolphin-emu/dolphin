// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <map>

#include "Common/ArmEmitter.h"
#include "Common/CommonTypes.h"

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
#include "Core/PowerPC/JitArm32/Jit.h"
#include "Core/PowerPC/JitArm32/JitArm_Tables.h"

using namespace ArmGen;

static int CODE_SIZE = 1024*1024*32;

void JitArm::Init()
{
	AllocCodeSpace(CODE_SIZE);
	blocks.Init();
	asm_routines.Init();
	gpr.Init(this);
	fpr.Init(this);
	jo.enableBlocklink = true;
	jo.optimizeGatherPipe = true;

	code_block.m_stats = &js.st;
	code_block.m_gpa = &js.gpa;
	code_block.m_fpa = &js.fpa;
	analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE);
}

void JitArm::ClearCache()
{
	ClearCodeSpace();
	blocks.Clear();
}

void JitArm::Shutdown()
{
	FreeCodeSpace();
	blocks.Shutdown();
	asm_routines.Shutdown();
}

// This is only called by FallBackToInterpreter() in this file. It will execute an instruction with the interpreter functions.
void JitArm::WriteCallInterpreter(UGeckoInstruction inst)
{
	gpr.Flush();
	fpr.Flush();
	Interpreter::_interpreterInstruction instr = GetInterpreterOp(inst);
	MOVI2R(R0, inst.hex);
	MOVI2R(R12, (u32)instr);
	BL(R12);
}
void JitArm::unknown_instruction(UGeckoInstruction inst)
{
	PanicAlert("unknown_instruction %08x - Fix me ;)", inst.hex);
}

void JitArm::FallBackToInterpreter(UGeckoInstruction _inst)
{
	WriteCallInterpreter(_inst.hex);
}

void JitArm::HLEFunction(UGeckoInstruction _inst)
{
	gpr.Flush();
	fpr.Flush();
	MOVI2R(R0, js.compilerPC);
	MOVI2R(R1, _inst.hex);
	QuickCallFunction(R14, (void*)&HLE::Execute);
	ARMReg rA = gpr.GetReg();
	LDR(rA, R9, PPCSTATE_OFF(npc));
	WriteExitDestInR(rA);
}

void JitArm::DoNothing(UGeckoInstruction _inst)
{
	// Yup, just don't do anything.
}

static const bool ImHereDebug = false;
static const bool ImHereLog = false;
static std::map<u32, int> been_here;

static void ImHere()
{
	static File::IOFile f;
	if (ImHereLog)
	{
		if (!f)
		{
			f.Open("log32.txt", "w");
		}
		fprintf(f.GetHandle(), "%08x\n", PC);
	}

	if (been_here.find(PC) != been_here.end())
	{
		been_here.find(PC)->second++;
		if ((been_here.find(PC)->second) & 1023)
			return;
	}

	DEBUG_LOG(DYNA_REC, "I'm here - PC = %08x , LR = %08x", PC, LR);
	been_here[PC] = 1;
}

void JitArm::Cleanup()
{
	if (jo.optimizeGatherPipe && js.fifoBytesThisBlock > 0)
	{
		PUSH(4, R0, R1, R2, R3);
		QuickCallFunction(R14, (void*)&GPFifo::CheckGatherPipe);
		POP(4, R0, R1, R2, R3);
	}
}
void JitArm::DoDownCount()
{
	ARMReg rA = gpr.GetReg();
	LDR(rA, R9, PPCSTATE_OFF(downcount));
	if (js.downcountAmount < 255) // We can enlarge this if we used rotations
	{
		SUBS(rA, rA, js.downcountAmount);
	}
	else
	{
		ARMReg rB = gpr.GetReg(false);
		MOVI2R(rB, js.downcountAmount);
		SUBS(rA, rA, rB);
	}
	STR(rA, R9, PPCSTATE_OFF(downcount));
	gpr.Unlock(rA);
}
void JitArm::WriteExitDestInR(ARMReg Reg)
{
	STR(Reg, R9, PPCSTATE_OFF(pc));
	Cleanup();
	DoDownCount();
	MOVI2R(Reg, (u32)asm_routines.dispatcher);
	B(Reg);
	gpr.Unlock(Reg);
}
void JitArm::WriteRfiExitDestInR(ARMReg Reg)
{
	STR(Reg, R9, PPCSTATE_OFF(pc));
	Cleanup();
	DoDownCount();

	ARMReg A = gpr.GetReg(false);

	LDR(A, R9, PPCSTATE_OFF(pc));
	STR(A, R9, PPCSTATE_OFF(npc));
		QuickCallFunction(A, (void*)&PowerPC::CheckExceptions);
	LDR(A, R9, PPCSTATE_OFF(npc));
	STR(A, R9, PPCSTATE_OFF(pc));
	gpr.Unlock(Reg); // This was locked in the instruction beforehand

	MOVI2R(A, (u32)asm_routines.dispatcher);
	B(A);
}
void JitArm::WriteExceptionExit()
{
	Cleanup();
	DoDownCount();

	ARMReg A = gpr.GetReg(false);

	LDR(A, R9, PPCSTATE_OFF(pc));
	STR(A, R9, PPCSTATE_OFF(npc));
		QuickCallFunction(A, (void*)&PowerPC::CheckExceptions);
	LDR(A, R9, PPCSTATE_OFF(npc));
	STR(A, R9, PPCSTATE_OFF(pc));

	MOVI2R(A, (u32)asm_routines.dispatcher);
	B(A);
}
void JitArm::WriteExit(u32 destination)
{
	Cleanup();

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
		ARMReg A = gpr.GetReg(false);
		MOVI2R(A, destination);
		STR(A, R9, PPCSTATE_OFF(pc));
		MOVI2R(A, (u32)asm_routines.dispatcher);
		B(A);
	}

	b->linkData.push_back(linkData);
}

void JitArm::Run()
{
	CompiledCode pExecAddr = (CompiledCode)asm_routines.enterCode;
	pExecAddr();
}

void JitArm::SingleStep()
{
	CompiledCode pExecAddr = (CompiledCode)asm_routines.enterCode;
	pExecAddr();
}

void JitArm::Trace()
{
	std::string regs;
	std::string fregs;

#ifdef JIT_LOG_GPR
	for (int i = 0; i < 32; i++)
	{
		regs += StringFromFormat("r%02d: %08x ", i, PowerPC::ppcState.gpr[i]);
	}
#endif

#ifdef JIT_LOG_FPR
	for (int i = 0; i < 32; i++)
	{
		fregs += StringFromFormat("f%02d: %016x ", i, riPS0(i));
	}
#endif

	DEBUG_LOG(DYNA_REC, "JIT64 PC: %08x SRR0: %08x SRR1: %08x FPSCR: %08x MSR: %08x LR: %08x %s %s",
		PC, SRR0, SRR1, PowerPC::ppcState.fpscr, PowerPC::ppcState.msr, PowerPC::ppcState.spr[8], regs.c_str(), fregs.c_str());
}

void JitArm::Jit(u32 em_address)
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
void JitArm::Break(UGeckoInstruction inst)
{
	ERROR_LOG(DYNA_REC, "%s called a Break instruction!", PPCTables::GetInstructionName(inst));
	BKPT(0x4444);
}

const u8* JitArm::DoJit(u32 em_address, PPCAnalyst::CodeBuffer *code_buf, JitBlock *b)
{
	int blockSize = code_buf->GetSize();

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableDebugging)
	{
		// Comment out the following to disable breakpoints (speed-up)
		if (!Profiler::g_ProfileBlocks)
		{
			if (PowerPC::GetState() == PowerPC::CPU_STEPPING)
				blockSize = 1;
			Trace();
		}
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
		SetCC(CC_MI);
		ARMReg rA = gpr.GetReg(false);
		MOVI2R(rA, js.blockStart);
		STR(rA, R9, PPCSTATE_OFF(pc));
		MOVI2R(rA, (u32)asm_routines.doTiming);
		B(rA);
		SetCC();
	}

	const u8 *normalEntry = GetCodePtr();
	b->normalEntry = normalEntry;

	if (ImHereDebug)
		QuickCallFunction(R14, (void *)&ImHere); //Used to get a trace of the last few blocks before a crash, sometimes VERY useful

	if (js.fpa.any)
	{
		// This block uses FPU - needs to add FP exception bailout
		ARMReg A = gpr.GetReg();
		ARMReg C = gpr.GetReg();
		Operand2 Shift(2, 10); // 1 << 13
		MOVI2R(C, js.blockStart); // R3
		LDR(A, R9, PPCSTATE_OFF(msr));
		TST(A, Shift);
		SetCC(CC_EQ);
		STR(C, R9, PPCSTATE_OFF(pc));

		LDR(A, R9, PPCSTATE_OFF(Exceptions));
		ORR(A, A, EXCEPTION_FPU_UNAVAILABLE);
		STR(A, R9, PPCSTATE_OFF(Exceptions));
			QuickCallFunction(A, (void*)&PowerPC::CheckExceptions);
		LDR(A, R9, PPCSTATE_OFF(npc));
		STR(A, R9, PPCSTATE_OFF(pc));

		MOVI2R(A, (u32)asm_routines.dispatcher);
		B(A);

		SetCC();
		gpr.Unlock(A, C);
	}

	// Conditionally add profiling code.
	if (Profiler::g_ProfileBlocks)
	{
		ARMReg rA = gpr.GetReg();
		ARMReg rB = gpr.GetReg();
		MOVI2R(rA, (u32)&b->runCount); // Load in to register
		LDR(rB, rA); // Load the actual value in to R11.
		ADD(rB, rB, 1); // Add one to the value
		STR(rB, rA); // Now store it back in the memory location
		// get start tic
		PROFILER_QUERY_PERFORMANCE_COUNTER(&b->ticStart);
		gpr.Unlock(rA, rB);
	}
	gpr.Start(js.gpa);
	fpr.Start(js.fpa);
	js.downcountAmount = 0;

	if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableDebugging)
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
			if (Profiler::g_ProfileBlocks)
			{
				// CAUTION!!! push on stack regs you use, do your stuff, then pop
				PROFILER_VPUSH;
				// get end tic
				PROFILER_QUERY_PERFORMANCE_COUNTER(&b->ticStop);
				// tic counter += (end tic - start tic)
				PROFILER_UPDATE_TIME(&b);
				PROFILER_VPOP;
			}
		}
		else
		{
			// help peephole optimizations
			js.next_inst = ops[i + 1].inst;
			js.next_compilerPC = ops[i + 1].address;
		}

		if (jo.optimizeGatherPipe && js.fifoBytesThisBlock >= 32)
		{
			js.fifoBytesThisBlock -= 32;
			PUSH(4, R0, R1, R2, R3);
			QuickCallFunction(R14, (void*)&GPFifo::CheckGatherPipe);
			POP(4, R0, R1, R2, R3);
		}

		if (Profiler::g_ProfileBlocks)
		{
			// Add run count
			static const u64 One = 1;
			ARMReg RA = gpr.GetReg();
			ARMReg RB = gpr.GetReg();
			ARMReg VA = fpr.GetReg();
			ARMReg VB = fpr.GetReg();
			MOVI2R(RA, (u32)&opinfo->runCount);
			MOVI2R(RB, (u32)&One);
			VLDR(VA, RA, 0);
			VLDR(VB, RB, 0);
			NEONXEmitter nemit(this);
			nemit.VADD(I_64, VA, VA, VB);
			VSTR(VA, RA, 0);
			gpr.Unlock(RA, RB);
			fpr.Unlock(VA);
			fpr.Unlock(VB);
		}

		if (!ops[i].skip)
		{
				if (js.memcheck && (opinfo->flags & FL_USE_FPU))
				{
					// Don't do this yet
					BKPT(0x7777);
				}
				JitArmTables::CompileInstruction(ops[i]);
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

	b->codeSize = (u32)(GetCodePtr() - normalEntry);
	b->originalSize = code_block.m_num_instructions;
	FlushIcache();
	return start;
}
