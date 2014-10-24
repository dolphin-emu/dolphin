// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cinttypes>
#include <ctime> // For profiling
#include <map>
#include <memory>
#include <string>

#include "Common/Common.h"
#include "Common/StdMakeUnique.h"
#include "Common/StringUtil.h"
#include "Core/PatchEngine.h"
#include "Core/HLE/HLE.h"
#include "Core/PowerPC/Profiler.h"
#include "Core/PowerPC/Jit64IL/JitIL.h"
#include "Core/PowerPC/Jit64IL/JitIL_Tables.h"

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
    (single precision can be used in write gather pipe, specialized fast check added)
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

#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#else
#include <memory>
#include <stdint.h>
#include <x86intrin.h>

#if defined(__clang__)
#if !__has_builtin(__builtin_ia32_rdtsc)
static inline uint64_t __rdtsc()
{
	uint32_t lo, hi;
#ifdef _LP64
	__asm__ __volatile__ ("xorl %%eax,%%eax \n cpuid"
			::: "%rax", "%rbx", "%rcx", "%rdx");
	__asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
	return (uint64_t)hi << 32 | lo;
#else
	__asm__ __volatile__ (
			"xor    %%eax,%%eax;"
			"push   %%ebx;"
			"cpuid;"
			"pop    %%ebx;"
			::: "%eax", "%ecx", "%edx");
	__asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
#endif
	return (uint64_t)hi << 32 | lo;
}
#endif
#endif
#endif

namespace JitILProfiler
{
	struct Block
	{
		u32 index;
		u64 codeHash;
		u64 totalElapsed;
		u64 numberOfCalls;

		Block() : index(0), codeHash(0), totalElapsed(0), numberOfCalls(0)
		{
		}
	};

	static std::vector<Block> blocks;
	static u32 blockIndex;
	static u64 beginTime;
	static Block& Add(u64 codeHash)
	{
		const u32 _blockIndex = (u32)blocks.size();
		blocks.push_back(Block());
		Block& block = blocks.back();
		block.index = _blockIndex;
		block.codeHash = codeHash;
		return block;
	}

	// These functions need to be static because they are called with
	// ABI_CallFunction().
	static void Begin(u32 index)
	{
		blockIndex = index;
		beginTime = __rdtsc();
	}

	static void End()
	{
		const u64 endTime = __rdtsc();
		const u64 duration = endTime - beginTime;
		Block& block = blocks[blockIndex];
		block.totalElapsed += duration;
		++block.numberOfCalls;
	}

	struct JitILProfilerFinalizer
	{
		virtual ~JitILProfilerFinalizer()
		{
			std::string filename = StringFromFormat("JitIL_profiling_%d.csv", (int)time(nullptr));
			File::IOFile file(filename, "w");
			setvbuf(file.GetHandle(), nullptr, _IOFBF, 1024 * 1024);
			fprintf(file.GetHandle(), "code hash,total elapsed,number of calls,elapsed per call\n");
			for (auto& block : blocks)
			{
				const u64 codeHash = block.codeHash;
				const u64 totalElapsed = block.totalElapsed;
				const u64 numberOfCalls = block.numberOfCalls;
				const double elapsedPerCall = totalElapsed / (double)numberOfCalls;
				fprintf(file.GetHandle(), "%016" PRIx64 ",%" PRId64 ",%" PRId64 ",%f\n", codeHash, totalElapsed, numberOfCalls, elapsedPerCall);
			}
		}
	};

	static std::unique_ptr<JitILProfilerFinalizer> finalizer;
	static void Init()
	{
		finalizer = std::make_unique<JitILProfilerFinalizer>();
	}

	static void Shutdown()
	{
		finalizer.reset();
	}
};

void JitIL::Init()
{
	jo.optimizeStack = true;
	EnableBlockLink();

	jo.fpAccurateFcmp = false;
	jo.optimizeGatherPipe = true;
	jo.fastInterrupts = false;
	jo.accurateSinglePrecision = false;
	js.memcheck = SConfig::GetInstance().m_LocalCoreStartupParameter.bMMU;

	trampolines.Init();
	AllocCodeSpace(CODE_SIZE);
	blocks.Init();
	asm_routines.Init(nullptr);

	farcode.Init(js.memcheck ? FARCODE_SIZE_MMU : FARCODE_SIZE);

	code_block.m_stats = &js.st;
	code_block.m_gpa = &js.gpa;
	code_block.m_fpa = &js.fpa;

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bJITILTimeProfiling)
	{
		JitILProfiler::Init();
	}
}

