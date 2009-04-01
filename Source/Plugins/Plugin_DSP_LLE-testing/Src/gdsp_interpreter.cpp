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

#include "gdsp_interface.h"
#include "gdsp_opcodes_helper.h"
#include "Tools.h"

//-------------------------------------------------------------------------------

SDSP g_dsp;

u16 SDSP::r[32];
u16 SDSP::pc = 0;
u16 SDSP::err_pc = 0;
u16* SDSP::iram   = 0;
u16* SDSP::dram   = 0;
u16* SDSP::irom   = 0;
u16* SDSP::drom   = 0;
u16* SDSP::coef   = 0;
u8* SDSP::cpu_ram = 0;
u16 SDSP::cr = 0;
u8 SDSP::reg_stack_ptr[4];
// lets make stack depth to 32 for now
u16 SDSP::reg_stack[4][DSP_STACK_DEPTH];
void (*SDSP::irq_request)() = NULL;
bool SDSP::exception_in_progress_hack = false;

// for debugger only
u32 SDSP::iram_crc = 0;
u64 SDSP::step_counter = 0;


//-------------------------------------------------------------------------------

static bool CR_HALT = true;
static bool CR_EXTERNAL_INT = false;

void UpdateCachedCR()
{
	CR_HALT = (g_dsp.cr & 0x4) != 0;
	CR_EXTERNAL_INT = (g_dsp.cr & 0x02) != 0;
}

//-------------------------------------------------------------------------------

void dbg_error(char* err_msg)
{
	return;
}

void gdsp_init()
{
	g_dsp.irom = (u16*)malloc(DSP_IROM_SIZE * sizeof(u16));
	g_dsp.iram = (u16*)malloc(DSP_IRAM_SIZE * sizeof(u16));
	g_dsp.drom = (u16*)malloc(DSP_DROM_SIZE * sizeof(u16));
	g_dsp.dram = (u16*)malloc(DSP_DRAM_SIZE * sizeof(u16));
	g_dsp.coef = (u16*)malloc(DSP_COEF_SIZE * sizeof(u16));

	for (int i = 0; i < DSP_IRAM_SIZE; i++)
	{
		g_dsp.iram[i] = 0x0021; // HALT opcode
	}

	for (int i = 0; i < DSP_DRAM_SIZE; i++)
	{
		g_dsp.dram[i] = 0x0021; // HALT opcode
	}

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

	// copied from a real console after the custom UCode has been loaded
	g_dsp.r[0x08] = 0xffff;
	g_dsp.r[0x09] = 0xffff;
	g_dsp.r[0x0a] = 0xffff;
	g_dsp.r[0x0b] = 0xffff;

	g_dsp.cr = 0x804;
	gdsp_ifx_init();

	UpdateCachedCR();
}


void gdsp_reset()
{
//	_assert_msg_(0, "gdsp_reset()");
    _assert_msg_(MASTER_LOG, !g_dsp.exception_in_progress_hack, "assert while exception");
    g_dsp.pc = DSP_RESET_VECTOR;
    g_dsp.exception_in_progress_hack = false;
}


u8 gdsp_exceptions = 0;
void gdsp_generate_exception(u8 level)
{
	gdsp_exceptions |= 1 << level;
}


bool gdsp_load_rom(char* fname)
{
	FILE* pFile = fopen(fname, "rb");

	if (pFile)
	{
		fread(g_dsp.irom, 1, DSP_IRAM_SIZE, pFile);
		fclose(pFile);
		return(true);
	}

	return(false);
}


bool gdsp_load_coef(char* fname)
{
	FILE* pFile = fopen(fname, "rb");

	if (pFile)
	{
		fread(g_dsp.coef, 1, DSP_COEF_SIZE, pFile);
		fclose(pFile);
		return(true);
	}

	return(false);
}


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


u16  gdsp_read_cr()
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

	return(g_dsp.cr);
}


// special loop step.. because exception in loop or loopi fails
// dunno how we have to fix it
// atm we execute this instructions directly inside the loop command
// so it cant be interrupted by an exception
void gdsp_loop_step()
{
	g_dsp.err_pc = g_dsp.pc;
	u16 opc = dsp_fetch_code();
	ComputeInstruction(UDSPInstruction(opc));
}

