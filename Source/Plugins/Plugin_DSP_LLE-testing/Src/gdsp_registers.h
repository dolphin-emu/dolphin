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

#include "Globals.h"

#define DSP_REG_ST0 0x0c
//#define DSP_REG_ST1 0x0d
//#define DSP_REG_ST2 0x0e
//#define DSP_REG_ST3 0x0f

#define DSP_STACK_C 0
#define DSP_STACK_D 1

void dsp_reg_store_stack(u8 stack_reg, u16 val);
u16 dsp_reg_load_stack(u8 stack_reg);


#endif
