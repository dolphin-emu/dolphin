/*====================================================================

   filename:     gdsp_interpreter.cpp
   project:      GCemu
   created:      2004-6-18
   mail:		  duddie@walla.com

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   ====================================================================*/

#include <stdio.h>
#include <stdlib.h>

#include "DSPTables.h"
#include "DSPAnalyzer.h"

#include "gdsp_interface.h"
#include "gdsp_opcodes_helper.h"
#include "Tools.h"
#include "MemoryUtil.h"

//-------------------------------------------------------------------------------

SDSP g_dsp;

volatile u32 gdsp_running;

static bool cr_halt = true;
static bool cr_external_int = false;

void UpdateCachedCR()
{
	cr_halt = (g_dsp.cr & 0x4) != 0;
	cr_external_int = (g_dsp.cr & 0x02) != 0;
}

//-------------------------------------------------------------------------------

void gdsp_init()
{
	// Dump IMEM when ucodes get uploaded. Why not... still a plugin heavily in dev.
	g_dsp.dump_imem = true;

	g_dsp.irom = (u16*)AllocateMemoryPages(DSP_IROM_BYTE_SIZE);
	g_dsp.iram = (u16*)AllocateMemoryPages(DSP_IRAM_BYTE_SIZE);
	g_dsp.dram = (u16*)AllocateMemoryPages(DSP_DRAM_BYTE_SIZE);
	g_dsp.coef = (u16*)AllocateMemoryPages(DSP_COEF_BYTE_SIZE);

	// Fill roms with zeros. 
	memset(g_dsp.irom, 0, DSP_IROM_BYTE_SIZE);
	memset(g_dsp.coef, 0, DSP_COEF_BYTE_SIZE);

	for (int i = 0; i < 32; i++)
	{
		g_dsp.r[i] = 0;
	}

	for (int i = 0; i < 4; i++)
	{
		g_dsp.reg_stack_ptr[i] = 0;

		for (int j = 0; j < DSP_STACK_DEPTH; j++)
		{
			g_dsp.reg_stack[i][j] = 0;
		}
	}

	// Fill IRAM with HALT opcodes.
	for (int i = 0; i < DSP_IRAM_SIZE; i++)
	{
		g_dsp.iram[i] = 0x0021; // HALT opcode
	}

	// Just zero out DRAM.
	for (int i = 0; i < DSP_DRAM_SIZE; i++)
	{
		g_dsp.dram[i] = 0x0021;
	}

	// copied from a real console after the custom UCode has been loaded
	g_dsp.r[0x08] = 0xffff;
	g_dsp.r[0x09] = 0xffff;
	g_dsp.r[0x0a] = 0xffff;
	g_dsp.r[0x0b] = 0xffff;

	g_dsp.cr = 0x804;
	gdsp_ifx_init();

	UpdateCachedCR();

	// Mostly keep IRAM write protected. We unprotect only when DMA-ing
	// in new ucodes.
	WriteProtectMemory(g_dsp.iram, DSP_IRAM_BYTE_SIZE, false);
	DSPAnalyzer::Analyze();
}

void gdsp_shutdown()
{
	FreeMemoryPages(g_dsp.irom, DSP_IROM_BYTE_SIZE);
	FreeMemoryPages(g_dsp.iram, DSP_IRAM_BYTE_SIZE);
	FreeMemoryPages(g_dsp.dram, DSP_DRAM_BYTE_SIZE);
	FreeMemoryPages(g_dsp.coef, DSP_COEF_BYTE_SIZE);
}


void gdsp_reset()
{
    _assert_msg_(MASTER_LOG, !g_dsp.exception_in_progress_hack, "reset while exception");
    g_dsp.pc = DSP_RESET_VECTOR;
    g_dsp.exception_in_progress_hack = false;
}


void gdsp_generate_exception(u8 level)
{
	g_dsp.exceptions |= 1 << level;
}

bool gdsp_load_irom(const char *fname)
{
	FILE *pFile = fopen(fname, "rb");
	if (pFile)
	{
		size_t size_in_bytes = DSP_IROM_SIZE * sizeof(u16);
		size_t read_bytes = fread(g_dsp.irom, 1, size_in_bytes, pFile);
		if (read_bytes != size_in_bytes)
		{
			PanicAlert("IROM too short : %i/%i", (int)read_bytes, (int)size_in_bytes);
			fclose(pFile);
			return false;
		}
		fclose(pFile);
		return true;
	}
	// Always keep IROM write protected.
	WriteProtectMemory(g_dsp.irom, DSP_IROM_BYTE_SIZE, false);
	return false;
}

