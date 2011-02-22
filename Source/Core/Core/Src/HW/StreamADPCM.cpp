// Copyright (C) 2003 Dolphin Project.

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

// Adapted from in_cube by hcs & destop

#include "StreamADPCM.h"

// STATE_TO_SAVE (not saved yet!)
static s32 histl1;
static s32 histl2;
static s32 histr1;
static s32 histr2;

s16 ADPDecodeSample(s32 bits, s32 q, s32& hist1, s32& hist2)
{
	s32 hist = 0;
	switch (q >> 4)
	{
	case 0:
		hist = 0;
		break;
	case 1:
		hist = (hist1 * 0x3c);
		break;
	case 2:
		hist = (hist1 * 0x73) - (hist2 * 0x34);
		break;
	case 3:
		hist = (hist1 * 0x62) - (hist2 * 0x37);
		break;
	}
	hist = (hist + 0x20) >> 6;
	if (hist >  0x1fffff) hist =  0x1fffff;
	if (hist < -0x200000) hist = -0x200000;

	s32 cur = (((s16)(bits << 12) >> (q & 0xf)) << 6) + hist;
	
	hist2 = hist1;
	hist1 = cur;

	cur >>= 6;

	if (cur < -0x8000) return -0x8000;
	if (cur >  0x7fff) return  0x7fff;

	return (s16)cur;
}

void NGCADPCM::InitFilter()
{
	histl1 = 0;
	histl2 = 0;
	histr1 = 0;
	histr2 = 0;
}

void NGCADPCM::DecodeBlock(s16 *pcm, const u8 *adpcm)
{
	for (int i = 0; i < SAMPLES_PER_BLOCK; i++)
	{
		pcm[i * 2]     = ADPDecodeSample(adpcm[i + (ONE_BLOCK_SIZE - SAMPLES_PER_BLOCK)] & 0xf, adpcm[0], histl1, histl2);
		pcm[i * 2 + 1] = ADPDecodeSample(adpcm[i + (ONE_BLOCK_SIZE - SAMPLES_PER_BLOCK)] >> 4,  adpcm[1], histr1, histr2);
	}
}
