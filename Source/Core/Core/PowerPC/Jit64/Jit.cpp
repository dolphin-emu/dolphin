// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
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
#include "Core/PowerPC/JitInterface.h"
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
	m_stack = static_cast<u8*>(Common::AllocateMemoryPages(STACK_SIZE));
	Common::ReadProtectMemory(m_stack, GUARD_SIZE);
	Common::ReadProtectMemory(m_stack + GUARD_OFFSET, GUARD_SIZE);
#endif
}

void Jit64::FreeStack()
{
#ifndef _WIN32
	if (m_stack)
	{
		Common::FreeMemoryPages(m_stack, STACK_SIZE);
		m_stack = nullptr;
	}
#endif
}

bool Jit64::HandleFault(uintptr_t access_address, SContext* ctx)
{
	uintptr_t stack = (uintptr_t)m_stack, diff = access_address - stack;
	// In the trap region?
	if (m_enable_blr_optimization && diff >= GUARD_OFFSET && diff < GUARD_OFFSET + GUARD_SIZE)
	{
		WARN_LOG(POWERPC, "BLR cache disabled due to excessive BL in the emulated program.");
		m_enable_blr_optimization = false;
		Common::UnWriteProtectMemory(m_stack + GUARD_OFFSET, GUARD_SIZE);
		// We're going to need to clear the whole cache to get rid of the bad
		// CALLs, but we can't yet.  Fake the downcount so we're forced to the
		// dispatcher (no block linking), and clear the cache so we're sent to
		// Jit.  Yeah, it's kind of gross.
		GetBlockCache()->InvalidateICache(0, 0xffffffff, true);
		CoreTiming::ForceExceptionCheck(0);
		m_clear_cache_asap = true;

		return true;
	}

	return Jitx86Base::HandleFault(access_address, ctx);
}



void Jit64::Init()
{
	EnableBlockLink();

	jo.optimizeGatherPipe = true;
	jo.accurateSinglePrecision = true;
	UpdateMemoryOptions();
	js.fastmemLoadStore = nullptr;
	js.compilerPC = 0;

	gpr.SetEmitter(this);
	fpr.SetEmitter(this);

	trampolines.Init(jo.memcheck ? TRAMPOLINE_CODE_SIZE_MMU : TRAMPOLINE_CODE_SIZE);
	AllocCodeSpace(CODE_SIZE);

	// BLR optimization has the same consequences as block linking, as well as
	// depending on the fault handler to be safe in the event of excessive BL.
	m_enable_blr_optimization = jo.enableBlocklink && SConfig::GetInstance().bFastmem && !SConfig::GetInstance().bEnableDebugging;
	m_clear_cache_asap = false;

	m_stack = nullptr;
	if (m_enable_blr_optimization)
		AllocStack();

	blocks.Init();
	asm_routines.Init(m_stack ? (m_stack + STACK_SIZE) : nullptr);

	// important: do this *after* generating the global asm routines, because we can't use farcode in them.
	// it'll crash because the farcode functions get cleared on JIT clears.
	farcode.Init(jo.memcheck ? FARCODE_SIZE_MMU : FARCODE_SIZE);

	code_block.m_stats = &js.st;
	code_block.m_gpa = &js.gpa;
	code_block.m_fpa = &js.fpa;
	EnableOptimization();
}

