// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include <map>

#include "Common.h"
#include "x64Emitter.h"
#include "ABI.h"
#include "Thunk.h"
#include "../../HLE/HLE.h"
#include "../../Core.h"
#include "../../CoreTiming.h"
#include "../PowerPC.h"
#include "../Profiler.h"
#include "../PPCTables.h"
#include "../PPCAnalyst.h"
#include "../../HW/Memmap.h"
#include "../../HW/GPFifo.h"
#include "Jit.h"
#include "JitAsm.h"
#include "JitCache.h"
#include "JitRegCache.h"

using namespace Gen;
using namespace PowerPC;

extern int blocksExecuted;
//X64 Win64 calling convention:
// Parameters in RCX RDX R8 R9
// Volatile      RAX R10 R11
// Non volatile (must be saved)
// RBX RSI RDI R12 R13 R14 R15

//Register allocation:

//RAX - Generic quicktemp register
//RBX - point to base of memory map
//RSI RDI R12 R13 R14 R15 - free for allocation
//RCX RDX R8 R9 R10 R11 - allocate in emergencies. These need to be flushed before functions are called.
//RSP - stack pointer, do not generally use, very dangerous
//RBP - ?

//RCX RDX R8 R9 are function parameters. We will only call 1 and 2 param functions from compiled code anyway.

//Calling out to the interpreter needs only to flush the volatile regs!

//IMPORTANT:
//Make sure that all generated code and all emulator state sits under the 2GB boundary so that
//RIP addressing can be used easily. Windows will always allocate static code under the 2GB boundary.
//Also make sure to use VirtualAlloc and specify EXECUTE permission.

//Since RIP stores/loads will not be possible to the high memory area, we will have to use
//a statically allocated base pointer in a register, and use displacement addressing off of that.
//A candidate for this is a non vol like R15, since we would not like to have to do a RIP load 
//to restore it all the time.
//No wait a minute, why not just keep the unprotected mappings below 2GB? 

//Another question to be addressed is if it is smart to have a static pointer reg to the base 
//of the PowerPC reg area. 
//Pro: Smaller accesses for GPR (8-bit displacement good enough only for gprs)
//Con: A taken nonvol register (may not be so bad)

//Should there be any statically allocated registers? r3, r4, r5, r8, r0 come to mind.. maybe sp

//When calling external functions, only volatile regs need to be saved.
//This means that they should be allocated last. RAX should probably never
//be allocated, it should just be a temporary to do non-destructive trinary ops.

//However, for the above to work and be a win, we need to store away the non volatiles before  
//entering "JIT space". However, once we're there, it will be a win.
//Also, JIT space will need to be surrounded with stack adjusting, since functions will be called.

//Many instructions have shorter forms for EAX. However, I believe their performance boost
//will be as small to be negligble, so I haven't dirtied up the code with that. AMD recommends it in their
//optimization manuals, though.

//IDEA: FPU exception emulation support by having all fpu blocks writeNTA to a spot in memory that
//is protected if FPU exceptions are enabled. The exception handler would then have to run the
//interpreter until rfi, at which point control can be returned. Of course all regs need to be
//flushed before this happens. This method is branch free but does a memory write or read in the fast case.
// Probably not worthwhile, a test/jz in every fpu block should be enough.

//Block linking is needed. Reserve space at the end of every block for a full 5-byte jmp. Save 16-bit offsets 
//from the starts of each block, marking the exits so that they can be nicely patched at any time.

//Blocks do NOT use call/ret, they only jmp to each other and to the dispatcher when necessary.

//All blocks that can be precompiled will be precompiled. Code will be memory protected - any write will mark
//the region as non-compilable, and all links to the page will be torn out and replaced with dispatcher jmps. 

//Alternatively, icbi instruction SHOULD mark where we can't compile

//IDEA: All major memory altering events (not singular accesses) should call Gfx::Snoop to let it know that memory chagned.

//Seldom-happening events will be handled by adding a decrement of a counter to all blr instructions (which are
//expensive anyway since we need to return to dispatcher, except when they can be predicted).

//TODO:

// TODO: SERIOUS synchronization problem with the video plugin setting tokens and breakpoints!!!
//        Somewhat fixed by disabling idle skipping when certain interrupts are enabled
//        This is no permantent reliable fix

// TODO: Zeldas go whacko when you hang the gfx thread

// Plan: 1. Byteswap Dolphin DONE!
//       2. Fix timing WORKING
//       3. Lay groundwork for x64 JIT WORKING
//       4. Get OneTri up to 60fps, and check compatibility from time to time (yea right)  ????
//       5. Add block linking to JIT << NOT SO IMPORTANT
//		 6. Optimize GFX plugin to hell << IMPORTANT
//       7. Watch Zelda do 20 fps.
//       8. Watch Zelda TP do 30 fps. DONE :D

