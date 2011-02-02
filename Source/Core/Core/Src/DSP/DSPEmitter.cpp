// Copyright (C) 2010 Dolphin Project.

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

#include <cstring>

#include "DSPEmitter.h"
#include "DSPMemoryMap.h"
#include "DSPCore.h"
#include "DSPHost.h"
#include "DSPInterpreter.h"
#include "DSPAnalyzer.h"
#include "Jit/DSPJitUtil.h"
#include "x64Emitter.h"
#include "ABI.h"

#define MAX_BLOCK_SIZE 250
#define DSP_IDLE_SKIP_CYCLES 0x1000

using namespace Gen;

DSPEmitter::DSPEmitter() : gpr(*this), storeIndex(-1), storeIndex2(-1)
{
	m_compiledCode = NULL;

	AllocCodeSpace(COMPILED_CODE_SIZE);

	blocks = new DSPCompiledCode[MAX_BLOCKS];
	blockLinks = new Block[MAX_BLOCKS];
	blockSize = new u16[MAX_BLOCKS];
	unresolvedJumps = new std::list<u16>[MAX_BLOCKS];
	
	compileSR = 0;
	compileSR |= SR_INT_ENABLE;
	compileSR |= SR_EXT_INT_ENABLE;

	CompileDispatcher();
	stubEntryPoint = CompileStub();

	//clear all of the block references
	for(int i = 0x0000; i < MAX_BLOCKS; i++)
	{
		blocks[i] = (DSPCompiledCode)stubEntryPoint;
		blockLinks[i] = 0;
		blockSize[i] = 0;
	}
}

DSPEmitter::~DSPEmitter() 
{
	delete[] blocks;
	delete[] blockLinks;
	delete[] blockSize;
	FreeCodeSpace();
}

void DSPEmitter::ClearIRAM() {
	// ClearCodeSpace();
	for(int i = 0x0000; i < 0x1000; i++)
	{
		blocks[i] = (DSPCompiledCode)stubEntryPoint;
		blockLinks[i] = 0;
		blockSize[i] = 0;
	}
}

// Must go out of block if exception is detected
void DSPEmitter::checkExceptions(u32 retval)
{
	// Check for interrupts and exceptions
#ifdef _M_IX86 // All32
	TEST(8, M(&g_dsp.exceptions), Imm8(0xff));
#else
	MOV(64, R(RAX), ImmPtr(&g_dsp.exceptions));
	TEST(8, MatR(RAX), Imm8(0xff));
#endif
	FixupBranch skipCheck = J_CC(CC_Z);

#ifdef _M_IX86 // All32
	MOV(16, M(&(g_dsp.pc)), Imm16(compilePC));
#else
	MOV(64, R(RAX), ImmPtr(&(g_dsp.pc)));
	MOV(16, MatR(RAX), Imm16(compilePC));
#endif

	DSPJitRegCache c(gpr);
	SaveDSPRegs();
	ABI_CallFunction((void *)&DSPCore_CheckExceptions);
	MOV(32, R(EAX), Imm32(retval));
	JMP(returnDispatcher, true);
	gpr.flushRegs(c,false);

	SetJumpTarget(skipCheck);
}

void DSPEmitter::Default(UDSPInstruction inst)
{
	if (opTable[inst]->reads_pc)
	{
		// Increment PC - we shouldn't need to do this for every instruction. only for branches and end of block.
		// Fallbacks to interpreter need this for fetching immediate values

#ifdef _M_IX86 // All32
		MOV(16, M(&(g_dsp.pc)), Imm16(compilePC + 1));
#else
		MOV(64, R(RAX), ImmPtr(&(g_dsp.pc)));
		MOV(16, MatR(RAX), Imm16(compilePC + 1));
#endif
	}

	// Fall back to interpreter
	SaveDSPRegs();
	ABI_CallFunctionC16((void*)opTable[inst]->intFunc, inst);
	LoadDSPRegs();
}

