// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <map>
#include <string>

// for the PROFILER stuff
#ifdef _WIN32
#include <windows.h>
#endif

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Core/PatchEngine.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/PowerPC/Profiler.h"
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/Jit64_Tables.h"
#include "Core/PowerPC/Jit64/JitAsm.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"
#if defined(_DEBUG) || defined(DEBUGFAST)
#include "Common/GekkoDisassembler.h"
#endif

using namespace Gen;
using namespace PowerPC;

// Dolphin's PowerPC->x86_64 JIT dynamic recompiler
// Written mostly by ector (hrydgard)
// Features:
// * Basic block linking
// * Fast dispatcher

// Unfeatures:
// * Does not recompile all instructions - sometimes falls back to inserting a CALL to the corresponding Interpreter function.

// Various notes below

// IMPORTANT:
// Make sure that all generated code and all emulator state sits under the 2GB boundary so that
// RIP addressing can be used easily. Windows will always allocate static code under the 2GB boundary.
// Also make sure to use VirtualAlloc and specify EXECUTE permission.

// Open questions
// * Should there be any statically allocated registers? r3, r4, r5, r8, r0 come to mind.. maybe sp
// * Does it make sense to finish off the remaining non-jitted instructions? Seems we are hitting diminishing returns.

// Other considerations
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
  * Track which registers a block clobbers without using, then take advantage of this knowledge
    when compiling a block that links to that block.
  * Track more dependencies between instructions, e.g. avoiding PPC_FP code, single/double
    conversion, movddup on non-paired singles, etc where possible.
  * Support loads/stores directly from xmm registers in jit_util and the backpatcher; this might
    help AMD a lot since gpr/xmm transfers are slower there.
  * Smarter register allocation in general; maybe learn to drop values once we know they won't be
    used again before being overwritten?
  * More flexible reordering; there's limits to how far we can go because of exception handling
    and such, but it's currently limited to integer ops only. This can definitely be made better.
*/

// The BLR optimization is nice, but it means that JITted code can overflow the
// native stack by repeatedly running BL.  (The chance of this happening in any
// retail game is close to 0, but correctness is correctness...) Also, the
// overflow might not happen directly in the JITted code but in a C++ function
// called from it, so we can't just adjust RSP in the case of a fault.
// Instead, we have to have extra stack space preallocated under the fault
// point which allows the code to continue, after wiping the JIT cache so we
// can reset things at a safe point.  Once this condition trips, the
// optimization is permanently disabled, under the assumption this will never
// happen in practice.

// On Unix, we just mark an appropriate region of the stack as PROT_NONE and
// handle it the same way as fastmem faults.  It's safe to take a fault with a
// bad RSP, because on Linux we can use sigaltstack and on OS X we're already
// on a separate thread.

// On Windows, the OS gets upset if RSP doesn't work, and I don't know any
// equivalent of sigaltstack.  Windows supports guard pages which, when
// accessed, immediately turn into regular pages but cause a trap... but
// putting them in the path of RSP just leads to something (in the kernel?)
// thinking a regular stack extension is required.  So this protection is not
// supported on Windows yet...

enum
{
	STACK_SIZE = 2 * 1024 * 1024,
	SAFE_STACK_SIZE = 512 * 1024,
	GUARD_SIZE = 0x10000, // two guards - bottom (permanent) and middle (see above)
	GUARD_OFFSET = STACK_SIZE - SAFE_STACK_SIZE - GUARD_SIZE,
};

void Jit64::AllocStack()
{
#ifndef _WIN32
	m_stack = (u8*)AllocateMemoryPages(STACK_SIZE);
	ReadProtectMemory(m_stack, GUARD_SIZE);
	ReadProtectMemory(m_stack + GUARD_OFFSET, GUARD_SIZE);
#endif
}

void Jit64::FreeStack()
{
#ifndef _WIN32
	if (m_stack)
	{
		FreeMemoryPages(m_stack, STACK_SIZE);
		m_stack = NULL;
	}
#endif
}