bool gdsp_load_coef(const char *fname)
{
	FILE *pFile = fopen(fname, "rb");
	if (pFile)
	{
		size_t size_in_bytes = DSP_COEF_SIZE * sizeof(u16);
		size_t read_bytes = fread(g_dsp.coef, 1, size_in_bytes, pFile);
		if (read_bytes != size_in_bytes)
		{
			PanicAlert("COEF too short : %i/%i", (int)read_bytes, (int)size_in_bytes);
			fclose(pFile);
			return false;
		}
		fclose(pFile);
		return true;
	}
	// Always keep COEF write protected. We unprotect only when DMA-ing
	WriteProtectMemory(g_dsp.coef, DSP_COEF_BYTE_SIZE, false);
	return false;
}

// Hm, should instructions that change CR use this? Probably not (but they
// should call UpdateCachedCR())
void gdsp_write_cr(u16 val)
{
	// reset
	if (val & 0x0001)
	{
		gdsp_reset();
	}

	val &= ~0x0001;

	// update cr
	g_dsp.cr = val;

	UpdateCachedCR();
}


// Hm, should instructions that read CR use this? (Probably not).
u16 gdsp_read_cr()
{
	if (g_dsp.pc & 0x8000)
	{
		g_dsp.cr |= 0x800;
	}
	else
	{
		g_dsp.cr &= ~0x800;
	}

	UpdateCachedCR();

	return g_dsp.cr;
}

void gdsp_step()
{
	g_dsp.step_counter++;

	g_dsp.err_pc = g_dsp.pc;

#if PROFILE
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

	// Handle looping hardware. 
	u16& rLoopCounter = g_dsp.r[DSP_REG_ST3];
	if (rLoopCounter > 0)
	{
		const u16 rCallAddress = g_dsp.r[DSP_REG_ST0];
		const u16 rLoopAddress = g_dsp.r[DSP_REG_ST2];

		if (g_dsp.pc == (rLoopAddress + 1))
		{
			rLoopCounter--;

			if (rLoopCounter > 0)
			{
				g_dsp.pc = rCallAddress;
			}
			else
			{
				// end of loop
				dsp_reg_load_stack(0);
				dsp_reg_load_stack(2);
				dsp_reg_load_stack(3);
			}
		}
	}

	// check if there is an external interrupt
	if (cr_external_int)
	{
		if (dsp_SR_is_flag_set(FLAG_ENABLE_INTERUPT) && (g_dsp.exception_in_progress_hack == false))
		{
			// level 7 is the interrupt exception
			gdsp_generate_exception(7);
			g_dsp.cr &= ~0x0002;
			UpdateCachedCR();
		}
	}

	// check exceptions
	if ((g_dsp.exceptions != 0) && (!g_dsp.exception_in_progress_hack))
	{
		for (int i = 0; i < 8; i++)
		{
			if (g_dsp.exceptions & (1 << i))
			{
				_assert_msg_(MASTER_LOG, !g_dsp.exception_in_progress_hack, "assert while exception");

				dsp_reg_store_stack(DSP_STACK_C, g_dsp.pc);
				dsp_reg_store_stack(DSP_STACK_D, g_dsp.r[DSP_REG_SR]);

				g_dsp.pc = i * 2;
				g_dsp.exceptions &= ~(1 << i);

				g_dsp.exception_in_progress_hack = true;
				break;
			}
		}
	}
}

// Used by thread mode.
void gdsp_run()
{
	gdsp_running = true;
	while (!cr_halt)
	{
		// Are we running?
		if (*g_dspInitialize.pEmulatorState)
			break;
		
		gdsp_step();

		if (!gdsp_running)
			break;
	}
	gdsp_running = false;
}

// Used by non-thread mode.
void gdsp_run_cycles(int cycles)
{
	// First, let's run a few cycles with no idle skipping so that things can progress a bit.
	for (int i = 0; i < 8; i++)
	{
		if (cr_halt)
			return;
		gdsp_step();
		cycles--;
	}
	// Next, let's run a few cycles with idle skipping, so that we can skip loops.
	for (int i = 0; i < 8; i++)
	{
		if (cr_halt)
			return;
		if (DSPAnalyzer::code_flags[g_dsp.pc] & DSPAnalyzer::CODE_IDLE_SKIP)
			return;
		gdsp_step();
		cycles--;
	}
	// Now, run the rest of the block without idle skipping. It might trip into a 
	// idle loop and if so we waste some time here. Might be beneficial to slice even further.
	while (cycles > 0)
	{
		if (cr_halt)
			return;
		gdsp_step();
		cycles--;
		// We don't bother directly supporting pause - if the main emu pauses,
		// it just won't call this function anymore.
	}
}

void gdsp_stop()
{
	gdsp_running = false;
}

