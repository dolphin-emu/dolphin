// Copyright (C) 2003-2009 Dolphin Project.

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

#include "../../HW/Memmap.h"
#include "../../HW/CPU.h"
#include "../../Host.h"
#include "../PPCTables.h"
#include "Interpreter.h"
#include "../../Debugger/Debugger_SymbolMap.h"
#include "../../CoreTiming.h"
#include "../../Core.h"
#include "PowerPCDisasm.h"
#include "../../IPC_HLE/WII_IPC_HLE.h"


namespace {
	u32 last_pc;
}

// function tables

namespace Interpreter
{
// cpu register to keep the code readable
u32 *m_GPR = PowerPC::ppcState.gpr;
bool m_EndBlock = false;

_interpreterInstruction m_opTable[64];
_interpreterInstruction m_opTable4[1024];
_interpreterInstruction m_opTable19[1024];
_interpreterInstruction m_opTable31[1024];
_interpreterInstruction m_opTable59[32];
_interpreterInstruction m_opTable63[1024];

void RunTable4(UGeckoInstruction _inst)  {m_opTable4 [_inst.SUBOP10](_inst);}
void RunTable19(UGeckoInstruction _inst) {m_opTable19[_inst.SUBOP10](_inst);}
void RunTable31(UGeckoInstruction _inst) {m_opTable31[_inst.SUBOP10](_inst);}
void RunTable59(UGeckoInstruction _inst) {m_opTable59[_inst.SUBOP5 ](_inst);}
void RunTable63(UGeckoInstruction _inst) {m_opTable63[_inst.SUBOP10](_inst);}

void Init()
{
}

void Shutdown()
{
}

void patches()
{
/*	if (Memory::Read_U16(0x90000880) == 0x130b)
	{
		PanicAlert("Memory::Read_U16(0x900008800) == 0x130b");
	}
*/
/*	if (PC == 0x80074cd4)
	{
		u16 command = Common::swap16(Memory::Read_U16(PowerPC::ppcState.gpr[3] + 8));
		if (command == 0x0b13)
		{
			PanicAlert("command: %x", command);
		}
	}*/
}

void SingleStepInner(void)
{
	static UGeckoInstruction instCode;

	NPC = PC + sizeof(UGeckoInstruction);
	instCode.hex = Memory::Read_Opcode(PC); 

	UReg_MSR& msr = (UReg_MSR&)MSR;
	if (msr.FP)  //If FPU is enabled, just execute
		m_opTable[instCode.OPCD](instCode);
	else
	{
		// check if we have to generate a FPU unavailable exception
		if (!PPCTables::UsesFPU(instCode))
			m_opTable[instCode.OPCD](instCode);
		else
		{
			PowerPC::ppcState.Exceptions |= EXCEPTION_FPU_UNAVAILABLE;
			PowerPC::CheckExceptions();
			m_EndBlock = true;
		}

	}
	last_pc = PC;
	PC = NPC;
	
	if (PowerPC::ppcState.gpr[1] == 0)
	{
		printf("%i Corrupt stack", PowerPC::ppcState.DebugCount);
	}
#if defined(_DEBUG) || defined(DEBUGFAST)
	PowerPC::ppcState.DebugCount++;
#endif
    patches();
}

void SingleStep()
{	
	SingleStepInner();
	
	CoreTiming::slicelength = 1;
	CoreTiming::downcount = 0;
	CoreTiming::Advance();

	if (PowerPC::ppcState.Exceptions)
	{
		PowerPC::CheckExceptions();
		PC = NPC;
	}
}

//#define SHOW_HISTORY
#ifdef SHOW_HISTORY
std::vector <int> PCVec;
std::vector <int> PCBlockVec;
int ShowBlocks = 30;
int ShowSteps = 300;
#endif

// FastRun - inspired by GCemu (to imitate the JIT so that they can be compared).
void Run()
{
	while (!PowerPC::GetState())
	{
		//we have to check exceptions at branches apparently (or maybe just rfi?)
		if (Core::GetStartupParameter().bEnableDebugging)
		{
			#ifdef SHOW_HISTORY
				PCBlockVec.push_back(PC);
				if (PCBlockVec.size() > ShowBlocks)
					PCBlockVec.erase(PCBlockVec.begin());
			#endif

			// Debugging friendly version of inner loop. Tries to do the timing as similarly to the
			// JIT as possible. Does not take into account that some instructions take multiple cycles.
			while (CoreTiming::downcount > 0)
			{
				m_EndBlock = false;
				int i;
				for (i = 0; !m_EndBlock; i++)
				{
					#ifdef SHOW_HISTORY
						PCVec.push_back(PC);
						if (PCVec.size() > ShowSteps)
							PCVec.erase(PCVec.begin());
					#endif

					//2: check for breakpoint
					if (PowerPC::breakpoints.IsAddressBreakPoint(PC))
					{
						#ifdef SHOW_HISTORY
							NOTICE_LOG(POWERPC, "----------------------------");
							NOTICE_LOG(POWERPC, "Blocks:");
							for (int j = 0; j < PCBlockVec.size(); j++)
								NOTICE_LOG(POWERPC, "PC: 0x%08x", PCBlockVec.at(j));
							NOTICE_LOG(POWERPC, "----------------------------");
							NOTICE_LOG(POWERPC, "Steps:");
							for (int j = 0; j < PCVec.size(); j++)
							{
								// Write space
								if (j > 0)
									if (PCVec.at(j) != PCVec.at(j-1) + 4)
										NOTICE_LOG(POWERPC, "");

								NOTICE_LOG(POWERPC, "PC: 0x%08x", PCVec.at(j));
							}
						#endif
						INFO_LOG(POWERPC, "Hit Breakpoint - %08x", PC);
						CCPU::Break();
						if (PowerPC::breakpoints.IsTempBreakPoint(PC))
							PowerPC::breakpoints.Remove(PC);

						Host_UpdateDisasmDialog();
						return;
					}
					SingleStepInner();
				}
				CoreTiming::downcount -= i;
			}
		}
		else
		{
			// "fast" version of inner loop. well, it's not so fast.
			while (CoreTiming::downcount > 0)
			{
				m_EndBlock = false;
				int i;
				for (i = 0; !m_EndBlock; i++)
				{
					SingleStepInner();
				}
				CoreTiming::downcount -= i;
			}
		}

		CoreTiming::Advance();

		if (PowerPC::ppcState.Exceptions)
		{
			PowerPC::CheckExceptions();
			PC = NPC;
		}
	}
}

void unknown_instruction(UGeckoInstruction _inst)
{
	char disasm[256];
	DisassembleGekko(Memory::ReadUnchecked_U32(last_pc), last_pc, disasm, 256);
	printf("Last PC = %08x : %s\n", last_pc, disasm);
	Debugger::PrintCallstack();
	_dbg_assert_msg_(POWERPC, 0, "\nIntCPU: Unknown instr %08x at PC = %08x  last_PC = %08x  LR = %08x\n", _inst.hex, PC, last_pc, LR);
}

}  // namespace
