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

#include "Thread.h"
#include "MemoryUtil.h"

#include "DSPCore.h"
#include "DSPHost.h"
#include "DSPTables.h"
#include "DSPAnalyzer.h"
#include "DSPAccelerator.h"
#include "DSPInterpreter.h"
#include "DSPHWInterface.h"

void gdsp_do_dma();

Common::CriticalSection g_CriticalSection;

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
	if (DSPHost_OnThread())
		g_CriticalSection.Enter();
	u32 value = ((gdsp_mbox[mbx][0] << 16) | gdsp_mbox[mbx][1]);
	if (DSPHost_OnThread())
		g_CriticalSection.Leave();
	return value;
}

void gdsp_mbox_write_h(u8 mbx, u16 val)
{
	if (DSPHost_OnThread())
		g_CriticalSection.Enter();
	gdsp_mbox[mbx][0] = val & 0x7fff;
	if (DSPHost_OnThread())
		g_CriticalSection.Leave();

	if (mbx == GDSP_MBOX_DSP)
	{
		DEBUG_LOG(DSPLLE, " - DSP writes mail to mbx %i: 0x%08x (pc=0x%04x)", mbx, gdsp_mbox_peek(GDSP_MBOX_DSP), g_dsp.pc);
	} else {
		// mailbox
		//		DSPCore_SetException(EXP_INT);
	}

}


void gdsp_mbox_write_l(u8 mbx, u16 val)
{
	if (DSPHost_OnThread())
		g_CriticalSection.Enter();
	gdsp_mbox[mbx][1]  = val;
	gdsp_mbox[mbx][0] |= 0x8000;
	if (DSPHost_OnThread())
		g_CriticalSection.Leave();

	if (mbx == GDSP_MBOX_DSP)
	{
		DEBUG_LOG(DSPLLE, " - DSP writes mail to mbx %i: 0x%08x (pc=0x%04x)", mbx, gdsp_mbox_peek(GDSP_MBOX_DSP), g_dsp.pc);
	} else {
		// mailbox
		DSPCore_SetException(EXP_INT);
	}
}


u16 gdsp_mbox_read_h(u8 mbx)
{
	return gdsp_mbox[mbx][0];  // TODO: mask away the top bit?
}


u16 gdsp_mbox_read_l(u8 mbx)
{
	if (DSPHost_OnThread())
		g_CriticalSection.Enter();

	u16 val = gdsp_mbox[mbx][1];
	gdsp_mbox[mbx][0] &= ~0x8000;

	DEBUG_LOG(DSPLLE, "- DSP reads mail from mbx %i: %08x (pc=0x%04x)", mbx, gdsp_mbox_peek(mbx), g_dsp.pc);

	if (DSPHost_OnThread())
		g_CriticalSection.Leave();
	return val;
}


void gdsp_ifx_write(u16 addr, u16 val)
{
	switch (addr & 0xff)
	{
	    case 0xfb: // DIRQ
		    if (val & 0x1)
			    DSPHost_InterruptRequest();
		    break;

	    case 0xfc: // DMBH
		    gdsp_mbox_write_h(GDSP_MBOX_DSP, val);
		    break;

	    case 0xfd: // DMBL
		    gdsp_mbox_write_l(GDSP_MBOX_DSP, val);
		    break;

	    case 0xcb: // DSBL
		    gdsp_ifx_regs[addr & 0xFF] = val;
		    gdsp_do_dma();
		    gdsp_ifx_regs[DSP_DSCR] &= ~0x0004;
		    break;

		case 0xd3:   // ZeldaUnk (accelerator WRITE)
		   	DEBUG_LOG(DSPLLE, "Write To ZeldaUnk pc=%04x (%04x)\n", g_dsp.pc, val);
			dsp_write_aram_d3(val);
			break;

		case 0xde:
			//if (val)
			//	PanicAlert("Gain written: %04x", val);   // BMX XXX does, and sounds HORRIBLE.
	    case 0xcd:
	    case 0xce:
	    case 0xcf:
	    case 0xc9:
		    gdsp_ifx_regs[addr & 0xFF] = val;
		    break;

	    default:
			if ((addr & 0xff) >= 0xa0) {
				if (pdlabels[(addr & 0xFF) - 0xa0].name) {
		   			INFO_LOG(DSPLLE, "%04x MW %s (%04x)\n", g_dsp.pc, pdlabels[(addr & 0xFF) - 0xa0].name, val);
				}
				else {
		   			ERROR_LOG(DSPLLE, "%04x MW %04x (%04x)\n", g_dsp.pc, addr, val);
				}
			}
			else {
		   	    ERROR_LOG(DSPLLE, "%04x MW %04x (%04x)\n", g_dsp.pc, addr, val);
			}
		    gdsp_ifx_regs[addr & 0xFF] = val;
		    break;
	}
}

