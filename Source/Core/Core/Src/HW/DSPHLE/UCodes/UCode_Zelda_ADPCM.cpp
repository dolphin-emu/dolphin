// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"
#include "UCode_Zelda.h"

void CUCode_Zelda::AFCdecodebuffer(const s16 *coef, const char *src, signed short *out, short *histp, short *hist2p, int type)
{
	// First 2 nibbles are ADPCM scale etc.
	short delta = 1 << (((*src) >> 4) & 0xf);
	short idx = (*src) & 0xf;
	src++;

	short nibbles[16];
	if (type == 9)
	{
		for (int i = 0; i < 16; i += 2)
		{
			nibbles[i + 0] = *src >> 4;
			nibbles[i + 1] = *src & 15;
			src++;
		}
		for (int i = 0; i < 16; i++) {
			if (nibbles[i] >= 8) 
				nibbles[i] = nibbles[i] - 16;
			nibbles[i] <<= 11;
		}
	}
	else
	{
		// In Pikmin, Dolphin's engine sound is using AFC type 5, even though such a sound is hard
		// to compare, it seems like to sound exactly like a real GC
		// In Super Mario Sunshine, you can get such a sound by talking to/jumping on anyone
		for (int i = 0; i < 16; i += 4)
		{
			nibbles[i + 0] = (*src >> 6) & 0x03;
			nibbles[i + 1] = (*src >> 4) & 0x03;
			nibbles[i + 2] = (*src >> 2) & 0x03;
			nibbles[i + 3] = (*src >> 0) & 0x03;
			src++;
		}

		for (int i = 0; i < 16; i++) 
		{
			if (nibbles[i] >= 2) 
				nibbles[i] = nibbles[i] - 4;
			nibbles[i] <<= 13;
		}
	}

	short hist = *histp;
	short hist2 = *hist2p;
	for (int i = 0; i < 16; i++)
	{
		int sample = delta * nibbles[i] + ((int)hist * coef[idx * 2]) + ((int)hist2 * coef[idx * 2 + 1]);
		sample >>= 11;
		if (sample > 32767)
			sample = 32767;
		if (sample < -32768)
			sample = -32768;
		out[i] = sample;
		hist2 = hist;
		hist = (short)sample;
	}
	*histp = hist;
	*hist2p = hist2;
} 