bool Jit64::HandleFault(uintptr_t access_address, SContext* ctx)
{
	uintptr_t stack = (uintptr_t)m_stack, diff = access_address - stack;
	// In the trap region?
	if (stack && diff >= GUARD_OFFSET && diff < GUARD_OFFSET + GUARD_SIZE)
	{
		WARN_LOG(POWERPC, "BLR cache disabled due to excessive BL in the emulated program.");
		m_enable_blr_optimization = false;
		UnWriteProtectMemory(m_stack + GUARD_OFFSET, GUARD_SIZE);
		// We're going to need to clear the whole cache to get rid of the bad
		// CALLs, but we can't yet.  Fake the downcount so we're forced to the
		// dispatcher (no block linking), and clear the cache so we're sent to
		// Jit.  Yeah, it's kind of gross.
		GetBlockCache()->InvalidateICache(0, 0xffffffff);
		CoreTiming::ForceExceptionCheck(0);
		m_clear_cache_asap = true;

		return true;
	}

	return Jitx86Base::HandleFault(access_address, ctx);
}



void Jit64::Init()
{
	jo.optimizeStack = true;
	jo.enableBlocklink = true;
	if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bJITBlockLinking ||
		 SConfig::GetInstance().m_LocalCoreStartupParameter.bMMU)
	{
		// TODO: support block linking with MMU
		jo.enableBlocklink = false;
	}
	jo.fpAccurateFcmp = SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableFPRF;
	jo.optimizeGatherPipe = true;
	jo.fastInterrupts = false;
	jo.accurateSinglePrecision = true;
	js.memcheck = SConfig::GetInstance().m_LocalCoreStartupParameter.bMMU;

	gpr.SetEmitter(this);
	fpr.SetEmitter(this);

	trampolines.Init();
	AllocCodeSpace(CODE_SIZE);

	// BLR optimization has the same consequences as block linking, as well as
	// depending on the fault handler to be safe in the event of excessive BL.
	m_enable_blr_optimization = jo.enableBlocklink && SConfig::GetInstance().m_LocalCoreStartupParameter.bFastmem;
	m_clear_cache_asap = false;

	m_stack = nullptr;
	if (m_enable_blr_optimization)
		AllocStack();

	blocks.Init();
	asm_routines.Init(m_stack ? (m_stack + STACK_SIZE) : nullptr);

	// important: do this *after* generating the global asm routines, because we can't use farcode in them.
	// it'll crash because the farcode functions get cleared on JIT clears.
	farcode.Init(js.memcheck ? FARCODE_SIZE_MMU : FARCODE_SIZE);

	code_block.m_stats = &js.st;
	code_block.m_gpa = &js.gpa;
	code_block.m_fpa = &js.fpa;
	analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE);
	analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_BRANCH_MERGE);
	analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_CARRY_MERGE);
}

void Jit64::ClearCache()
{
	blocks.Clear();
	trampolines.ClearCodeSpace();
	farcode.ClearCodeSpace();
	ClearCodeSpace();
}

void Jit64::Shutdown()
{
	FreeStack();
	FreeCodeSpace();

	blocks.Shutdown();
	trampolines.Shutdown();
	asm_routines.Shutdown();
	farcode.Shutdown();
}

