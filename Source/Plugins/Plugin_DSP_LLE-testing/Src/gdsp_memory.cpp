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
#include "gdsp_ifx.h"

u16 dsp_swap16(u16 x)
{
	return((x >> 8) | (x << 8));
}


u16* gdsp_get_iram(void)
{
	return(g_dsp.iram);
}


u16* gdsp_get_irom(void)
{
	return(g_dsp.irom);
}


u16* gdsp_get_dram(void)
{
	return(g_dsp.dram);
}


u16* gdsp_get_drom(void)
{
	return(g_dsp.drom);
}


u16 dsp_imem_read(u16 addr)
{
	u16 opc;

	if (g_dsp.pc & 0x8000)
	{
		opc = g_dsp.irom[addr & DSP_IROM_MASK];
	}
	else
	{
		opc = g_dsp.iram[addr & DSP_IRAM_MASK];
	}

	return(dsp_swap16(opc));
}


u16  dsp_dmem_read(u16 addr)
{
	u16 val;

	switch (addr >> 12)
	{
	    case 0x0: // 0xxx DRAM
		    val = g_dsp.dram[addr & DSP_DRAM_MASK];
		    val = dsp_swap16(val);
		    break;

	    case 0x8: // 8xxx DROM
		    DEBUG_LOG(DSPHLE, "someone reads from ROM\n");
		    val = g_dsp.drom[addr & DSP_DROM_MASK];
		    val = dsp_swap16(val);
		    break;

		case 0x1: // 1xxx COEF
		    val = g_dsp.coef[addr & DSP_DROM_MASK];
		    val = dsp_swap16(val);
		    break;

	    case 0xf: // Fxxx HW regs
		    val = gdsp_ifx_read(addr);
		    break;

	    default: // error
//		ERROR_LOG(DSPHLE, "%04x DSP ERROR: Read from UNKNOWN (%04x) memory\n", g_dsp.pc, addr);
		    val = 0;
		    break;
	}

	return(val);
}


bool dsp_dmem_write(u16 addr, u16 val)
{
	switch (addr >> 12)
	{
	    case 0x8: // 8xxx DROM
		    DEBUG_LOG(DSPHLE, "someone writes to ROM\n");
		    /* val = dsp_swap16(val);
		       g_dsp.drom[addr & DSP_DROM_MASK] = val;*/
		    break;

	    case 0xf: // Fxxx HW regs
		    gdsp_ifx_write(addr, val);
		    break;

	    case 0x0: // 0xxx DRAM
		    val = dsp_swap16(val);
		    g_dsp.dram[addr & DSP_DRAM_MASK] = val;
		    break;

	    default: // error
		    DEBUG_LOG(DSPHLE, "%04x DSP ERROR: Write to UNKNOWN (%04x) memory\n", g_dsp.pc, addr);
		    break;
	}

	return(true);
}

u16 dsp_fetch_code()
{
	u16 opc = dsp_imem_read(g_dsp.pc);
	g_dsp.pc++;
	return opc;
}

u16 dsp_peek_code(void)
{
	return dsp_imem_read(g_dsp.pc);
}
