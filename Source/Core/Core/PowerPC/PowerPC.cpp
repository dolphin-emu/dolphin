// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/PowerPC.h"

#include <cstring>
#include <vector>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/FPURoundMode.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"

#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/HW/CPU.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/Host.h"
#include "Core/PowerPC/CPUCoreBase.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/JitInterface.h"

namespace PowerPC
{
// STATE_TO_SAVE
PowerPCState ppcState;

static CPUCoreBase* s_cpu_core_base = nullptr;
static bool s_cpu_core_base_is_injected = false;
Interpreter* const s_interpreter = Interpreter::getInstance();
static CoreMode s_mode = CoreMode::Interpreter;

Watches watches;
BreakPoints breakpoints;
MemChecks memchecks;
PPCDebugInterface debug_interface;

static CoreTiming::EventType* s_invalidate_cache_thread_safe;
static void InvalidateCacheThreadSafe(u64 userdata, s64 cyclesLate)
{
  ppcState.iCache.Invalidate(static_cast<u32>(userdata));
}

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

void DoState(PointerWrap& p)
{
  // some of this code has been disabled, because
  // it changes registers even in MODE_MEASURE (which is suspicious and seems like it could cause
  // desyncs)
  // and because the values it's changing have been added to CoreTiming::DoState, so it might
  // conflict to mess with them here.

  // rSPR(SPR_DEC) = SystemTimers::GetFakeDecrementer();
  // *((u64 *)&TL) = SystemTimers::GetFakeTimeBase(); //works since we are little endian and TL
  // comes first :)

  p.DoArray(ppcState.gpr);
  p.Do(ppcState.pc);
  p.Do(ppcState.npc);
  p.DoArray(ppcState.cr_val);
  p.Do(ppcState.msr);
  p.Do(ppcState.fpscr);
  p.Do(ppcState.Exceptions);
  p.Do(ppcState.downcount);
  p.Do(ppcState.xer_ca);
  p.Do(ppcState.xer_so_ov);
  p.Do(ppcState.xer_stringctrl);
  p.DoArray(ppcState.ps);
  p.DoArray(ppcState.sr);
  p.DoArray(ppcState.spr);
  p.DoArray(ppcState.tlb);
  p.Do(ppcState.pagetable_base);
  p.Do(ppcState.pagetable_hashmask);

  ppcState.iCache.DoState(p);

  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    IBATUpdated();
    DBATUpdated();
  }

  // SystemTimers::DecrementerSet();
  // SystemTimers::TimeBaseSet();

  JitInterface::DoState(p);
}

static void ResetRegisters()
{
  memset(ppcState.ps, 0, sizeof(ppcState.ps));
  memset(ppcState.sr, 0, sizeof(ppcState.sr));
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
  ppcState.spr[SPR_HID1] = 0x80000000;  // We're running at 3x the bus clock
  ppcState.spr[SPR_ECID_U] = 0x0d96e200;
  ppcState.spr[SPR_ECID_M] = 0x1840c00d;
  ppcState.spr[SPR_ECID_L] = 0x82bb08e8;

  ppcState.fpscr = 0;
  ppcState.pc = 0;
  ppcState.npc = 0;
  ppcState.Exceptions = 0;
  for (auto& v : ppcState.cr_val)
    v = 0x8000000000000001;

  DBATUpdated();
  IBATUpdated();

  TL = 0;
  TU = 0;
  SystemTimers::TimeBaseSet();

  // MSR should be 0x40, but we don't emulate BS1, so it would never be turned off :}
  ppcState.msr = 0;
  rDEC = 0xFFFFFFFF;
  SystemTimers::DecrementerSet();
}

static void InitializeCPUCore(int cpu_core)
{
  // We initialize the interpreter because
  // it is used on boot and code window independently.
  s_interpreter->Init();

  switch (cpu_core)
  {
  case PowerPC::CORE_INTERPRETER:
    s_cpu_core_base = s_interpreter;
    break;

  default:
    s_cpu_core_base = JitInterface::InitJitCore(cpu_core);
    if (!s_cpu_core_base)  // Handle Situations where JIT core isn't available
    {
      WARN_LOG(POWERPC, "Jit core %d not available. Defaulting to interpreter.", cpu_core);
      s_cpu_core_base = s_interpreter;
    }
    break;
  }

  if (s_cpu_core_base != s_interpreter)
  {
    s_mode = CoreMode::JIT;
  }
  else
  {
    s_mode = CoreMode::Interpreter;
  }
}

const std::vector<CPUCore>& AvailableCPUCores()
{
  static const std::vector<CPUCore> cpu_cores = {
      CORE_INTERPRETER, CORE_CACHEDINTERPRETER,
#ifdef _M_X86_64
      CORE_JIT64,
#elif defined(_M_ARM_64)
      CORE_JITARM64,
#endif
  };

  return cpu_cores;
}

CPUCore DefaultCPUCore()
{
#ifdef _M_X86_64
  return CORE_JIT64;
#elif defined(_M_ARM_64)
  return CORE_JITARM64;
#else
  return CORE_CACHEDINTERPRETER;
#endif
}

