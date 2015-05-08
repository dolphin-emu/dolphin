/*====================================================================

   filename:     gdsp_interpreter.cpp
   project:      GCemu
   created:      2004-6-18
   mail:         duddie@walla.com

   Copyright (c) 2005 Duddie & Tratax

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

   ====================================================================*/

#include "Core/DSP/DSPAnalyzer.h"
#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPHWInterface.h"
#include "Core/DSP/DSPInterpreter.h"
#include "Core/DSP/DSPIntUtil.h"
#include "Core/DSP/DSPMemoryMap.h"
#include "Core/DSP/DSPTables.h"

namespace DSPInterpreter {

// NOTE: These have nothing to do with g_dsp.r.cr !

void WriteCR(u16 val)
{
	// reset
	if (val & 1)
	{
		INFO_LOG(DSPLLE,"DSP_CONTROL RESET");
		DSPCore_Reset();
		val &= ~1;
	}
	// init
	else if (val == 4)
	{
		// HAX!
		// OSInitAudioSystem ucode should send this mail - not DSP core itself
		INFO_LOG(DSPLLE,"DSP_CONTROL INIT");
		init_hax = true;
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

	if (DSPAnalyzer::code_flags[g_dsp.pc - 1] & DSPAnalyzer::CODE_LOOP_END)
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
		if (dsp_breakpoints.IsAddressBreakPoint(g_dsp.pc))
		{
			DSPCore_SetState(DSPCORE_STEPPING);
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
			if (dsp_breakpoints.IsAddressBreakPoint(g_dsp.pc))
			{
				DSPCore_SetState(DSPCORE_STEPPING);
				return cycles;
			}
			// Idle skipping.
			if (DSPAnalyzer::code_flags[g_dsp.pc] & DSPAnalyzer::CODE_IDLE_SKIP)
				return 0;
			Step();
			cycles--;
			if (cycles < 0)
				return 0;
		}

		// Now, lets run some more without idle skipping.
		for (int i = 0; i < 200; i++)
		{
			if (dsp_breakpoints.IsAddressBreakPoint(g_dsp.pc))
			{
				DSPCore_SetState(DSPCORE_STEPPING);
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
			if (DSPAnalyzer::code_flags[g_dsp.pc] & DSPAnalyzer::CODE_IDLE_SKIP)
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

}  // namespace