void JitIL::ClearCache()
{
	blocks.Clear();
	trampolines.ClearCodeSpace();
	ClearCodeSpace();
}

void JitIL::Shutdown()
{
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bJITILTimeProfiling)
	{
		JitILProfiler::Shutdown();
	}

	FreeCodeSpace();

	blocks.Shutdown();
	trampolines.Shutdown();
	asm_routines.Shutdown();
	farcode.Shutdown();
}


void JitIL::WriteCallInterpreter(UGeckoInstruction inst)
{
	if (js.isLastInstruction)
	{
		MOV(32, PPCSTATE(pc), Imm32(js.compilerPC));
		MOV(32, PPCSTATE(npc), Imm32(js.compilerPC + 4));
	}
	Interpreter::_interpreterInstruction instr = GetInterpreterOp(inst);
	ABI_CallFunctionC((void*)instr, inst.hex);
	if (js.isLastInstruction)
	{
		MOV(32, R(RSCRATCH), PPCSTATE(npc));
		WriteRfiExitDestInOpArg(R(RSCRATCH));
	}
}

void JitIL::unknown_instruction(UGeckoInstruction inst)
{
	// CCPU::Break();
	PanicAlert("unknown_instruction %08x - Fix me ;)", inst.hex);
}

void JitIL::FallBackToInterpreter(UGeckoInstruction _inst)
{
	ibuild.EmitFallBackToInterpreter(
		ibuild.EmitIntConst(_inst.hex),
		ibuild.EmitIntConst(js.compilerPC));
}

void JitIL::HLEFunction(UGeckoInstruction _inst)
{
	ABI_CallFunctionCC((void*)&HLE::Execute, js.compilerPC, _inst.hex);
	MOV(32, R(RSCRATCH), PPCSTATE(npc));
	WriteExitDestInOpArg(R(RSCRATCH));
}

void JitIL::DoNothing(UGeckoInstruction _inst)
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
			f.Open("log64.txt", "w");
		}
		fprintf(f.GetHandle(), "%08x r0: %08x r5: %08x r6: %08x\n", PC, PowerPC::ppcState.gpr[0],
			PowerPC::ppcState.gpr[5], PowerPC::ppcState.gpr[6]);
		f.Flush();
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

void JitIL::Cleanup()
{
	if (jo.optimizeGatherPipe && js.fifoBytesThisBlock > 0)
	{
		ABI_CallFunction((void *)&GPFifo::CheckGatherPipe);
	}

	// SPEED HACK: MMCR0/MMCR1 should be checked at run-time, not at compile time.
	if (MMCR0.Hex || MMCR1.Hex)
		ABI_CallFunctionCCC((void *)&PowerPC::UpdatePerformanceMonitor, js.downcountAmount, jit->js.numLoadStoreInst, jit->js.numFloatingPointInst);
}

void JitIL::WriteExit(u32 destination)
{
	Cleanup();
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bJITILTimeProfiling)
	{
		ABI_CallFunction((void *)JitILProfiler::End);
	}
	SUB(32, PPCSTATE(downcount), Imm32(js.downcountAmount));

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
		MOV(32, PPCSTATE(pc), Imm32(destination));
		JMP(asm_routines.dispatcher, true);
	}
	b->linkData.push_back(linkData);
}

void JitIL::WriteExitDestInOpArg(const Gen::OpArg& arg)
{
	MOV(32, PPCSTATE(pc), arg);
	Cleanup();
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bJITILTimeProfiling)
	{
		ABI_CallFunction((void *)JitILProfiler::End);
	}
	SUB(32, PPCSTATE(downcount), Imm32(js.downcountAmount));
	JMP(asm_routines.dispatcher, true);
}

