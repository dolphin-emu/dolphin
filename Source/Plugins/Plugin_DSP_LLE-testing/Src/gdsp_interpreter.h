// Copyright (C) 2003-2009 Dolphin Project.

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
	static u16 r[32];
	static u16 pc;
	static u16 err_pc;
	static u16* iram;
	static u16* dram;
	static u16* irom;
	static u16* drom;
	static u16* coef;
	static u8* cpu_ram;
	static u16 cr;
	static u8 reg_stack_ptr[4];
	// lets make stack depth to 32 for now
	static u16 reg_stack[4][DSP_STACK_DEPTH];
	static void (* irq_request)(void);

	// for debugger only
	static u32 iram_crc;
	static u64 step_counter;
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
bool gdsp_runx(u16 cnt);
void gdsp_stop(void);

void gdsp_write_cr(u16 val);
u16  gdsp_read_cr(void);

u16* gdsp_get_iram(void);
u16* gdsp_get_irom(void);
u16* gdsp_get_dram(void);
u16* gdsp_get_drom(void);

#endif