void Jit64::ClearCache()
{
	blocks.Clear();
	trampolines.ClearCodeSpace();
	farcode.ClearCodeSpace();
	ClearCodeSpace();
	UpdateMemoryOptions();
	m_clear_cache_asap = false;
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

void Jit64::FallBackToInterpreter(UGeckoInstruction inst)
{
	gpr.Flush();
	fpr.Flush();
	if (js.op->opinfo->flags & FL_ENDBLOCK)
	{
		MOV(32, PPCSTATE(pc), Imm32(js.compilerPC));
		MOV(32, PPCSTATE(npc), Imm32(js.compilerPC + 4));
	}
	Interpreter::Instruction instr = GetInterpreterOp(inst);
	ABI_PushRegistersAndAdjustStack({}, 0);
	ABI_CallFunctionC((void*)instr, inst.hex);
	ABI_PopRegistersAndAdjustStack({}, 0);
	if (js.op->opinfo->flags & FL_ENDBLOCK)
	{
		if (js.isLastInstruction)
		{
			MOV(32, R(RSCRATCH), PPCSTATE(npc));
			MOV(32, PPCSTATE(pc), R(RSCRATCH));
			WriteExceptionExit();
		}
		else
		{
			MOV(32, R(RSCRATCH), PPCSTATE(npc));
			CMP(32, R(RSCRATCH), Imm32(js.compilerPC + 4));
			FixupBranch c = J_CC(CC_Z);
			MOV(32, PPCSTATE(pc), R(RSCRATCH));
			WriteExceptionExit();
			SetJumpTarget(c);
		}
	}
}

void Jit64::HLEFunction(UGeckoInstruction _inst)
{
	gpr.Flush();
	fpr.Flush();
	ABI_PushRegistersAndAdjustStack({}, 0);
	ABI_CallFunctionCC((void*)&HLE::Execute, js.compilerPC, _inst.hex);
	ABI_PopRegistersAndAdjustStack({}, 0);
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
		ABI_PushRegistersAndAdjustStack({}, 0);
		ABI_CallFunction((void *)&GPFifo::FastCheckGatherPipe);
		ABI_PopRegistersAndAdjustStack({}, 0);
		did_something = true;
	}

	// SPEED HACK: MMCR0/MMCR1 should be checked at run-time, not at compile time.
	if (MMCR0.Hex || MMCR1.Hex)
	{
		ABI_PushRegistersAndAdjustStack({}, 0);
		ABI_CallFunctionCCC((void *)&PowerPC::UpdatePerformanceMonitor, js.downcountAmount, js.numLoadStoreInst, js.numFloatingPointInst);
		ABI_PopRegistersAndAdjustStack({}, 0);
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
	MOV(32, R(RSCRATCH2), Imm32(js.downcountAmount));
	CMP(64, R(RSCRATCH), MDisp(RSP, 8));
	J_CC(CC_NE, asm_routines.dispatcherMispredictedBLR);
	SUB(32, PPCSTATE(downcount), R(RSCRATCH2));
	RET();
}

void Jit64::WriteRfiExitDestInRSCRATCH()
{
	MOV(32, PPCSTATE(pc), R(RSCRATCH));
	MOV(32, PPCSTATE(npc), R(RSCRATCH));
	Cleanup();
	ABI_PushRegistersAndAdjustStack({}, 0);
	ABI_CallFunction(reinterpret_cast<void *>(&PowerPC::CheckExceptions));
	ABI_PopRegistersAndAdjustStack({}, 0);
	SUB(32, PPCSTATE(downcount), Imm32(js.downcountAmount));
	JMP(asm_routines.dispatcher, true);
}

void Jit64::WriteExceptionExit()
{
	Cleanup();
	MOV(32, R(RSCRATCH), PPCSTATE(pc));
	MOV(32, PPCSTATE(npc), R(RSCRATCH));
	ABI_PushRegistersAndAdjustStack({}, 0);
	ABI_CallFunction(reinterpret_cast<void *>(&PowerPC::CheckExceptions));
	ABI_PopRegistersAndAdjustStack({}, 0);
	SUB(32, PPCSTATE(downcount), Imm32(js.downcountAmount));
	JMP(asm_routines.dispatcher, true);
}

void Jit64::WriteExternalExceptionExit()
{
	Cleanup();
	MOV(32, R(RSCRATCH), PPCSTATE(pc));
	MOV(32, PPCSTATE(npc), R(RSCRATCH));
	ABI_PushRegistersAndAdjustStack({}, 0);
	ABI_CallFunction(reinterpret_cast<void *>(&PowerPC::CheckExternalExceptions));
	ABI_PopRegistersAndAdjustStack({}, 0);
	SUB(32, PPCSTATE(downcount), Imm32(js.downcountAmount));
	JMP(asm_routines.dispatcher, true);
}

void Jit64::Run()
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

void Jit64::Jit(u32 em_address)
{
	if (IsAlmostFull() || farcode.IsAlmostFull() || trampolines.IsAlmostFull() ||
	    blocks.IsFull() ||
	    SConfig::GetInstance().bJITNoBlockCache ||
	    m_clear_cache_asap)
	{
		ClearCache();
	}

	int blockSize = code_buffer.GetSize();

	if (SConfig::GetInstance().bEnableDebugging)
	{
		// We can link blocks as long as we are not single stepping and there are no breakpoints here
		EnableBlockLink();
		EnableOptimization();

		// Comment out the following to disable breakpoints (speed-up)
		if (!Profiler::g_ProfileBlocks)
		{
			if (GetState() == CPU_STEPPING)
			{
				blockSize = 1;

				// Do not link this block to other blocks While single stepping
				jo.enableBlocklink = false;
				analyzer.ClearOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE);
				analyzer.ClearOption(PPCAnalyst::PPCAnalyzer::OPTION_BRANCH_MERGE);
				analyzer.ClearOption(PPCAnalyst::PPCAnalyzer::OPTION_CROR_MERGE);
				analyzer.ClearOption(PPCAnalyst::PPCAnalyzer::OPTION_CARRY_MERGE);
			}
			Trace();
		}
	}

	// Analyze the block, collect all instructions it is made of (including inlining,
	// if that is enabled), reorder instructions for optimal performance, and join joinable instructions.
	u32 nextPC = analyzer.Analyze(em_address, &code_block, &code_buffer, blockSize);

	if (code_block.m_memory_exception)
	{
		// Address of instruction could not be translated
		NPC = nextPC;
		PowerPC::ppcState.Exceptions |= EXCEPTION_ISI;
		PowerPC::CheckExceptions();
		WARN_LOG(POWERPC, "ISI exception at 0x%08x", nextPC);
		return;
	}

	int block_num = blocks.AllocateBlock(em_address);
	JitBlock *b = blocks.GetBlock(block_num);
	blocks.FinalizeBlock(block_num, jo.enableBlocklink, DoJit(em_address, &code_buffer, b, nextPC));
}

