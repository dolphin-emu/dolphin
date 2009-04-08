/*====================================================================

   filename:     gdsp_memory.cpp
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

#include "Globals.h"
#include "gdsp_interpreter.h"
#include "gdsp_memory.h"
#include "gdsp_interface.h"

u16 dsp_imem_read(u16 addr)
{
	switch (addr >> 12)
	{
	case 0:
		return g_dsp.iram[addr & DSP_IRAM_MASK];
	case 8:
		return g_dsp.irom[addr & DSP_IROM_MASK];
	default:
		ERROR_LOG(DSPLLE, "%04x DSP ERROR: Executing from invalid (%04x) memory", g_dsp.pc, addr);
		return 0;
	}
}

u16 dsp_dmem_read(u16 addr)
{
	switch (addr >> 12)
	{
	case 0x0: // 0xxx DRAM
		return g_dsp.dram[addr & DSP_DRAM_MASK];
		
	case 0x1: // 1xxx COEF
		return g_dsp.coef[addr & DSP_COEF_MASK];

		// FIXME: unknown addresses used by zelda 
 /*	case 0x2:
	case 0x3:
	case 0x4:
	break;*/

	case 0xf: // Fxxx HW regs
		return gdsp_ifx_read(addr);
		
	default: // error
		ERROR_LOG(DSPLLE, "%04x DSP ERROR: Read from UNKNOWN (%04x) memory", g_dsp.pc, addr);
		return 0;
	}
}

void dsp_dmem_write(u16 addr, u16 val)
{
	switch (addr >> 12)
	{
	case 0x0: // 0xxx DRAM
		g_dsp.dram[addr & DSP_DRAM_MASK] = val;
		break;

	case 0x1: // 1xxx COEF
		ERROR_LOG(DSPLLE, "someone writes to COEF (pc = %02x)", g_dsp.pc);
		break;

	case 0xf: // Fxxx HW regs
		gdsp_ifx_write(addr, val);
		break;

	default: // error
		ERROR_LOG(DSPLLE, "%04x DSP ERROR: Write to UNKNOWN (%04x) memory", g_dsp.pc, addr);
		break;
	}
}