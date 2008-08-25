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

#include <float.h>

#include "../../HW/Memmap.h"
#include "../../HW/CPU.h"
#include "../PPCTables.h"
#include "Interpreter.h"
#include "../../Debugger/Debugger_SymbolMap.h"
#include "../../CoreTiming.h"
#include "../../Core.h"
#include "PowerPCDisasm.h"
#include "../../IPC_HLE/WII_IPC_HLE.h"

enum {
	FPU_PREC_24 = 0 << 8,
	FPU_PREC_53 = 2 << 8,
	FPU_PREC_64 = 3 << 8,
	FPU_PREC_MASK = 3 << 8,
};

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


//#include "../../Plugins/Plugin_DSP.h"
void patches()
{
//     if (Memory::Read_U32(0x80095AC0) != -1)  CCPU::Break();
  //  if (Memory::Read_U32(0x8003E574) != 0)  CCPU::Break();

    //	if (PC == 0x800077e8)
/*    u32 op = Memory::Read_U32(0x80015180);
    if ((op != 0xc0028218) && (op != 0))
    {
        PanicAlert("hrehre %x", op);
        CCPU::Break();
    } */

     // if (PC == 0x80022588) CCPU::Break(); 

//    WII_IPC_HLE_Interface::Update();

}
void SingleStepInner(void)
{
/*	static int count = 0;
	count++;
	if ((count % 50) == 0)
		PluginDSP::DSP_Update(); */
	static UGeckoInstruction instCode;

	NPC = PC + sizeof(UGeckoInstruction);
	instCode.hex = Memory::ReadFast32(PC);  //  Memory::ReadUnchecked_U32(PC);

    //Memory::Read_Instruction(PC);	// use the memory functions to read from the memory !!!!!!
	//if (PowerPC::ppcState.DebugCount > 0x10f233a) { // 50721ef253a
	//	printf("> %08x - %08x - %s\n", PC, instCode.hex, DisassembleGekko(instCode.hex, PC));
	//}

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
	
	if (PowerPC::ppcState.gpr[1] == 0) {
		printf("%i Corrupt stack", PowerPC::ppcState.DebugCount);
//		CCPU::Break();	
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

// sFastRun - inspired by GCemu
void Run()
{
	while (!PowerPC::state)
	{
		//we have to check exceptions at branches apparently (or maybe just rfi?)
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
	CCPU::Break();
	printf("Last PC = %08x : %s\n", last_pc, DisassembleGekko(Memory::ReadUnchecked_U32(last_pc), last_pc));
	Debugger::PrintCallstack();
	_dbg_assert_msg_(GEKKO, 0, "\nIntCPU: Unknown instr %08x at PC = %08x  last_PC = %08x  LR = %08x\n", _inst.hex, PC, last_pc, LR);
}

}  // namespace