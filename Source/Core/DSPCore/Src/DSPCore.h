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

#include "Breakpoints.h"

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


// Register table taken from libasnd
#define DSP_REG_AR0         0x00    // address registers
#define DSP_REG_AR1         0x01
#define DSP_REG_AR2         0x02
#define DSP_REG_AR3         0x03

#define DSP_REG_IX0         0x04    // indexing registers (actually, mostly used as increments)
#define DSP_REG_IX1         0x05
#define DSP_REG_IX2         0x06
#define DSP_REG_IX3         0x07

#define DSP_REG_WR0         0x08    // address wrapping registers. should be initialized to 0xFFFF if not used.
#define DSP_REG_WR1         0x09
#define DSP_REG_WR2         0x0a
#define DSP_REG_WR3         0x0b

#define DSP_REG_ST0         0x0c    // stacks.
#define DSP_REG_ST1         0x0d
#define DSP_REG_ST2         0x0e
#define DSP_REG_ST3         0x0f

#define DSP_REG_CR          0x12    // Seems to be the top 8 bits of LRS/SRS.
#define DSP_REG_SR          0x13

#define DSP_REG_PRODL       0x14    // product.
#define DSP_REG_PRODM       0x15
#define DSP_REG_PRODH       0x16
#define DSP_REG_PRODM2      0x17 

#define DSP_REG_AXL0        0x18
#define DSP_REG_AXL1        0x19
#define DSP_REG_AXH0        0x1a
#define DSP_REG_AXH1        0x1b

#define DSP_REG_ACC0        0x1c    // accumulator (global)
#define DSP_REG_ACC1        0x1d

#define DSP_REG_ACL0        0x1c    // Low accumulator
#define DSP_REG_ACL1        0x1d
#define DSP_REG_ACM0        0x1e    // Mid accumulator
#define DSP_REG_ACM1        0x1f
#define DSP_REG_ACH0        0x10    // Sign extended 8 bit register 0
#define DSP_REG_ACH1        0x11    // Sign extended 8 bit register 1

// Hardware registers address

#define DSP_REG_DSCR        0xffc9  // DSP DMA Control Reg
#define DSP_REG_DSBL        0xffcb  // DSP DMA Block Length
#define DSP_REG_DSPA        0xffcd  // DSP DMA DMEM Address
#define DSP_REG_DSMAH       0xffce  // DSP DMA Mem Address H
#define DSP_REG_DSMAL       0xffcf  // DSP DMA Mem Address L

#define DSP_REG_DIRQ        0xfffb  // DSP Irq Rest
#define DSP_REG_DMBH        0xfffc  // DSP Mailbox H
#define DSP_REG_DMBL        0xfffd  // DSP Mailbox L
#define DSP_REG_CMBH        0xfffe  // CPU Mailbox H
#define DSP_REG_CMBL        0xffff  // CPU Mailbox L

#define DMA_TO_DSP          0
#define DMA_TO_CPU          1

// Stacks
#define DSP_STACK_C 0
#define DSP_STACK_D 1

// cr (Not g_dsp.r[CR]) bits
// See HW/DSP.cpp.
#define CR_HALT         0x0004
#define CR_EXTERNAL_INT 0x0002

// SR bits
#define SR_CARRY        0x0001
#define SR_2            0x0002   // overflow???
#define SR_ARITH_ZERO   0x0004
#define SR_SIGN         0x0008
#define SR_10           0x0010   // seem to be set by tst
#define SR_TOP2BITS     0x0020   // this is an odd one. (set by tst)
#define SR_LOGIC_ZERO   0x0040
#define SR_INT_ENABLE   0x0200   // Not 100% sure but duddie says so. This should replace the hack, if so.
#define SR_800          0x0800   // Appears in zelda - what is it? where in the zelda ucode?
#define SR_MUL_MODIFY   0x2000   // 1 = normal. 0 = x2   (M0, M2)
#define SR_40_MODE_BIT  0x4000   // 0 = "16", 1 = "40"  (SET16, SET40)  Controls sign extension when loading mid accums.
#define SR_MUL_UNSIGNED 0x8000   // 0 = normal. 1 = unsigned  (CLR15, SET15) If set, treats operands as unsigned. Tested with mulx only so far.

// This should be the bits affected by CMP. Does not include logic zero.
#define SR_CMP_MASK     0x3f


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
extern BreakPoints dsp_breakpoints;

bool DSPCore_Init(const char *irom_filename, const char *coef_filename);
void DSPCore_Reset();
void DSPCore_Shutdown();  // Frees all allocated memory.

void DSPCore_CheckExternalInterrupt();
void DSPCore_CheckExceptions();

// sets a flag in the pending exception register.
void DSPCore_SetException(u8 level);

#endif  // _DSPCORE_H
