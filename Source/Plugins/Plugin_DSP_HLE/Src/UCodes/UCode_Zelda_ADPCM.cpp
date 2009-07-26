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

#include "Common.h"
#include "UCode_Zelda.h"

void CUCode_Zelda::AFCdecodebuffer(const s16 *coef, const char *input, signed short *out, short *histp, short *hist2p, int type)
{
    short nibbles[16];
    short hist = *histp;
    short hist2 = *hist2p;

    const char *src = input;
	char *dst = (char*)out;

	// First 2 nibbles are ADPCM scale etc.
    short delta = 1 << (((*src) >> 4) & 0xf);
    short idx = (*src) & 0xf;
    src++;

    if (type == 9)
    {
        for (int i = 0; i < 16; i = i + 2) {
            int j = (*src & 255) >> 4;
            nibbles[i] = j;
            j = *src & 255 & 15;
            nibbles[i+1] = j;
            src++;
        }

        for (int i = 0; i < 16; i = i + 1) {
            if (nibbles[i] >= 8) 
                nibbles[i] = nibbles[i] - 16;
        }
    }
    else
    {
        // untested !!! i havnt seen such a sample yet :)

        ERROR_LOG(DSPHLE, "Untested AFC sample");
        
        for (int i = 0; i < 16; i += 4)
        {
            int j = (*src >> 0) & 0x02;
            nibbles[i] = j;
 
			j = (*src >> 2) & 0x02;
            nibbles[i+1] = j;

            j = (*src >> 4) & 0x02;
            nibbles[i+2] = j;

            j = (*src >> 6) & 0x02;
            nibbles[i+3] = j;

            src++;
        }

        for (int i = 0; i < 16; i++) 
        {
            if (nibbles[i] >= 2) 
                nibbles[i] = nibbles[i] - 4;
        }
    }

    for (int i = 0; i < 16; i++)
	{
        int sample = (delta * nibbles[i]) << 11;
        sample += ((long)hist * coef[idx * 2]) + ((long)hist2 * coef[idx * 2 + 1]);
        sample = sample >> 11;

        if (sample > 32767) {
            sample = 32767;
        }
        if (sample < -32768) {
            sample = -32768;
        }
        *(short*)dst = (short)sample;
        dst = dst + 2;
        hist2 = hist;
        hist = (short)sample;
    }
	*histp = hist;
    *hist2p = hist2;
} 
