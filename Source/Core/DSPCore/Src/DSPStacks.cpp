/*====================================================================

   filename:     gdsp_registers.cpp
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
#include "DSPStacks.h"

// Stacks. The stacks are outside the DSP RAM, in dedicated hardware.

void dsp_reg_stack_push(u8 stack_reg)
{
	g_dsp.reg_stack_ptr[stack_reg]++;
	g_dsp.reg_stack_ptr[stack_reg] &= DSP_STACK_MASK;
	g_dsp.reg_stack[stack_reg][g_dsp.reg_stack_ptr[stack_reg]] = g_dsp.r[DSP_REG_ST0 + stack_reg];
}

void dsp_reg_stack_pop(u8 stack_reg)
{
	g_dsp.r[DSP_REG_ST0 + stack_reg] = g_dsp.reg_stack[stack_reg][g_dsp.reg_stack_ptr[stack_reg]];
	g_dsp.reg_stack_ptr[stack_reg]--;
	g_dsp.reg_stack_ptr[stack_reg] &= DSP_STACK_MASK;
}

void dsp_reg_store_stack(u8 stack_reg, u16 val)
{
	dsp_reg_stack_push(stack_reg);
	g_dsp.r[DSP_REG_ST0 + stack_reg] = val;
}

u16 dsp_reg_load_stack(u8 stack_reg)
{
	u16 val = g_dsp.r[DSP_REG_ST0 + stack_reg];
	dsp_reg_stack_pop(stack_reg);
	return val;
}
