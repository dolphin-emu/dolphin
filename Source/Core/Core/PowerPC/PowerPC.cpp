// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Atomic.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/FPURoundMode.h"
#include "Common/MathUtil.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/Host.h"
#include "Core/HW/CPU.h"
#include "Core/HW/EXI.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"

#include "Core/PowerPC/CPUCoreBase.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"


CPUCoreBase *cpu_core_base;

namespace PowerPC
{

// STATE_TO_SAVE
PowerPCState GC_ALIGNED16(ppcState);
static volatile CPUState state = CPU_POWERDOWN;

Interpreter * const interpreter = Interpreter::getInstance();
static CoreMode mode;

BreakPoints breakpoints;
MemChecks memchecks;
PPCDebugInterface debug_interface;

u32 CompactCR()
{
	u32 new_cr = 0;
	for (int i = 0; i < 8; i++)
	{
		new_cr |= GetCRField(i) << (28 - i * 4);
	}
	return new_cr;
}

void ExpandCR(u32 cr)
{
	for (int i = 0; i < 8; i++)
	{
		SetCRField(i, (cr >> (28 - i * 4)) & 0xF);
	}
}

void DoState(PointerWrap &p)
{
	// some of this code has been disabled, because
	// it changes registers even in MODE_MEASURE (which is suspicious and seems like it could cause desyncs)
	// and because the values it's changing have been added to CoreTiming::DoState, so it might conflict to mess with them here.

	// rSPR(SPR_DEC) = SystemTimers::GetFakeDecrementer();
	// *((u64 *)&TL) = SystemTimers::GetFakeTimeBase(); //works since we are little endian and TL comes first :)

	p.DoPOD(ppcState);

	// SystemTimers::DecrementerSet();
	// SystemTimers::TimeBaseSet();

	JitInterface::DoState(p);
}

static void ResetRegisters()
{
	memset(ppcState.ps, 0, sizeof(ppcState.ps));
	memset(ppcState.gpr, 0, sizeof(ppcState.gpr));
	memset(ppcState.spr, 0, sizeof(ppcState.spr));
	/*
	0x00080200 = lonestar 2.0
	0x00088202 = lonestar 2.2
	0x70000100 = gekko 1.0
	0x00080100 = gekko 2.0
	0x00083203 = gekko 2.3a
	0x00083213 = gekko 2.3b
	0x00083204 = gekko 2.4
	0x00083214 = gekko 2.4e (8SE) - retail HW2
	*/
	ppcState.spr[SPR_PVR] = 0x00083214;
	ppcState.spr[SPR_HID1] = 0x80000000; // We're running at 3x the bus clock
	ppcState.spr[SPR_ECID_U] = 0x0d96e200;
	ppcState.spr[SPR_ECID_M] = 0x1840c00d;
	ppcState.spr[SPR_ECID_L] = 0x82bb08e8;

	ppcState.fpscr = 0;
	ppcState.pc = 0;
	ppcState.npc = 0;
	ppcState.Exceptions = 0;
	for (auto& v : ppcState.cr_val)
		v = 0x8000000000000001;

	TL = 0;
	TU = 0;
	SystemTimers::TimeBaseSet();

	// MSR should be 0x40, but we don't emulate BS1, so it would never be turned off :}
	ppcState.msr = 0;
	rDEC = 0xFFFFFFFF;
	SystemTimers::DecrementerSet();
}

void Init(int cpu_core)
{
	FPURoundMode::SetPrecisionMode(FPURoundMode::PREC_53);

	memset(ppcState.sr, 0, sizeof(ppcState.sr));
	ppcState.dtlb_last = 0;
	memset(ppcState.dtlb_va, 0, sizeof(ppcState.dtlb_va));
	memset(ppcState.dtlb_pa, 0, sizeof(ppcState.dtlb_pa));
	ppcState.itlb_last = 0;
	memset(ppcState.itlb_va, 0, sizeof(ppcState.itlb_va));
	memset(ppcState.itlb_pa, 0, sizeof(ppcState.itlb_pa));
	ppcState.pagetable_base = 0;
	ppcState.pagetable_hashmask = 0;

	ResetRegisters();
	PPCTables::InitTables(cpu_core);

	// We initialize the interpreter because
	// it is used on boot and code window independently.
	interpreter->Init();

	switch (cpu_core)
	{
		case 0:
		{
			cpu_core_base = interpreter;
			break;
		}
		default:
			cpu_core_base = JitInterface::InitJitCore(cpu_core);
			if (!cpu_core_base) // Handle Situations where JIT core isn't available
			{
				WARN_LOG(POWERPC, "Jit core %d not available. Defaulting to interpreter.", cpu_core);
				cpu_core_base = interpreter;
			}
		break;
	}

	if (cpu_core_base != interpreter)
	{
		mode = MODE_JIT;
	}
	else
	{
		mode = MODE_INTERPRETER;
	}
	state = CPU_STEPPING;

	ppcState.iCache.Init();

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableDebugging)
		breakpoints.ClearAllTemporary();
}

