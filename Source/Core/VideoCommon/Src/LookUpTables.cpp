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

#include "LookUpTables.h"

const int lut3to8[] = { 0x00,0x24,0x48,0x6D,0x91,0xB6,0xDA,0xFF};
const int lut4to8[] = { 0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
                        0x88,0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF};
const int lut5to8[] = { 0x00,0x08,0x10,0x18,0x20,0x29,0x31,0x39,
                        0x41,0x4A,0x52,0x5A,0x62,0x6A,0x73,0x7B,
                        0x83,0x8B,0x94,0x9C,0xA4,0xAC,0xB4,0xBD,
                        0xC5,0xCD,0xD5,0xDE,0xE6,0xEE,0xF6,0xFF};
int lut6to8[64];
float lutu8tosfloat[256];
float lutu8toufloat[256];
float luts8tosfloat[256];
float shiftLookup[32];

void InitLUTs()
{
	for (int i = 0; i < 32; i++)
		shiftLookup[i] = 1.0f / float(1 << i);
	for (int i = 0; i < 64; i++)
		lut6to8[i] = (i*255) / 63;
	for (int i = 0; i < 256; i++)
	{
		lutu8tosfloat[i] = (float)(i - 128) / 127.0f;
		lutu8toufloat[i] = (float)(i) / 255.0f;
		luts8tosfloat[i] = ((float)(signed char)(char)i) / 127.0f;
	}
}
