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

#define BLOCK_SIZE 250

using namespace Gen;

DSPEmitter::DSPEmitter()
{
	m_compiledCode = NULL;

	AllocCodeSpace(COMPILED_CODE_SIZE);

	blocks = new CompiledCode[MAX_BLOCKS];
	endBlock = new bool[MAX_BLOCKS];

	for(int i = 0x0000; i < MAX_BLOCKS; i++)
	{
		blocks[i] = CompileCurrent;
		blockSize[i] = 0;
		endBlock[i] = false;
	}
	compileSR = 0;
	compileSR |= SR_INT_ENABLE;
	compileSR |= SR_EXT_INT_ENABLE;
}

DSPEmitter::~DSPEmitter() 
{
	delete[] blocks;
	delete[] endBlock;
	FreeCodeSpace();
}

void DSPEmitter::ClearIRAM() {
	// TODO: Does not clear codespace
	for(int i = 0x0000; i < 0x1000; i++)
	{
		blocks[i] = CompileCurrent;
		blockSize[i] = 0;
		endBlock[i] = false;
	}
}

void DSPEmitter::WriteCallInterpreter(UDSPInstruction inst)
{
	const DSPOPCTemplate *tinst = GetOpTemplate(inst);

	// Call extended
	if (tinst->extended) {
		if ((inst >> 12) == 0x3) {
			if (! extOpTable[inst & 0x7F]->jitFunc) {
				ABI_CallFunctionC16((void*)extOpTable[inst & 0x7F]->intFunc, inst);
			} else {
				(this->*extOpTable[inst & 0x7F]->jitFunc)(inst);
			}
		} else {
			if (!extOpTable[inst & 0xFF]->jitFunc) {
				ABI_CallFunctionC16((void*)extOpTable[inst & 0xFF]->intFunc, inst);
			} else {
				(this->*extOpTable[inst & 0xFF]->jitFunc)(inst);
			}
		}
	}
	
	// Main instruction
	if (!opTable[inst]->jitFunc)
		ABI_CallFunctionC16((void*)opTable[inst]->intFunc, inst);
	else
		(this->*opTable[inst]->jitFunc)(inst);

	// Backlog
	// TODO if for jit
	if (tinst->extended) {
		ABI_CallFunction((void*)applyWriteBackLog);
	}
}

void DSPEmitter::unknown_instruction(UDSPInstruction inst)
{
	PanicAlert("unknown_instruction %04x - Fix me ;)", inst);
}

void DSPEmitter::Default(UDSPInstruction _inst)
{
	WriteCallInterpreter(_inst);
}

const u8 *DSPEmitter::Compile(int start_addr) {
	AlignCode16();
	const u8 *entryPoint = GetCodePtr();
	ABI_AlignStack(0);

	int addr = start_addr;

	while (addr < start_addr + BLOCK_SIZE)
	{
		UDSPInstruction inst = dsp_imem_read(addr);
		const DSPOPCTemplate *opcode = GetOpTemplate(inst);

		// Check for interrupts and exceptions
		TEST(8, M(&g_dsp.exceptions), Imm8(0xff));
		FixupBranch skipCheck = J_CC(CC_Z);
		
		ABI_CallFunction((void *)&DSPCore_CheckExceptions);
		
		MOV(32, R(EAX), M(&g_dsp.exception_in_progress));
		CMP(32, R(EAX), Imm32(0));
		FixupBranch noExceptionOccurred = J_CC(CC_L);
		
		//	ABI_CallFunction((void *)DSPInterpreter::HandleLoop);
		ABI_RestoreStack(0);
		RET();
		
		SetJumpTarget(skipCheck);
		SetJumpTarget(noExceptionOccurred);
		
		// Increment PC
		ADD(16, M(&(g_dsp.pc)), Imm16(1));

		WriteCallInterpreter(inst);
				
		blockSize[start_addr]++;

		// Handle loop condition.  Change to TEST
		MOVZX(32, 16, EAX, M(&(g_dsp.r[DSP_REG_ST2])));
		CMP(32, R(EAX), Imm32(0));
		FixupBranch rLoopAddressExit = J_CC(CC_LE);
		
		MOVZX(32, 16, EAX, M(&(g_dsp.r[DSP_REG_ST3])));
		CMP(32, R(EAX), Imm32(0));
		FixupBranch rLoopCounterExit = J_CC(CC_LE);

		// These functions branch and therefore only need to be called in the
		// end of each block and in this order
	    ABI_CallFunction((void *)&DSPInterpreter::HandleLoop);
		ABI_RestoreStack(0);
		RET();

		SetJumpTarget(rLoopAddressExit);
		SetJumpTarget(rLoopCounterExit);

		// End the block where the loop ends
		if ((inst & 0xffe0) == 0x0060 || (inst & 0xff00) == 0x1100) {
			// BLOOP, BLOOPI
			endBlock[dsp_imem_read(addr + 1)] = true;
		} else if ((inst & 0xffe0) == 0x0040 || (inst & 0xff00) == 0x1000) {
			// LOOP, LOOPI
			endBlock[addr + 1] = true;
		}

		if (opcode->branch || endBlock[addr] 
			|| (DSPAnalyzer::code_flags[addr] & DSPAnalyzer::CODE_IDLE_SKIP)) {
			break;
		}
		addr += opcode->size;
	}

	ABI_RestoreStack(0);
	RET();

	blocks[start_addr] = (CompiledCode)entryPoint;

	return entryPoint;
}

void STACKALIGN DSPEmitter::RunBlock(int cycles)
{
	static int idleskip = 0;
	// Trigger an external interrupt at the start of the cycle
	u16 block_cycles = 501;

	while (!(g_dsp.cr & CR_HALT))
	{
		if (block_cycles > 500)
		{
			if(g_dsp.cr & CR_EXTERNAL_INT)
				DSPCore_CheckExternalInterrupt();
			block_cycles = 0;
		}

		// Compile the block if needed
		if (blocks[g_dsp.pc] == CompileCurrent)
		{
			blockSize[g_dsp.pc] = 0;
			blocks[g_dsp.pc]();
		}
		
		// Execute the block if we have enough cycles
		if (cycles > blockSize[g_dsp.pc])
		{
			u16 start_addr = g_dsp.pc;
			if (idleskip % 100 > 95 && (DSPAnalyzer::code_flags[g_dsp.pc] & DSPAnalyzer::CODE_IDLE_SKIP)) {
				block_cycles = 0;
			} else
				blocks[g_dsp.pc]();
			
			idleskip++;

			if (idleskip % 500 == 0)
				idleskip = 0;

			block_cycles += blockSize[start_addr];
			cycles -= blockSize[start_addr];		
		}
		else {
			break;
		}
	}
}