u16 gdsp_ifx_read(u16 addr)
{
	switch (addr & 0xff)
	{
	    case 0xfc:  // DMBH
		    return gdsp_mbox_read_h(GDSP_MBOX_DSP);

	    case 0xfe:  // CMBH
		    return gdsp_mbox_read_h(GDSP_MBOX_CPU);

	    case 0xff:  // CMBL
		    return gdsp_mbox_read_l(GDSP_MBOX_CPU);

	    case 0xc9:
		    return gdsp_ifx_regs[addr & 0xFF];

	    case 0xdd:  // ADPCM Accelerator reads
		    return dsp_read_accelerator();

	    case 0xd3:
	   		ERROR_LOG(DSPLLE, "DSP read aram D3");
		    return dsp_read_aram_d3();

	    default:
			if ((addr & 0xff) >= 0xa0) {
				if (pdlabels[(addr & 0xFF) - 0xa0].name) {
	   				INFO_LOG(DSPLLE, "%04x MR %s (%04x)\n", g_dsp.pc, pdlabels[(addr & 0xFF) - 0xa0].name, gdsp_ifx_regs[addr & 0xFF]);
				}
				else {
	   				ERROR_LOG(DSPLLE, "%04x MR %04x (%04x)\n", g_dsp.pc, addr, gdsp_ifx_regs[addr & 0xFF]);
				}
			}
			else {
   				ERROR_LOG(DSPLLE, "%04x MR %04x (%04x)\n", g_dsp.pc, addr, gdsp_ifx_regs[addr & 0xFF]);
			}
		    return gdsp_ifx_regs[addr & 0xFF];
	}
}

void gdsp_idma_in(u16 dsp_addr, u32 addr, u32 size)
{
	UnWriteProtectMemory(g_dsp.iram, DSP_IRAM_BYTE_SIZE, false);

	u8* dst = ((u8*)g_dsp.iram);
	for (u32 i = 0; i < size; i += 2)
	{ 
		// TODO : this may be different on Wii.
		*(u16*)&dst[dsp_addr + i] = Common::swap16(*(const u16*)&g_dsp.cpu_ram[(addr + i) & 0x0fffffff]);
	}
	WriteProtectMemory(g_dsp.iram, DSP_IRAM_BYTE_SIZE, false);
	
	INFO_LOG(DSPLLE, "*** Copy new UCode from 0x%08x to 0x%04x (crc: %8x)\n", addr, dsp_addr, g_dsp.iram_crc);
	g_dsp.iram_crc = DSPHost_CodeLoaded(g_dsp.cpu_ram + (addr & 0x0fffffff), size);
	DSPAnalyzer::Analyze();
}


void gdsp_idma_out(u16 dsp_addr, u32 addr, u32 size)
{
	ERROR_LOG(DSPLLE, "*** idma_out IRAM_DSP (0x%04x) -> RAM (0x%08x) : size (0x%08x)\n", dsp_addr / 2, addr, size);
}


// TODO: These should eat clock cycles.
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
		*(u16*)&dst[dsp_addr + i] = Common::swap16(*(const u16*)&g_dsp.cpu_ram[(addr + i) & 0x7FFFFFFF]);
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

	const u8* src = ((const u8*)g_dsp.dram);

	for (u32 i = 0; i < size; i += 2)
	{
		*(u16*)&g_dsp.cpu_ram[(addr + i) & 0x7FFFFFFF] = Common::swap16(*(const u16*)&src[dsp_addr + i]);
	}

	INFO_LOG(DSPLLE, "*** ddma_out DRAM_DSP (0x%04x) -> RAM (0x%08x) : size (0x%08x)\n", dsp_addr / 2, addr, size);
}

void gdsp_do_dma()
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
