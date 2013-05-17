// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <map>

#include "Common.h"
#include "x64Emitter.h"
#include "x64ABI.h"
#include "Thunk.h"
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
#include "../JitCommon/JitCache.h"
#include "JitIL.h"
#include "JitILAsm.h"
#include "JitIL_Tables.h"

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


// For profiling
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#else
#include <memory>
#include <stdint.h>
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
			"xor	%%eax,%%eax;"
			"push	%%ebx;"
			"cpuid;"
			"pop	%%ebx;"
			::: "%eax", "%ecx", "%edx");
	__asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
#endif
	return (uint64_t)hi << 32 | lo;
}
#endif

namespace JitILProfiler
{
	struct Block
	{
		u32 index;
		u64 codeHash;
		u64 totalElapsed;
		u64 numberOfCalls;
		Block() : index(0), codeHash(0), totalElapsed(0), numberOfCalls(0) { }
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
			char buffer[1024];
			sprintf(buffer, "JitIL_profiling_%d.csv", (int)time(NULL));
			File::IOFile file(buffer, "w");
			setvbuf(file.GetHandle(), NULL, _IOFBF, 1024 * 1024);
			fprintf(file.GetHandle(), "code hash,total elapsed,number of calls,elapsed per call\n");
			for (std::vector<Block>::iterator it = blocks.begin(), itEnd = blocks.end(); it != itEnd; ++it)
			{
				const u64 codeHash = it->codeHash;
				const u64 totalElapsed = it->totalElapsed;
				const u64 numberOfCalls = it->numberOfCalls;
				const double elapsedPerCall = totalElapsed / (double)numberOfCalls;
				fprintf(file.GetHandle(), "%016llx,%lld,%lld,%f\n", codeHash, totalElapsed, numberOfCalls, elapsedPerCall);
			}
		}
	};
	std::auto_ptr<JitILProfilerFinalizer> finalizer;
	static void Init()
	{
		finalizer = std::auto_ptr<JitILProfilerFinalizer>(new JitILProfilerFinalizer);
	}
	static void Shutdown()
	{
		finalizer.reset();
	}
};

static int CODE_SIZE = 1024*1024*32;

namespace CPUCompare
{
	extern u32 m_BlockStart;
}

void JitIL::Init()
{
	jo.optimizeStack = true;

	if (Core::g_CoreStartupParameter.bEnableDebugging)
	{
		jo.enableBlocklink = false;
		Core::g_CoreStartupParameter.bSkipIdle = false;
	}
	else
	{
		if (!Core::g_CoreStartupParameter.bJITBlockLinking)
			jo.enableBlocklink = false;
		else
			// Speed boost, but not 100% safe
			jo.enableBlocklink = !Core::g_CoreStartupParameter.bMMU;
	}

	jo.fpAccurateFcmp = false;
	jo.optimizeGatherPipe = true;
	jo.fastInterrupts = false;
	jo.accurateSinglePrecision = false;
	js.memcheck = Core::g_CoreStartupParameter.bMMU;

	trampolines.Init();
	AllocCodeSpace(CODE_SIZE);

	blocks.Init();
	asm_routines.Init();

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bJITILTimeProfiling) {
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
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bJITILTimeProfiling) {
		JitILProfiler::Shutdown();
	}

	FreeCodeSpace();

	blocks.Shutdown();
	trampolines.Shutdown();
	asm_routines.Shutdown();
}


void JitIL::WriteCallInterpreter(UGeckoInstruction inst)
{
	if (js.isLastInstruction)
	{
		MOV(32, M(&PC), Imm32(js.compilerPC));
		MOV(32, M(&NPC), Imm32(js.compilerPC + 4));
	}
	Interpreter::_interpreterInstruction instr = GetInterpreterOp(inst);
	ABI_CallFunctionC((void*)instr, inst.hex);
	if (js.isLastInstruction)
	{
		MOV(32, R(EAX), M(&NPC));
		WriteRfiExitDestInOpArg(R(EAX));
	}
}

void JitIL::unknown_instruction(UGeckoInstruction inst)
{
	//	CCPU::Break();
	PanicAlert("unknown_instruction %08x - Fix me ;)", inst.hex);
}

void JitIL::Default(UGeckoInstruction _inst)
{
	ibuild.EmitInterpreterFallback(
		ibuild.EmitIntConst(_inst.hex),
		ibuild.EmitIntConst(js.compilerPC));
}

