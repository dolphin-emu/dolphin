// Copyright (C) 2010 Dolphin Project.

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

#ifndef __DSPJITUTIL_H__
#define __DSPJITUTIL_H__

#include "../DSPMemoryMap.h"
#include "../DSPHWInterface.h"
#include "../DSPEmitter.h"

static u16 *reg_ptr(int reg) {
	switch(reg) {
	case DSP_REG_AR0:
	case DSP_REG_AR1:
	case DSP_REG_AR2:
	case DSP_REG_AR3:
		return &g_dsp._r.ar[reg - DSP_REG_AR0];
	case DSP_REG_IX0:
	case DSP_REG_IX1:
	case DSP_REG_IX2:
	case DSP_REG_IX3:
		return &g_dsp._r.ix[reg - DSP_REG_IX0];
	case DSP_REG_WR0:
	case DSP_REG_WR1:
	case DSP_REG_WR2:
	case DSP_REG_WR3:
		return &g_dsp._r.wr[reg - DSP_REG_WR0];
	case DSP_REG_ST0:
	case DSP_REG_ST1:
	case DSP_REG_ST2:
	case DSP_REG_ST3:
		return &g_dsp._r.st[reg - DSP_REG_ST0];
	case DSP_REG_ACH0:
	case DSP_REG_ACH1:
		return &g_dsp._r.ac[reg - DSP_REG_ACH0].h;
	case DSP_REG_CR:     return &g_dsp._r.cr;
	case DSP_REG_SR:     return &g_dsp._r.sr;
	case DSP_REG_PRODL:  return &g_dsp._r.prod.l;
	case DSP_REG_PRODM:  return &g_dsp._r.prod.m;
	case DSP_REG_PRODH:  return &g_dsp._r.prod.h;
	case DSP_REG_PRODM2: return &g_dsp._r.prod.m2;
	case DSP_REG_AXL0:
	case DSP_REG_AXL1:
		return &g_dsp._r.ax[reg - DSP_REG_AXL0].l;
	case DSP_REG_AXH0:
	case DSP_REG_AXH1:
		return &g_dsp._r.ax[reg - DSP_REG_AXH0].h;
	case DSP_REG_ACL0:
	case DSP_REG_ACL1:
		return &g_dsp._r.ac[reg - DSP_REG_ACL0].l;
	case DSP_REG_ACM0:
	case DSP_REG_ACM1:
		return &g_dsp._r.ac[reg - DSP_REG_ACM0].m;
	default:
		_assert_msg_(DSP_JIT, 0, "cannot happen");
		return NULL;
	}
}

#endif /*__DSPJITUTIL_H__*/
