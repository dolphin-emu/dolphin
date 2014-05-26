// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <map>

// for the PROFILER stuff
#ifdef _WIN32
#include <windows.h>
#endif

#include "Common/Common.h"
#include "Core/PatchEngine.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/PowerPC/Profiler.h"
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/Jit64_Tables.h"
#include "Core/PowerPC/Jit64/JitAsm.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"
#if defined(_DEBUG) || defined(DEBUGFAST)
#include "PowerPCDisasm.h"
#endif

using namespace Gen;
using namespace PowerPC;

// Dolphin's PowerPC->x86 JIT dynamic recompiler
// (Nearly) all code by ector (hrydgard)
// Features:
// * x86 & x64 support, lots of shared code.
// * Basic block linking
// * Fast dispatcher

// Unfeatures:
// * Does not recompile all instructions - sometimes falls back to inserting a CALL to the corresponding Interpreter function.

// Various notes below

// Register allocation
//   RAX - Generic quicktemp register
//   RBX - point to base of memory map
//   RSI RDI R12 R13 R14 R15 - free for allocation
//   RCX RDX R8 R9 R10 R11 - allocate in emergencies. These need to be flushed before functions are called.
//   RSP - stack pointer, do not generally use, very dangerous
//   RBP - ?

// IMPORTANT:
// Make sure that all generated code and all emulator state sits under the 2GB boundary so that
// RIP addressing can be used easily. Windows will always allocate static code under the 2GB boundary.
// Also make sure to use VirtualAlloc and specify EXECUTE permission.

// Open questions
// * Should there be any statically allocated registers? r3, r4, r5, r8, r0 come to mind.. maybe sp
// * Does it make sense to finish off the remaining non-jitted instructions? Seems we are hitting diminishing returns.

// Other considerations
//
// Many instructions have shorter forms for EAX. However, I believe their performance boost
// will be as small to be negligible, so I haven't dirtied up the code with that. AMD recommends it in their
// optimization manuals, though.
//
// We support block linking. Reserve space at the exits of every block for a full 5-byte jmp. Save 16-bit offsets
// from the starts of each block, marking the exits so that they can be nicely patched at any time.
//
// Blocks do NOT use call/ret, they only jmp to each other and to the dispatcher when necessary.
//
// All blocks that can be precompiled will be precompiled. Code will be memory protected - any write will mark
// the region as non-compilable, and all links to the page will be torn out and replaced with dispatcher jmps.
//
// Alternatively, icbi instruction SHOULD mark where we can't compile
//
// Seldom-happening events is handled by adding a decrement of a counter to all blr instructions (which are
// expensive anyway since we need to return to dispatcher, except when they can be predicted).

// TODO: SERIOUS synchronization problem with the video backend setting tokens and breakpoints in dual core mode!!!
//       Somewhat fixed by disabling idle skipping when certain interrupts are enabled
//       This is no permanent reliable fix
// TODO: Zeldas go whacko when you hang the gfx thread

// Idea - Accurate exception handling
// Compute register state at a certain instruction by running the JIT in "dry mode", and stopping at the right place.
// Not likely to be done :P


// Optimization Ideas -
/*
  * Assume SP is in main RAM (in Wii mode too?) - partly done
  * Assume all floating point loads and double precision loads+stores are to/from main ram
    (single precision stores can be used in write gather pipe, specialized fast check added)
  * AMD only - use movaps instead of movapd when loading ps from memory?
  * HLE functions like floorf, sin, memcpy, etc - they can be much faster
  * ABI optimizations - drop F0-F13 on blr, for example. Watch out for context switching.
    CR2-CR4 are non-volatile, rest of CR is volatile -> dropped on blr.
	R5-R12 are volatile -> dropped on blr.
  * classic inlining across calls.

Low hanging fruit:
stfd -- guaranteed in memory
cmpl
mulli
stfs
stwu
lb/stzx

bcx - optimize!
bcctr
stfs
psq_st
addx
orx
rlwimix
fcmpo
DSP_UpdateARAMDMA
lfd
stwu
cntlzwx
bcctrx
WriteBigEData

TODO
lha
srawx
addic_rc
addex
subfcx
subfex

fmaddx
fmulx
faddx
fnegx
frspx
frsqrtex
ps_sum0
ps_muls0
ps_adds1

*/