void JitIL::HLEFunction(UGeckoInstruction _inst)
{
	ABI_CallFunctionCC((void*)&HLE::Execute, js.compilerPC, _inst.hex);
	MOV(32, R(EAX), M(&NPC));
	WriteExitDestInOpArg(R(EAX));
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
#ifdef _M_X64
			f.Open("log64.txt", "w");
#else
			f.Open("log32.txt", "w");
#endif
		}
		fprintf(f.GetHandle(), "%08x r0: %08x r5: %08x r6: %08x\n", PC, PowerPC::ppcState.gpr[0],
			PowerPC::ppcState.gpr[5], PowerPC::ppcState.gpr[6]);
		f.Flush();
	}
	if (been_here.find(PC) != been_here.end()) {
		been_here.find(PC)->second++;
		if ((been_here.find(PC)->second) & 1023)
			return;
	}
	DEBUG_LOG(DYNA_REC, "I'm here - PC = %08x , LR = %08x", PC, LR);
	//		printf("I'm here - PC = %08x , LR = %08x", PC, LR);
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

void JitIL::WriteExit(u32 destination, int exit_num)
{
	Cleanup();
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bJITILTimeProfiling) {
		ABI_CallFunction((void *)JitILProfiler::End);
	}
	SUB(32, M(&CoreTiming::downcount), js.downcountAmount > 127 ? Imm32(js.downcountAmount) : Imm8(js.downcountAmount)); 

	//If nobody has taken care of this yet (this can be removed when all branches are done)
	JitBlock *b = js.curBlock;
	b->exitAddress[exit_num] = destination;
	b->exitPtrs[exit_num] = GetWritableCodePtr();
	
	// Link opportunity!
	int block = blocks.GetBlockNumberFromStartAddress(destination);
	if (block >= 0 && jo.enableBlocklink) 
	{
		// It exists! Joy of joy!
		JMP(blocks.GetBlock(block)->checkedEntry, true);
		b->linkStatus[exit_num] = true;
	}
	else 
	{
		MOV(32, M(&PC), Imm32(destination));
		JMP(asm_routines.dispatcher, true);
	}
}

void JitIL::WriteExitDestInOpArg(const Gen::OpArg& arg)
{
	MOV(32, M(&PC), arg);
	Cleanup();
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bJITILTimeProfiling) {
		ABI_CallFunction((void *)JitILProfiler::End);
	}
	SUB(32, M(&CoreTiming::downcount), js.downcountAmount > 127 ? Imm32(js.downcountAmount) : Imm8(js.downcountAmount)); 
	JMP(asm_routines.dispatcher, true);
}

void JitIL::WriteRfiExitDestInOpArg(const Gen::OpArg& arg)
{
	MOV(32, M(&PC), arg);
	Cleanup();
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bJITILTimeProfiling) {
		ABI_CallFunction((void *)JitILProfiler::End);
	}
	SUB(32, M(&CoreTiming::downcount), js.downcountAmount > 127 ? Imm32(js.downcountAmount) : Imm8(js.downcountAmount)); 
	JMP(asm_routines.testExceptions, true);
}

void JitIL::WriteExceptionExit()
{
	Cleanup();
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bJITILTimeProfiling) {
		ABI_CallFunction((void *)JitILProfiler::End);
	}
	SUB(32, M(&CoreTiming::downcount), js.downcountAmount > 127 ? Imm32(js.downcountAmount) : Imm8(js.downcountAmount)); 
	JMP(asm_routines.testExceptions, true);
}

void STACKALIGN JitIL::Run()
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
	char regs[500] = "";
	char fregs[750] = "";

#ifdef JIT_LOG_GPR
	for (int i = 0; i < 32; i++)
	{
		char reg[50];
		sprintf(reg, "r%02d: %08x ", i, PowerPC::ppcState.gpr[i]);
		strncat(regs, reg, 500);
	}
#endif

#ifdef JIT_LOG_FPR
	for (int i = 0; i < 32; i++)
	{
		char reg[50];
		sprintf(reg, "f%02d: %016x ", i, riPS0(i));
		strncat(fregs, reg, 750);
	}
