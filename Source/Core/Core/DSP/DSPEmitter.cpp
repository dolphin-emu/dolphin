// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstring>

#include "Core/DSP/DSPAnalyzer.h"
#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPEmitter.h"
#include "Core/DSP/DSPHost.h"
#include "Core/DSP/DSPInterpreter.h"
#include "Core/DSP/DSPMemoryMap.h"

#define MAX_BLOCK_SIZE 250
#define DSP_IDLE_SKIP_CYCLES 0x1000

using namespace Gen;

DSPEmitter::DSPEmitter() : gpr(*this), storeIndex(-1), storeIndex2(-1)
{
	m_compiledCode = nullptr;

	AllocCodeSpace(COMPILED_CODE_SIZE);

	blocks = new DSPCompiledCode[MAX_BLOCKS];
	blockLinks = new Block[MAX_BLOCKS];
	blockSize = new u16[MAX_BLOCKS];

	compileSR = 0;
	compileSR |= SR_INT_ENABLE;
	compileSR |= SR_EXT_INT_ENABLE;

	CompileDispatcher();
	stubEntryPoint = CompileStub();

	//clear all of the block references
	for (int i = 0x0000; i < MAX_BLOCKS; i++)
	{
		blocks[i] = (DSPCompiledCode)stubEntryPoint;
		blockLinks[i] = nullptr;
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

void DSPEmitter::ClearIRAM()
{
	for (int i = 0x0000; i < 0x1000; i++)
	{
		blocks[i] = (DSPCompiledCode)stubEntryPoint;
		blockLinks[i] = nullptr;
		blockSize[i] = 0;
		unresolvedJumps[i].clear();
	}
	g_dsp.reset_dspjit_codespace = true;
}

void DSPEmitter::ClearIRAMandDSPJITCodespaceReset()
{
	ClearCodeSpace();
	CompileDispatcher();
	stubEntryPoint = CompileStub();

	for (int i = 0x0000; i < 0x10000; i++)
	{
		blocks[i] = (DSPCompiledCode)stubEntryPoint;
		blockLinks[i] = nullptr;
		blockSize[i] = 0;
		unresolvedJumps[i].clear();
	}
	g_dsp.reset_dspjit_codespace = false;
}


// Must go out of block if exception is detected
void DSPEmitter::checkExceptions(u32 retval)
{
	// Check for interrupts and exceptions
	TEST(8, M(&g_dsp.exceptions), Imm8(0xff));
	FixupBranch skipCheck = J_CC(CC_Z, true);

	MOV(16, M(&(g_dsp.pc)), Imm16(compilePC));

	DSPJitRegCache c(gpr);
	gpr.saveRegs();
	ABI_CallFunction((void *)&DSPCore_CheckExceptions);
	MOV(32, R(EAX), Imm32(retval));
	JMP(returnDispatcher, true);
	gpr.loadRegs(false);
	gpr.flushRegs(c,false);

	SetJumpTarget(skipCheck);
}

bool DSPEmitter::FlagsNeeded()
{
	if (!(DSPAnalyzer::code_flags[compilePC] & DSPAnalyzer::CODE_START_OF_INST) ||
		(DSPAnalyzer::code_flags[compilePC] & DSPAnalyzer::CODE_UPDATE_SR))
		return true;
	else
		return false;
}

void DSPEmitter::Default(UDSPInstruction inst)
{
	if (opTable[inst]->reads_pc)
	{
		// Increment PC - we shouldn't need to do this for every instruction. only for branches and end of block.
		// Fallbacks to interpreter need this for fetching immediate values

		MOV(16, M(&(g_dsp.pc)), Imm16(compilePC + 1));
	}

	// Fall back to interpreter
	gpr.pushRegs();
	_assert_msg_(DSPLLE, opTable[inst]->intFunc, "No function for %04x",inst);
	ABI_CallFunctionC16((void*)opTable[inst]->intFunc, inst);
	gpr.popRegs();
}

void DSPEmitter::EmitInstruction(UDSPInstruction inst)
{
	const DSPOPCTemplate *tinst = GetOpTemplate(inst);
	bool ext_is_jit = false;

	// Call extended
	if (tinst->extended)
	{
		if ((inst >> 12) == 0x3)
		{
			if (! extOpTable[inst & 0x7F]->jitFunc)
			{
				// Fall back to interpreter
				gpr.pushRegs();
				ABI_CallFunctionC16((void*)extOpTable[inst & 0x7F]->intFunc, inst);
				gpr.popRegs();
				INFO_LOG(DSPLLE, "Instruction not JITed(ext part): %04x\n", inst);
				ext_is_jit = false;
			}
			else
			{
				(this->*extOpTable[inst & 0x7F]->jitFunc)(inst);
				ext_is_jit = true;
			}
		}
		else
		{
			if (!extOpTable[inst & 0xFF]->jitFunc)
			{
				// Fall back to interpreter
				gpr.pushRegs();
				ABI_CallFunctionC16((void*)extOpTable[inst & 0xFF]->intFunc, inst);
				gpr.popRegs();
				INFO_LOG(DSPLLE, "Instruction not JITed(ext part): %04x\n", inst);
				ext_is_jit = false;
			}
			else
			{
				(this->*extOpTable[inst & 0xFF]->jitFunc)(inst);
				ext_is_jit = true;
			}
		}
	}

	// Main instruction
	if (!opTable[inst]->jitFunc)
	{
		Default(inst);
		INFO_LOG(DSPLLE, "Instruction not JITed(main part): %04x\n", inst);
	}
	else
	{
		(this->*opTable[inst]->jitFunc)(inst);
	}

	// Backlog
	if (tinst->extended)
	{
		if (!ext_is_jit)
		{
			//need to call the online cleanup function because
			//the writeBackLog gets populated at runtime
			gpr.pushRegs();
			ABI_CallFunction((void*)::applyWriteBackLog);
			gpr.popRegs();
		}
		else
		{
			popExtValueToReg();
		}
	}
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

	gpr.loadRegs();

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
			MOVZX(32, 16, EAX, M(&(g_dsp.r.st[2])));
			CMP(32, R(EAX), Imm32(0));
			FixupBranch rLoopAddressExit = J_CC(CC_LE, true);

			MOVZX(32, 16, EAX, M(&g_dsp.r.st[3]));
			CMP(32, R(EAX), Imm32(0));
			FixupBranch rLoopCounterExit = J_CC(CC_LE, true);

			if (!opcode->branch)
			{
				//branch insns update the g_dsp.pc
				MOV(16, M(&(g_dsp.pc)), Imm16(compilePC));
			}

			// These functions branch and therefore only need to be called in the
			// end of each block and in this order
			DSPJitRegCache c(gpr);
			HandleLoop();
			gpr.saveRegs();
			if (!DSPHost::OnThread() && DSPAnalyzer::code_flags[start_addr] & DSPAnalyzer::CODE_IDLE_SKIP)
			{
				MOV(16, R(EAX), Imm16(DSP_IDLE_SKIP_CYCLES));
			}
			else
			{
				MOV(16, R(EAX), Imm16(blockSize[start_addr]));
			}
			JMP(returnDispatcher, true);
			gpr.loadRegs(false);
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
				MOV(16, R(AX), M(&g_dsp.pc));
				CMP(16, R(AX), Imm16(compilePC));
				FixupBranch rNoBranch = J_CC(CC_Z, true);

				DSPJitRegCache c(gpr);
				//don't update g_dsp.pc -- the branch insn already did
				gpr.saveRegs();
				if (!DSPHost::OnThread() && DSPAnalyzer::code_flags[start_addr] & DSPAnalyzer::CODE_IDLE_SKIP)
				{
					MOV(16, R(EAX), Imm16(DSP_IDLE_SKIP_CYCLES));
				}
				else
				{
					MOV(16, R(EAX), Imm16(blockSize[start_addr]));
				}
				JMP(returnDispatcher, true);
				gpr.loadRegs(false);
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

	if (fixup_pc)
	{
		MOV(16, M(&(g_dsp.pc)), Imm16(compilePC));
	}

	blocks[start_addr] = (DSPCompiledCode)entryPoint;

	// Mark this block as a linkable destination if it does not contain
	// any unresolved CALL's
	if (unresolvedJumps[start_addr].empty())
	{
		blockLinks[start_addr] = blockLinkEntry;

		for (u16 i = 0x0000; i < 0xffff; ++i)
		{
			if (!unresolvedJumps[i].empty())
			{
				// Check if there were any blocks waiting for this block to be linkable
				size_t size = unresolvedJumps[i].size();
				unresolvedJumps[i].remove(start_addr);
				if (unresolvedJumps[i].size() < size)
				{
					// Mark the block to be recompiled again
					blocks[i] = (DSPCompiledCode)stubEntryPoint;
					blockLinks[i] = nullptr;
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

	gpr.saveRegs();
	if (!DSPHost::OnThread() && DSPAnalyzer::code_flags[start_addr] & DSPAnalyzer::CODE_IDLE_SKIP)
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
	XOR(32, R(EAX), R(EAX)); // Return 0 cycles executed
	JMP(returnDispatcher);
	return entryPoint;
}

void DSPEmitter::CompileDispatcher()
{
	enterDispatcher = AlignCode16();
	// We don't use floating point (high 16 bits).
	BitSet32 registers_used = ABI_ALL_CALLEE_SAVED & BitSet32(0xffff);
	ABI_PushRegistersAndAdjustStack(registers_used, 8);

	const u8 *dispatcherLoop = GetCodePtr();

	FixupBranch exceptionExit;
	if (DSPHost::OnThread())
	{
		CMP(8, M(const_cast<bool*>(&g_dsp.external_interrupt_waiting)), Imm8(0));
		exceptionExit = J_CC(CC_NE);
	}

	// Check for DSP halt
	TEST(8, M(&g_dsp.cr), Imm8(CR_HALT));
	FixupBranch _halt = J_CC(CC_NE);


	// Execute block. Cycles executed returned in EAX.
	MOVZX(64, 16, ECX, M(&g_dsp.pc));
	MOV(64, R(RBX), ImmPtr(blocks));
	JMPptr(MComplex(RBX, RCX, SCALE_8, 0));

	returnDispatcher = GetCodePtr();

	// Decrement cyclesLeft
	SUB(16, M(&cyclesLeft), R(EAX));

	J_CC(CC_A, dispatcherLoop);

	// DSP gave up the remaining cycles.
	SetJumpTarget(_halt);
	if (DSPHost::OnThread())
	{
		SetJumpTarget(exceptionExit);
	}
	//MOV(32, M(&cyclesLeft), Imm32(0));
	ABI_PopRegistersAndAdjustStack(registers_used, 8);
	RET();
}