// This is only called by FallBackToInterpreter() in this file. It will execute an instruction with the interpreter functions.
void Jit64::WriteCallInterpreter(UGeckoInstruction inst)
{
	gpr.Flush();
	fpr.Flush();
	if (js.isLastInstruction)
	{
		MOV(32, PPCSTATE(pc), Imm32(js.compilerPC));
		MOV(32, PPCSTATE(npc), Imm32(js.compilerPC + 4));
	}
	Interpreter::_interpreterInstruction instr = GetInterpreterOp(inst);
	ABI_PushRegistersAndAdjustStack(0, 0);
	ABI_CallFunctionC((void*)instr, inst.hex);
	ABI_PopRegistersAndAdjustStack(0, 0);
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
	gpr.Flush();
	fpr.Flush();
	ABI_PushRegistersAndAdjustStack(0, 0);
	ABI_CallFunctionCC((void*)&HLE::Execute, js.compilerPC, _inst.hex);
	ABI_PopRegistersAndAdjustStack(0, 0);
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
			f.Open("log64.txt", "w");

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

bool Jit64::Cleanup()
{
	bool did_something = false;

	if (jo.optimizeGatherPipe && js.fifoBytesThisBlock > 0)
	{
		ABI_PushRegistersAndAdjustStack(0, 0);
		ABI_CallFunction((void *)&GPFifo::CheckGatherPipe);
		ABI_PopRegistersAndAdjustStack(0, 0);
		did_something = true;
	}

	// SPEED HACK: MMCR0/MMCR1 should be checked at run-time, not at compile time.
	if (MMCR0.Hex || MMCR1.Hex)
	{
		ABI_PushRegistersAndAdjustStack(0, 0);
		ABI_CallFunctionCCC((void *)&PowerPC::UpdatePerformanceMonitor, js.downcountAmount, jit->js.numLoadStoreInst, jit->js.numFloatingPointInst);
		ABI_PopRegistersAndAdjustStack(0, 0);
		did_something = true;
	}

	return did_something;
}

void Jit64::WriteExit(u32 destination, bool bl, u32 after)
{
	if (!m_enable_blr_optimization)
		bl = false;

	Cleanup();

	if (bl)
	{
		MOV(32, R(RSCRATCH2), Imm32(after));
		PUSH(RSCRATCH2);
	}

	SUB(32, PPCSTATE(downcount), Imm32(js.downcountAmount));

	JustWriteExit(destination, bl, after);
}

void Jit64::JustWriteExit(u32 destination, bool bl, u32 after)
{
	//If nobody has taken care of this yet (this can be removed when all branches are done)
	JitBlock *b = js.curBlock;
	JitBlock::LinkData linkData;
	linkData.exitAddress = destination;
	linkData.linkStatus = false;

	// Link opportunity!
	int block;
	if (jo.enableBlocklink && (block = blocks.GetBlockNumberFromStartAddress(destination)) >= 0)
	{
		// It exists! Joy of joy!
		JitBlock* jb = blocks.GetBlock(block);
		const u8* addr = jb->checkedEntry;
		linkData.exitPtrs = GetWritableCodePtr();
		if (bl)
			CALL(addr);
		else
			JMP(addr, true);
		linkData.linkStatus = true;
	}
	else
	{
		MOV(32, PPCSTATE(pc), Imm32(destination));
		linkData.exitPtrs = GetWritableCodePtr();
		if (bl)
			CALL(asm_routines.dispatcher);
		else
			JMP(asm_routines.dispatcher, true);
	}

	b->linkData.push_back(linkData);

	if (bl)
	{
		POP(RSCRATCH);
		JustWriteExit(after, false, 0);
	}
}

void Jit64::WriteExitDestInRSCRATCH(bool bl, u32 after)
{
	if (!m_enable_blr_optimization)
		bl = false;
	MOV(32, PPCSTATE(pc), R(RSCRATCH));
	Cleanup();

	if (bl)
	{
		MOV(32, R(RSCRATCH2), Imm32(after));
		PUSH(RSCRATCH2);
	}

	SUB(32, PPCSTATE(downcount), Imm32(js.downcountAmount));
	if (bl)
	{
		CALL(asm_routines.dispatcher);
		POP(RSCRATCH);
		JustWriteExit(after, false, 0);
	}
	else
	{
		JMP(asm_routines.dispatcher, true);
	}
}

void Jit64::WriteBLRExit()
{
	if (!m_enable_blr_optimization)
	{
		WriteExitDestInRSCRATCH();
		return;
	}
	MOV(32, PPCSTATE(pc), R(RSCRATCH));
	bool disturbed = Cleanup();
	if (disturbed)
		MOV(32, R(RSCRATCH), PPCSTATE(pc));
	CMP(64, R(RSCRATCH), MDisp(RSP, 8));
	MOV(32, R(RSCRATCH), Imm32(js.downcountAmount));
	J_CC(CC_NE, asm_routines.dispatcherMispredictedBLR);
	SUB(32, PPCSTATE(downcount), R(RSCRATCH));
	RET();
}

void Jit64::WriteRfiExitDestInRSCRATCH()
{
	MOV(32, PPCSTATE(pc), R(RSCRATCH));
	MOV(32, PPCSTATE(npc), R(RSCRATCH));
	Cleanup();
	ABI_PushRegistersAndAdjustStack(0, 0);
	ABI_CallFunction(reinterpret_cast<void *>(&PowerPC::CheckExceptions));
	ABI_PopRegistersAndAdjustStack(0, 0);
	SUB(32, PPCSTATE(downcount), Imm32(js.downcountAmount));
	JMP(asm_routines.dispatcher, true);
}

void Jit64::WriteExceptionExit()
{
	Cleanup();
	MOV(32, R(RSCRATCH), PPCSTATE(pc));
	MOV(32, PPCSTATE(npc), R(RSCRATCH));
	ABI_PushRegistersAndAdjustStack(0, 0);
	ABI_CallFunction(reinterpret_cast<void *>(&PowerPC::CheckExceptions));
	ABI_PopRegistersAndAdjustStack(0, 0);
	SUB(32, PPCSTATE(downcount), Imm32(js.downcountAmount));
	JMP(asm_routines.dispatcher, true);
}

void Jit64::WriteExternalExceptionExit()
{
	Cleanup();
	MOV(32, R(RSCRATCH), PPCSTATE(pc));
	MOV(32, PPCSTATE(npc), R(RSCRATCH));
	ABI_PushRegistersAndAdjustStack(0, 0);
	ABI_CallFunction(reinterpret_cast<void *>(&PowerPC::CheckExternalExceptions));
	ABI_PopRegistersAndAdjustStack(0, 0);
	SUB(32, PPCSTATE(downcount), Imm32(js.downcountAmount));
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

void STACKALIGN Jit64::Jit(u32 em_address)
{
	if (GetSpaceLeft() < 0x10000 ||
	    farcode.GetSpaceLeft() < 0x10000 ||
		blocks.IsFull() ||
		SConfig::GetInstance().m_LocalCoreStartupParameter.bJITNoBlockCache ||
		m_clear_cache_asap)
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

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableDebugging)
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
	MOV(32, PPCSTATE(pc), Imm32(js.blockStart));
	JMP(asm_routines.doTiming, true);  // downcount hit zero - go doTiming.
	SetJumpTarget(skip);

	const u8 *normalEntry = GetCodePtr();
	b->normalEntry = normalEntry;

	if (ImHereDebug)
	{
		ABI_PushRegistersAndAdjustStack(0, 0);
		ABI_CallFunction((void *)&ImHere); //Used to get a trace of the last few blocks before a crash, sometimes VERY useful
		ABI_PopRegistersAndAdjustStack(0, 0);
	}

	// Conditionally add profiling code.
	if (Profiler::g_ProfileBlocks)
	{
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
	MOV(32, PPCSTATE(pc), Imm32(js.blockStart));
#endif

	// Start up the register allocators
	// They use the information in gpa/fpa to preload commonly used registers.
	gpr.Start();
	fpr.Start();

	js.downcountAmount = 0;
	if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableDebugging)
		js.downcountAmount += PatchEngine::GetSpeedhackCycles(code_block.m_address);

	js.skipnext = false;
	js.carryFlagSet = false;
	js.carryFlagInverted = false;
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
				PROFILER_ADD_DIFF_LARGE_INTEGER(&b->ticCounter, &b->ticStop, &b->ticStart);
				PROFILER_VPOP;
			}
		}
		else
		{
			// help peephole optimizations
			js.next_inst = ops[i + 1].inst;
			js.next_compilerPC = ops[i + 1].address;
			js.next_op = &ops[i + 1];
		}

		if (jo.optimizeGatherPipe && js.fifoBytesThisBlock >= 32)
		{
			js.fifoBytesThisBlock -= 32;
			MOV(32, PPCSTATE(pc), Imm32(jit->js.compilerPC)); // Helps external systems know which instruction triggered the write
			u32 registersInUse = CallerSavedRegistersInUse();
			ABI_PushRegistersAndAdjustStack(registersInUse, 0);
			ABI_CallFunction((void *)&GPFifo::CheckGatherPipe);
			ABI_PopRegistersAndAdjustStack(registersInUse, 0);
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
						MOV(32, R(RSCRATCH), PPCSTATE(npc));
						js.downcountAmount += js.st.numCycles;
						WriteExitDestInRSCRATCH();
						break;
					}
				}
			}
		}

		if (!ops[i].skip)
		{
			if ((opinfo->flags & FL_USE_FPU) && !js.firstFPInstructionFound)
			{
				//This instruction uses FPU - needs to add FP exception bailout
				TEST(32, PPCSTATE(msr), Imm32(1 << 13)); // Test FP enabled bit
				FixupBranch b1 = J_CC(CC_Z, true);
				SwitchToFarCode();
				SetJumpTarget(b1);
				gpr.Flush(FLUSH_MAINTAIN_STATE);
				fpr.Flush(FLUSH_MAINTAIN_STATE);

				// If a FPU exception occurs, the exception handler will read
				// from PC.  Update PC with the latest value in case that happens.
				MOV(32, PPCSTATE(pc), Imm32(ops[i].address));
				OR(32, PPCSTATE(Exceptions), Imm32(EXCEPTION_FPU_UNAVAILABLE));
				WriteExceptionExit();

				SwitchToNearCode();
				js.firstFPInstructionFound = true;
			}

			// Add an external exception check if the instruction writes to the FIFO.
			if (jit->js.fifoWriteAddresses.find(ops[i].address) != jit->js.fifoWriteAddresses.end())
			{
				TEST(32, PPCSTATE(Exceptions), Imm32(EXCEPTION_ISI | EXCEPTION_PROGRAM | EXCEPTION_SYSCALL | EXCEPTION_FPU_UNAVAILABLE | EXCEPTION_DSI | EXCEPTION_ALIGNMENT));
				FixupBranch clearInt = J_CC(CC_NZ);
				TEST(32, PPCSTATE(Exceptions), Imm32(EXCEPTION_EXTERNAL_INT));
				FixupBranch extException = J_CC(CC_NZ, true);
				SwitchToFarCode();
				SetJumpTarget(extException);
				TEST(32, PPCSTATE(msr), Imm32(0x0008000));
				FixupBranch noExtIntEnable = J_CC(CC_Z, true);
				TEST(32, M((void *)&ProcessorInterface::m_InterruptCause), Imm32(ProcessorInterface::INT_CAUSE_CP | ProcessorInterface::INT_CAUSE_PE_TOKEN | ProcessorInterface::INT_CAUSE_PE_FINISH));
				FixupBranch noCPInt = J_CC(CC_Z, true);

				gpr.Flush(FLUSH_MAINTAIN_STATE);
				fpr.Flush(FLUSH_MAINTAIN_STATE);

				MOV(32, PPCSTATE(pc), Imm32(ops[i].address));
				WriteExternalExceptionExit();

				SwitchToNearCode();

				SetJumpTarget(noCPInt);
				SetJumpTarget(noExtIntEnable);
				SetJumpTarget(clearInt);
			}

			if (SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableDebugging && breakpoints.IsAddressBreakPoint(ops[i].address) && GetState() != CPU_STEPPING)
			{
				gpr.Flush();
				fpr.Flush();

				MOV(32, PPCSTATE(pc), Imm32(ops[i].address));
				ABI_PushRegistersAndAdjustStack(0, 0);
				ABI_CallFunction(reinterpret_cast<void *>(&PowerPC::CheckBreakPoints));
				ABI_PopRegistersAndAdjustStack(0, 0);
				TEST(32, M((void*)PowerPC::GetStatePtr()), Imm32(0xFFFFFFFF));
				FixupBranch noBreakpoint = J_CC(CC_Z);

				WriteExit(ops[i].address);
				SetJumpTarget(noBreakpoint);
			}

			Jit64Tables::CompileInstruction(ops[i]);

			// If we have a register that will never be used again, flush it.
			for (int j = 0; j < 32; j++)
			{
				if (!(ops[i].gprInUse & (1 << j)))
					gpr.StoreFromRegister(j);
				if (!(ops[i].fprInUse & (1 << j)))
					fpr.StoreFromRegister(j);
			}

			if (js.memcheck && (opinfo->flags & FL_LOADSTORE))
			{
				TEST(32, PPCSTATE(Exceptions), Imm32(EXCEPTION_DSI));
				FixupBranch memException = J_CC(CC_NZ, true);

				SwitchToFarCode();
				SetJumpTarget(memException);

				gpr.Flush(FLUSH_MAINTAIN_STATE);
				fpr.Flush(FLUSH_MAINTAIN_STATE);

				// If a memory exception occurs, the exception handler will read
				// from PC.  Update PC with the latest value in case that happens.
				MOV(32, PPCSTATE(pc), Imm32(ops[i].address));
				WriteExceptionExit();
				SwitchToNearCode();
			}

			if (opinfo->flags & FL_LOADSTORE)
				++jit->js.numLoadStoreInst;

			if (opinfo->flags & FL_USE_FPU)
				++jit->js.numFloatingPointInst;
		}

