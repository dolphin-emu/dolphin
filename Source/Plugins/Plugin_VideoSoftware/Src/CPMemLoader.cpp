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
#include "HW/Memmap.h"

// CP state
static u8 *_cached_arraybases[16];

// STATE_TO_SAVE
static u32 _arraybases[16];
static u32 _arraystrides[16];
static TMatrixIndexA _MatrixIndexA;
static TMatrixIndexB _MatrixIndexB;
static TVtxDesc _g_VtxDesc;
static VAT _g_VtxAttr[8];

void SWLoadCPReg(u32 sub_cmd, u32 value)
{
    switch (sub_cmd & 0xF0)
	{
	case 0x30:
        _MatrixIndexA.Hex = value;
		break;

	case 0x40:
		_MatrixIndexB.Hex = value;
		break;

	case 0x50:
		_g_VtxDesc.Hex &= ~0x1FFFF;  // keep the Upper bits
		_g_VtxDesc.Hex |= value;
		break;

	case 0x60:
		_g_VtxDesc.Hex &= 0x1FFFF;  // keep the lower 17Bits
		_g_VtxDesc.Hex |= (u64)value << 17;
		break;

	case 0x70:
		_assert_((sub_cmd & 0x0F) < 8);
		_g_VtxAttr[sub_cmd & 7].g0.Hex = value;
		break;

	case 0x80:
		_assert_((sub_cmd & 0x0F) < 8);
		_g_VtxAttr[sub_cmd & 7].g1.Hex = value;
		break;

	case 0x90:
		_assert_((sub_cmd & 0x0F) < 8);
		_g_VtxAttr[sub_cmd & 7].g2.Hex = value;
		break;

	// Pointers to vertex arrays in GC RAM
	case 0xA0:
		_arraybases[sub_cmd & 0xF] = value;
        _cached_arraybases[sub_cmd & 0xF] = Memory::GetPointer(value);
		break;

	case 0xB0:
		_arraystrides[sub_cmd & 0xF] = value & 0xFF;
		break;
	}
}
