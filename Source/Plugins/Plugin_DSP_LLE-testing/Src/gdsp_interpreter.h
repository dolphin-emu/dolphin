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

#define DSP_IRAM_BYTE_SIZE   0x2000
#define DSP_IRAM_SIZE        0x1000
#define DSP_IRAM_MASK        0x0fff

#define DSP_IROM_BYTE_SIZE   0x2000
#define DSP_IROM_SIZE        0x1000
#define DSP_IROM_MASK        0x0fff

#define DSP_DRAM_BYTE_SIZE   0x2000
#define DSP_DRAM_SIZE        0x1000
#define DSP_DRAM_MASK        0x0fff

#define DSP_COEF_BYTE_SIZE   0x2000
#define DSP_COEF_SIZE        0x1000
#define DSP_COEF_MASK        0x0fff

#define DSP_RESET_VECTOR     0x8000

#define DSP_STACK_DEPTH 0x20
#define DSP_STACK_MASK  0x1f

struct SDSP
{
	u16 r[32];
	u16 pc;
	u16 err_pc;
	u16* iram;
	u16* dram;
	u16* irom;
	u16* coef;
	u8* cpu_ram;
	u16 cr;
	u8 reg_stack_ptr[4];
	u8 exceptions;   // pending exceptiosn?

	// lets make stack depth to 32 for now
	u16 reg_stack[4][DSP_STACK_DEPTH];
	void (* irq_request)(void);

	// for debugger only
	bool dump_imem;
	u32 iram_crc;
	u64 step_counter;
	bool exception_in_progress_hack;
};

extern SDSP g_dsp;


void gdsp_init();
void gdsp_reset();
void gdsp_shutdown();

bool gdsp_load_irom(const char *fname);
bool gdsp_load_coef(const char *fname);


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

// sets a flag in the pending exception register.
void gdsp_generate_exception(u8 level);

#endif