//Optimizations -
/*
  * Assume SP is in main RAM (in Wii mode too?)
  * Assume all floating point loads and double precision loads+stores are to/from main ram
    (single precision can be used in write gather)
    (this is valid on Wii too when using the VM emulator)

  * AMD only - use movaps instead of movapd when loading ps from memory?
  * HLE functions like floorf, sin, memcpy, etc - they can be much faster
  * Optimal sequence to store floats
  * TODO: find optimal sequence to store doubles as floats

  cvtpd2ps xmm0, xmm0
  movss xmm0, f
  movss tempspace, xmm0
  mov eax, tempspace
  bswap eax
  mov [edi], eax

  I think pshufb does it faster.

BLOCK EXIT DESIGN

TEST whatever
JZ skip
MOV NPC, exit1
JMP dispatcher
skip:
MOV NPC, exit2
JMP dispatcher

This can be patched into (when both exits are known):
JZ exit2
JMP exit1

The problem is, we still need to fit the downcount somewhere...

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


detect immediates in stb stw sth

TODO
lha
srawx
addic_rc
addex
subfcx
subfex
000000000A42BD7F  mov         ecx,0FCBF41BAh 
000000000A42BD85  call        CInterpreter::fmaddx (5BA3A0h) 
000000000A42BD8A  mov         ecx,0FC8D0132h 
000000000A42BD90  call        CInterpreter::fmulx (5BA540h) 
000000000A42BD95  mov         ecx,0FC85202Ah 
000000000A42BD9B  call        CInterpreter::faddx (5BA220h) 
000000000A42BDA0  mov         ecx,0FC81113Ah 
000000000A42BDA6  call        CInterpreter::fmaddx (5BA3A0h) 
000000000A42C11A  call        CInterpreter::fnegx (5BA0B0h)
000000000A42C604  call        CInterpreter::frspx (5BA170h) 
000000000A428FDC  call        CInterpreter::ps_sum0 (5C9730h) 
000000000A428FE1  mov         ecx,0FCA02034h 
000000000A428FE7  call        CInterpreter::frsqrtex (5BA7C0h)
000000000A429062  call        CInterpreter::ps_muls0 (5C9820h) 
000000000A4290AF  call        CInterpreter::psq_st (5C9DF0h) 

*/

// Accurate exception handling
// Compute register state at a certain instruction by running the JIT in "dry mode", and stopping at the right place.
// Not likely to be done :P

// Evil
namespace CPUCompare
{
	extern u32 m_BlockStart;
}


namespace Jit64
{
	JitState js;
	JitOptions jo;

	void Init()
	{
		jo.optimizeStack = true;
		jo.enableBlocklink = true;  // Speed boost, but not 100% safe
#ifdef _M_X64
		jo.enableFastMem = Core::GetStartupParameter().bUseFastMem;
#else
		jo.enableFastMem = false;
#endif
		jo.assumeFPLoadFromMem = true;
		jo.fpAccurateFlags = true;
		jo.optimizeGatherPipe = true;
		jo.interpretFPU = false;
	}

	void WriteCallInterpreter(UGeckoInstruction _inst)
	{
		gpr.Flush(FLUSH_ALL);
		fpr.Flush(FLUSH_ALL);
		if (js.isLastInstruction)
		{
			MOV(32, M(&PC), Imm32(js.compilerPC));
			MOV(32, M(&NPC), Imm32(js.compilerPC + 4));
		}
		Interpreter::_interpreterInstruction instr = GetInterpreterOp(_inst);
		ABI_CallFunctionC((void*)instr, _inst.hex);
	}

	void Default(UGeckoInstruction _inst)
	{
		WriteCallInterpreter(_inst.hex);
	}

	void HLEFunction(UGeckoInstruction _inst)
	{
		gpr.Flush(FLUSH_ALL);
		fpr.Flush(FLUSH_ALL);
		ABI_CallFunctionCC((void*)&HLE::Execute, js.compilerPC, _inst.hex);
		MOV(32, R(EAX), M(&NPC));
		WriteExitDestInEAX(0);
	}

	void DoNothing(UGeckoInstruction _inst)
	{
		// Yup, just don't do anything.
	}

	static const bool ImHereDebug = false;
	static const bool ImHereLog = false;
	static std::map<u32, int> been_here;
	void ImHere()
	{
		static FILE *f = 0;
		if (ImHereLog) {
			if (!f)
			{
#ifdef _M_X64
				f = fopen("log64.txt", "w");
#else
				f = fopen("log32.txt", "w");
#endif
			}
			fprintf(f, "%08x\n", PC);
		}
		if (been_here.find(PC) != been_here.end()) {
			been_here.find(PC)->second++;
			if ((been_here.find(PC)->second) & 1023)
				return;
		}
		LOG(DYNA_REC, "I'm here - PC = %08x , LR = %08x", PC, LR);
		printf("I'm here - PC = %08x , LR = %08x", PC, LR);
		been_here[PC] = 1;
	}

	void Cleanup()
	{
		if (jo.optimizeGatherPipe && js.fifoBytesThisBlock > 0)
			CALL((void *)&GPFifo::CheckGatherPipe);
	}