void Init(int cpu_core)
{
  // NOTE: This function runs on EmuThread, not the CPU Thread.
  //   Changing the rounding mode has a limited effect.
  FPURoundMode::SetPrecisionMode(FPURoundMode::PREC_53);

  s_invalidate_cache_thread_safe =
      CoreTiming::RegisterEvent("invalidateEmulatedCache", InvalidateCacheThreadSafe);

  Reset();

  InitializeCPUCore(cpu_core);
  ppcState.iCache.Init();

  if (SConfig::GetInstance().bEnableDebugging)
    breakpoints.ClearAllTemporary();
}

void Reset()
{
  ppcState.pagetable_base = 0;
  ppcState.pagetable_hashmask = 0;
  ppcState.tlb = {};

  ResetRegisters();
  ppcState.iCache.Reset();
}

void ScheduleInvalidateCacheThreadSafe(u32 address)
{
  if (CPU::GetState() == CPU::State::Running)
  {
    CoreTiming::ScheduleEvent(0, s_invalidate_cache_thread_safe, address,
                              CoreTiming::FromThread::NON_CPU);
  }
  else
  {
    PowerPC::ppcState.iCache.Invalidate(static_cast<u32>(address));
  }
}

void Shutdown()
{
  InjectExternalCPUCore(nullptr);
  JitInterface::Shutdown();
  s_interpreter->Shutdown();
  s_cpu_core_base = nullptr;
}

CoreMode GetMode()
{
  return !s_cpu_core_base_is_injected ? s_mode : CoreMode::Interpreter;
}

static void ApplyMode()
{
  switch (s_mode)
  {
  case CoreMode::Interpreter:  // Switching from JIT to interpreter
    s_cpu_core_base = s_interpreter;
    break;

  case CoreMode::JIT:  // Switching from interpreter to JIT.
    // Don't really need to do much. It'll work, the cache will refill itself.
    s_cpu_core_base = JitInterface::GetCore();
    if (!s_cpu_core_base)  // Has a chance to not get a working JIT core if one isn't active on host
      s_cpu_core_base = s_interpreter;
    break;
  }
}

void SetMode(CoreMode new_mode)
{
  if (new_mode == s_mode)
    return;  // We don't need to do anything.

  s_mode = new_mode;

  // If we're using an external CPU core implementation then don't do anything.
  if (s_cpu_core_base_is_injected)
    return;

  ApplyMode();
}

const char* GetCPUName()
{
  return s_cpu_core_base->GetName();
}

void InjectExternalCPUCore(CPUCoreBase* new_cpu)
{
  // Previously injected.
  if (s_cpu_core_base_is_injected)
    s_cpu_core_base->Shutdown();

  // nullptr means just remove
  if (!new_cpu)
  {
    if (s_cpu_core_base_is_injected)
    {
      s_cpu_core_base_is_injected = false;
      ApplyMode();
    }
    return;
  }

  new_cpu->Init();
  s_cpu_core_base = new_cpu;
  s_cpu_core_base_is_injected = true;
}

void SingleStep()
{
  s_cpu_core_base->SingleStep();
}

void RunLoop()
{
  s_cpu_core_base->Run();
  Host_UpdateDisasmDialog();
}

