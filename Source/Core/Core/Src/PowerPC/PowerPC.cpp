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

#include "Common.h"
#include "ChunkFile.h"

#include "../HW/Memmap.h"
#include "../HW/CPU.h"
#include "../Core.h"
#include "../CoreTiming.h"

#include "Interpreter/Interpreter.h"
#include "Jit64/JitCore.h"
#include "Jit64/JitCache.h"
#include "PowerPC.h"
#include "PPCTables.h"

#include "../Host.h"

namespace PowerPC
{
	// STATE_TO_SAVE
	PowerPCState GC_ALIGNED16(ppcState);
	volatile CPUState state = CPU_STEPPING;

	static CoreMode mode;

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
		Jit64::Core::Init();
		// ... but start as interpreter by default.
		_mode = MODE_INTERPRETER;
		state = CPU_STEPPING;
	}

	void Shutdown()
	{
		// Shutdown both execution engines. Doesn't matter which one is active.
		Jit64::Core::Shutdown();
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
			Jit64::ClearCache();  // Remove all those nasty JIT patches.
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
			Jit64::Core::SingleStep();
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
			Jit64::Core::Run();
			break;
		}
        Host_UpdateDisasmDialog();
	}

	void Start()
	{
		state = Core::g_CoreStartupParameter.bEnableDebugging ? CPU_RUNNINGDEBUG : CPU_RUNNING;
		if (Core::bReadTrace || Core::bWriteTrace)
		{
			state = CPU_RUNNING;
		}
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

		// TODO(ector): 
		// gcemu uses the mask 0x87C0FFFF instead of 0x0780FF77
        // Investigate!

		if (ppcState.Exceptions & EXCEPTION_FPU_UNAVAILABLE)
		{			
			//This happens a lot - Gamecube OS uses deferred FPU context switching
			SRR0 = PC;	// re-execute the instruction
			SRR1 = MSR & 0x0780FF77;
			NPC = 0x80000800;

			LOG(GEKKO, "EXCEPTION_FPU_UNAVAILABLE");
			ppcState.Exceptions &= ~EXCEPTION_FPU_UNAVAILABLE;
			SRR1 |= 0x02;  //recoverable
		}
		else if (ppcState.Exceptions & EXCEPTION_SYSCALL)
		{	
			SRR0 = NPC; // execute next instruction when we come back from handler
			SRR1 = MSR & 0x0780FF77;
			NPC = 0x80000C00;

			LOG(GEKKO, "EXCEPTION_SYSCALL (PC=%08x)",PC);
			ppcState.Exceptions &= ~EXCEPTION_SYSCALL;
			SRR1 |= 0x02;  //recoverable
		}
		else if (ppcState.Exceptions & EXCEPTION_DSI)
		{
			SRR0 = PC;  // re-execute the instruction
			SRR1 = MSR & 0x0780FF77; 
			NPC = 0x80000300;

			LOG(GEKKO, "EXCEPTION_DSI");
			ppcState.Exceptions &= ~EXCEPTION_DSI;			
			//SRR1 |= 0x02;  //make recoverable ?
		}
		else if (ppcState.Exceptions & EXCEPTION_ISI)
		{
			SRR0 = PC;
			SRR1 = (MSR & 0x0780FF77) | 0x40000000;
			NPC = 0x80000400;

			LOG(GEKKO, "EXCEPTION_ISI");
			ppcState.Exceptions &= ~EXCEPTION_ISI;			
			//SRR1 |= 0x02;  //make recoverable ?
		}
		else if (ppcState.Exceptions & EXCEPTION_ALIGNMENT)
		{
			//This never happens ATM
			SRR0 = NPC;
			SRR1 = MSR & 0x0780FF77;
			NPC = 0x80000600;

			LOG(GEKKO, "EXCEPTION_ALIGNMENT");
			ppcState.Exceptions &= ~EXCEPTION_ALIGNMENT;			
			//SRR1 |= 0x02;  //make recoverable ?
		}
		
		// EXTERNAL INTTERUPT
		else if (MSR & 0x0008000)
		{
			if (ppcState.Exceptions & EXCEPTION_EXTERNAL_INT)
			{
				// Pokemon gets this "too early", it hasn't a handler yet
				ppcState.Exceptions &= ~EXCEPTION_EXTERNAL_INT;	// clear exception

				SRR0 = NPC;
				NPC = 0x80000500;
				SRR1 = (MSR & 0x0780FF77);
				
				LOG(GEKKO, "EXCEPTION_EXTERNAL_INT");

				SRR1 |= 0x02; //set it to recoverable
				_dbg_assert_msg_(GEKKO, (SRR1 & 0x02) != 0, "GEKKO", "EXTERNAL_INT unrecoverable???");  // unrecoverable exception !?!
			}
			else if (ppcState.Exceptions & EXCEPTION_DECREMENTER)
			{
				SRR0 = NPC;
				SRR1 = MSR & 0x0000FF77;
				NPC = 0x80000900;

				ppcState.Exceptions &= ~EXCEPTION_DECREMENTER;

				LOG(GEKKO, "EXCEPTION_DECREMENTER");
				SRR1 |= 0x02;  //make recoverable
			}
			else
			{
				_dbg_assert_msg_(GEKKO, 0, "Unknown EXT interrupt: Exceptions == %08x", ppcState.Exceptions);
				LOG(GEKKO, "Unknown EXTERNAL INTERRUPT exception: Exceptions == %08x", ppcState.Exceptions);
			}
		}
		MSR &= ~0x0008000;		// clear EE-bit so interrupts aren't possible anymore
	}

	void OnIdle(u32 _uThreadAddr)
	{
		u32 nextThread = Memory::Read_U32(_uThreadAddr);
		//do idle skipping
		if (nextThread == 0)
		{
			CoreTiming::Idle();
		}
	}
}

