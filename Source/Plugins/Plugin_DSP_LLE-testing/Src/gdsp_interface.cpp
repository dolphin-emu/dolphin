/*====================================================================

   filename:     gdsp_interface.h
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

#include <stdlib.h>
#include "Globals.h"
#include "Thread.h"

#include "gdsp_aram.h"
#include "gdsp_interpreter.h"
#include "gdsp_interface.h"

#include "Tools.h"

#ifndef NULL
#define NULL 0
#endif

const char* reg_names[] =
{
	// a0
	"COEF_A1_0", "COEF_A2_0", "COEF_A1_1", "COEF_A2_1", "COEF_A1_2", "COEF_A2_2", "COEF_A1_3", "COEF_A2_3",
	"COEF_A1_4", "COEF_A2_4", "COEF_A1_5", "COEF_A2_5", "COEF_A1_6", "COEF_A2_6", "COEF_A1_7", "COEF_A2_7",
	// b0
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	// c0
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   "DSCR",  NULL,   "DSBL",  NULL,   "DSPA",  "DSMAH", "DSMAL",
	// d0
	NULL,   "SampleFormat",  NULL,           NULL,   "ACSAH", "ACSAL", "ACEAH", "ACEAL",
	"ACCAH", "ACCAL",     "PRED_SCALE",    "YN1",   "YN2",       "ARAM",      "GAIN",      NULL,
	// e0
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   "AMDM",
	// f0
	NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,   NULL,
	NULL,   NULL,   NULL,   NULL,   "DMBH", "DMBL", "CMBH", "CMBL",
};


void gdsp_dma();

#if WITH_DSP_ON_THREAD
Common::CriticalSection g_CriticalSection;
#endif

static volatile u16 gdsp_mbox[2][2];

u16 gdsp_ifx_regs[256];

void gdsp_ifx_init()
{
	for (int i = 0; i < 256; i++)
	{
		gdsp_ifx_regs[i] = 0;
	}

	gdsp_mbox[0][0] = 0;
	gdsp_mbox[0][1] = 0;
	gdsp_mbox[1][0] = 0;
	gdsp_mbox[1][1] = 0;
}


u32 gdsp_mbox_peek(u8 mbx)
{
#if WITH_DSP_ON_THREAD
	g_CriticalSection.Enter();
#endif
	u32 value = ((gdsp_mbox[mbx][0] << 16) | gdsp_mbox[mbx][1]);
#if WITH_DSP_ON_THREAD
	g_CriticalSection.Leave();
#endif
	return value;
}


void gdsp_mbox_write_h(u8 mbx, u16 val)
{
#if WITH_DSP_ON_THREAD
	g_CriticalSection.Enter();
#endif

	gdsp_mbox[mbx][0] = val & 0x7fff;

#if WITH_DSP_ON_THREAD
	g_CriticalSection.Leave();
#endif
}


void gdsp_mbox_write_l(u8 mbx, u16 val)
{
#if WITH_DSP_ON_THREAD
	g_CriticalSection.Enter();
#endif

	gdsp_mbox[mbx][1]  = val;
	gdsp_mbox[mbx][0] |= 0x8000;

#if WITH_DSP_ON_THREAD
	g_CriticalSection.Leave();
#endif

	if (mbx == GDSP_MBOX_DSP)
	{
		DEBUG_LOG(DSPLLE, " - DSP writes mail to mbx %i: 0x%08x (pc=0x%04x)", mbx, gdsp_mbox_peek(GDSP_MBOX_DSP), g_dsp.err_pc);
	}
}


u16 gdsp_mbox_read_h(u8 mbx)
{
	return (gdsp_mbox[mbx][0]);  // TODO: mask away the top bit?
}


u16 gdsp_mbox_read_l(u8 mbx)
{
	u16 val;
#if WITH_DSP_ON_THREAD
	g_CriticalSection.Enter();
#endif

	val = gdsp_mbox[mbx][1];
	gdsp_mbox[mbx][0] &= ~0x8000;

	DEBUG_LOG(DSPLLE, "- DSP reads mail from mbx %i: %08x (pc=0x%04x)", mbx, gdsp_mbox_peek(mbx), g_dsp.err_pc);

#if WITH_DSP_ON_THREAD
	g_CriticalSection.Leave();
#endif
	return val;
}


void gdsp_ifx_write(u16 addr, u16 val)
{
	addr &= 0xff;

	switch (addr & 0xff)
	{
	    case 0xfb: // DIRQ

		    if (val & 0x1)
		    {
			    g_dsp.irq_request();
		    }

		    break;

	    case 0xfc: // DMBH
		    gdsp_mbox_write_h(GDSP_MBOX_DSP, val);
		    break;

	    case 0xfd: // DMBL
		    gdsp_mbox_write_l(GDSP_MBOX_DSP, val);
		    break;

	    case 0xcb: // DSBL
		    gdsp_ifx_regs[addr] = val;
		    gdsp_dma();
		    gdsp_ifx_regs[DSP_DSCR] &= ~0x0004;
		    break;

	    case 0xcd:
	    case 0xce:
	    case 0xcf:
	    case 0xc9:
		    gdsp_ifx_regs[addr] = val;
		    break;

	    default:
/*		if ((addr & 0xff) >= 0xa0 && reg_names[addr - 0xa0])
   	    DEBUG_LOG(DSPLLE, "%04x MW %s (%04x)\n", g_dsp.pc, reg_names[addr - 0xa0], val);
   	else
   	    DEBUG_LOG(DSPLLE, "%04x MW %04x (%04x)\n", g_dsp.pc, addr, val);*/
		    gdsp_ifx_regs[addr] = val;
		    break;
	}
}


