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
#include "DSPInterpreter.h"
#include "DSPAnalyzer.h"
#include "x64Emitter.h"
#include "ABI.h"

#define MAX_BLOCK_SIZE 250

using namespace Gen;

DSPEmitter::DSPEmitter() : storeIndex(-1)
{
	m_compiledCode = NULL;

	AllocCodeSpace(COMPILED_CODE_SIZE);

	blocks = new CompiledCode[MAX_BLOCKS];
	blockSize = new u16[0x10000];
	
	//clear all of the block references
	for(int i = 0x0000; i < MAX_BLOCKS; i++)
	{
		blocks[i] = NULL;
		blockSize[i] = 0;
	}

	compileSR = 0;
	compileSR |= SR_INT_ENABLE;
	compileSR |= SR_EXT_INT_ENABLE;

	CompileDispatcher();
}

DSPEmitter::~DSPEmitter() 
{
	delete[] blocks;
	delete[] blockSize;
	FreeCodeSpace();
}

void DSPEmitter::ClearIRAM() {
	// ClearCodeSpace();
	for(int i = 0x0000; i < 0x1000; i++)
	{
		blocks[i] = NULL;
		blockSize[i] = 0;
	}
}


// Must go out of block if exception is detected
void DSPEmitter::checkExceptions(u32 retval) {
	// Check for interrupts and exceptions
#ifdef _M_IX86 // All32
	TEST(8, M(&g_dsp.exceptions), Imm8(0xff));
#else
	MOV(64, R(RAX), ImmPtr(&g_dsp.exceptions));
	TEST(8, MDisp(RAX,0), Imm8(0xff));
#endif
	FixupBranch skipCheck = J_CC(CC_Z);
	
	ABI_CallFunction((void *)&DSPCore_CheckExceptions);
	
	//	ABI_RestoreStack(0);
	ABI_PopAllCalleeSavedRegsAndAdjustStack();
	MOV(32,R(EAX),Imm32(retval));
	RET();
	
	SetJumpTarget(skipCheck);
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
				ABI_CallFunctionC16((void*)extOpTable[inst & 0x7F]->intFunc, inst);
				ext_is_jit = false;
			} else {
				(this->*extOpTable[inst & 0x7F]->jitFunc)(inst);
				ext_is_jit = true;
			}
		} else {
			if (!extOpTable[inst & 0xFF]->jitFunc) {
				// Fall back to interpreter
				ABI_CallFunctionC16((void*)extOpTable[inst & 0xFF]->intFunc, inst);
				ext_is_jit = false;
			} else {
				(this->*extOpTable[inst & 0xFF]->jitFunc)(inst);
				ext_is_jit = true;
			}
		}
	}
	
	// Main instruction
	if (!opTable[inst]->jitFunc) {
		// Fall back to interpreter
		ABI_CallFunctionC16((void*)opTable[inst]->intFunc, inst);
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
			ABI_CallFunction((void*)::applyWriteBackLog);
		} else {
			popExtValueToReg();
		}
	}
}

void DSPEmitter::unknown_instruction(UDSPInstruction inst)
{
	PanicAlert("unknown_instruction %04x - Fix me ;)", inst);
}

void DSPEmitter::Default(UDSPInstruction _inst)
{
	EmitInstruction(_inst);
}