void UpdatePerformanceMonitor(u32 cycles, u32 num_load_stores, u32 num_fp_inst)
{
  switch (MMCR0.PMC1SELECT)
  {
  case 0:  // No change
    break;
  case 1:  // Processor cycles
    PowerPC::ppcState.spr[SPR_PMC1] += cycles;
    break;
  default:
    break;
  }

  switch (MMCR0.PMC2SELECT)
  {
  case 0:  // No change
    break;
  case 1:  // Processor cycles
    PowerPC::ppcState.spr[SPR_PMC2] += cycles;
    break;
  case 11:  // Number of loads and stores completed
    PowerPC::ppcState.spr[SPR_PMC2] += num_load_stores;
    break;
  default:
    break;
  }

  switch (MMCR1.PMC3SELECT)
  {
  case 0:  // No change
    break;
  case 1:  // Processor cycles
    PowerPC::ppcState.spr[SPR_PMC3] += cycles;
    break;
  case 11:  // Number of FPU instructions completed
    PowerPC::ppcState.spr[SPR_PMC3] += num_fp_inst;
    break;
  default:
    break;
  }

  switch (MMCR1.PMC4SELECT)
  {
  case 0:  // No change
    break;
  case 1:  // Processor cycles
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
  u32 exceptions = ppcState.Exceptions;

  // Example procedure:
  // set SRR0 to either PC or NPC
  // SRR0 = NPC;
  // save specified MSR bits
  // SRR1 = MSR & 0x87C0FFFF;
  // copy ILE bit to LE
  // MSR |= (MSR >> 16) & 1;
  // clear MSR as specified
  // MSR &= ~0x04EF36; // 0x04FF36 also clears ME (only for machine check exception)
  // set to exception type entry point
  // NPC = 0x00000x00;

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

    DEBUG_LOG(POWERPC, "EXCEPTION_ISI");
    ppcState.Exceptions &= ~EXCEPTION_ISI;
  }
  else if (exceptions & EXCEPTION_PROGRAM)
  {
    SRR0 = PC;
    // say that it's a trap exception
    SRR1 = (MSR & 0x87C0FFFF) | 0x20000;
    MSR |= (MSR >> 16) & 1;
    MSR &= ~0x04EF36;
    PC = NPC = 0x00000700;

    DEBUG_LOG(POWERPC, "EXCEPTION_PROGRAM");
    ppcState.Exceptions &= ~EXCEPTION_PROGRAM;
  }
  else if (exceptions & EXCEPTION_SYSCALL)
  {
    SRR0 = NPC;
    SRR1 = MSR & 0x87C0FFFF;
    MSR |= (MSR >> 16) & 1;
    MSR &= ~0x04EF36;
    PC = NPC = 0x00000C00;

    DEBUG_LOG(POWERPC, "EXCEPTION_SYSCALL (PC=%08x)", PC);
    ppcState.Exceptions &= ~EXCEPTION_SYSCALL;
  }
  else if (exceptions & EXCEPTION_FPU_UNAVAILABLE)
  {
    // This happens a lot - GameCube OS uses deferred FPU context switching
    SRR0 = PC;  // re-execute the instruction
    SRR1 = MSR & 0x87C0FFFF;
    MSR |= (MSR >> 16) & 1;
    MSR &= ~0x04EF36;
    PC = NPC = 0x00000800;

    DEBUG_LOG(POWERPC, "EXCEPTION_FPU_UNAVAILABLE");
    ppcState.Exceptions &= ~EXCEPTION_FPU_UNAVAILABLE;
  }
  else if (exceptions & EXCEPTION_FAKE_MEMCHECK_HIT)
  {
    ppcState.Exceptions &= ~EXCEPTION_DSI & ~EXCEPTION_FAKE_MEMCHECK_HIT;
  }
  else if (exceptions & EXCEPTION_DSI)
  {
    SRR0 = PC;
    SRR1 = MSR & 0x87C0FFFF;
    MSR |= (MSR >> 16) & 1;
    MSR &= ~0x04EF36;
    PC = NPC = 0x00000300;
    // DSISR and DAR regs are changed in GenerateDSIException()

    DEBUG_LOG(POWERPC, "EXCEPTION_DSI");
    ppcState.Exceptions &= ~EXCEPTION_DSI;
  }
  else if (exceptions & EXCEPTION_ALIGNMENT)
  {
    // This never happens ATM
    // perhaps we can get dcb* instructions to use this :p
    SRR0 = PC;
    SRR1 = MSR & 0x87C0FFFF;
    MSR |= (MSR >> 16) & 1;
    MSR &= ~0x04EF36;
    PC = NPC = 0x00000600;

    // TODO crazy amount of DSISR options to check out

    DEBUG_LOG(POWERPC, "EXCEPTION_ALIGNMENT");
    ppcState.Exceptions &= ~EXCEPTION_ALIGNMENT;
  }

  // EXTERNAL INTERRUPT
  else
  {
    CheckExternalExceptions();
  }
}

void CheckExternalExceptions()
{
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

      DEBUG_LOG(POWERPC, "EXCEPTION_EXTERNAL_INT");
      ppcState.Exceptions &= ~EXCEPTION_EXTERNAL_INT;

      _dbg_assert_msg_(POWERPC, (SRR1 & 0x02) != 0, "EXTERNAL_INT unrecoverable???");
    }
    else if (exceptions & EXCEPTION_PERFORMANCE_MONITOR)
    {
      SRR0 = NPC;
      SRR1 = MSR & 0x87C0FFFF;
      MSR |= (MSR >> 16) & 1;
      MSR &= ~0x04EF36;
      PC = NPC = 0x00000F00;

      DEBUG_LOG(POWERPC, "EXCEPTION_PERFORMANCE_MONITOR");
      ppcState.Exceptions &= ~EXCEPTION_PERFORMANCE_MONITOR;
    }
    else if (exceptions & EXCEPTION_DECREMENTER)
    {
      SRR0 = NPC;
      SRR1 = MSR & 0x87C0FFFF;
      MSR |= (MSR >> 16) & 1;
      MSR &= ~0x04EF36;
      PC = NPC = 0x00000900;

      DEBUG_LOG(POWERPC, "EXCEPTION_DECREMENTER");
      ppcState.Exceptions &= ~EXCEPTION_DECREMENTER;
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
    CPU::Break();
    if (PowerPC::breakpoints.IsTempBreakPoint(PC))
      PowerPC::breakpoints.Remove(PC);
  }
}

}  // namespace

// FPSCR update functions

void UpdateFPRF(double dvalue)
{
  FPSCR.FPRF = MathUtil::ClassifyDouble(dvalue);
  // if (FPSCR.FPRF == 0x11)
  //	PanicAlert("QNAN alert");
}