	void WriteExit(u32 destination, int exit_num)
	{
		Cleanup();
		SUB(32, M(&CoreTiming::downcount), js.downcountAmount > 127 ? Imm32(js.downcountAmount) : Imm8(js.downcountAmount)); 

		//If nobody has taken care of this yet (this can be removed when all branches are done)
		JitBlock *b = js.curBlock;
		b->exitAddress[exit_num] = destination;
		b->exitPtrs[exit_num] = GetWritableCodePtr();
		
		// Link opportunity!
		int block = GetBlockNumberFromAddress(destination);
		if (block >= 0 && jo.enableBlocklink) 
		{
			// It exists! Joy of joy!
			JMP(GetBlock(block)->checkedEntry, true);
			b->linkStatus[exit_num] = true;
		}
		else 
		{
			MOV(32, M(&PC), Imm32(destination));
			JMP(Asm::dispatcher, true);
		}
	}

	void WriteExitDestInEAX(int exit_num) 
	{
		MOV(32, M(&PC), R(EAX));
		Cleanup();
		SUB(32, M(&CoreTiming::downcount), js.downcountAmount > 127 ? Imm32(js.downcountAmount) : Imm8(js.downcountAmount)); 
		JMP(Asm::dispatcher, true);
	}

	void WriteRfiExitDestInEAX() 
	{
		MOV(32, M(&PC), R(EAX));
		Cleanup();
		SUB(32, M(&CoreTiming::downcount), js.downcountAmount > 127 ? Imm32(js.downcountAmount) : Imm8(js.downcountAmount)); 
		JMP(Asm::testExceptions, true);
	}

	void WriteExceptionExit(u32 exception)
	{
		Cleanup();
		OR(32, M(&PowerPC::ppcState.Exceptions), Imm32(exception));
		MOV(32, M(&PC), Imm32(js.compilerPC + 4));
		JMP(Asm::testExceptions, true);
	}

	const u8* DoJit(u32 emaddress, JitBlock &b)
	{
		if (emaddress == 0)
			PanicAlert("ERROR : Trying to compile at 0. LR=%08x", LR);

		u32 size;
		js.isLastInstruction = false;
		js.blockStart = emaddress;
		js.fifoBytesThisBlock = 0;
		js.curBlock = &b;
		js.blockSetsQuantizers = false;

		//Analyze the block, collect all instructions it is made of (including inlining,
		//if that is enabled), reorder instructions for optimal performance, and join joinable instructions.
		PPCAnalyst::CodeOp *ops = PPCAnalyst::Flatten(emaddress, size, js.st, js.gpa, js.fpa);
		const u8 *start = AlignCode4(); //TODO: Test if this or AlignCode16 make a difference from GetCodePtr
		b.checkedEntry = start;
		b.runCount = 0;
		FixupBranch skip = J_CC(CC_NBE);
		MOV(32, M(&PC), Imm32(js.blockStart));
		JMP(Asm::doTiming, true);
		SetJumpTarget(skip);

		const u8 *normalEntry = GetCodePtr();
		if (ImHereDebug) CALL((void *)&ImHere); //Used to get a trace of the last few blocks before a crash, sometimes VERY useful
		
		if (js.fpa.any)
		{
			//This block uses FPU - needs to add FP exception bailout
			TEST(32, M(&PowerPC::ppcState.msr), Imm32(1 << 13)); //Test FP enabled bit
			FixupBranch b1 = J_CC(CC_NZ);
			MOV(32, M(&PC), Imm32(js.blockStart));
			JMP(Asm::fpException, true);
			SetJumpTarget(b1);
		}

		if (Profiler::g_ProfileBlocks) {
			ADD(32, M(&b.runCount), Imm8(1));
		}

		//Start up the register allocators
		//They use the information in gpa/fpa to preload commonly used registers.
		gpr.Start(js.gpa);
		fpr.Start(js.fpa);

		js.downcountAmount = js.st.numCycles;
		js.blockSize = size;
		// Translate instructions
		for (int i = 0; i < (int)size; i++)
		{
			// gpr.Flush(FLUSH_ALL);
			// if (PPCTables::UsesFPU(_inst))
			// fpr.Flush(FLUSH_ALL);
			js.compilerPC = ops[i].address;
			js.op = &ops[i];
			js.instructionNumber = i;
			if (i == (int)size - 1) js.isLastInstruction = true;
			
			// const GekkoOpInfo *info = GetOpInfo();
			// if (js.isLastInstruction)
			if (jo.interpretFPU && PPCTables::UsesFPU(ops[i].inst))
				Default(ops[i].inst);
			else
				PPCTables::CompileInstruction(ops[i].inst);
			// else
			// 	Default(ops[i].inst);
			gpr.SanityCheck();
			fpr.SanityCheck();
			if (jo.optimizeGatherPipe && js.fifoBytesThisBlock >= 32)
			{
				js.fifoBytesThisBlock -= 32;
				CALL(ProtectFunction((void *)&GPFifo::CheckGatherPipe, 0));
			}
		}
		js.compilerPC += 4;

		b.codeSize = (u32)(GetCodePtr() - start);
		b.originalSize = js.compilerPC - emaddress;
		return normalEntry;
	}
}
