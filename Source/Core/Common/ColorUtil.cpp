// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/ColorUtil.h"
#include "Common/CommonFuncs.h"

namespace ColorUtil
{

static const int s_lut5to8[] = {
	0x00,0x08,0x10,0x18,0x20,0x29,0x31,0x39,
	0x41,0x4A,0x52,0x5A,0x62,0x6A,0x73,0x7B,
	0x83,0x8B,0x94,0x9C,0xA4,0xAC,0xB4,0xBD,
	0xC5,0xCD,0xD5,0xDE,0xE6,0xEE,0xF6,0xFF
};

static const int s_lut4to8[] = {
	0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
	0x88,0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF
};

static const int s_lut3to8[] = {
	0x00,0x24,0x48,0x6D,0x91,0xB6,0xDA,0xFF
};

static u32 Decode5A3(u16 val)
{
	const u32 bg_color = 0x00000000;

	int r, g, b, a;

	if (val & 0x8000)
	{
		r = s_lut5to8[(val >> 10) & 0x1f];
		g = s_lut5to8[(val >> 5) & 0x1f];
		b = s_lut5to8[(val) & 0x1f];
		a = 0xFF;
	}
	else
	{
		a = s_lut3to8[(val >> 12) & 0x7];
		r = (s_lut4to8[(val >> 8) & 0xf] * a + (bg_color & 0xFF) * (255 - a)) / 255;
		g = (s_lut4to8[(val >> 4) & 0xf] * a + ((bg_color >> 8) & 0xFF) * (255 - a)) / 255;
		b = (s_lut4to8[(val) & 0xf] * a + ((bg_color >> 16) & 0xFF) * (255 - a)) / 255;
		a = 0xFF;
	}
	return (a << 24) | (r << 16) | (g << 8) | b;
}

void decode5A3image(u32* dst, u16* src, int width, int height)
{
	for (int y = 0; y < height; y += 4)
	{
		for (int x = 0; x < width; x += 4)
		{
			for (int iy = 0; iy < 4; iy++, src += 4)
			{
				for (int ix = 0; ix < 4; ix++)
				{
					u32 RGBA = Decode5A3(Common::swap16(src[ix]));
					dst[(y + iy) * width + (x + ix)] = RGBA;
				}
			}
		}
	}
}

void decodeCI8image(u32* dst, u8* src, u16* pal, int width, int height)
{
	for (int y = 0; y < height; y += 4)
	{
		for (int x = 0; x < width; x += 8)
		{
			for (int iy = 0; iy < 4; iy++, src += 8)
			{
				u32 *tdst = dst+(y+iy)*width+x;
				for (int ix = 0; ix < 8; ix++)
				{
					// huh, this seems wrong. CI8, not 5A3, no?
					tdst[ix] = ColorUtil::Decode5A3(Common::swap16(pal[src[ix]]));
				}
			}
		}
	}
}

}  // namespace
