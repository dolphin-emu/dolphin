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
#include "Thread.h"
#include "DSPCore.h"
#include "DSPEmitter.h"
#include "DSPHost.h"
#include "DSPAnalyzer.h"
#include "MemoryUtil.h"

#include "DSPHWInterface.h"
#include "DSPIntUtil.h"

SDSP g_dsp;
DSPBreakpoints dsp_breakpoints;
DSPCoreState core_state = DSPCORE_STOP;
u16 cyclesLeft = 0;
DSPEmitter *jit = NULL;
Common::Event step_event;

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
		for (int i = 0; i < size_in_words; i++)
			rom[i] = Common::swap16(rom[i]);

		return true;
	}
	PanicAlert("Failed to load DSP Rom : %s",fname);
	// Always keep ROMs write protected.
	WriteProtectMemory(g_dsp.irom, size_in_bytes, false);
	return false;
}

bool DSPCore_Init(const char *irom_filename, const char *coef_filename,
				  bool bUsingJIT)
{
	g_dsp.step_counter = 0;
	cyclesLeft = 0;
	jit = NULL;
	
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

	memset(&g_dsp.r,0,sizeof(g_dsp.r));

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
	g_dsp.r.wr[0] = 0xffff;
	g_dsp.r.wr[1] = 0xffff;
	g_dsp.r.wr[2] = 0xffff;
	g_dsp.r.wr[3] = 0xffff;

	g_dsp.r.sr |= SR_INT_ENABLE;
	g_dsp.r.sr |= SR_EXT_INT_ENABLE;

	g_dsp.cr = 0x804;
	gdsp_ifx_init();
	// Mostly keep IRAM write protected. We unprotect only when DMA-ing
	// in new ucodes.
	WriteProtectMemory(g_dsp.iram, DSP_IRAM_BYTE_SIZE, false);

	// Initialize JIT, if necessary
	if(bUsingJIT) 
		jit = new DSPEmitter();

	DSPAnalyzer::Analyze();

	step_event.Init();

	core_state = DSPCORE_RUNNING;

	return true;
}

void DSPCore_Shutdown()
{
	core_state = DSPCORE_STOP;

	if(jit) {
		delete jit;
		jit = NULL;
	}
	step_event.Shutdown();
	FreeMemoryPages(g_dsp.irom, DSP_IROM_BYTE_SIZE);
	FreeMemoryPages(g_dsp.iram, DSP_IRAM_BYTE_SIZE);
	FreeMemoryPages(g_dsp.dram, DSP_DRAM_BYTE_SIZE);
	FreeMemoryPages(g_dsp.coef, DSP_COEF_BYTE_SIZE);
}

void DSPCore_Reset()
{
    g_dsp.pc = DSP_RESET_VECTOR;

	g_dsp.r.wr[0] = 0xffff;
	g_dsp.r.wr[1] = 0xffff;
	g_dsp.r.wr[2] = 0xffff;
	g_dsp.r.wr[3] = 0xffff;
	
}

void DSPCore_SetException(u8 level)
{
	g_dsp.exceptions |= 1 << level;
}

// Comming from the CPU
void DSPCore_CheckExternalInterrupt()
{
	if (! dsp_SR_is_flag_set(SR_EXT_INT_ENABLE))
		return;

	// Signal the SPU about new mail
	DSPCore_SetException(EXP_INT);
	
	g_dsp.cr &= ~CR_EXTERNAL_INT;
}


void DSPCore_CheckExceptions()
{
	// Early out to skip the loop in the common case.
	if (g_dsp.exceptions == 0)
		return;

	for (int i = 7; i > 0; i--) {
		// Seems exp int are not masked by sr_int_enable
		if (g_dsp.exceptions & (1 << i)) {
			if (dsp_SR_is_flag_set(SR_INT_ENABLE) || (i == EXP_INT)) {

				// store pc and sr until RTI
				dsp_reg_store_stack(DSP_STACK_C, g_dsp.pc);
				dsp_reg_store_stack(DSP_STACK_D, g_dsp.r.sr);

				g_dsp.pc = i * 2;
				g_dsp.exceptions &= ~(1 << i);
				if (i == 7)
					g_dsp.r.sr &= ~SR_EXT_INT_ENABLE;
				else
					g_dsp.r.sr &= ~SR_INT_ENABLE;
				break;
			} else {
#if defined(_DEBUG) || defined(DEBUGFAST)
				ERROR_LOG(DSPLLE, "Firing exception %d failed", i);
#endif
			}
		}
	}
}

// Delegate to JIT or interpreter as appropriate.
// Handle state changes and stepping.
int DSPCore_RunCycles(int cycles)
{
	if (jit)
	{
		// DSPCore_CheckExceptions();
		// DSPCore_CheckExternalInterrupt();
		cyclesLeft = cycles;

		CompiledCode pExecAddr = (CompiledCode)jit->enterDispatcher;
		pExecAddr();

		// To use the C++ dispatcher, uncomment the line below and comment out the two lines above
		//jit->RunForCycles(cyclesLeft);
		return cyclesLeft;
	}

	while (cycles > 0) {
	reswitch:
		switch (core_state)
		{
		case DSPCORE_RUNNING:
			// Seems to slow things down
#if defined(_DEBUG) || defined(DEBUGFAST)
			cycles = DSPInterpreter::RunCyclesDebug(cycles);
#else
			cycles = DSPInterpreter::RunCycles(cycles);
#endif
			break;

		case DSPCORE_STEPPING:
			step_event.Wait();
			if (core_state != DSPCORE_STEPPING)
				goto reswitch;

			DSPInterpreter::Step();
			cycles--;

			DSPHost_UpdateDebugger();
			break;
		case DSPCORE_STOP:
			break;
		}
	}
	return cycles;
}

