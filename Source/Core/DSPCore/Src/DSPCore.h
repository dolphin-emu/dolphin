/*====================================================================

   filename:     DSPCore.h
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

#ifndef _DSPCORE_H
#define _DSPCORE_H

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

#define DSP_CR_IMEM     2
#define DSP_CR_DMEM     0
#define DSP_CR_TO_CPU   1
#define DSP_CR_FROM_CPU 0

struct SDSP
{
	u16 r[32];
	u16 pc;
#if PROFILE
	u16 err_pc;
#endif
	
	// This is NOT the same cr as r[DSP_REG_CR].
	// This register is shared with the main emulation, see DSP.cpp
	// The plugin has control over 0x0C07 of this reg.
	// Bits are defined in a struct in DSP.cpp.
	u16 cr;

	u8 reg_stack_ptr[4];
	u8 exceptions;   // pending exceptions?
	bool exception_in_progress_hack;  // is this the same as "exception enabled"?

	// Let's make stack depth 32 for now. The real DSP has different depths
	// for the different stacks, but it would be strange if any ucode relied on stack
	// overflows since on the DSP, when the stack overflows, you're screwed.
	u16 reg_stack[4][DSP_STACK_DEPTH];

	// For debugging.
	u32 iram_crc;
	u64 step_counter;

	// When state saving, all of the above can just be memcpy'd into the save state.
	// The below needs special handling.
	u16 *iram;
	u16 *dram;
	u16 *irom;
	u16 *coef;

	// This one doesn't really belong here.
	u8  *cpu_ram;
};

extern SDSP g_dsp;

bool DSPCore_Init(const char *irom_filename, const char *coef_filename);
void DSPCore_Reset();
void DSPCore_Shutdown();  // Frees all allocated memory.

void DSPCore_CheckExternalInterrupt();
void DSPCore_CheckExceptions();

// sets a flag in the pending exception register.
void DSPCore_SetException(u8 level);

#endif  // _DSPCORE_H
