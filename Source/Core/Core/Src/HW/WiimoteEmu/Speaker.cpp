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

#include "WiimoteEmu.h"
#include <fstream>

// Yamaha ADPCM decoder code based on The ffmpeg Project (Copyright (c) 2001-2003)

typedef struct ADPCMChannelStatus
{
	int predictor;
	int step;
} ADPCMChannelStatus;

static const int yamaha_difflookup[] = {
	1, 3, 5, 7, 9, 11, 13, 15,
	-1, -3, -5, -7, -9, -11, -13, -15
};

static const int yamaha_indexscale[] = {
	230, 230, 230, 230, 307, 409, 512, 614,
	230, 230, 230, 230, 307, 409, 512, 614
};

static u16 av_clip_int16(int a)
{
	if ((a+32768) & ~65535) return (a>>31) ^ 32767;
	else                    return a;
}

static int av_clip(int a, int amin, int amax)
{
	if      (a < amin) return amin;
	else if (a > amax) return amax;
	else               return a;
}

static s16 adpcm_yamaha_expand_nibble(ADPCMChannelStatus *c, unsigned char nibble)
{
	if(!c->step) {
		c->predictor = 0;
		c->step = 127;
	}

	c->predictor += (c->step * yamaha_difflookup[nibble]) / 8;
	c->predictor = av_clip_int16(c->predictor);
	c->step = (c->step * yamaha_indexscale[nibble]) >> 8;
	c->step = av_clip(c->step, 127, 24567);
	return c->predictor;
}

namespace WiimoteEmu
{
ADPCMChannelStatus cs;

void Wiimote::SpeakerData(wm_speaker_data* sd)
{
	s16 samples[40];
	u16 sampleRate = 6000000 / Common::swap16(m_reg_speaker.sample_rate);

	if (m_reg_speaker.format == 0x40)
	{
		// 8 bit PCM
		for (int i = 0; i < 20; ++i)
		{
			samples[i] = (s16)(s8)sd->data[i];
		}
	}
	else if (m_reg_speaker.format == 0x00)
	{
		// 4 bit Yamaha ADPCM
		// TODO: The first byte of the source data = length?
		for (int i = 0; i < 20; ++i)
		{
			samples[i * 2] = adpcm_yamaha_expand_nibble(&cs, sd->data[i] & 0x0F);
			samples[i * 2 + 1] = adpcm_yamaha_expand_nibble(&cs, (sd->data[i] >> 4) & 0x0F);
		}
	}
}

}
