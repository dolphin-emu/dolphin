/*====================================================================

   filename:     gdsp_interpreter.h
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

#ifndef _GDSP_INTERPRETER_H
#define _GDSP_INTERPRETER_H

#include "Globals.h"

#define DSP_IRAM_SIZE   (0x1000)
#define DSP_IRAM_MASK   (0x0fff)
#define DSP_IROM_SIZE   (0x1000)
#define DSP_IROM_MASK   (0x0fff)
#define DSP_DRAM_SIZE   (0x1000)
#define DSP_DRAM_MASK   (0x0fff)
#define DSP_DROM_SIZE   (0x1000)
#define DSP_DROM_MASK   (0x0fff)
#define DSP_COEF_SIZE   (0x1000)
#define DSP_COEF_MASK   (0x0fff)

#define DSP_RESET_VECTOR    (0x8000)

#define DSP_STACK_DEPTH 0x20
#define DSP_STACK_MASK  0x1f

struct SDSP
{
	static uint16 r[32];
	static uint16 pc;
	static uint16 err_pc;
	static uint16* iram;
	static uint16* dram;
	static uint16* irom;
	static uint16* drom;
	static uint16* coef;
	static uint8* cpu_ram;
	static uint16 cr;
	static uint8 reg_stack_ptr[4];
	// lets make stack depth to 32 for now
	static uint16 reg_stack[4][DSP_STACK_DEPTH];
	static void (* irq_request)(void);

	// for debugger only
	static uint32 iram_crc;
	static uint64 step_counter;
	static bool exception_in_progress_hack;
};

extern SDSP g_dsp;


void gdsp_init(void);
void gdsp_reset(void);
bool gdsp_load_rom(char* fname);
bool gdsp_load_coef(char* fname);


// steps through DSP code, returns false if error occured
void gdsp_step(void);
void gdsp_loop_step();
bool gdsp_run(void);
bool gdsp_runx(uint16 cnt);
void gdsp_stop(void);

void    gdsp_write_cr(uint16 val);
uint16  gdsp_read_cr(void);

uint16* gdsp_get_iram(void);
uint16* gdsp_get_irom(void);
uint16* gdsp_get_dram(void);
uint16* gdsp_get_drom(void);

#endif