u16 HLE_ROM_80E7_81F8();
void hacks();
void gdsp_step()
{
	g_dsp.step_counter++;

	if (g_dsp.pc == 0x80e7)
	{
		//g_dsp.pc = HLE_ROM_80E7_81F8();
	}

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
	ComputeInstruction(UDSPInstruction(opc));

	u16& rLoopCounter = g_dsp.r[DSP_REG_ST0 + 3];

	if (rLoopCounter > 0)
	{
		const u16& rCallAddress = g_dsp.r[DSP_REG_ST0 + 0];
		const u16& rLoopAddress = g_dsp.r[DSP_REG_ST0 + 2];

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
	if  (CR_EXTERNAL_INT)
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
	if ((gdsp_exceptions > 0) && (!g_dsp.exception_in_progress_hack))
	{
		for (u8 i=0; i<8; i++)
		{
			if (gdsp_exceptions & (1<<i))
			{
                            _assert_msg_(MASTER_LOG, !g_dsp.exception_in_progress_hack, "assert while exception");

				dsp_reg_store_stack(DSP_STACK_C, g_dsp.pc);
				dsp_reg_store_stack(DSP_STACK_D, g_dsp.r[R_SR]);

				g_dsp.pc = i * 2;
				gdsp_exceptions &= ~(1<<i);

				g_dsp.exception_in_progress_hack = true;
				break;
			}
		}
	}
}


bool gdsp_running;
extern volatile u32 dsp_running;

bool gdsp_run()
{
	gdsp_running = true;

	while (!CR_HALT)
	{
		gdsp_step();
	}

	gdsp_running = false;
	return(true);
}


bool gdsp_runx(u16 cnt)
{
	gdsp_running = true;

	while (!(g_dsp.cr & 0x4) && gdsp_running)
	{
		gdsp_step();
		cnt--;

		if (cnt == 0)
		{
			break;
		}
	}

	gdsp_running = false;
	return(true);
}


void gdsp_stop()
{
	gdsp_running = false;
}

///////////////////////////////////////////////////////////////////////////////
//
// From fires for fires :)
//
//


#include "disassemble.h"
//#include "WaveFile.h"
#include "Mixer.h"

u16 r30 = 0, r31 = 0;
//extern WaveFileWriter g_wave_writer;

extern u16 dsp_swap16(u16 x);
void Hacks()
{
	// if (g_wave_writer.GetAudioSize() > 1024*1024*1)

	/*	if (g_dsp.pc == 0x165)
	{
	PanicAlert("Opcode_06");

	}
	if (g_dsp.pc == 0x43b)
	{
	PanicAlert("Opcode_14");

	}
	if (g_dsp.pc == 0xb37)
	{
	PanicAlert("Opcode_08");

	}*/
	/*	if (g_dsp.pc == 0x1bc)
	{
	r30 = g_dsp.r[30];
	r31 = g_dsp.r[31];
	}
	else if (g_dsp.pc == 0x384)
	{
	// if ((r30 == 0x1bc) && (r31 == 0xaff))
	{
	//PanicAlert("%x, %x", r30, r31);

	const int numSamples = 0x280;
	static short Buffer[numSamples];

	u16 bufferAddr = 0x280; //dsp_dmem_read(0xe44);
	for (int i=0; i<numSamples; i++)
	{
	Buffer[i] = dsp_dmem_read(bufferAddr+i);
	}

	g_wave_writer.AddStereoSamples(Buffer, numSamples/2); // 2 channels

	if (g_wave_writer.GetAudioSize() > 1024*1024*2)
	{
	//PanicAlert("%x", bufferAddr);
	g_wave_writer.Stop();
	exit(1);
	}
	}
	} */

	if (g_dsp.pc == 0x468)
	{
		int numSamples = g_dsp.r[25] / 2;
		u16 bufferAddr = g_dsp.r[27]; 

		// PanicAlert("%x %x", bufferAddr, numSamples);

		short samples[1024];
		for (int i=0; i<numSamples; i++)
		{
			samples[i] = dsp_dmem_read(bufferAddr+i);
		}
		soundStream->GetMixer()->PushSamples(samples, numSamples / 2, 32000); //sample_rate);

		//	g_wave_writer.AddStereoSamples(samples, numSamples/2); // 2 channels
		
		//		if (g_wave_writer.GetAudioSize() > 1024*1024*2)
		//	{
			//PanicAlert("%x", bufferAddr);
		//			g_wave_writer.Stop();
		//			exit(1);
		//		}
	}
}