u16 gdsp_ifx_read(u16 addr)
{
	u16 val;

	addr &= 0xff;

	switch (addr & 0xff)
	{
	    case 0xfc: // DMBH
		    val = gdsp_mbox_read_h(GDSP_MBOX_DSP);
		    break;

	    case 0xfe: // CMBH
		    val = gdsp_mbox_read_h(GDSP_MBOX_CPU);
		    break;

	    case 0xff: // CMBL
		    val = gdsp_mbox_read_l(GDSP_MBOX_CPU);
		    break;

	    case 0xc9:
		    val = gdsp_ifx_regs[addr];
		    break;

	    case 0xdd:
		    val = dsp_read_aram();
		    break;

	    default:
		    val = gdsp_ifx_regs[addr];
/*		if ((addr & 0xff) >= 0xc0 && reg_names[addr & 0x3f])
   	    printf("%04x MR %s (%04x)\n", g_dsp.pc, reg_names[addr & 0x3f], val);
   	else
   	    printf("%04x MR %04x (%04x)\n", g_dsp.pc, addr, val);*/
		    break;
	}

	return(val);
}


void gdsp_idma_in(u16 dsp_addr, u32 addr, u32 size)
{
	u8* dst = ((u8*)g_dsp.iram);

	for (u32 i = 0; i < size; i += 2)
	{
		// TODO : this may be different on Wii.
		*(u16*)&dst[dsp_addr + i] = *(u16*)&g_dsp.cpu_ram[(addr + i) & 0x0fffffff];
	}

	g_dsp.iram_crc = GenerateCRC(g_dsp.cpu_ram + (addr & 0x0fffffff), size);
	INFO_LOG(DSPLLE, "*** Copy new UCode from 0x%08x to 0x%04x (crc: %8x)\n", addr, dsp_addr, g_dsp.iram_crc);

	if (g_dsp.dump_imem)
		DumpDSPCode(&dst[dsp_addr], size, g_dsp.iram_crc);
}


void gdsp_idma_out(u16 dsp_addr, u32 addr, u32 size)
{
	ERROR_LOG(DSPLLE, "*** idma_out IRAM_DSP (0x%04x) -> RAM (0x%08x) : size (0x%08x)\n", dsp_addr / 2, addr, size);
}


void gdsp_ddma_in(u16 dsp_addr, u32 addr, u32 size)
{
	if ((addr & 0x7FFFFFFF) > 0x01FFFFFF)
	{
		ERROR_LOG(DSPLLE, "*** ddma_in read from invalid addr (0x%08x)\n", addr);
		return;
	}

	u8* dst = ((u8*)g_dsp.dram);

	for (u32 i = 0; i < size; i += 2)
	{
		*(u16*)&dst[dsp_addr + i] = *(u16*)&g_dsp.cpu_ram[(addr + i) & 0x7FFFFFFF];
	}

	INFO_LOG(DSPLLE, "*** ddma_in RAM (0x%08x) -> DRAM_DSP (0x%04x) : size (0x%08x)\n", addr, dsp_addr / 2, size);
}


void gdsp_ddma_out(u16 dsp_addr, u32 addr, u32 size)
{
	if ((addr & 0x7FFFFFFF) > 0x01FFFFFF)
	{
		ERROR_LOG(DSPLLE, "*** gdsp_ddma_out to invalid addr (0x%08x)\n", addr);
		return;
	}

	u8* src = ((u8*)g_dsp.dram);

	for (u32 i = 0; i < size; i += 2)
	{
		*(u16*)&g_dsp.cpu_ram[(addr + i) & 0x7FFFFFFF] = *(u16*)&src[dsp_addr + i];
	}

	INFO_LOG(DSPLLE, "*** ddma_out DRAM_DSP (0x%04x) -> RAM (0x%08x) : size (0x%08x)\n", dsp_addr / 2, addr, size);
}


#define DSP_CR_IMEM     (2)
#define DSP_CR_DMEM     (0)
#define DSP_CR_TO_CPU   (1)
#define DSP_CR_FROM_CPU (0)

void gdsp_dma()
{
	u16 ctl;
	u32 addr;
	u16 dsp_addr;
	u16 len;

	addr = (gdsp_ifx_regs[DSP_DSMAH] << 16) | gdsp_ifx_regs[DSP_DSMAL];
	ctl = gdsp_ifx_regs[DSP_DSCR];
	dsp_addr = gdsp_ifx_regs[DSP_DSPA] * 2;
	len = gdsp_ifx_regs[DSP_DSBL];

	if ((ctl > 3) || (len > 0x4000))
	{
		ERROR_LOG(DSPLLE, "DMA ERROR pc: %04x ctl: %04x addr: %08x da: %04x size: %04x\n", g_dsp.pc, ctl, addr, dsp_addr, len);
		exit(0);
	}

	switch (ctl & 0x3)
	{
	    case (DSP_CR_DMEM | DSP_CR_TO_CPU):
		    gdsp_ddma_out(dsp_addr, addr, len);
		    break;

	    case (DSP_CR_DMEM | DSP_CR_FROM_CPU):
		    gdsp_ddma_in(dsp_addr, addr, len);
		    break;

	    case (DSP_CR_IMEM | DSP_CR_TO_CPU):
		    gdsp_idma_out(dsp_addr, addr, len);
		    break;

	    case (DSP_CR_IMEM | DSP_CR_FROM_CPU):
		    gdsp_idma_in(dsp_addr, addr, len);
		    break;
	}
}


