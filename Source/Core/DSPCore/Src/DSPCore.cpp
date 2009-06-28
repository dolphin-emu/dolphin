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

#include "Common.h"
#include "DSPCore.h"
#include "DSPAnalyzer.h"
#include "MemoryUtil.h"

#include "DSPHWInterface.h"
#include "DSPIntUtil.h"

SDSP g_dsp;
BreakPoints dsp_breakpoints;

static bool LoadRom(const char *fname, int size_in_words, u16 *rom)
{
	FILE *pFile = fopen(fname, "rb");
	const size_t size_in_bytes = size_in_words * sizeof(u16);
	if (pFile)
	{
		size_t read_bytes = fread(rom, 1, size_in_bytes, pFile);
		if (read_bytes != size_in_bytes)
		{
			PanicAlert("ROM %s too short : %i/%i", fname, (int)read_bytes, (int)size_in_bytes);
			fclose(pFile);
			return false;
		}
		fclose(pFile);
	
		// Byteswap the rom.
		for (int i = 0; i < DSP_IROM_SIZE; i++)
			rom[i] = Common::swap16(rom[i]);

		return true;
	}
	// Always keep ROMs write protected.
	WriteProtectMemory(g_dsp.irom, size_in_bytes, false);
	return false;
}

bool DSPCore_Init(const char *irom_filename, const char *coef_filename)
{
	g_dsp.step_counter = 0;

	g_dsp.irom = (u16*)AllocateMemoryPages(DSP_IROM_BYTE_SIZE);
	g_dsp.iram = (u16*)AllocateMemoryPages(DSP_IRAM_BYTE_SIZE);
	g_dsp.dram = (u16*)AllocateMemoryPages(DSP_DRAM_BYTE_SIZE);
	g_dsp.coef = (u16*)AllocateMemoryPages(DSP_COEF_BYTE_SIZE);

	// Fill roms with zeros. 
	memset(g_dsp.irom, 0, DSP_IROM_BYTE_SIZE);
	memset(g_dsp.coef, 0, DSP_COEF_BYTE_SIZE);

	// Try to load real ROM contents. Failing this, only homebrew will work correctly with the DSP.
	LoadRom(irom_filename, DSP_IROM_SIZE, g_dsp.irom);
	LoadRom(coef_filename, DSP_COEF_SIZE, g_dsp.coef);

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
		g_dsp.dram[i] = 0;
	}

	// Copied from a real console after the custom UCode has been loaded.
	// These are the indexing wrapping registers.
	g_dsp.r[DSP_REG_WR0] = 0xffff;
	g_dsp.r[DSP_REG_WR1] = 0xffff;
	g_dsp.r[DSP_REG_WR2] = 0xffff;
	g_dsp.r[DSP_REG_WR3] = 0xffff;

	g_dsp.cr = 0x804;
	gdsp_ifx_init();

	// Mostly keep IRAM write protected. We unprotect only when DMA-ing
	// in new ucodes.
	WriteProtectMemory(g_dsp.iram, DSP_IRAM_BYTE_SIZE, false);
	DSPAnalyzer::Analyze();

	return true;
}

void DSPCore_Shutdown()
{
	FreeMemoryPages(g_dsp.irom, DSP_IROM_BYTE_SIZE);
	FreeMemoryPages(g_dsp.iram, DSP_IRAM_BYTE_SIZE);
	FreeMemoryPages(g_dsp.dram, DSP_DRAM_BYTE_SIZE);
	FreeMemoryPages(g_dsp.coef, DSP_COEF_BYTE_SIZE);
}

void DSPCore_Reset()
{
    _assert_msg_(MASTER_LOG, !g_dsp.exception_in_progress_hack, "reset while exception");
    g_dsp.pc = DSP_RESET_VECTOR;
    g_dsp.exception_in_progress_hack = false;

	g_dsp.r[DSP_REG_WR0] = 0xffff;
	g_dsp.r[DSP_REG_WR1] = 0xffff;
	g_dsp.r[DSP_REG_WR2] = 0xffff;
	g_dsp.r[DSP_REG_WR3] = 0xffff;
}

void DSPCore_SetException(u8 level)
{
	g_dsp.exceptions |= 1 << level;
}

void DSPCore_CheckExternalInterrupt()
{
	// check if there is an external interrupt
	if (g_dsp.cr & CR_EXTERNAL_INT)
	{
		if (dsp_SR_is_flag_set(FLAG_ENABLE_INTERUPT) && (g_dsp.exception_in_progress_hack == false))
		{
			// level 7 is the interrupt exception
			DSPCore_SetException(7);
			
			g_dsp.cr &= ~CR_EXTERNAL_INT;
		}
	}
}

void DSPCore_CheckExceptions()
{
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