const u8* Jit64::DoJit(u32 em_address, PPCAnalyst::CodeBuffer *code_buf, JitBlock *b, u32 nextPC)
{
	js.firstFPInstructionFound = false;
	js.isLastInstruction = false;
	js.blockStart = em_address;
	js.fifoBytesThisBlock = 0;
	js.curBlock = b;
	js.numLoadStoreInst = 0;
	js.numFloatingPointInst = 0;

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
		ABI_PushRegistersAndAdjustStack({}, 0);
		ABI_CallFunction((void *)&ImHere); //Used to get a trace of the last few blocks before a crash, sometimes VERY useful
		ABI_PopRegistersAndAdjustStack({}, 0);
	}

	// Conditionally add profiling code.
	if (Profiler::g_ProfileBlocks)
	{
		MOV(64, R(RSCRATCH), Imm64((u64)&b->runCount));
		ADD(32, MatR(RSCRATCH), Imm8(1));
		b->ticCounter = 0;
		b->ticStart = 0;
		b->ticStop = 0;
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
	if (!SConfig::GetInstance().bEnableDebugging)
		js.downcountAmount += PatchEngine::GetSpeedhackCycles(code_block.m_address);

	js.skipInstructions = 0;
	js.carryFlagSet = false;
	js.carryFlagInverted = false;
	js.assumeNoPairedQuantize = false;

	// If the block only uses one GQR and the GQR is zero at compile time, make a guess that the block
	// never uses quantized loads/stores. Many paired-heavy games use largely float loads and stores,
	// which are significantly faster when inlined (especially in MMU mode, where this lets them use
	// fastmem).
	// Insert a check that the GQR is still zero at the start of the block in case our guess turns out
	// wrong.
	// TODO: support any other constant GQR value, not merely zero/unquantized: we can optimize quantized
	// loadstores too, it'd just be more code.
	if (code_block.m_gqr_used.Count() == 1 && js.pairedQuantizeAddresses.find(js.blockStart) == js.pairedQuantizeAddresses.end())
	{
		int gqr = *code_block.m_gqr_used.begin();
		if (!code_block.m_gqr_modified[gqr] && !GQR(gqr))
		{
			CMP(32, PPCSTATE(spr[SPR_GQR0 + gqr]), Imm8(0));
			FixupBranch failure = J_CC(CC_NZ, true);
			SwitchToFarCode();
				SetJumpTarget(failure);
				MOV(32, PPCSTATE(pc), Imm32(js.blockStart));
				ABI_PushRegistersAndAdjustStack({}, 0);
				ABI_CallFunctionC((void *)&JitInterface::CompileExceptionCheck,
				                  (u32)JitInterface::ExceptionType::EXCEPTIONS_PAIRED_QUANTIZE);
				ABI_PopRegistersAndAdjustStack({}, 0);
				JMP(asm_routines.dispatcher, true);
			SwitchToNearCode();
			js.assumeNoPairedQuantize = true;
		}
	}

	// Translate instructions
	for (u32 i = 0; i < code_block.m_num_instructions; i++)
	{
		js.compilerPC = ops[i].address;
		js.op = &ops[i];
		js.instructionNumber = i;
		js.instructionsLeft = (code_block.m_num_instructions - 1) - i;
		const GekkoOPInfo *opinfo = ops[i].opinfo;
		js.downcountAmount += opinfo->numCycles;
		js.fastmemLoadStore = nullptr;
		js.fixupExceptionHandler = false;
		js.revertGprLoad = -1;
		js.revertFprLoad = -1;

		if (i == (code_block.m_num_instructions - 1))
		{
			if (Profiler::g_ProfileBlocks)
			{
				// WARNING - cmp->branch merging will screw this up.
				PROFILER_VPUSH;
				// get end tic
				PROFILER_QUERY_PERFORMANCE_COUNTER(&b->ticStop);
				// tic counter += (end tic - start tic)
				PROFILER_UPDATE_TIME(b);
				PROFILER_VPOP;
			}
			js.isLastInstruction = true;
		}

		// Gather pipe writes using a non-immediate address are discovered by profiling.
		bool gatherPipeIntCheck = js.fifoWriteAddresses.find(ops[i].address) != js.fifoWriteAddresses.end();

		// Gather pipe writes using an immediate address are explicitly tracked.
		if (jo.optimizeGatherPipe && js.fifoBytesThisBlock >= 32)
		{
			js.fifoBytesThisBlock -= 32;
			BitSet32 registersInUse = CallerSavedRegistersInUse();
			ABI_PushRegistersAndAdjustStack(registersInUse, 0);
			ABI_CallFunction((void *)&GPFifo::FastCheckGatherPipe);
			ABI_PopRegistersAndAdjustStack(registersInUse, 0);
			gatherPipeIntCheck = true;
		}

		// Gather pipe writes can generate an exception; add an exception check.
		// TODO: This doesn't really match hardware; the CP interrupt is
		// asynchronous.
		if (gatherPipeIntCheck)
		{
			TEST(32, PPCSTATE(Exceptions), Imm32(EXCEPTION_EXTERNAL_INT));
			FixupBranch extException = J_CC(CC_NZ, true);

			SwitchToFarCode();
				SetJumpTarget(extException);
				TEST(32, PPCSTATE(msr), Imm32(0x0008000));
				FixupBranch noExtIntEnable = J_CC(CC_Z, true);
				TEST(32, M(&ProcessorInterface::m_InterruptCause), Imm32(ProcessorInterface::INT_CAUSE_CP |
				                                                         ProcessorInterface::INT_CAUSE_PE_TOKEN |
				                                                         ProcessorInterface::INT_CAUSE_PE_FINISH));
				FixupBranch noCPInt = J_CC(CC_Z, true);

				gpr.Flush(FLUSH_MAINTAIN_STATE);
				fpr.Flush(FLUSH_MAINTAIN_STATE);

				MOV(32, PPCSTATE(pc), Imm32(ops[i].address));
				WriteExternalExceptionExit();
			SwitchToNearCode();

			SetJumpTarget(noCPInt);
			SetJumpTarget(noExtIntEnable);
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

			if (SConfig::GetInstance().bEnableDebugging && breakpoints.IsAddressBreakPoint(ops[i].address) && GetState() != CPU_STEPPING)
			{
				// Turn off block linking if there are breakpoints so that the Step Over command does not link this block.
				jo.enableBlocklink = false;

				gpr.Flush();
				fpr.Flush();

				MOV(32, PPCSTATE(pc), Imm32(ops[i].address));
				ABI_PushRegistersAndAdjustStack({}, 0);
				ABI_CallFunction(reinterpret_cast<void *>(&PowerPC::CheckBreakPoints));
				ABI_PopRegistersAndAdjustStack({}, 0);
				TEST(32, M(PowerPC::GetStatePtr()), Imm32(0xFFFFFFFF));
				FixupBranch noBreakpoint = J_CC(CC_Z);

				WriteExit(ops[i].address);
				SetJumpTarget(noBreakpoint);
			}

			// If we have an input register that is going to be used again, load it pre-emptively,
			// even if the instruction doesn't strictly need it in a register, to avoid redundant
			// loads later. Of course, don't do this if we're already out of registers.
			// As a bit of a heuristic, make sure we have at least one register left over for the
			// output, which needs to be bound in the actual instruction compilation.
			// TODO: make this smarter in the case that we're actually register-starved, i.e.
			// prioritize the more important registers.
			for (int reg : ops[i].regsIn)
			{
				if (gpr.NumFreeRegisters() < 2)
					break;
				if (ops[i].gprInReg[reg] && !gpr.R(reg).IsImm())
					gpr.BindToRegister(reg, true, false);
			}
			for (int reg : ops[i].fregsIn)
			{
				if (fpr.NumFreeRegisters() < 2)
					break;
				if (ops[i].fprInXmm[reg])
					fpr.BindToRegister(reg, true, false);
			}

			Jit64Tables::CompileInstruction(ops[i]);

			if (jo.memcheck && (opinfo->flags & FL_LOADSTORE))
			{
				// If we have a fastmem loadstore, we can omit the exception check and let fastmem handle it.
				FixupBranch memException;
				_assert_msg_(DYNA_REC, !(js.fastmemLoadStore && js.fixupExceptionHandler),
					"Fastmem loadstores shouldn't have exception handler fixups (PC=%x)!", ops[i].address);
				if (!js.fastmemLoadStore && !js.fixupExceptionHandler)
				{
					TEST(32, PPCSTATE(Exceptions), Imm32(EXCEPTION_DSI));
					memException = J_CC(CC_NZ, true);
				}

				SwitchToFarCode();
					if (!js.fastmemLoadStore)
					{
						exceptionHandlerAtLoc[js.fastmemLoadStore] = nullptr;
						SetJumpTarget(js.fixupExceptionHandler ? js.exceptionHandler : memException);
					}
					else
					{
						exceptionHandlerAtLoc[js.fastmemLoadStore] = GetWritableCodePtr();
					}

					BitSet32 gprToFlush = BitSet32::AllTrue(32);
					BitSet32 fprToFlush = BitSet32::AllTrue(32);
					if (js.revertGprLoad >= 0)
						gprToFlush[js.revertGprLoad] = false;
					if (js.revertFprLoad >= 0)
						fprToFlush[js.revertFprLoad] = false;
					gpr.Flush(FLUSH_MAINTAIN_STATE, gprToFlush);
					fpr.Flush(FLUSH_MAINTAIN_STATE, fprToFlush);

					// If a memory exception occurs, the exception handler will read
					// from PC.  Update PC with the latest value in case that happens.
					MOV(32, PPCSTATE(pc), Imm32(ops[i].address));
					WriteExceptionExit();
				SwitchToNearCode();
			}

			// If we have a register that will never be used again, flush it.
			for (int j : ~ops[i].gprInUse)
				gpr.StoreFromRegister(j);
			for (int j : ~ops[i].fprInUse)
				fpr.StoreFromRegister(j);

			if (opinfo->flags & FL_LOADSTORE)
				++js.numLoadStoreInst;

			if (opinfo->flags & FL_USE_FPU)
				++js.numFloatingPointInst;
		}

#if defined(_DEBUG) || defined(DEBUGFAST)
		if (gpr.SanityCheck() || fpr.SanityCheck())
		{
			std::string ppc_inst = GekkoDisassembler::Disassemble(ops[i].inst.hex, em_address);
			//NOTICE_LOG(DYNA_REC, "Unflushed register: %s", ppc_inst.c_str());
		}
#endif
		i += js.skipInstructions;
		js.skipInstructions = 0;
	}

	if (code_block.m_broken)
	{
		gpr.Flush();
		fpr.Flush();
		WriteExit(nextPC);
	}

	b->codeSize = (u32)(GetCodePtr() - start);
	b->originalSize = code_block.m_num_instructions;

#ifdef JIT_LOG_X86
	LogGeneratedX86(code_block.m_num_instructions, code_buf, start, b);
#endif

	return normalEntry;
}

BitSet32 Jit64::CallerSavedRegistersInUse()
{
	BitSet32 result;
	for (int i = 0; i < NUMXREGS; i++)
	{
		if (!gpr.IsFreeX(i))
			result[i] = true;
		if (!fpr.IsFreeX(i))
			result[16 + i] = true;
	}
	return result & ABI_ALL_CALLER_SAVED;
}

void Jit64::EnableBlockLink()
{
	jo.enableBlocklink = true;
	if (SConfig::GetInstance().bJITNoBlockLinking)
		jo.enableBlocklink = false;
}

void Jit64::EnableOptimization()
{
	analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE);
	analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_BRANCH_MERGE);
	analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_CROR_MERGE);
	analyzer.SetOption(PPCAnalyst::PPCAnalyzer::OPTION_CARRY_MERGE);
}