static int CODE_SIZE = 1024*1024*32;

namespace CPUCompare
{
	extern u32 m_BlockStart;
}

void Jit64::Init()
{
	jo.optimizeStack = true;
	/* This will enable block linking in JitBlockCache::FinalizeBlock(), it gives faster execution but may not
	   be as stable as the alternative (to not link the blocks). However, I have not heard about any good examples
	   where this cause problems, so I'm enabling this by default, since I seem to get perhaps as much as 20% more
	   fps with this option enabled. If you suspect that this option cause problems you can also disable it from the
	   debugging window. */
	if (Core::g_CoreStartupParameter.bEnableDebugging)
	{
		jo.enableBlocklink = false;
		Core::g_CoreStartupParameter.bSkipIdle = false;
	}
	else
	{
		if (!Core::g_CoreStartupParameter.bJITBlockLinking)
		{
			jo.enableBlocklink = false;
		}
		else
			jo.enableBlocklink = !Core::g_CoreStartupParameter.bMMU;
	}
	jo.fpAccurateFcmp = Core::g_CoreStartupParameter.bEnableFPRF;
	jo.optimizeGatherPipe = true;
	jo.fastInterrupts = false;
	jo.accurateSinglePrecision = true;
	js.memcheck = Core::g_CoreStartupParameter.bMMU;

	gpr.SetEmitter(this);
	fpr.SetEmitter(this);

	trampolines.Init();
	AllocCodeSpace(CODE_SIZE);

	blocks.Init();
	asm_routines.Init();

	code_block.m_stats = &js.st;
	code_block.m_gpa = &js.gpa;
	code_block.m_fpa = &js.fpa;
	analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE);
}

void Jit64::ClearCache()
{
	blocks.Clear();
	trampolines.ClearCodeSpace();
	ClearCodeSpace();
}

void Jit64::Shutdown()
{
	FreeCodeSpace();

	blocks.Shutdown();
	trampolines.Shutdown();
	asm_routines.Shutdown();
}

// This is only called by FallBackToInterpreter() in this file. It will execute an instruction with the interpreter functions.
void Jit64::WriteCallInterpreter(UGeckoInstruction inst)
{
	gpr.Flush(FLUSH_ALL);
	fpr.Flush(FLUSH_ALL);
	if (js.isLastInstruction)
	{
		MOV(32, M(&PC), Imm32(js.compilerPC));
		MOV(32, M(&NPC), Imm32(js.compilerPC + 4));
	}
	Interpreter::_interpreterInstruction instr = GetInterpreterOp(inst);
	ABI_CallFunctionC((void*)instr, inst.hex);
}

void Jit64::unknown_instruction(UGeckoInstruction inst)
{
	PanicAlert("unknown_instruction %08x - Fix me ;)", inst.hex);
}

void Jit64::FallBackToInterpreter(UGeckoInstruction _inst)
{
	WriteCallInterpreter(_inst.hex);
}

void Jit64::HLEFunction(UGeckoInstruction _inst)
{
	gpr.Flush(FLUSH_ALL);
	fpr.Flush(FLUSH_ALL);
	ABI_CallFunctionCC((void*)&HLE::Execute, js.compilerPC, _inst.hex);
}

void Jit64::DoNothing(UGeckoInstruction _inst)
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
#if _M_X86_64
			f.Open("log64.txt", "w");
#else
			f.Open("log32.txt", "w");
#endif
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