void DSPEmitter::EmitInstruction(UDSPInstruction inst)
{
	const DSPOPCTemplate *tinst = GetOpTemplate(inst);
	bool ext_is_jit = false;

	// Call extended
	if (tinst->extended) {
		if ((inst >> 12) == 0x3) {
			if (! extOpTable[inst & 0x7F]->jitFunc) {
				// Fall back to interpreter
				SaveDSPRegs();
				ABI_CallFunctionC16((void*)extOpTable[inst & 0x7F]->intFunc, inst);
				LoadDSPRegs();
				INFO_LOG(DSPLLE, "Instruction not JITed(ext part): %04x\n", inst);
				ext_is_jit = false;
			} else {
				(this->*extOpTable[inst & 0x7F]->jitFunc)(inst);
				ext_is_jit = true;
			}
		} else {
			if (!extOpTable[inst & 0xFF]->jitFunc) {
				// Fall back to interpreter
				SaveDSPRegs();
				ABI_CallFunctionC16((void*)extOpTable[inst & 0xFF]->intFunc, inst);
				LoadDSPRegs();
				INFO_LOG(DSPLLE, "Instruction not JITed(ext part): %04x\n", inst);
				ext_is_jit = false;
			} else {
				(this->*extOpTable[inst & 0xFF]->jitFunc)(inst);
				ext_is_jit = true;
			}
		}
	}
	
	// Main instruction
	if (!opTable[inst]->jitFunc) {
		Default(inst);
		INFO_LOG(DSPLLE, "Instruction not JITed(main part): %04x\n", inst);
	}
	else
	{
		(this->*opTable[inst]->jitFunc)(inst);
	}

	// Backlog
	if (tinst->extended) {
		if (!ext_is_jit) {
			//need to call the online cleanup function because
			//the writeBackLog gets populated at runtime
			SaveDSPRegs();
			ABI_CallFunction((void*)::applyWriteBackLog);
			LoadDSPRegs();
		} else {
			popExtValueToReg();
		}
	}
}

void DSPEmitter::unknown_instruction(UDSPInstruction inst)
{
	PanicAlert("unknown_instruction %04x - Fix me ;)", inst);
}