#if defined(_DEBUG) || defined(DEBUGFAST)
		if (gpr.SanityCheck() || fpr.SanityCheck())
		{
			std::string ppc_inst = GekkoDisassembler::Disassemble(ops[i].inst.hex, em_address);
			//NOTICE_LOG(DYNA_REC, "Unflushed register: %s", ppc_inst.c_str());
		}
#endif
		if (js.skipnext)
		{
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
		MOV(32, PPCSTATE(npc), Imm32(js.compilerPC));

		OR(32, PPCSTATE(Exceptions), Imm32(EXCEPTION_ISI));

		// Remove the invalid instruction from the icache, forcing a recompile
		MOV(64, R(RSCRATCH), ImmPtr(jit->GetBlockCache()->GetICachePtr(js.compilerPC)));
		MOV(32,MatR(RSCRATCH),Imm32(JIT_ICACHE_INVALID_WORD));

		WriteExceptionExit();
	}

	if (code_block.m_broken)
	{
		gpr.Flush();
		fpr.Flush();
		WriteExit(nextPC);
	}

	b->codeSize = (u32)(GetCodePtr() - normalEntry);
	b->originalSize = code_block.m_num_instructions;

#ifdef JIT_LOG_X86
	LogGeneratedX86(code_block.m_num_instructions, code_buf, normalEntry, b);
#endif

	return normalEntry;
}

u32 Jit64::CallerSavedRegistersInUse()
{
	u32 result = 0;
	for (int i = 0; i < NUMXREGS; i++)
	{
		if (!gpr.IsFreeX(i))
			result |= (1 << i);
		if (!fpr.IsFreeX(i))
			result |= (1 << (16 + i));
	}
	return result & ABI_ALL_CALLER_SAVED;
}