const u8 *DSPEmitter::Compile(int start_addr) {
	AlignCode16();
	const u8 *entryPoint = GetCodePtr();
	ABI_PushAllCalleeSavedRegsAndAdjustStack();
	//	ABI_AlignStack(0);

	/*
	// check if there is an external interrupt
	if (! dsp_SR_is_flag_set(SR_EXT_INT_ENABLE))
		return;

	if (! (g_dsp.cr & CR_EXTERNAL_INT)) 
		return;

	g_dsp.cr &= ~CR_EXTERNAL_INT;

	// Check for other exceptions
	if (dsp_SR_is_flag_set(SR_INT_ENABLE))
		return;

	if (g_dsp.exceptions == 0)
		return;	
	*/
	ABI_CallFunction((void *)&DSPCore_CheckExternalInterrupt);

	int addr = start_addr;
	blockSize[start_addr] = 0;
	while (addr < start_addr + MAX_BLOCK_SIZE)
	{
		checkExceptions(blockSize[start_addr]);

		UDSPInstruction inst = dsp_imem_read(addr);
		const DSPOPCTemplate *opcode = GetOpTemplate(inst);

		// Increment PC - we shouldn't need to do this for every instruction. only for branches and end of block.
		// fallbacks to interpreter need this for fetching immedate values
#ifdef _M_IX86 // All32
		ADD(16, M(&(g_dsp.pc)), Imm16(1));
#else
		MOV(64, R(RAX), ImmPtr(&(g_dsp.pc)));
		ADD(16, MDisp(RAX,0), Imm16(1));
#endif

		EmitInstruction(inst);

		blockSize[start_addr]++;
		addr += opcode->size;

		// Handle loop condition, only if current instruction was flagged as a loop destination
		// by the analyzer.
		if (DSPAnalyzer::code_flags[addr-1] & DSPAnalyzer::CODE_LOOP_END)
		{
			// TODO: Change to TEST for some reason (who added this comment?)
#ifdef _M_IX86 // All32
			MOVZX(32, 16, EAX, M(&(g_dsp.r[DSP_REG_ST2])));
#else
			MOV(64, R(R11), ImmPtr(&g_dsp.r));
			MOVZX(32, 16, EAX, MDisp(R11,DSP_REG_ST2*2));
#endif
			CMP(32, R(EAX), Imm32(0));
			FixupBranch rLoopAddressExit = J_CC(CC_LE);
		
#ifdef _M_IX86 // All32
			MOVZX(32, 16, EAX, M(&(g_dsp.r[DSP_REG_ST3])));
#else
			MOVZX(32, 16, EAX, MDisp(R11,DSP_REG_ST3*2));
#endif
			CMP(32, R(EAX), Imm32(0));
			FixupBranch rLoopCounterExit = J_CC(CC_LE);

			// These functions branch and therefore only need to be called in the
			// end of each block and in this order
			ABI_CallFunction((void *)&DSPInterpreter::HandleLoop);
			//		ABI_RestoreStack(0);
			ABI_PopAllCalleeSavedRegsAndAdjustStack();
			MOV(32,R(EAX),Imm32(blockSize[start_addr]));
			RET();

			SetJumpTarget(rLoopAddressExit);
			SetJumpTarget(rLoopCounterExit);
		}

		if (opcode->branch) {
			if (opcode->uncond_branch) {
				break;
			} else {
				//look at g_dsp.pc if we actually branched
#ifdef _M_IX86 // All32
				MOV(16, R(AX), M(&(g_dsp.pc)));
#else
				MOV(64, R(RAX), ImmPtr(&(g_dsp.pc)));
				MOV(16, R(AX), MDisp(RAX,0));
#endif
				CMP(16, R(AX), Imm16(addr));
				FixupBranch rNoBranch = J_CC(CC_Z);

				//		ABI_RestoreStack(0);
				ABI_PopAllCalleeSavedRegsAndAdjustStack();
				MOV(32,R(EAX),Imm32(blockSize[start_addr]));
				RET();

				SetJumpTarget(rNoBranch);
			}
		}

		// End the block if we're before an idle skip address
		if (DSPAnalyzer::code_flags[addr] & DSPAnalyzer::CODE_IDLE_SKIP) {
			break;
		}
	}

	blocks[start_addr] = (CompiledCode)entryPoint;
	if (blockSize[start_addr] == 0) 
	{
		// just a safeguard, should never happen anymore.
		// if it does we might get stuck over in RunForCycles.
		ERROR_LOG(DSPLLE, "Block at 0x%04x has zero size", start_addr);
		blockSize[start_addr] = 1;
	}

	//	ABI_RestoreStack(0);
	ABI_PopAllCalleeSavedRegsAndAdjustStack();
	MOV(32,R(EAX),Imm32(blockSize[start_addr]));
	RET();

	return entryPoint;
}

void STACKALIGN DSPEmitter::CompileDispatcher()
{
	/*
	// TODO	
	enterDispatcher = GetCodePtr();
	AlignCode16();
	ABI_PushAllCalleeSavedRegsAndAdjustStack();
	
	const u8 *outer_loop = GetCodePtr();

		
	//Landing pad for drec space
	ABI_PopAllCalleeSavedRegsAndAdjustStack();
	RET();*/
}

// Don't use the % operator in the inner loop. It's slow.
int STACKALIGN DSPEmitter::RunForCycles(int cycles)
{
	const int idle_cycles = 1000;

	while (!(g_dsp.cr & CR_HALT))
	{
		// Compile the block if needed
		u16 block_addr = g_dsp.pc;
		if (!blocks[block_addr])
		{
			CompileCurrent();
		}
		int block_size = blockSize[block_addr];
		// Execute the block if we have enough cycles
		if (cycles > block_size)
		{
			int c = blocks[block_addr]();
			if (DSPAnalyzer::code_flags[block_addr] & DSPAnalyzer::CODE_IDLE_SKIP) {
				if (cycles > idle_cycles)
					cycles -= idle_cycles;
				else
					cycles = 0;
			} else {
				cycles -= c;
			}
		}
		else {
			break;
		}
	}
	return cycles;
}
