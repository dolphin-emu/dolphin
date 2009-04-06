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

#include "Globals.h"
#include "gdsp_interface.h"
#include "gdsp_interpreter.h"

extern u16 dsp_swap16(u16 x);

// The hardware adpcm decoder :)
s16 ADPCM_Step(u32& _rSamplePos, u32 _BaseAddress)
{
	s16* pCoefTable = (s16*)&gdsp_ifx_regs[DSP_COEF_A1_0];

	if (((_rSamplePos) & 15) == 0)
	{
		gdsp_ifx_regs[DSP_PRED_SCALE] = g_dspInitialize.pARAM_Read_U8((_rSamplePos & ~15) >> 1);
		_rSamplePos += 2;
	}

	int scale = 1 << (gdsp_ifx_regs[DSP_PRED_SCALE] & 0xF);
	int coef_idx = gdsp_ifx_regs[DSP_PRED_SCALE] >> 4;

	s32 coef1 = pCoefTable[coef_idx * 2 + 0];
	s32 coef2 = pCoefTable[coef_idx * 2 + 1];

	int temp = (_rSamplePos & 1) ?
		   (g_dspInitialize.pARAM_Read_U8(_rSamplePos >> 1) & 0xF) :
		   (g_dspInitialize.pARAM_Read_U8(_rSamplePos >> 1) >> 4);

	if (temp >= 8)
		temp -= 16;

	// 0x400 = 0.5  in 11-bit fixed point
	int val = (scale * temp) + ((0x400 + coef1 * (s16)gdsp_ifx_regs[DSP_YN1] + coef2 * (s16)gdsp_ifx_regs[DSP_YN2]) >> 11);

	// Clamp values.
	if (val > 0x7FFF)
		val = 0x7FFF;
	else if (val < -0x7FFF)
		val = -0x7FFF;

	gdsp_ifx_regs[DSP_YN2] = gdsp_ifx_regs[DSP_YN1];
	gdsp_ifx_regs[DSP_YN1] = val;

	_rSamplePos++;

    // The advanced interpolation (linear, polyphase,...) is done by the UCode, so we don't
	// need to bother with it here.
	return val;
}

u16 dsp_read_aram()
{
  //	u32 BaseAddress = (gdsp_ifx_regs[DSP_ACSAH] << 16) | gdsp_ifx_regs[DSP_ACSAL];
	u32 EndAddress = (gdsp_ifx_regs[DSP_ACEAH] << 16) | gdsp_ifx_regs[DSP_ACEAL];
	u32 Address = (gdsp_ifx_regs[DSP_ACCAH] << 16) | gdsp_ifx_regs[DSP_ACCAL];

	u16 val;

	// lets the "hardware" decode
	switch (gdsp_ifx_regs[DSP_FORMAT])
	{
	    case 0x00:
		    val = ADPCM_Step(Address, EndAddress);
		    break;

	    case 0x0A:
		    val = (g_dspInitialize.pARAM_Read_U8(Address) << 8) | g_dspInitialize.pARAM_Read_U8(Address + 1);

		    gdsp_ifx_regs[DSP_YN2] = gdsp_ifx_regs[DSP_YN1];
		    gdsp_ifx_regs[DSP_YN1] = val;

		    Address += 2;
		    break;

	    default:
		    val = (g_dspInitialize.pARAM_Read_U8(Address) << 8) | g_dspInitialize.pARAM_Read_U8(Address + 1);
		    Address += 2;
		    ERROR_LOG(DSPLLE, "Unknown DSP Format %i", gdsp_ifx_regs[DSP_FORMAT]);
		    break;
	}

	// TODO: Take ifx GAIN into account.


	// check for loop
	if (Address > EndAddress)
	{
		Address = (gdsp_ifx_regs[DSP_ACSAH] << 16) | gdsp_ifx_regs[DSP_ACSAL];
		gdsp_generate_exception(3);
		gdsp_generate_exception(5);

		// Somehow, YN1 and YN2 must be initialized with their "loop" values, so yeah,
		// it seems likely that we should raise an exception to let the DSP program do that,
		// at least if DSP_FORMAT == 0x0A.
	}

	gdsp_ifx_regs[DSP_ACCAH] = Address >> 16;
	gdsp_ifx_regs[DSP_ACCAL] = Address & 0xffff;
	return(val);
}
