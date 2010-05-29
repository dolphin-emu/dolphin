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
	
	ClearIRAM();

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
void DSPEmitter::checkExceptions() {
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
	// Check for interrupts and exceptions
	TEST(8, M(&g_dsp.exceptions), Imm8(0xff));
	FixupBranch skipCheck = J_CC(CC_Z);
	
	ABI_CallFunction((void *)&DSPCore_CheckExceptions);
	
	//	ABI_RestoreStack(0);
	ABI_PopAllCalleeSavedRegsAndAdjustStack();
	RET();
	
	SetJumpTarget(skipCheck);
}

void DSPEmitter::EmitInstruction(UDSPInstruction inst)
{
	const DSPOPCTemplate *tinst = GetOpTemplate(inst);

	// Call extended
	if (tinst->extended) {
		if ((inst >> 12) == 0x3) {
			if (! extOpTable[inst & 0x7F]->jitFunc) {
				// Fall back to interpreter
				ABI_CallFunctionC16((void*)extOpTable[inst & 0x7F]->intFunc, inst);
			} else {
				(this->*extOpTable[inst & 0x7F]->jitFunc)(inst);
			}
		} else {
			if (!extOpTable[inst & 0xFF]->jitFunc) {
				// Fall back to interpreter
				ABI_CallFunctionC16((void*)extOpTable[inst & 0xFF]->intFunc, inst);
			} else {
				(this->*extOpTable[inst & 0xFF]->jitFunc)(inst);
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
		if (! extOpTable[inst & 0x7F]->jitFunc) {
			ABI_CallFunction((void*)applyWriteBackLog);
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

	int addr = start_addr;
	checkExceptions();
	while (addr < start_addr + MAX_BLOCK_SIZE)
	{
		UDSPInstruction inst = dsp_imem_read(addr);
		const DSPOPCTemplate *opcode = GetOpTemplate(inst);
		
		// Increment PC - we shouldn't need to do this for every instruction. only for branches and end of block.
		ADD(16, M(&(g_dsp.pc)), Imm16(1));

		EmitInstruction(inst);
				
		// Handle loop condition, only if current instruction was flagged as a loop destination
		// by the analyzer.  COMMENTED OUT - this breaks Zelda TP. Bah.

		// if (DSPAnalyzer::code_flags[addr] & DSPAnalyzer::CODE_LOOP_END)
		{
			// TODO: Change to TEST for some reason (who added this comment?)
			MOVZX(32, 16, EAX, M(&(g_dsp.r[DSP_REG_ST2])));
			CMP(32, R(EAX), Imm32(0));
			FixupBranch rLoopAddressExit = J_CC(CC_LE);
		
			MOVZX(32, 16, EAX, M(&(g_dsp.r[DSP_REG_ST3])));
			CMP(32, R(EAX), Imm32(0));
			FixupBranch rLoopCounterExit = J_CC(CC_LE);

			// These functions branch and therefore only need to be called in the
			// end of each block and in this order
			ABI_CallFunction((void *)&DSPInterpreter::HandleLoop);
			//		ABI_RestoreStack(0);
			ABI_PopAllCalleeSavedRegsAndAdjustStack();
			RET();

			SetJumpTarget(rLoopAddressExit);
			SetJumpTarget(rLoopCounterExit);
		}

		// End the block if we're at a loop end.
		if (opcode->branch ||
			(DSPAnalyzer::code_flags[addr] & DSPAnalyzer::CODE_LOOP_END) ||
			(DSPAnalyzer::code_flags[addr] & DSPAnalyzer::CODE_IDLE_SKIP)) {
			break;
		}
		addr += opcode->size;

		blockSize[start_addr]++;
	}

	//	ABI_RestoreStack(0);
	ABI_PopAllCalleeSavedRegsAndAdjustStack();
	RET();

	blocks[start_addr] = (CompiledCode)entryPoint;

	return entryPoint;
}

void STACKALIGN DSPEmitter::CompileDispatcher()
{
	// TODO	
}

// Don't use the % operator in the inner loop. It's slow.
void STACKALIGN DSPEmitter::RunBlock(int cycles)
{
	// How does this variable work?
	static int idleskip = 0;

#define BURST_LENGTH 512   // Must be a power of two
	u16 block_cycles = BURST_LENGTH + 1;

	// Trigger an external interrupt at the start of the cycle
	while (!(g_dsp.cr & CR_HALT))
	{
		if (block_cycles > BURST_LENGTH)
		{
			block_cycles = 0;
		}

		// Compile the block if needed
		if (!blocks[g_dsp.pc])
		{
			blockSize[g_dsp.pc] = 0;
			CompileCurrent();
		}
		
		// Execute the block if we have enough cycles
		if (cycles > blockSize[g_dsp.pc])
		{
			u16 start_addr = g_dsp.pc;

			// 5%. Not sure where the rationale originally came from.
			if (((idleskip & 127) > 121) &&
				(DSPAnalyzer::code_flags[g_dsp.pc] & DSPAnalyzer::CODE_IDLE_SKIP)) {
				block_cycles = 0;
			} else {
				blocks[g_dsp.pc]();
			}
			idleskip++;
			if ((idleskip & (BURST_LENGTH - 1)) == 0)
				idleskip = 0;
			block_cycles += blockSize[start_addr];
			cycles -= blockSize[start_addr];		
		}
		else {
			break;
		}
	}
}