void Shutdown()
{
	JitInterface::Shutdown();
	interpreter->Shutdown();
	cpu_core_base = nullptr;
	state = CPU_POWERDOWN;
}

CoreMode GetMode()
{
	return mode;
}

void SetMode(CoreMode new_mode)
{
	if (new_mode == mode)
		return;  // We don't need to do anything.

	mode = new_mode;

	switch (mode)
	{
	case MODE_INTERPRETER:  // Switching from JIT to interpreter
		cpu_core_base = interpreter;
		break;

	case MODE_JIT:  // Switching from interpreter to JIT.
		// Don't really need to do much. It'll work, the cache will refill itself.
		cpu_core_base = JitInterface::GetCore();
		if (!cpu_core_base) // Has a chance to not get a working JIT core if one isn't active on host
			cpu_core_base = interpreter;
		break;
	}
}

void SingleStep()
{
	cpu_core_base->SingleStep();
}

void RunLoop()
{
	state = CPU_RUNNING;
	cpu_core_base->Run();
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

void UpdatePerformanceMonitor(u32 cycles, u32 num_load_stores, u32 num_fp_inst)
{
	switch (MMCR0.PMC1SELECT)
	{
	case 0: // No change
		break;
	case 1: // Processor cycles
		PowerPC::ppcState.spr[SPR_PMC1] += cycles;
		break;
	default:
		break;
	}

	switch (MMCR0.PMC2SELECT)
	{
	case 0: // No change
		break;
	case 1: // Processor cycles
		PowerPC::ppcState.spr[SPR_PMC2] += cycles;
		break;
	case 11: // Number of loads and stores completed
		PowerPC::ppcState.spr[SPR_PMC2] += num_load_stores;
		break;
	default:
		break;
	}

	switch (MMCR1.PMC3SELECT)
	{
	case 0: // No change
		break;
	case 1: // Processor cycles
		PowerPC::ppcState.spr[SPR_PMC3] += cycles;
		break;
	case 11: // Number of FPU instructions completed
		PowerPC::ppcState.spr[SPR_PMC3] += num_fp_inst;
		break;
	default:
		break;
	}

	switch (MMCR1.PMC4SELECT)
	{
	case 0: // No change
		break;
	case 1: // Processor cycles
		PowerPC::ppcState.spr[SPR_PMC4] += cycles;
		break;
	default:
		break;
	}

	if ((MMCR0.PMC1INTCONTROL && (PowerPC::ppcState.spr[SPR_PMC1] & 0x80000000) != 0) ||
		(MMCR0.PMCINTCONTROL && (PowerPC::ppcState.spr[SPR_PMC2] & 0x80000000) != 0) ||
		(MMCR0.PMCINTCONTROL && (PowerPC::ppcState.spr[SPR_PMC3] & 0x80000000) != 0) ||
		(MMCR0.PMCINTCONTROL && (PowerPC::ppcState.spr[SPR_PMC4] & 0x80000000) != 0))
		PowerPC::ppcState.Exceptions |= EXCEPTION_PERFORMANCE_MONITOR;
}

void CheckExceptions()
{
	// Make sure we are checking against the latest EXI status. This is required
	// for devices which interrupt frequently, such as the gc mic
	ExpansionInterface::UpdateInterrupts();

	// Read volatile data once
	u32 exceptions = ppcState.Exceptions;

	// Example procedure:
	// set SRR0 to either PC or NPC
	//SRR0 = NPC;
	// save specified MSR bits
	//SRR1 = MSR & 0x87C0FFFF;
	// copy ILE bit to LE
	//MSR |= (MSR >> 16) & 1;
	// clear MSR as specified
	//MSR &= ~0x04EF36; // 0x04FF36 also clears ME (only for machine check exception)
	// set to exception type entry point
	//NPC = 0x00000x00;

	// TODO(delroth): Exception priority is completely wrong here: depending on
	// the instruction class, exceptions should be executed in a given order,
	// which is very different from the one arbitrarily chosen here. See §6.1.5
	// in 6xx_pem.pdf.

	if (exceptions & EXCEPTION_ISI)
	{
		SRR0 = NPC;
		// Page fault occurred
		SRR1 = (MSR & 0x87C0FFFF) | (1 << 30);
		MSR |= (MSR >> 16) & 1;
		MSR &= ~0x04EF36;
		PC = NPC = 0x00000400;

		INFO_LOG(POWERPC, "EXCEPTION_ISI");
		Common::AtomicAnd(ppcState.Exceptions, ~EXCEPTION_ISI);
	}
	else if (exceptions & EXCEPTION_PROGRAM)
	{
		SRR0 = PC;
		// say that it's a trap exception
		SRR1 = (MSR & 0x87C0FFFF) | 0x20000;
		MSR |= (MSR >> 16) & 1;
		MSR &= ~0x04EF36;
		PC = NPC = 0x00000700;

		INFO_LOG(POWERPC, "EXCEPTION_PROGRAM");
		Common::AtomicAnd(ppcState.Exceptions, ~EXCEPTION_PROGRAM);
	}
	else if (exceptions & EXCEPTION_SYSCALL)
	{
		SRR0 = NPC;
		SRR1 = MSR & 0x87C0FFFF;
		MSR |= (MSR >> 16) & 1;
		MSR &= ~0x04EF36;
		PC = NPC = 0x00000C00;

		INFO_LOG(POWERPC, "EXCEPTION_SYSCALL (PC=%08x)", PC);
		Common::AtomicAnd(ppcState.Exceptions, ~EXCEPTION_SYSCALL);
	}
	else if (exceptions & EXCEPTION_FPU_UNAVAILABLE)
	{
		//This happens a lot - GameCube OS uses deferred FPU context switching
		SRR0 = PC; // re-execute the instruction
		SRR1 = MSR & 0x87C0FFFF;
		MSR |= (MSR >> 16) & 1;
		MSR &= ~0x04EF36;
		PC = NPC = 0x00000800;

		INFO_LOG(POWERPC, "EXCEPTION_FPU_UNAVAILABLE");
		Common::AtomicAnd(ppcState.Exceptions, ~EXCEPTION_FPU_UNAVAILABLE);
	}
	else if (exceptions & EXCEPTION_DSI)
	{
		SRR0 = PC;
		SRR1 = MSR & 0x87C0FFFF;
		MSR |= (MSR >> 16) & 1;
		MSR &= ~0x04EF36;
		PC = NPC = 0x00000300;
		//DSISR and DAR regs are changed in GenerateDSIException()

		INFO_LOG(POWERPC, "EXCEPTION_DSI");
		Common::AtomicAnd(ppcState.Exceptions, ~EXCEPTION_DSI);
	}
	else if (exceptions & EXCEPTION_ALIGNMENT)
	{
		//This never happens ATM
		// perhaps we can get dcb* instructions to use this :p
		SRR0 = PC;
		SRR1 = MSR & 0x87C0FFFF;
		MSR |= (MSR >> 16) & 1;
		MSR &= ~0x04EF36;
		PC = NPC = 0x00000600;

		//TODO crazy amount of DSISR options to check out

		INFO_LOG(POWERPC, "EXCEPTION_ALIGNMENT");
		Common::AtomicAnd(ppcState.Exceptions, ~EXCEPTION_ALIGNMENT);
	}

	// EXTERNAL INTERRUPT
	else if (MSR & 0x0008000)  // Handling is delayed until MSR.EE=1.
	{
		if (exceptions & EXCEPTION_EXTERNAL_INT)
		{
			// Pokemon gets this "too early", it hasn't a handler yet
			SRR0 = NPC;
			SRR1 = MSR & 0x87C0FFFF;
			MSR |= (MSR >> 16) & 1;
			MSR &= ~0x04EF36;
			PC = NPC = 0x00000500;

			INFO_LOG(POWERPC, "EXCEPTION_EXTERNAL_INT");
			Common::AtomicAnd(ppcState.Exceptions, ~EXCEPTION_EXTERNAL_INT);

			_dbg_assert_msg_(POWERPC, (SRR1 & 0x02) != 0, "EXTERNAL_INT unrecoverable???");
		}
		else if (exceptions & EXCEPTION_PERFORMANCE_MONITOR)
		{
			SRR0 = NPC;
			SRR1 = MSR & 0x87C0FFFF;
			MSR |= (MSR >> 16) & 1;
			MSR &= ~0x04EF36;
			PC = NPC = 0x00000F00;

			INFO_LOG(POWERPC, "EXCEPTION_PERFORMANCE_MONITOR");
			Common::AtomicAnd(ppcState.Exceptions, ~EXCEPTION_PERFORMANCE_MONITOR);
		}
		else if (exceptions & EXCEPTION_DECREMENTER)
		{
			SRR0 = NPC;
			SRR1 = MSR & 0x87C0FFFF;
			MSR |= (MSR >> 16) & 1;
			MSR &= ~0x04EF36;
			PC = NPC = 0x00000900;

			INFO_LOG(POWERPC, "EXCEPTION_DECREMENTER");
			Common::AtomicAnd(ppcState.Exceptions, ~EXCEPTION_DECREMENTER);
		}
	}
}

void CheckExternalExceptions()
{
	// Read volatile data once
	u32 exceptions = ppcState.Exceptions;

	// EXTERNAL INTERRUPT
	if (exceptions && (MSR & 0x0008000))  // Handling is delayed until MSR.EE=1.
	{
		if (exceptions & EXCEPTION_EXTERNAL_INT)
		{
			// Pokemon gets this "too early", it hasn't a handler yet
			SRR0 = NPC;
			SRR1 = MSR & 0x87C0FFFF;
			MSR |= (MSR >> 16) & 1;
			MSR &= ~0x04EF36;
			PC = NPC = 0x00000500;

			INFO_LOG(POWERPC, "EXCEPTION_EXTERNAL_INT");
			Common::AtomicAnd(ppcState.Exceptions, ~EXCEPTION_EXTERNAL_INT);

			_dbg_assert_msg_(POWERPC, (SRR1 & 0x02) != 0, "EXTERNAL_INT unrecoverable???");
		}
		else if (exceptions & EXCEPTION_PERFORMANCE_MONITOR)
		{
			SRR0 = NPC;
			SRR1 = MSR & 0x87C0FFFF;
			MSR |= (MSR >> 16) & 1;
			MSR &= ~0x04EF36;
			PC = NPC = 0x00000F00;

			INFO_LOG(POWERPC, "EXCEPTION_PERFORMANCE_MONITOR");
			Common::AtomicAnd(ppcState.Exceptions, ~EXCEPTION_PERFORMANCE_MONITOR);
		}
		else if (exceptions & EXCEPTION_DECREMENTER)
		{
			SRR0 = NPC;
			SRR1 = MSR & 0x87C0FFFF;
			MSR |= (MSR >> 16) & 1;
			MSR &= ~0x04EF36;
			PC = NPC = 0x00000900;

			INFO_LOG(POWERPC, "EXCEPTION_DECREMENTER");
			Common::AtomicAnd(ppcState.Exceptions, ~EXCEPTION_DECREMENTER);
		}
		else
		{
			_dbg_assert_msg_(POWERPC, 0, "Unknown EXT interrupt: Exceptions == %08x", exceptions);
			ERROR_LOG(POWERPC, "Unknown EXTERNAL INTERRUPT exception: Exceptions == %08x", exceptions);
		}
	}
}

void CheckBreakPoints()
{
	if (PowerPC::breakpoints.IsAddressBreakPoint(PC))
	{
		PowerPC::Pause();
		if (PowerPC::breakpoints.IsTempBreakPoint(PC))
			PowerPC::breakpoints.Remove(PC);
	}
}

void OnIdle(u32 _uThreadAddr)
{
	u32 nextThread = Memory::Read_U32(_uThreadAddr);
	//do idle skipping
	if (nextThread == 0)
		CoreTiming::Idle();
}

void OnIdleIL()
{
	CoreTiming::Idle();
}

}  // namespace


// FPSCR update functions

void UpdateFPRF(double dvalue)
{
	FPSCR.FPRF = MathUtil::ClassifyDouble(dvalue);
	//if (FPSCR.FPRF == 0x11)
	//	PanicAlert("QNAN alert");
}
