// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Core/ConfigManager.h"
#include "Core/HLE/HLE.h"
#include "Core/PowerPC/CachedInterpreter.h"
#include "Core/PowerPC/PowerPC.h"

void CachedInterpreter::Init()
{
	m_code.reserve(CODE_SIZE / sizeof(Instruction));

	jo.enableBlocklink = false;

	JitBaseBlockCache::Init();

	code_block.m_stats = &js.st;
	code_block.m_gpa = &js.gpa;
	code_block.m_fpa = &js.fpa;
}

void CachedInterpreter::Shutdown()
{
	JitBaseBlockCache::Shutdown();
}

void CachedInterpreter::Run()
{
	while (!PowerPC::GetState())
	{
		SingleStep();
	}

	// Let the waiting thread know we are done leaving
	PowerPC::FinishStateMove();
}

void CachedInterpreter::SingleStep()
{
	int block = GetBlockNumberFromStartAddress(PC);
	if (block >= 0)
	{
		Instruction* code = (Instruction*)GetCompiledCodeFromBlock(block);

		while (true)
		{
			switch (code->type)
			{
			case Instruction::INSTRUCTION_ABORT:
				return;

			case Instruction::INSTRUCTION_TYPE_COMMON:
				code->common_callback(UGeckoInstruction(code->data));
				code++;
				break;

			case Instruction::INSTRUCTION_TYPE_CONDITIONAL:
				bool ret = code->conditional_callback(code->data);
				code++;
				if (ret)
					return;
				break;
			}
		}
	}

	Jit(PC);
}

static void EndBlock(UGeckoInstruction data)
{
	PC = NPC;
	PowerPC::CheckExceptions();
	PowerPC::ppcState.downcount -= data.hex;
	if (PowerPC::ppcState.downcount <= 0)
	{
		CoreTiming::Advance();
	}
}

static void WritePC(UGeckoInstruction data)
{
	PC = data.hex;
	NPC = data.hex + 4;
}

static bool CheckFPU(u32 data)
{
	UReg_MSR& msr = (UReg_MSR&)MSR;
	if (!msr.FP)
	{
		PC = NPC = data;
		PowerPC::ppcState.Exceptions |= EXCEPTION_FPU_UNAVAILABLE;
		PowerPC::CheckExceptions();
		return true;
	}
	return false;
}

void CachedInterpreter::Jit(u32 address)
{
	if (m_code.size() >= CODE_SIZE / sizeof(Instruction) - 0x1000 || IsFull() || SConfig::GetInstance().bJITNoBlockCache)
	{
		ClearCache();
	}

	u32 nextPC = analyzer.Analyze(PC, &code_block, &code_buffer, code_buffer.GetSize());
	if (code_block.m_memory_exception)
	{
		// Address of instruction could not be translated
		NPC = nextPC;
		PowerPC::ppcState.Exceptions |= EXCEPTION_ISI;
		PowerPC::CheckExceptions();
		WARN_LOG(POWERPC, "ISI exception at 0x%08x", nextPC);
		return;
	}

	int block_num = AllocateBlock(PC);
	JitBlock *b = GetBlock(block_num);

	js.blockStart = PC;
	js.firstFPInstructionFound = false;
	js.fifoBytesThisBlock = 0;
	js.downcountAmount = 0;
	js.curBlock = b;

	PPCAnalyst::CodeOp *ops = code_buffer.codebuffer;

	b->checkedEntry = GetCodePtr();
	b->normalEntry = GetCodePtr();
	b->runCount = 0;

	for (u32 i = 0; i < code_block.m_num_instructions; i++)
	{
		js.downcountAmount += ops[i].opinfo->numCycles;

		u32 function = HLE::GetFunctionIndex(ops[i].address);
		if (function != 0)
		{
			int type = HLE::GetFunctionTypeByIndex(function);
			if (type == HLE::HLE_HOOK_START || type == HLE::HLE_HOOK_REPLACE)
			{
				int flags = HLE::GetFunctionFlagsByIndex(function);
				if (HLE::IsEnabled(flags))
				{
					m_code.emplace_back(WritePC, ops[i].address);
					m_code.emplace_back(Interpreter::HLEFunction, ops[i].inst);
					if (type == HLE::HLE_HOOK_REPLACE)
					{
						m_code.emplace_back(EndBlock, js.downcountAmount);
						m_code.emplace_back();
						break;
					}
				}
			}
		}

		if (!ops[i].skip)
		{
			if ((ops[i].opinfo->flags & FL_USE_FPU) && !js.firstFPInstructionFound)
			{
				m_code.emplace_back(CheckFPU, ops[i].address);
				js.firstFPInstructionFound = true;
			}

			if (ops[i].opinfo->flags & FL_ENDBLOCK)
				m_code.emplace_back(WritePC, ops[i].address);
			m_code.emplace_back(GetInterpreterOp(ops[i].inst), ops[i].inst);
			if (ops[i].opinfo->flags & FL_ENDBLOCK)
				m_code.emplace_back(EndBlock, js.downcountAmount);
		}
	}
	if (code_block.m_broken)
	{
		m_code.emplace_back(WritePC, nextPC);
		m_code.emplace_back(EndBlock, js.downcountAmount);
	}
	m_code.emplace_back();

	b->codeSize = (u32)(GetCodePtr() - b->checkedEntry);
	b->originalSize = code_block.m_num_instructions;

	FinalizeBlock(block_num, jo.enableBlocklink, b->checkedEntry);
}

void CachedInterpreter::ClearCache()
{
	m_code.clear();
	JitBaseBlockCache::Clear();
}

void CachedInterpreter::WriteDestroyBlock(const u8* location, u32 address)
{
}

void CachedInterpreter::WriteLinkBlock(u8* location, const u8* address)
{
}
