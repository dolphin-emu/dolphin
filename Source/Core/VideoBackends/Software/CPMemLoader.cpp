// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CommonFuncs.h"
#include "Core/HW/Memmap.h"
#include "VideoBackends/Software/CPMemLoader.h"
#include "VideoCommon/VideoCommon.h"


void SWLoadCPReg(u32 sub_cmd, u32 value)
{
	switch (sub_cmd & 0xF0)
	{
	case 0x30:
		g_main_cp_state.matrix_index_a.Hex = value;
		break;

	case 0x40:
		g_main_cp_state.matrix_index_b.Hex = value;
		break;

	case 0x50:
		g_main_cp_state.vtx_desc.Hex &= ~0x1FFFF;  // keep the Upper bits
		g_main_cp_state.vtx_desc.Hex |= value;
		break;

	case 0x60:
		g_main_cp_state.vtx_desc.Hex &= 0x1FFFF;  // keep the lower 17Bits
		g_main_cp_state.vtx_desc.Hex |= (u64)value << 17;
		break;

	case 0x70:
		_assert_((sub_cmd & 0x0F) < 8);
		g_main_cp_state.vtx_attr[sub_cmd & 7].g0.Hex = value;
		break;

	case 0x80:
		_assert_((sub_cmd & 0x0F) < 8);
		g_main_cp_state.vtx_attr[sub_cmd & 7].g1.Hex = value;
		break;

	case 0x90:
		_assert_((sub_cmd & 0x0F) < 8);
		g_main_cp_state.vtx_attr[sub_cmd & 7].g2.Hex = value;
		break;

	// Pointers to vertex arrays in GC RAM
	case 0xA0:
		g_main_cp_state.array_bases[sub_cmd & 0xF] = value;
		cached_arraybases[sub_cmd & 0xF] = Memory::GetPointer(value);
		break;

	case 0xB0:
		g_main_cp_state.array_strides[sub_cmd & 0xF] = value & 0xFF;
		break;
	}
}