#endif	

	DEBUG_LOG(DYNA_REC, "JITIL PC: %08x SRR0: %08x SRR1: %08x CRfast: %02x%02x%02x%02x%02x%02x%02x%02x FPSCR: %08x MSR: %08x LR: %08x %s %s", 
		PC, SRR0, SRR1, PowerPC::ppcState.cr_fast[0], PowerPC::ppcState.cr_fast[1], PowerPC::ppcState.cr_fast[2], PowerPC::ppcState.cr_fast[3], 
		PowerPC::ppcState.cr_fast[4], PowerPC::ppcState.cr_fast[5], PowerPC::ppcState.cr_fast[6], PowerPC::ppcState.cr_fast[7], PowerPC::ppcState.fpscr, 
		PowerPC::ppcState.msr, PowerPC::ppcState.spr[8], regs, fregs);
}

void STACKALIGN JitIL::Jit(u32 em_address)
{
	if (GetSpaceLeft() < 0x10000 || blocks.IsFull() || Core::g_CoreStartupParameter.bJITNoBlockCache)
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

	// Memory exception on instruction fetch
	bool memory_exception = false;

	// A broken block is a block that does not end in a branch
	bool broken_block = false;

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

	if (em_address == 0)
	{
		// Memory exception occurred during instruction fetch
		memory_exception = true;
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
	js.cancel = false;
	jit->js.numLoadStoreInst = 0;
	jit->js.numFloatingPointInst = 0;

	// Analyze the block, collect all instructions it is made of (including inlining,
	// if that is enabled), reorder instructions for optimal performance, and join joinable instructions.
	b->exitAddress[0] = em_address;
	u32 merged_addresses[32];
	const int capacity_of_merged_addresses = sizeof(merged_addresses) / sizeof(merged_addresses[0]);
	int size_of_merged_addresses = 0;
	if (!memory_exception)
	{
		// If there is a memory exception inside a block (broken_block==true), compile up to that instruction.
		b->exitAddress[0] = PPCAnalyst::Flatten(em_address, &size, &js.st, &js.gpa, &js.fpa, broken_block, code_buf, blockSize, merged_addresses, capacity_of_merged_addresses, size_of_merged_addresses);
	}
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
		ABI_CallFunction((void *)&ImHere); // Used to get a trace of the last few blocks before a crash, sometimes VERY useful

	if (js.fpa.any)
	{
		// This block uses FPU - needs to add FP exception bailout
		TEST(32, M(&PowerPC::ppcState.msr), Imm32(1 << 13)); //Test FP enabled bit
		FixupBranch b1 = J_CC(CC_NZ);
		MOV(32, M(&PC), Imm32(js.blockStart));
		JMP(asm_routines.fpException, true);
		SetJumpTarget(b1);
	}

	js.rewriteStart = (u8*)GetCodePtr();

	u64 codeHash = -1;
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bJITILTimeProfiling ||
		SConfig::GetInstance().m_LocalCoreStartupParameter.bJITILOutputIR)
	{
		// For profiling and IR Writer
		for (int i = 0; i < (int)size; i++)
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
	if (!Core::g_CoreStartupParameter.bEnableDebugging)
	{
		for (int i = 0; i < size_of_merged_addresses; ++i)
		{
			const u32 address = merged_addresses[i];
			js.downcountAmount += PatchEngine::GetSpeedhackCycles(address);
		}
	}

	// Translate instructions
	for (int i = 0; i < (int)size; i++)
	{
		js.compilerPC = ops[i].address;
		js.op = &ops[i];
		js.instructionNumber = i;
		const GekkoOPInfo *opinfo = GetOpInfo(ops[i].inst);
		js.downcountAmount += (opinfo->numCyclesMinusOne + 1);

		if (i == (int)size - 1)
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
						MOV(32, R(EAX), M(&NPC));
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

			if (Core::g_CoreStartupParameter.bEnableDebugging && breakpoints.IsAddressBreakPoint(ops[i].address) && GetState() != CPU_STEPPING)
			{
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

	if (memory_exception)
	{
		ibuild.EmitISIException(ibuild.EmitIntConst(em_address));
	}

	// Perform actual code generation
	WriteCode();

	b->codeSize = (u32)(GetCodePtr() - normalEntry);
	b->originalSize = size;

#ifdef JIT_LOG_X86
	LogGeneratedX86(size, code_buf, normalEntry, b);
#endif

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bJITILOutputIR)
	{
		ibuild.WriteToFile(codeHash);
	}

	return normalEntry;
}
