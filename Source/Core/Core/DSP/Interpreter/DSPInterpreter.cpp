// Copyright 2008 Dolphin Emulator Project
// Copyright 2004 Duddie & Tratax
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/DSP/Interpreter/DSPInterpreter.h"

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

#include "Core/DSP/DSPAnalyzer.h"
#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPMemoryMap.h"
#include "Core/DSP/DSPTables.h"
#include "Core/DSP/Interpreter/DSPIntTables.h"

namespace DSP::Interpreter
{
namespace
{
void ExecuteInstruction(const UDSPInstruction inst)
{
  const DSPOPCTemplate* opcode_template = GetOpTemplate(inst);

  if (opcode_template->extended)
  {
    GetExtOp(inst)(inst);
  }

  GetOp(inst)(inst);

  if (opcode_template->extended)
  {
    ApplyWriteBackLog();
  }
}
}  // Anonymous namespace

// NOTE: These have nothing to do with g_dsp.r.cr !
void WriteCR(u16 val)
{
  // reset
  if (val & 1)
  {
    INFO_LOG(DSPLLE, "DSP_CONTROL RESET");
    DSPCore_Reset();
    val &= ~1;
  }
  // init
  else if (val == 4)
  {
    // HAX!
    // OSInitAudioSystem ucode should send this mail - not DSP core itself
    INFO_LOG(DSPLLE, "DSP_CONTROL INIT");
    g_init_hax = true;
    val |= 0x800;
  }

  // update cr
  g_dsp.cr = val;
}

u16 ReadCR()
{
  if (g_dsp.pc & 0x8000)
  {
    g_dsp.cr |= 0x800;
  }
  else
  {
    g_dsp.cr &= ~0x800;
  }

  return g_dsp.cr;
}

void Step()
{
  DSPCore_CheckExceptions();

  g_dsp.step_counter++;

#if PROFILE
  g_dsp.err_pc = g_dsp.pc;

  ProfilerAddDelta(g_dsp.err_pc, 1);
  if (g_dsp.step_counter == 1)
  {
    ProfilerInit();
  }

  if ((g_dsp.step_counter & 0xFFFFF) == 0)
  {
    ProfilerDump(g_dsp.step_counter);
  }
#endif

  u16 opc = dsp_fetch_code();
  ExecuteInstruction(UDSPInstruction(opc));

  if (Analyzer::GetCodeFlags(static_cast<u16>(g_dsp.pc - 1u)) & Analyzer::CODE_LOOP_END)
    HandleLoop();
}

// Used by thread mode.
int RunCyclesThread(int cycles)
{
  while (true)
  {
    if (g_dsp.cr & CR_HALT)
      return 0;

    if (g_dsp.external_interrupt_waiting)
    {
      DSPCore_CheckExternalInterrupt();
      DSPCore_SetExternalInterrupt(false);
    }

    Step();
    cycles--;
    if (cycles < 0)
      return 0;
  }
}

// This one has basic idle skipping, and checks breakpoints.
int RunCyclesDebug(int cycles)
{
  // First, let's run a few cycles with no idle skipping so that things can progress a bit.
  for (int i = 0; i < 8; i++)
  {
    if (g_dsp.cr & CR_HALT)
      return 0;
    if (g_dsp_breakpoints.IsAddressBreakPoint(g_dsp.pc))
    {
      DSPCore_SetState(State::Stepping);
      return cycles;
    }
    Step();
    cycles--;
    if (cycles < 0)
      return 0;
  }

  while (true)
  {
    // Next, let's run a few cycles with idle skipping, so that we can skip
    // idle loops.
    for (int i = 0; i < 8; i++)
    {
      if (g_dsp.cr & CR_HALT)
        return 0;
      if (g_dsp_breakpoints.IsAddressBreakPoint(g_dsp.pc))
      {
        DSPCore_SetState(State::Stepping);
        return cycles;
      }
      // Idle skipping.
      if (Analyzer::GetCodeFlags(g_dsp.pc) & Analyzer::CODE_IDLE_SKIP)
        return 0;
      Step();
      cycles--;
      if (cycles < 0)
        return 0;
    }

    // Now, lets run some more without idle skipping.
    for (int i = 0; i < 200; i++)
    {
      if (g_dsp_breakpoints.IsAddressBreakPoint(g_dsp.pc))
      {
        DSPCore_SetState(State::Stepping);
        return cycles;
      }
      Step();
      cycles--;
      if (cycles < 0)
        return 0;
      // We don't bother directly supporting pause - if the main emu pauses,
      // it just won't call this function anymore.
    }
  }
}

// Used by non-thread mode. Meant to be efficient.
int RunCycles(int cycles)
{
  // First, let's run a few cycles with no idle skipping so that things can
  // progress a bit.
  for (int i = 0; i < 8; i++)
  {
    if (g_dsp.cr & CR_HALT)
      return 0;
    Step();
    cycles--;
    if (cycles < 0)
      return 0;
  }

  while (true)
  {
    // Next, let's run a few cycles with idle skipping, so that we can skip
    // idle loops.
    for (int i = 0; i < 8; i++)
    {
      if (g_dsp.cr & CR_HALT)
        return 0;
      // Idle skipping.
      if (Analyzer::GetCodeFlags(g_dsp.pc) & Analyzer::CODE_IDLE_SKIP)
        return 0;
      Step();
      cycles--;
      if (cycles < 0)
        return 0;
    }

    // Now, lets run some more without idle skipping.
    for (int i = 0; i < 200; i++)
    {
      Step();
      cycles--;
      if (cycles < 0)
        return 0;
      // We don't bother directly supporting pause - if the main emu pauses,
      // it just won't call this function anymore.
    }
  }
}

void nop(const UDSPInstruction opc)
{
  // The real nop is 0. Anything else is bad.
  if (opc == 0)
    return;

  ERROR_LOG(DSPLLE, "LLE: Unrecognized opcode 0x%04x", opc);
}
}  // namespace DSP::Interpreter
