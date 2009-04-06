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

#include <float.h>

#include "Common.h"
#include "ChunkFile.h"

#include "../HW/Memmap.h"
#include "../HW/CPU.h"
#include "../Core.h"
#include "../CoreTiming.h"

#include "Interpreter/Interpreter.h"
#include "Jit64/Jit.h"
#include "PowerPC.h"
#include "PPCTables.h"

#include "../Host.h"

namespace PowerPC
{

// STATE_TO_SAVE
PowerPCState GC_ALIGNED16(ppcState);
volatile CPUState state = CPU_STEPPING;

static CoreMode mode;

void CompactCR()
{
	ppcState.cr = 0;
	for (int i = 0; i < 8; i++) {
		ppcState.cr |= ppcState.cr_fast[i] << (28 - i * 4);
	}
}

void ExpandCR()
{
	for (int i = 0; i < 8; i++) {
		ppcState.cr_fast[i] = (ppcState.cr >> (28 - i * 4)) & 0xF;
	}
}


void DoState(PointerWrap &p)
{
	p.Do(ppcState);
}

void ResetRegisters()
{
	for (int i = 0; i < 32; i++)
	{
		ppcState.gpr[i] = 0;
		riPS0(i) = 0;
		riPS1(i) = 0;
	}

	memset(ppcState.spr, 0, sizeof(ppcState.spr));

	ppcState.cr = 0;
	ppcState.fpscr = 0;
	ppcState.pc = 0;
	ppcState.npc = 0;
	ppcState.Exceptions = 0;

	TL = 0;
	TU = 0;

	ppcState.msr = 0;
	rDEC = 0xFFFFFFFF;
}

void Init()
{
	enum {
		FPU_PREC_24 = 0 << 8,
		FPU_PREC_53 = 2 << 8,
		FPU_PREC_64 = 3 << 8,
		FPU_PREC_MASK = 3 << 8,
	};
#ifdef _M_IX86
	// sets the floating-point lib to 53-bit
	// PowerPC has a 53bit floating pipeline only
	// eg: sscanf is very sensitive
#ifdef _WIN32
	_control87(_PC_53, MCW_PC);
#else
	unsigned short _mode;
	asm ("fstcw %0" : : "m" (_mode));
	_mode = (_mode & ~FPU_PREC_MASK) | FPU_PREC_53;
	asm ("fldcw %0" : : "m" (_mode));
#endif
#else
	//x64 doesn't need this - fpu is done with SSE
	//but still - set any useful sse options here
#endif

	ResetRegisters();
	PPCTables::InitTables();

	// Initialize both execution engines ... 
	Interpreter::Init();
	jit.Init();
	// ... but start as interpreter by default.
	mode = MODE_INTERPRETER;
	state = CPU_STEPPING;
}

void Shutdown()
{
	// Shutdown both execution engines. Doesn't matter which one is active.
	jit.Shutdown();
	Interpreter::Shutdown();
}

void SetMode(CoreMode new_mode)
{
	if (new_mode == mode)
		return;  // We don't need to do anything.

	mode = new_mode;
	switch (mode)
	{
	case MODE_INTERPRETER:  // Switching from JIT to interpreter
		jit.ClearCache();  // Remove all those nasty JIT patches.
		break;

	case MODE_JIT:  // Switching from interpreter to JIT.
		// Don't really need to do much. It'll work, the cache will refill itself.
		break;
	}
}

void SingleStep() 
{
	switch (mode)
	{
	case MODE_INTERPRETER:
		Interpreter::SingleStep();
		break;
	case MODE_JIT:
		jit.SingleStep();
		break;
	}
}

void RunLoop()
{
	state = CPU_RUNNING;
	switch (mode) 
	{
	case MODE_INTERPRETER:
		Interpreter::Run();
		break;
	case MODE_JIT:
		jit.Run();
		break;
	}
	Host_UpdateDisasmDialog();
}

CPUState GetState()
{
	return state;
}

volatile CPUState *GetStatePtr()
{
	return &state;
}

void Start()
{
	state = CPU_RUNNING;
	Host_UpdateDisasmDialog();
}

void Pause()
{
	state = CPU_STEPPING;
	Host_UpdateDisasmDialog();
}

void Stop()
{
	state = CPU_POWERDOWN;
	Host_UpdateDisasmDialog();
}

void CheckExceptions()
{
	// This check is unnecessary in JIT mode. However, it probably doesn't really hurt.
	if (!ppcState.Exceptions)
		return;

	// gcemu uses the mask 0x87C0FFFF instead of 0x0780FF77
	// shuffle2: the MSR bits saved to SRR1 depend on the type of
	// exception being taken, the common mask is 0x87C00008.
	// I guess gcemu just uses 0x87C0FFFF for simplicity
	// I think we should too, or else we would have to check what type of
	// exception a rfi is returning from - I doubt a real cpu does this

	// Example procedure:
	// set SRR0 to either PC or NPC
	//SRR0 = NPC;
	// save specified MSR bits
	//SRR1 = MSR & 0x87C0FFFF;
	// copy ILE bit to LE
	//MSR |= (MSR >> 17) & 1;
	// clear MSR as specified
	//MSR &= ~0x04EF36; // 0x04FF36 also clears ME (only for machine check exception)
	// set to exception type entry point
	//NPC = 0x80000x00;

	if (ppcState.Exceptions & EXCEPTION_ISI)
	{
		SRR0 = NPC;
		//GenerateISIException() sets up SRR1
		SRR1 |= MSR & 0x87C0FFFF;
		MSR |= (MSR >> 17) & 1;
		MSR &= ~0x04EF36;
		NPC = 0x80000400;

		INFO_LOG(GEKKO, "EXCEPTION_ISI");
		ppcState.Exceptions &= ~EXCEPTION_ISI;
	}
	else if (ppcState.Exceptions & EXCEPTION_PROGRAM)
	{
		SRR0 = PC;
		SRR1 = MSR & 0x87C0FFFF;
		// say that it's a trap exception
		SRR1 |= 0x40000;
		MSR |= (MSR >> 17) & 1;
		MSR &= ~0x04EF36;
		NPC = 0x80000700;

		INFO_LOG(GEKKO, "EXCEPTION_PROGRAM");
		ppcState.Exceptions &= ~EXCEPTION_PROGRAM;
	} 
	else if (ppcState.Exceptions & EXCEPTION_SYSCALL)
	{
		SRR0 = NPC;
		SRR1 = MSR & 0x87C0FFFF;
		MSR |= (MSR >> 17) & 1;
		MSR &= ~0x04EF36;
		NPC = 0x80000C00;

		INFO_LOG(GEKKO, "EXCEPTION_SYSCALL (PC=%08x)", PC);
		ppcState.Exceptions &= ~EXCEPTION_SYSCALL;
	}
	else if (ppcState.Exceptions & EXCEPTION_FPU_UNAVAILABLE)
	{			
		//This happens a lot - Gamecube OS uses deferred FPU context switching
		SRR0 = PC;	// re-execute the instruction
		SRR1 = MSR & 0x87C0FFFF;
		MSR |= (MSR >> 17) & 1;
		MSR &= ~0x04EF36;
		NPC = 0x80000800;

		INFO_LOG(GEKKO, "EXCEPTION_FPU_UNAVAILABLE");
		ppcState.Exceptions &= ~EXCEPTION_FPU_UNAVAILABLE;
	}
	else if (ppcState.Exceptions & EXCEPTION_DSI)
	{
		SRR0 = PC;
		SRR1 = MSR & 0x87C0FFFF;
		MSR |= (MSR >> 17) & 1;
		MSR &= ~0x04EF36;
		NPC = 0x80000300;
		//DSISR and DAR regs are changed in GenerateDSIException()

		INFO_LOG(GEKKO, "EXCEPTION_DSI");
		ppcState.Exceptions &= ~EXCEPTION_DSI;
	} 
	else if (ppcState.Exceptions & EXCEPTION_ALIGNMENT)
	{
		//This never happens ATM
		// perhaps we can get dcb* instructions to use this :p
		SRR0 = PC;
		SRR1 = MSR & 0x87C0FFFF;
		MSR |= (MSR >> 17) & 1;
		MSR &= ~0x04EF36;
		NPC = 0x80000600;

		//TODO crazy amount of DSISR options to check out

		INFO_LOG(GEKKO, "EXCEPTION_ALIGNMENT");
		ppcState.Exceptions &= ~EXCEPTION_ALIGNMENT;
	}

	// EXTERNAL INTTERUPT
	else if (MSR & 0x0008000) //hacky...the exception shouldn't be generated if EE isn't set...
	{
		if (ppcState.Exceptions & EXCEPTION_EXTERNAL_INT)
		{
			// Pokemon gets this "too early", it hasn't a handler yet
			SRR0 = NPC;
			SRR1 = MSR & 0x87C0FFFF;
			MSR |= (MSR >> 17) & 1;
			MSR &= ~0x04EF36;
			NPC = 0x80000500;

			INFO_LOG(GEKKO, "EXCEPTION_EXTERNAL_INT");
			ppcState.Exceptions &= ~EXCEPTION_EXTERNAL_INT;

			_dbg_assert_msg_(GEKKO, (SRR1 & 0x02) != 0, "GEKKO", "EXTERNAL_INT unrecoverable???");
		}
		else if (ppcState.Exceptions & EXCEPTION_DECREMENTER)
		{
			SRR0 = NPC;
			SRR1 = MSR & 0x87C0FFFF;
			MSR |= (MSR >> 17) & 1;
			MSR &= ~0x04EF36;
			NPC = 0x80000900;

			INFO_LOG(GEKKO, "EXCEPTION_DECREMENTER");
			ppcState.Exceptions &= ~EXCEPTION_DECREMENTER;
		}
		else
		{
			_dbg_assert_msg_(GEKKO, 0, "Unknown EXT interrupt: Exceptions == %08x", ppcState.Exceptions);
			ERROR_LOG(GEKKO, "Unknown EXTERNAL INTERRUPT exception: Exceptions == %08x", ppcState.Exceptions);
		}
	}
}

void OnIdle(u32 _uThreadAddr)
{
	u32 nextThread = Memory::Read_U32(_uThreadAddr);
	//do idle skipping
	if (nextThread == 0)
		CoreTiming::Idle();
}

}  // namespace