/*====================================================================

   filename:     gdsp_registers.h
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
#ifndef _GDSP_REGISTERS_H
#define _GDSP_REGISTERS_H

#include "Common.h"

// Register table taken from libasnd
#define DSP_REG_AR0         0x00    // address registers
#define DSP_REG_AR1         0x01
#define DSP_REG_AR2         0x02
#define DSP_REG_AR3         0x03    // used as jump function selector

#define DSP_REG_IX0         0x04    // LEFT_VOLUME accel
#define DSP_REG_IX1         0x05    // RIGHT_VOLUME accel
#define DSP_REG_IX2         0x06    // ADDRH_SMP accel
#define DSP_REG_IX3         0x07    // ADDRL_SMP accel

#define DSP_REG_R08         0x08    // fixed to 48000 value
#define DSP_REG_R09         0x09    // problems using this
#define DSP_REG_R0A         0x0a    // ADDREH_SMP accel
#define DSP_REG_R0B         0x0b    // ADDREL_SMP accel

#define DSP_REG_ST0         0x0c
#define DSP_REG_ST1         0x0d
#define DSP_REG_ST2         0x0e
#define DSP_REG_ST3         0x0f

#define DSP_REG_CONFIG      0x12
#define DSP_REG_SR          0x13

#define DSP_REG_PRODL       0x14
#define DSP_REG_PRODM       0x15
#define DSP_REG_PRODH       0x16
#define DSP_REG_PRODM2      0x17 

#define DSP_REG_AXL0        0x18
#define DSP_REG_AXL1        0x19
#define DSP_REG_AXH0        0x1a    // SMP_R accel
#define DSP_REG_AXH1        0x1b    // SMP_L accel

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

// These are probably not accurate. Do not use yet.
#define SR_UNKNOWN    0x0002   // ????????
#define SR_ARITH_ZERO 0x0004
#define SR_SIGN       0x0008
#define SR_TOP2BITS   0x0020   // this is an odd one.
#define SR_LOGIC_ZERO 0x0040   // ?? duddie's doc sometimes say & 1<<6 (0x40), sometimes 1<<14 (0x4000), while we have 0x20 .. eh
#define SR_INT_ENABLE 0x0200   // Not 100% sure but duddie says so. This should replace the hack, if so.
#define SR_MUL_MODIFY 0x2000   // 1 = normal. 0 = x2   (M0, M2)
#define SR_TOP_BIT_UNK 0x8000   // 1 = normal. 0 = x2  (CLR15, SET15)
#define SR_16_BIT      0x4000   // 1 = "16", 0 = "40"  (SET40, SET16) .. actually, seems it's the reverse. Controls sign extension when loading mid accums.

void dsp_reg_store_stack(u8 stack_reg, u16 val);
u16 dsp_reg_load_stack(u8 stack_reg);

#endif