void Jit64::Cleanup()
{
	if (jo.optimizeGatherPipe && js.fifoBytesThisBlock > 0)
	{
		ABI_CallFunction((void *)&GPFifo::CheckGatherPipe);
	}

	// SPEED HACK: MMCR0/MMCR1 should be checked at run-time, not at compile time.
	if (MMCR0.Hex || MMCR1.Hex)
		ABI_CallFunctionCCC((void *)&PowerPC::UpdatePerformanceMonitor, js.downcountAmount, jit->js.numLoadStoreInst, jit->js.numFloatingPointInst);
}

void Jit64::WriteExit(u32 destination)
{
	Cleanup();

	SUB(32, M(&CoreTiming::downcount), js.downcountAmount > 127 ? Imm32(js.downcountAmount) : Imm8(js.downcountAmount));

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
		JMP(blocks.GetBlock(block)->checkedEntry, true);
		linkData.linkStatus = true;
	}
	else
	{
		MOV(32, M(&PC), Imm32(destination));
		JMP(asm_routines.dispatcher, true);
	}

	b->linkData.push_back(linkData);
}

void Jit64::WriteExitDestInEAX()
{
	MOV(32, M(&PC), R(EAX));
	Cleanup();
	SUB(32, M(&CoreTiming::downcount), js.downcountAmount > 127 ? Imm32(js.downcountAmount) : Imm8(js.downcountAmount));
	JMP(asm_routines.dispatcher, true);
}

void Jit64::WriteRfiExitDestInEAX()
{
	MOV(32, M(&PC), R(EAX));
	MOV(32, M(&NPC), R(EAX));
	Cleanup();
	ABI_CallFunction(reinterpret_cast<void *>(&PowerPC::CheckExceptions));
	SUB(32, M(&CoreTiming::downcount), js.downcountAmount > 127 ? Imm32(js.downcountAmount) : Imm8(js.downcountAmount));
	JMP(asm_routines.dispatcher, true);
}

void Jit64::WriteExceptionExit()
{
	Cleanup();
	MOV(32, R(EAX), M(&PC));
	MOV(32, M(&NPC), R(EAX));
	ABI_CallFunction(reinterpret_cast<void *>(&PowerPC::CheckExceptions));
	SUB(32, M(&CoreTiming::downcount), js.downcountAmount > 127 ? Imm32(js.downcountAmount) : Imm8(js.downcountAmount));
	JMP(asm_routines.dispatcher, true);
}

void Jit64::WriteExternalExceptionExit()
{
	Cleanup();
	MOV(32, R(EAX), M(&PC));
	MOV(32, M(&NPC), R(EAX));
	ABI_CallFunction(reinterpret_cast<void *>(&PowerPC::CheckExternalExceptions));
	SUB(32, M(&CoreTiming::downcount), js.downcountAmount > 127 ? Imm32(js.downcountAmount) : Imm8(js.downcountAmount));
	JMP(asm_routines.dispatcher, true);
}

void STACKALIGN Jit64::Run()
{
	CompiledCode pExecAddr = (CompiledCode)asm_routines.enterCode;
	pExecAddr();
}

void Jit64::SingleStep()
{
	CompiledCode pExecAddr = (CompiledCode)asm_routines.enterCode;
	pExecAddr();
}

void Jit64::Trace()
{
	char regs[500] = "";
	char fregs[750] = "";

#ifdef JIT_LOG_GPR
	for (int i = 0; i < 32; i++)
	{
		char reg[50];
		sprintf(reg, "r%02d: %08x ", i, PowerPC::ppcState.gpr[i]);
		strncat(regs, reg, sizeof(regs) - 1);
	}
#endif

#ifdef JIT_LOG_FPR
	for (int i = 0; i < 32; i++)
	{
		char reg[50];
		sprintf(reg, "f%02d: %016x ", i, riPS0(i));
		strncat(fregs, reg, sizeof(fregs) - 1);
	}
#endif

	DEBUG_LOG(DYNA_REC, "JIT64 PC: %08x SRR0: %08x SRR1: %08x CRfast: %02x%02x%02x%02x%02x%02x%02x%02x FPSCR: %08x MSR: %08x LR: %08x %s %s",
		PC, SRR0, SRR1, PowerPC::ppcState.cr_fast[0], PowerPC::ppcState.cr_fast[1], PowerPC::ppcState.cr_fast[2], PowerPC::ppcState.cr_fast[3],
		PowerPC::ppcState.cr_fast[4], PowerPC::ppcState.cr_fast[5], PowerPC::ppcState.cr_fast[6], PowerPC::ppcState.cr_fast[7], PowerPC::ppcState.fpscr,
		PowerPC::ppcState.msr, PowerPC::ppcState.spr[8], regs, fregs);
}