void DSPEmitter::Compile(u16 start_addr)
{
	// Remember the current block address for later
	startAddr = start_addr;
	unresolvedJumps[start_addr].clear();

	const u8 *entryPoint = AlignCode16();

	/*
	// Check for other exceptions
	if (dsp_SR_is_flag_set(SR_INT_ENABLE))
		return;

	if (g_dsp.exceptions == 0)
		return;	
	*/

	LoadDSPRegs();

	blockLinkEntry = GetCodePtr();

	compilePC = start_addr;
	bool fixup_pc = false;
	blockSize[start_addr] = 0;

	while (compilePC < start_addr + MAX_BLOCK_SIZE)
	{
		if (DSPAnalyzer::code_flags[compilePC] & DSPAnalyzer::CODE_CHECK_INT)
			checkExceptions(blockSize[start_addr]);

		UDSPInstruction inst = dsp_imem_read(compilePC);
		const DSPOPCTemplate *opcode = GetOpTemplate(inst);

		EmitInstruction(inst);

		blockSize[start_addr]++;
		compilePC += opcode->size;

		// If the block was trying to link into itself, remove the link
		unresolvedJumps[start_addr].remove(compilePC);

		fixup_pc = true;

		// Handle loop condition, only if current instruction was flagged as a loop destination
		// by the analyzer.
		if (DSPAnalyzer::code_flags[compilePC-1] & DSPAnalyzer::CODE_LOOP_END)
		{
#ifdef _M_IX86 // All32
			MOVZX(32, 16, EAX, M(&(g_dsp.r.st[2])));
#else
			MOV(64, R(R11), ImmPtr(&g_dsp.r));
			MOVZX(32, 16, EAX, MDisp(R11, STRUCT_OFFSET(g_dsp.r, st[2])));
#endif
			CMP(32, R(EAX), Imm32(0));
			FixupBranch rLoopAddressExit = J_CC(CC_LE, true);
		
#ifdef _M_IX86 // All32
			MOVZX(32, 16, EAX, M(&g_dsp.r.st[3]));
#else
			MOVZX(32, 16, EAX, MDisp(R11, STRUCT_OFFSET(g_dsp.r, st[3])));
#endif
			CMP(32, R(EAX), Imm32(0));
			FixupBranch rLoopCounterExit = J_CC(CC_LE, true);

			if (!opcode->branch)
			{
				//branch insns update the g_dsp.pc
#ifdef _M_IX86 // All32
				MOV(16, M(&(g_dsp.pc)), Imm16(compilePC));
#else
				MOV(64, R(RAX), ImmPtr(&(g_dsp.pc)));
				MOV(16, MatR(RAX), Imm16(compilePC));
#endif
			}

			// These functions branch and therefore only need to be called in the
			// end of each block and in this order
			DSPJitRegCache c(gpr);
			HandleLoop();
			SaveDSPRegs();
			if (!DSPHost_OnThread() && DSPAnalyzer::code_flags[start_addr] & DSPAnalyzer::CODE_IDLE_SKIP)
			{
				MOV(16, R(EAX), Imm16(DSP_IDLE_SKIP_CYCLES));
			}
			else
			{
				MOV(16, R(EAX), Imm16(blockSize[start_addr]));
			}
			JMP(returnDispatcher, true);
			gpr.flushRegs(c,false);

			SetJumpTarget(rLoopAddressExit);
			SetJumpTarget(rLoopCounterExit);
		}

		if (opcode->branch)
		{
			//don't update g_dsp.pc -- the branch insn already did
			fixup_pc = false;
			if (opcode->uncond_branch)
			{
				break;
			}
			else if (!opcode->jitFunc)
			{
				//look at g_dsp.pc if we actually branched
#ifdef _M_IX86 // All32
				MOV(16, R(AX), M(&g_dsp.pc));
#else
				MOV(64, R(RAX), ImmPtr(&(g_dsp.pc)));
				MOV(16, R(AX), MatR(RAX));
#endif
				CMP(16, R(AX), Imm16(compilePC));
				FixupBranch rNoBranch = J_CC(CC_Z);

				DSPJitRegCache c(gpr);
				//don't update g_dsp.pc -- the branch insn already did
				SaveDSPRegs();
				if (!DSPHost_OnThread() && DSPAnalyzer::code_flags[start_addr] & DSPAnalyzer::CODE_IDLE_SKIP)
				{
					MOV(16, R(EAX), Imm16(DSP_IDLE_SKIP_CYCLES));
				}
				else
				{
					MOV(16, R(EAX), Imm16(blockSize[start_addr]));
				}
				JMP(returnDispatcher, true);
				gpr.flushRegs(c,false);

				SetJumpTarget(rNoBranch);
			}
		}

		// End the block if we're before an idle skip address
		if (DSPAnalyzer::code_flags[compilePC] & DSPAnalyzer::CODE_IDLE_SKIP)
		{
			break;
		}
	}

	if (fixup_pc) {
#ifdef _M_IX86 // All32
		MOV(16, M(&(g_dsp.pc)), Imm16(compilePC));
#else
		MOV(64, R(RAX), ImmPtr(&(g_dsp.pc)));
		MOV(16, MatR(RAX), Imm16(compilePC));
#endif
	}

	blocks[start_addr] = (DSPCompiledCode)entryPoint;

	// Mark this block as a linkable destination if it does not contain
	// any unresolved CALL's
	if (unresolvedJumps[start_addr].empty())
	{
		blockLinks[start_addr] = blockLinkEntry;

		for(u16 i = 0x0000; i < 0xffff; ++i)
		{
			if (!unresolvedJumps[i].empty())
			{
				// Check if there were any blocks waiting for this block to be linkable
				unsigned int size = unresolvedJumps[i].size();
				unresolvedJumps[i].remove(start_addr);
				if (unresolvedJumps[i].size() < size)
				{
					// Mark the block to be recompiled again
					blocks[i] = (DSPCompiledCode)stubEntryPoint;
					blockLinks[i] = 0;
					blockSize[i] = 0;
				}
			}
		}
	}

	if (blockSize[start_addr] == 0) 
	{
		// just a safeguard, should never happen anymore.
		// if it does we might get stuck over in RunForCycles.
		ERROR_LOG(DSPLLE, "Block at 0x%04x has zero size", start_addr);
		blockSize[start_addr] = 1;
	}

	SaveDSPRegs();
	if (!DSPHost_OnThread() && DSPAnalyzer::code_flags[start_addr] & DSPAnalyzer::CODE_IDLE_SKIP)
	{
		MOV(16, R(EAX), Imm16(DSP_IDLE_SKIP_CYCLES));
	}
	else
	{
		MOV(16, R(EAX), Imm16(blockSize[start_addr]));
	}	
	JMP(returnDispatcher, true);
}