void JitIL::WriteRfiExitDestInOpArg(const Gen::OpArg& arg)
{
	MOV(32, PPCSTATE(pc), arg);
	MOV(32, PPCSTATE(npc), arg);
	Cleanup();
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bJITILTimeProfiling)
	{
		ABI_CallFunction((void *)JitILProfiler::End);
	}
	ABI_CallFunction(reinterpret_cast<void *>(&PowerPC::CheckExceptions));
	SUB(32, PPCSTATE(downcount), Imm32(js.downcountAmount));
	JMP(asm_routines.dispatcher, true);
}

void JitIL::WriteExceptionExit()
{
	Cleanup();
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bJITILTimeProfiling)
	{
		ABI_CallFunction((void *)JitILProfiler::End);
	}
	MOV(32, R(EAX), PPCSTATE(pc));
	MOV(32, PPCSTATE(npc), R(EAX));
	ABI_CallFunction(reinterpret_cast<void *>(&PowerPC::CheckExceptions));
	SUB(32, PPCSTATE(downcount), Imm32(js.downcountAmount));
	JMP(asm_routines.dispatcher, true);
}

void JitIL::Run()
{
	CompiledCode pExecAddr = (CompiledCode)asm_routines.enterCode;
	pExecAddr();
	//Will return when PowerPC::state changes
}

void JitIL::SingleStep()
{
	CompiledCode pExecAddr = (CompiledCode)asm_routines.enterCode;
	pExecAddr();
}

void JitIL::Trace()
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

	DEBUG_LOG(DYNA_REC, "JITIL PC: %08x SRR0: %08x SRR1: %08x CRval: %016lx%016lx%016lx%016lx%016lx%016lx%016lx%016lx FPSCR: %08x MSR: %08x LR: %08x %s %s",
		PC, SRR0, SRR1, (unsigned long) PowerPC::ppcState.cr_val[0], (unsigned long) PowerPC::ppcState.cr_val[1], (unsigned long) PowerPC::ppcState.cr_val[2],
		(unsigned long) PowerPC::ppcState.cr_val[3], (unsigned long) PowerPC::ppcState.cr_val[4], (unsigned long) PowerPC::ppcState.cr_val[5],
		(unsigned long) PowerPC::ppcState.cr_val[6], (unsigned long) PowerPC::ppcState.cr_val[7], PowerPC::ppcState.fpscr, PowerPC::ppcState.msr,
		PowerPC::ppcState.spr[8], regs.c_str(), fregs.c_str());
}

void JitIL::Jit(u32 em_address)
{
	if (GetSpaceLeft() < 0x10000 || farcode.GetSpaceLeft() < 0x10000 || blocks.IsFull() ||
		SConfig::GetInstance().m_LocalCoreStartupParameter.bJITNoBlockCache)
	{
		ClearCache();
	}
	int block_num = blocks.AllocateBlock(em_address);
	JitBlock *b = blocks.GetBlock(block_num);
	blocks.FinalizeBlock(block_num, jo.enableBlocklink, DoJit(em_address, &code_buffer, b));
}