void STACKALIGN Jit64::Jit(u32 em_address)
{
	if (GetSpaceLeft() < 0x10000 || blocks.IsFull() || Core::g_CoreStartupParameter.bJITNoBlockCache)
	{
		ClearCache();
	}

	int block_num = blocks.AllocateBlock(em_address);
	JitBlock *b = blocks.GetBlock(block_num);
	blocks.FinalizeBlock(block_num, jo.enableBlocklink, DoJit(em_address, &code_buffer, b));
}

const u8* Jit64::DoJit(u32 em_address, PPCAnalyst::CodeBuffer *code_buf, JitBlock *b)
{
	int blockSize = code_buf->GetSize();

	if (Core::g_CoreStartupParameter.bEnableDebugging)
	{
		// Comment out the following to disable breakpoints (speed-up)
		if (!Profiler::g_ProfileBlocks)
		{
			if (GetState() == CPU_STEPPING)
				blockSize = 1;
			Trace();
		}
	}

	js.firstFPInstructionFound = false;
	js.isLastInstruction = false;
	js.blockStart = em_address;
	js.fifoBytesThisBlock = 0;
	js.curBlock = b;
	jit->js.numLoadStoreInst = 0;
	jit->js.numFloatingPointInst = 0;

	u32 nextPC = em_address;
	// Analyze the block, collect all instructions it is made of (including inlining,
	// if that is enabled), reorder instructions for optimal performance, and join joinable instructions.
	nextPC = analyzer.Analyze(em_address, &code_block, code_buf, blockSize);

	PPCAnalyst::CodeOp *ops = code_buf->codebuffer;

	const u8 *start = AlignCode4(); // TODO: Test if this or AlignCode16 make a difference from GetCodePtr
	b->checkedEntry = start;
	b->runCount = 0;

	// Downcount flag check. The last block decremented downcounter, and the flag should still be available.
	FixupBranch skip = J_CC(CC_NBE);
	MOV(32, M(&PC), Imm32(js.blockStart));
	JMP(asm_routines.doTiming, true);  // downcount hit zero - go doTiming.
	SetJumpTarget(skip);

	const u8 *normalEntry = GetCodePtr();
	b->normalEntry = normalEntry;

	if (ImHereDebug)
		ABI_CallFunction((void *)&ImHere); //Used to get a trace of the last few blocks before a crash, sometimes VERY useful

	// Conditionally add profiling code.
	if (Profiler::g_ProfileBlocks) {
		ADD(32, M(&b->runCount), Imm8(1));
#ifdef _WIN32
		b->ticCounter = 0;
		b->ticStart = 0;
		b->ticStop = 0;
#else
//TODO
#endif
		// get start tic
		PROFILER_QUERY_PERFORMANCE_COUNTER(&b->ticStart);
	}
#if defined(_DEBUG) || defined(DEBUGFAST) || defined(NAN_CHECK)
	// should help logged stack-traces become more accurate
	MOV(32, M(&PC), Imm32(js.blockStart));
#endif

	// Start up the register allocators
	// They use the information in gpa/fpa to preload commonly used registers.
	gpr.Start(js.gpa);
	fpr.Start(js.fpa);

	js.downcountAmount = 0;
	if (!Core::g_CoreStartupParameter.bEnableDebugging)
		js.downcountAmount += PatchEngine::GetSpeedhackCycles(code_block.m_address);

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

		if (jo.optimizeGatherPipe && js.fifoBytesThisBlock >= 32)
		{
			js.fifoBytesThisBlock -= 32;
			MOV(32, M(&PC), Imm32(jit->js.compilerPC)); // Helps external systems know which instruction triggered the write
			u32 registersInUse = RegistersInUse();
			ABI_PushRegistersAndAdjustStack(registersInUse, false);
			ABI_CallFunction((void *)&GPFifo::CheckGatherPipe);
			ABI_PopRegistersAndAdjustStack(registersInUse, false);
		}

		u32 function = HLE::GetFunctionIndex(ops[i].address);
		if (function != 0)
		{
			int type = HLE::GetFunctionTypeByIndex(function);
			if (type == HLE::HLE_HOOK_START || type == HLE::HLE_HOOK_REPLACE)
			{
				int flags = HLE::GetFunctionFlagsByIndex(function);
				if (HLE::IsEnabled(flags))
				{
					HLEFunction(function);
					if (type == HLE::HLE_HOOK_REPLACE)
					{
						MOV(32, R(EAX), M(&NPC));
						js.downcountAmount += js.st.numCycles;
						WriteExitDestInEAX();
						break;
					}
				}
			}
		}

		if (!ops[i].skip)
		{
			if ((opinfo->flags & FL_USE_FPU) && !js.firstFPInstructionFound)
			{
				gpr.Flush(FLUSH_ALL);
				fpr.Flush(FLUSH_ALL);

				//This instruction uses FPU - needs to add FP exception bailout
				TEST(32, M(&PowerPC::ppcState.msr), Imm32(1 << 13)); // Test FP enabled bit
				FixupBranch b1 = J_CC(CC_NZ, true);

				// If a FPU exception occurs, the exception handler will read
				// from PC.  Update PC with the latest value in case that happens.
				MOV(32, M(&PC), Imm32(ops[i].address));
				OR(32, M((void *)&PowerPC::ppcState.Exceptions), Imm32(EXCEPTION_FPU_UNAVAILABLE));
				WriteExceptionExit();

				SetJumpTarget(b1);

				js.firstFPInstructionFound = true;
			}

			// Add an external exception check if the instruction writes to the FIFO.
			if (jit->js.fifoWriteAddresses.find(ops[i].address) != jit->js.fifoWriteAddresses.end())
			{
				gpr.Flush(FLUSH_ALL);
				fpr.Flush(FLUSH_ALL);

				TEST(32, M((void *)&PowerPC::ppcState.Exceptions), Imm32(EXCEPTION_ISI | EXCEPTION_PROGRAM | EXCEPTION_SYSCALL | EXCEPTION_FPU_UNAVAILABLE | EXCEPTION_DSI | EXCEPTION_ALIGNMENT));
				FixupBranch clearInt = J_CC(CC_NZ, true);
				TEST(32, M((void *)&PowerPC::ppcState.Exceptions), Imm32(EXCEPTION_EXTERNAL_INT));
				FixupBranch noExtException = J_CC(CC_Z, true);
				TEST(32, M((void *)&PowerPC::ppcState.msr), Imm32(0x0008000));
				FixupBranch noExtIntEnable = J_CC(CC_Z, true);
				TEST(32, M((void *)&ProcessorInterface::m_InterruptCause), Imm32(ProcessorInterface::INT_CAUSE_CP | ProcessorInterface::INT_CAUSE_PE_TOKEN | ProcessorInterface::INT_CAUSE_PE_FINISH));
				FixupBranch noCPInt = J_CC(CC_Z, true);

				MOV(32, M(&PC), Imm32(ops[i].address));
				WriteExternalExceptionExit();

				SetJumpTarget(noCPInt);
				SetJumpTarget(noExtIntEnable);
				SetJumpTarget(noExtException);
				SetJumpTarget(clearInt);
			}

			if (Core::g_CoreStartupParameter.bEnableDebugging && breakpoints.IsAddressBreakPoint(ops[i].address) && GetState() != CPU_STEPPING)
			{
				gpr.Flush(FLUSH_ALL);
				fpr.Flush(FLUSH_ALL);

				MOV(32, M(&PC), Imm32(ops[i].address));
				ABI_CallFunction(reinterpret_cast<void *>(&PowerPC::CheckBreakPoints));
				TEST(32, M((void*)PowerPC::GetStatePtr()), Imm32(0xFFFFFFFF));
				FixupBranch noBreakpoint = J_CC(CC_Z);

				WriteExit(ops[i].address);
				SetJumpTarget(noBreakpoint);
			}

			Jit64Tables::CompileInstruction(ops[i]);

			if (js.memcheck && (opinfo->flags & FL_LOADSTORE))
			{
				// In case we are about to jump to the dispatcher, flush regs
				gpr.Flush(FLUSH_ALL);
				fpr.Flush(FLUSH_ALL);

				TEST(32, M((void *)&PowerPC::ppcState.Exceptions), Imm32(EXCEPTION_DSI));
				FixupBranch noMemException = J_CC(CC_Z, true);

				// If a memory exception occurs, the exception handler will read
				// from PC.  Update PC with the latest value in case that happens.
				MOV(32, M(&PC), Imm32(ops[i].address));
				WriteExceptionExit();
				SetJumpTarget(noMemException);
			}

			if (opinfo->flags & FL_LOADSTORE)
				++jit->js.numLoadStoreInst;

			if (opinfo->flags & FL_USE_FPU)
				++jit->js.numFloatingPointInst;
		}

#if defined(_DEBUG) || defined(DEBUGFAST)
		if (gpr.SanityCheck() || fpr.SanityCheck())
		{
			char ppcInst[256];
			DisassembleGekko(ops[i].inst.hex, em_address, ppcInst, 256);
			//NOTICE_LOG(DYNA_REC, "Unflushed register: %s", ppcInst);
		}
#endif
		if (js.skipnext) {
			js.skipnext = false;
			i++; // Skip next instruction
		}
	}

	u32 function = HLE::GetFunctionIndex(js.blockStart);
	if (function != 0)
	{
		int type = HLE::GetFunctionTypeByIndex(function);
		if (type == HLE::HLE_HOOK_END)
		{
			int flags = HLE::GetFunctionFlagsByIndex(function);
			if (HLE::IsEnabled(flags))
			{
				HLEFunction(function);
			}
		}
	}

	if (code_block.m_memory_exception)
	{
		// Address of instruction could not be translated
		MOV(32, M(&NPC), Imm32(js.compilerPC));

		OR(32, M((void *)&PowerPC::ppcState.Exceptions), Imm32(EXCEPTION_ISI));

		// Remove the invalid instruction from the icache, forcing a recompile
#if _M_X86_32
		MOV(32, M(jit->GetBlockCache()->GetICachePtr(js.compilerPC)), Imm32(JIT_ICACHE_INVALID_WORD));
#else
		MOV(64, R(RAX), ImmPtr(jit->GetBlockCache()->GetICachePtr(js.compilerPC)));
		MOV(32,MatR(RAX),Imm32(JIT_ICACHE_INVALID_WORD));
#endif

		WriteExceptionExit();
	}

	if (code_block.m_broken)
	{
		gpr.Flush(FLUSH_ALL);
		fpr.Flush(FLUSH_ALL);
		WriteExit(nextPC);
	}

	b->codeSize = (u32)(GetCodePtr() - normalEntry);
	b->originalSize = code_block.m_num_instructions;

#ifdef JIT_LOG_X86
	LogGeneratedX86(code_block.m_num_instructions, code_buf, normalEntry, b);
#endif

	return normalEntry;
}

u32 Jit64::RegistersInUse()
{
	u32 result = 0;
	for (int i = 0; i < NUMXREGS; i++)
	{
		if (!gpr.IsFreeX(i))
			result |= (1 << i);
		if (!fpr.IsFreeX(i))
			result |= (1 << (16 + i));
	}
	return result;
}