const u8 *DSPEmitter::CompileStub()
{
	const u8 *entryPoint = AlignCode16();
	ABI_CallFunction((void *)&CompileCurrent);
	//MOVZX(32, 16, ECX, M(&g_dsp.pc));
	XOR(32, R(EAX), R(EAX)); // Return 0 cycles executed
	JMP(returnDispatcher);
	return entryPoint;
}

void DSPEmitter::CompileDispatcher()
{
	enterDispatcher = AlignCode16();
	ABI_PushAllCalleeSavedRegsAndAdjustStack();

	const u8 *dispatcherLoop = GetCodePtr();

	FixupBranch exceptionExit;
	if (DSPHost_OnThread())
	{
		CMP(8, M(&g_dsp.external_interrupt_waiting), Imm8(0));
		exceptionExit = J_CC(CC_NE);
	}

	// Check for DSP halt
#ifdef _M_IX86
	TEST(8, M(&g_dsp.cr), Imm8(CR_HALT));
#else
	MOV(64, R(RAX), ImmPtr(&g_dsp.cr));
	TEST(8, MatR(RAX), Imm8(CR_HALT));
#endif
	FixupBranch _halt = J_CC(CC_NE);

#ifdef _M_IX86
	MOVZX(32, 16, ECX, M(&g_dsp.pc));
#else
	MOV(64, R(RCX), ImmPtr(&g_dsp.pc));
	MOVZX(64, 16, RCX, MatR(RCX));
#endif

	// Execute block. Cycles executed returned in EAX.
#ifdef _M_IX86
	MOV(32, R(EBX), ImmPtr(blocks));
	JMPptr(MComplex(EBX, ECX, SCALE_4, 0));
#else
	MOV(64, R(RBX), ImmPtr(blocks));
	JMPptr(MComplex(RBX, RCX, SCALE_8, 0));
#endif

	returnDispatcher = GetCodePtr();

	// Decrement cyclesLeft
#ifdef _M_IX86
	SUB(16, M(&cyclesLeft), R(EAX));
#else
	MOV(64, R(R12), ImmPtr(&cyclesLeft));
	SUB(16, MatR(R12), R(EAX));
#endif

	J_CC(CC_A, dispatcherLoop);
	
	// DSP gave up the remaining cycles.
	SetJumpTarget(_halt);
	if (DSPHost_OnThread())
	{
		SetJumpTarget(exceptionExit);
	}
	//MOV(32, M(&cyclesLeft), Imm32(0));
	ABI_PopAllCalleeSavedRegsAndAdjustStack();
	RET();
}