const u8* JitIL::DoJit(u32 em_address, PPCAnalyst::CodeBuffer *code_buf, JitBlock *b)
{
	int blockSize = code_buf->GetSize();

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableDebugging)
	{
		// We can link blocks as long as we are not single stepping and there are no breakpoints here
		EnableBlockLink();

		// Comment out the following to disable breakpoints (speed-up)
		if (!Profiler::g_ProfileBlocks)
		{
			if (GetState() == CPU_STEPPING)
			{
				blockSize = 1;

				// Do not link this block to other blocks While single stepping
				jo.enableBlocklink = false;
			}
			Trace();
		}
	}

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
		ABI_CallFunction((void *)&ImHere); // Used to get a trace of the last few blocks before a crash, sometimes VERY useful

	if (js.fpa.any)
	{
		// This block uses FPU - needs to add FP exception bailout
		TEST(32, PPCSTATE(msr), Imm32(1 << 13)); //Test FP enabled bit
		FixupBranch b1 = J_CC(CC_NZ);

		// If a FPU exception occurs, the exception handler will read
		// from PC.  Update PC with the latest value in case that happens.
		MOV(32, PPCSTATE(pc), Imm32(js.blockStart));
		OR(32, PPCSTATE(Exceptions), Imm32(EXCEPTION_FPU_UNAVAILABLE));
		WriteExceptionExit();

		SetJumpTarget(b1);
	}

	js.rewriteStart = (u8*)GetCodePtr();

	u64 codeHash = -1;
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bJITILTimeProfiling ||
	    SConfig::GetInstance().m_LocalCoreStartupParameter.bJITILOutputIR)
	{
		// For profiling and IR Writer
		for (u32 i = 0; i < code_block.m_num_instructions; i++)
		{
			const u64 inst = ops[i].inst.hex;
			// Ported from boost::hash
			codeHash ^= inst + (codeHash << 6) + (codeHash >> 2);
		}
	}

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bJITILTimeProfiling)
	{
		JitILProfiler::Block& block = JitILProfiler::Add(codeHash);
		ABI_CallFunctionC((void *)JitILProfiler::Begin, block.index);
	}

	// Start up IR builder (structure that collects the
	// instruction processed by the JIT routines)
	ibuild.Reset();

	js.downcountAmount = 0;
	if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableDebugging)
		js.downcountAmount += PatchEngine::GetSpeedhackCycles(code_block.m_address);

	// Translate instructions
	for (u32 i = 0; i < code_block.m_num_instructions; i++)
	{
		js.compilerPC = ops[i].address;
		js.op = &ops[i];
		js.instructionNumber = i;
		const GekkoOPInfo *opinfo = GetOpInfo(ops[i].inst);
		js.downcountAmount += opinfo->numCycles;

		if (i == (code_block.m_num_instructions - 1))
		{
			js.isLastInstruction = true;
			js.next_inst = 0;
		}
		else
		{
			// help peephole optimizations
			js.next_inst = ops[i + 1].inst;
			js.next_compilerPC = ops[i + 1].address;
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
						MOV(32, R(EAX), PPCSTATE(npc));
						jit->js.downcountAmount += jit->js.st.numCycles;
						WriteExitDestInOpArg(R(EAX));
						break;
					}
				}
			}
		}

		if (!ops[i].skip)
		{
			if (js.memcheck && (opinfo->flags & FL_USE_FPU))
			{
				ibuild.EmitFPExceptionCheck(ibuild.EmitIntConst(ops[i].address));
			}

			if (jit->js.fifoWriteAddresses.find(js.compilerPC) != jit->js.fifoWriteAddresses.end())
			{
				ibuild.EmitExtExceptionCheck(ibuild.EmitIntConst(ops[i].address));
			}

			if (SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableDebugging && breakpoints.IsAddressBreakPoint(ops[i].address) && GetState() != CPU_STEPPING)
			{
				// Turn off block linking if there are breakpoints so that the Step Over command does not link this block.
				jo.enableBlocklink = false;

				ibuild.EmitBreakPointCheck(ibuild.EmitIntConst(ops[i].address));
			}

			JitILTables::CompileInstruction(ops[i]);

			if (js.memcheck && (opinfo->flags & FL_LOADSTORE))
			{
				ibuild.EmitDSIExceptionCheck(ibuild.EmitIntConst(ops[i].address));
			}

			if (opinfo->flags & FL_LOADSTORE)
				++jit->js.numLoadStoreInst;

			if (opinfo->flags & FL_USE_FPU)
				++jit->js.numFloatingPointInst;
		}
	}

	u32 function = HLE::GetFunctionIndex(jit->js.blockStart);
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
		ibuild.EmitISIException(ibuild.EmitIntConst(em_address));
	}

	// Perform actual code generation
	WriteCode(nextPC);

	b->codeSize = (u32)(GetCodePtr() - normalEntry);
	b->originalSize = code_block.m_num_instructions;

#ifdef JIT_LOG_X86
	LogGeneratedX86(code_block.m_num_instructions, code_buf, normalEntry, b);
#endif

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bJITILOutputIR)
	{
		ibuild.WriteToFile(codeHash);
	}

	return normalEntry;
}

void JitIL::EnableBlockLink()
{
	jo.enableBlocklink = true;
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bJITNoBlockLinking ||
		SConfig::GetInstance().m_LocalCoreStartupParameter.bMMU)
	{
		// TODO: support block linking with MMU
		jo.enableBlocklink = false;
	}
}
