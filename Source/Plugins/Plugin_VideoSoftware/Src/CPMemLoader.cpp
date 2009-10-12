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

#include "VideoCommon.h"

#include "CPMemLoader.h"


// CP state
u8 *cached_arraybases[16];

// STATE_TO_SAVE
u32 arraybases[16];
u32 arraystrides[16];
TMatrixIndexA MatrixIndexA;
TMatrixIndexB MatrixIndexB;
TVtxDesc g_VtxDesc;
VAT g_VtxAttr[8];


void LoadCPReg(u32 sub_cmd, u32 value)
{
    switch (sub_cmd & 0xF0)
	{
	case 0x30:
        MatrixIndexA.Hex = value;
		break;

	case 0x40:
		MatrixIndexB.Hex = value;
		break;

	case 0x50:
		g_VtxDesc.Hex &= ~0x1FFFF;  // keep the Upper bits
		g_VtxDesc.Hex |= value;
		break;

	case 0x60:
		g_VtxDesc.Hex &= 0x1FFFF;  // keep the lower 17Bits
		g_VtxDesc.Hex |= (u64)value << 17;
		break;

	case 0x70:
		_assert_((sub_cmd & 0x0F) < 8);
		g_VtxAttr[sub_cmd & 7].g0.Hex = value;
		break;

	case 0x80:
		_assert_((sub_cmd & 0x0F) < 8);
		g_VtxAttr[sub_cmd & 7].g1.Hex = value;
		break;

	case 0x90:
		_assert_((sub_cmd & 0x0F) < 8);
		g_VtxAttr[sub_cmd & 7].g2.Hex = value;
		break;

	// Pointers to vertex arrays in GC RAM
	case 0xA0:
		arraybases[sub_cmd & 0xF] = value;
        cached_arraybases[sub_cmd & 0xF] = Memory_GetPtr(value);
		break;

	case 0xB0:
		arraystrides[sub_cmd & 0xF] = value & 0xFF;
		break;
	}
}

