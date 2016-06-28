// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <gccore.h>
#include <ogc/dsp.h>
#include <ogc/irq.h>
#include <ogc/machine/asm.h>
#include <ogc/machine/processor.h>
#include <ogcsys.h>

#include "dsp_interface.h"
#include "real_dsp.h"

static vu16* const _dspReg = (u16*)0xCC005000;

// Handler for DSP interrupt.
static void dsp_irq_handler(u32 nIrq, void* pCtx)
{
  // Acknowledge interrupt?
  _dspReg[5] = (_dspReg[5] & ~(DSPCR_AIINT | DSPCR_ARINT)) | DSPCR_DSPINT;
}

void RealDSP::Init()
{
  _dspReg[5] = (_dspReg[5] & ~(DSPCR_AIINT | DSPCR_ARINT | DSPCR_DSPINT)) | DSPCR_DSPRESET;
  _dspReg[5] = (_dspReg[5] & ~(DSPCR_HALT | DSPCR_AIINT | DSPCR_ARINT | DSPCR_DSPINT));

  u32 level;
  _CPU_ISR_Disable(level);
  IRQ_Request(IRQ_DSP_DSP, dsp_irq_handler, nullptr);
  _CPU_ISR_Restore(level);
}

void RealDSP::Reset()
{
  // Reset the DSP.
  _dspReg[5] = (_dspReg[5] & ~(DSPCR_AIINT | DSPCR_ARINT | DSPCR_DSPINT)) | DSPCR_DSPRESET;
  _dspReg[5] = (_dspReg[5] & ~(DSPCR_HALT | DSPCR_AIINT | DSPCR_ARINT | DSPCR_DSPINT));
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
