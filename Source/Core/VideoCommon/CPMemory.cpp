// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "VideoCommon/CPMemory.h"

// CP state
u8 *cached_arraybases[16];

CPState g_main_cp_state;

void DoCPState(PointerWrap& p)
{
	p.DoArray(g_main_cp_state.array_bases, 16);
	p.DoArray(g_main_cp_state.array_strides, 16);
	p.Do(g_main_cp_state.matrix_index_a);
	p.Do(g_main_cp_state.matrix_index_b);
	p.Do(g_main_cp_state.vtx_desc.Hex);
	p.DoArray(g_main_cp_state.vtx_attr, 8);
	p.DoMarker("CP Memory");
}
