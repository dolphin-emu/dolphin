// Copyright (C) 2003-2008 Dolphin Project.

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

#ifndef _UCODE_AX_ADPCM_H
#define _UCODE_AX_ADPCM_H

inline s16 ADPCM_Step(PBADPCMInfo &adpcm, u32& samplePos, u32 newSamplePos, u16 frac)
{
	while (samplePos < newSamplePos)
	{
		if ((samplePos & 15) == 0)
		{
			adpcm.pred_scale = g_dspInitialize.pARAM_Read_U8((samplePos & ~15) >> 1);
			samplePos    += 2;
			newSamplePos += 2;
		}

		int scale = 1 << (adpcm.pred_scale & 0xF);
		int coef_idx = adpcm.pred_scale >> 4;

		s32 coef1 = adpcm.coefs[coef_idx * 2 + 0];
		s32 coef2 = adpcm.coefs[coef_idx * 2 + 1];

		int temp = (samplePos & 1) ?
			   (g_dspInitialize.pARAM_Read_U8(samplePos >> 1) & 0xF) :
			   (g_dspInitialize.pARAM_Read_U8(samplePos >> 1) >> 4);

		if (temp >= 8)
			temp -= 16;

		// 0x400 = 0.5 in 11-bit fixed point
		int val = (scale * temp) + ((0x400 + coef1 * adpcm.yn1 + coef2 * adpcm.yn2) >> 11);

		if (val > 0x7FFF)
			val = 0x7FFF;
		else if (val < -0x7FFF)
			val = -0x7FFF;
		
		adpcm.yn2 = adpcm.yn1;
		adpcm.yn1 = val;

		samplePos++;
	}

	return adpcm.yn1;
}

// =======================================================================================
// Volume control (ramping)
// --------------
inline u16 ADPCM_Vol(u16 vol, u16 delta, u16 mixer_control)
{
	int x = vol;
	if (delta && delta < 0x5000)
		x += delta * 20 * 8; // unsure what the right step is
	else if (delta && delta > 0x5000)
		//x -= (0x10000 - delta); // this is to small, it's often 1
		x -= (0x10000 - delta) * 20 * 16; // if this was 20 * 8 the sounds in Fire Emblem and Paper Mario
			// did not have time to go to zero before the were closed

	 // make lower limits
	if (x < 0) x = 0;	
	//if (pb.mixer_control < 1000 && x < pb.mixer_control) x = pb.mixer_control; // does this make
		// any sense?

	// make upper limits
	//if (mixer_control > 1000 && x > mixer_control) x = mixer_control; // maybe mixer_control also
		// has a volume target?
	//if (x >= 0x7fff) x = 0x7fff; // this seems a little high
	if (x >= 0x4e20) x = 0x4e20; // add a definitive limit at 20 000
	return x; // update volume
}
// ==============

#endif  // _UCODE_AX_ADPCM_H
