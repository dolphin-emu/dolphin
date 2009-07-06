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

#include <gccore.h>
#include <ogcsys.h>
#include <ogc/dsp.h>
#include <ogc/irq.h>
#include <ogc/machine/asm.h>
#include <ogc/machine/processor.h>

#include "dsp_interface.h"
#include "real_dsp.h"

static vu16* const _dspReg = (u16*)0xCC005000;

// Handler for DSP interrupt.
static void dsp_irq_handler(u32 nIrq, void *pCtx)
{
	// Acknowledge interrupt?
	_dspReg[5] = (_dspReg[5] & ~(DSPCR_AIINT|DSPCR_ARINT)) | DSPCR_DSPINT;
}

void RealDSP::Init()
{
	_dspReg[5] = (_dspReg[5] & ~(DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT)) | DSPCR_DSPRESET;
	_dspReg[5] = (_dspReg[5] & ~(DSPCR_HALT|DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT));

	// This code looks odd - shouldn't we initialize level?
	u32 level;
	_CPU_ISR_Disable(level);
	IRQ_Request(IRQ_DSP_DSP, dsp_irq_handler, NULL);
	_CPU_ISR_Restore(level);
}

void RealDSP::Reset()
{
	// Reset the DSP.
	_dspReg[5] = (_dspReg[5] & ~(DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT)) | DSPCR_DSPRESET;
	_dspReg[5] = (_dspReg[5] & ~(DSPCR_HALT|DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT));
	_dspReg[5] |= DSPCR_RES;
	while (_dspReg[5] & DSPCR_RES)
		;
	_dspReg[9] = 0x63;
}

u32 RealDSP::CheckMailTo()
{
	return DSP_CheckMailTo();
}

void RealDSP::SendMailTo(u32 mail)
{
	DSP_SendMailTo(mail);
}
