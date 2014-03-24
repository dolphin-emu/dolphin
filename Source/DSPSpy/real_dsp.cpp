// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <gccore.h>
#include <ogcsys.h>
#include <ogc/dsp.h>
#include <ogc/irq.h>
#include <ogc/machine/asm.h>
#include <ogc/machine/processor.h>

#include "dsp_interface.h"
#include "real_dsp.h"

static vu16* const dspReg = (u16*)0xCC005000;

// Handler for DSP interrupt.
static void dsp_irq_handler(u32 nIrq, void *pCtx)
{
	// Acknowledge interrupt?
	dspReg[5] = (dspReg[5] & ~(DSPCR_AIINT|DSPCR_ARINT)) | DSPCR_DSPINT;
}

void RealDSP::Init()
{
	dspReg[5] = (dspReg[5] & ~(DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT)) | DSPCR_DSPRESET;
	dspReg[5] = (dspReg[5] & ~(DSPCR_HALT|DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT));

	u32 level;
	CPU_ISR_Disable(level);
	IRQ_Request(IRQ_DSP_DSP, dsp_irq_handler, nullptr);
	CPU_ISR_Restore(level);
}

void RealDSP::Reset()
{
	// Reset the DSP.
	dspReg[5] = (dspReg[5] & ~(DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT)) | DSPCR_DSPRESET;
	dspReg[5] = (dspReg[5] & ~(DSPCR_HALT|DSPCR_AIINT|DSPCR_ARINT|DSPCR_DSPINT));
	dspReg[5] |= DSPCR_RES;
	while (dspReg[5] & DSPCR_RES)
		;
	dspReg[9] = 0x63;
}

u32 RealDSP::CheckMailTo()
{
	return DSP_CheckMailTo();
}

void RealDSP::SendMailTo(u32 mail)
{
	DSP_SendMailTo(mail);
}
