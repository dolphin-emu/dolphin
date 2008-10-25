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

#include "VertexShaderManager.h"
#include "VertexLoader.h"
#include "VertexLoaderManager.h"

// The one and only vtx_desc
TVtxDesc g_vtx_desc;
// There are 8 vtx_attr structures. They will soon live here.

const TVtxDesc &GetVtxDesc()
{
	return g_vtx_desc;
}

namespace VertexLoaderManager {

void Init() {
	g_vtx_desc.Hex = 0;
}

void Shutdown() {

}

void RunVertices(int vtx_attr_group, int primitive, int count)
{
	if (!count)
		return;
	// TODO - grab load the correct vertex loader if anything is dirty.
	g_VertexLoaders[vtx_attr_group].RunVertices(primitive, count);
}

}  // namespace


void LoadCPReg(u32 sub_cmd, u32 value)
{
	switch (sub_cmd & 0xF0)
	{
	case 0x30:
		VertexShaderMngr::SetTexMatrixChangedA(value);
		break;
	case 0x40:
		VertexShaderMngr::SetTexMatrixChangedB(value);
		break;

	case 0x50:
		g_vtx_desc.Hex &= ~0x1FFFF; // keep the Upper bits
		g_vtx_desc.Hex |= value;
		break;
	case 0x60:
		g_vtx_desc.Hex &= 0x1FFFF; // keep the lower 17Bits
		g_vtx_desc.Hex |= (u64)value << 17;
		break;

	case 0x70: g_VertexLoaders[sub_cmd & 7].SetVAT_group0(value); _assert_((sub_cmd & 0x0F) < 8); break;
	case 0x80: g_VertexLoaders[sub_cmd & 7].SetVAT_group1(value); _assert_((sub_cmd & 0x0F) < 8); break;
	case 0x90: g_VertexLoaders[sub_cmd & 7].SetVAT_group2(value); _assert_((sub_cmd & 0x0F) < 8); break;

	case 0xA0: arraybases[sub_cmd & 0xF]   = value & 0xFFFFFFFF; break;
	case 0xB0: arraystrides[sub_cmd & 0xF] = value & 0xFF; break;
	}
}
