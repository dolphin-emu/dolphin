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

#include "Common.h"
#include "ColorUtil.h"

namespace ColorUtil
{

const int lut5to8[] = { 0x00,0x08,0x10,0x18,0x20,0x29,0x31,0x39,
						0x41,0x4A,0x52,0x5A,0x62,0x6A,0x73,0x7B,
						0x83,0x8B,0x94,0x9C,0xA4,0xAC,0xB4,0xBD,
						0xC5,0xCD,0xD5,0xDE,0xE6,0xEE,0xF6,0xFF };
const int lut4to8[] = { 0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
						0x88,0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF };
const int lut3to8[] = { 0x00,0x24,0x48,0x6D,0x91,0xB6,0xDA,0xFF };

u32 Decode5A3(u16 val)
{
	const u32 bg_color = 0x00000000;

	int r, g, b, a;

	if (val & 0x8000)
	{
		r = lut5to8[(val >> 10) & 0x1f];
		g = lut5to8[(val >> 5) & 0x1f];
		b = lut5to8[(val) & 0x1f];
		a = 0xFF;
	}
	else
	{
		a = lut3to8[(val >> 12) & 0x7];
		r = (lut4to8[(val >> 8) & 0xf] * a + (bg_color & 0xFF) * (255 - a)) / 255;
		g = (lut4to8[(val >> 4) & 0xf] * a + ((bg_color >> 8) & 0xFF) * (255 - a)) / 255;
		b = (lut4to8[(val) & 0xf] * a + ((bg_color >> 16) & 0xFF) * (255 - a)) / 255;
		a = 0xFF;
	}
	return (a << 24) | (r << 16) | (g << 8) | b;
}

}  // namespace