void DSPCore_SetState(DSPCoreState new_state)
{
	core_state = new_state;
	// kick the event, in case we are waiting
	if (new_state == DSPCORE_RUNNING)
		step_event.Set();
	// Sleep(10);
	DSPHost_UpdateDebugger();
}

DSPCoreState DSPCore_GetState()
{
	return core_state;
}

void DSPCore_Step()
{
	if (core_state == DSPCORE_STEPPING)
		step_event.Set();
}

void CompileCurrent()
{
	jit->Compile(g_dsp.pc);

	bool retry = true;

	while (retry)
	{
		retry = false;
		for(u16 i = 0x0000; i < 0xffff; ++i)
		{
			if (!jit->unresolvedJumps[i].empty())
			{
				u16 addrToCompile = jit->unresolvedJumps[i].front();
				jit->Compile(addrToCompile);
				if (!jit->unresolvedJumps[i].empty())
					retry = true;
			}
		}
	}
}

u16 DSPCore_ReadRegister(int reg) {
	switch(reg) {
	case DSP_REG_AR0:
	case DSP_REG_AR1:
	case DSP_REG_AR2:
	case DSP_REG_AR3:
		return g_dsp.r.ar[reg - DSP_REG_AR0];
	case DSP_REG_IX0:
	case DSP_REG_IX1:
	case DSP_REG_IX2:
	case DSP_REG_IX3:
		return g_dsp.r.ix[reg - DSP_REG_IX0];
	case DSP_REG_WR0:
	case DSP_REG_WR1:
	case DSP_REG_WR2:
	case DSP_REG_WR3:
		return g_dsp.r.wr[reg - DSP_REG_WR0];
	case DSP_REG_ST0:
	case DSP_REG_ST1:
	case DSP_REG_ST2:
	case DSP_REG_ST3:
		return g_dsp.r.st[reg - DSP_REG_ST0];
	case DSP_REG_ACH0:
	case DSP_REG_ACH1:
		return g_dsp.r.ac[reg - DSP_REG_ACH0].h;
	case DSP_REG_CR:     return g_dsp.r.cr;
	case DSP_REG_SR:     return g_dsp.r.sr;
	case DSP_REG_PRODL:  return g_dsp.r.prod.l;
	case DSP_REG_PRODM:  return g_dsp.r.prod.m;
	case DSP_REG_PRODH:  return g_dsp.r.prod.h;
	case DSP_REG_PRODM2: return g_dsp.r.prod.m2;
	case DSP_REG_AXL0:
	case DSP_REG_AXL1:
		return g_dsp.r.ax[reg - DSP_REG_AXL0].l;
	case DSP_REG_AXH0:
	case DSP_REG_AXH1:
		return g_dsp.r.ax[reg - DSP_REG_AXH0].h;
	case DSP_REG_ACL0:
	case DSP_REG_ACL1:
		return g_dsp.r.ac[reg - DSP_REG_ACL0].l;
	case DSP_REG_ACM0:
	case DSP_REG_ACM1:
		return g_dsp.r.ac[reg - DSP_REG_ACM0].m;
	default:
		_assert_msg_(DSP_CORE, 0, "cannot happen");
		return 0;
	}
}

void DSPCore_WriteRegister(int reg, u16 val) {
	switch(reg) {
	case DSP_REG_AR0:
	case DSP_REG_AR1:
	case DSP_REG_AR2:
	case DSP_REG_AR3:
		g_dsp.r.ar[reg - DSP_REG_AR0] = val;
		break;
	case DSP_REG_IX0:
	case DSP_REG_IX1:
	case DSP_REG_IX2:
	case DSP_REG_IX3:
		g_dsp.r.ix[reg - DSP_REG_IX0] = val;
		break;
	case DSP_REG_WR0:
	case DSP_REG_WR1:
	case DSP_REG_WR2:
	case DSP_REG_WR3:
		g_dsp.r.wr[reg - DSP_REG_WR0] = val;
		break;
	case DSP_REG_ST0:
	case DSP_REG_ST1:
	case DSP_REG_ST2:
	case DSP_REG_ST3:
		g_dsp.r.st[reg - DSP_REG_ST0] = val;
		break;
	case DSP_REG_ACH0:
	case DSP_REG_ACH1:
		g_dsp.r.ac[reg - DSP_REG_ACH0].h = val;
		break;
	case DSP_REG_CR:     g_dsp.r.cr = val; break;
	case DSP_REG_SR:     g_dsp.r.sr = val; break;
	case DSP_REG_PRODL:  g_dsp.r.prod.l = val; break;
	case DSP_REG_PRODM:  g_dsp.r.prod.m = val; break;
	case DSP_REG_PRODH:  g_dsp.r.prod.h = val; break;
	case DSP_REG_PRODM2: g_dsp.r.prod.m2 = val; break;
	case DSP_REG_AXL0:
	case DSP_REG_AXL1:
		g_dsp.r.ax[reg - DSP_REG_AXL0].l = val;
		break;
	case DSP_REG_AXH0:
	case DSP_REG_AXH1:
		g_dsp.r.ax[reg - DSP_REG_AXH0].h = val;
		break;
	case DSP_REG_ACL0:
	case DSP_REG_ACL1:
		g_dsp.r.ac[reg - DSP_REG_ACL0].l = val;
		break;
	case DSP_REG_ACM0:
	case DSP_REG_ACM1:
		g_dsp.r.ac[reg - DSP_REG_ACM0].m = val;
		break;
	}
}
