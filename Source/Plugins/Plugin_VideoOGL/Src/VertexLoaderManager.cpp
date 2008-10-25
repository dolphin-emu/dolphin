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

static bool s_desc_dirty;
static bool s_attr_dirty[8];

// TODO - change into array of pointers. Keep a map of all seen so far.
static VertexLoader g_VertexLoaders[8];

namespace VertexLoaderManager
{

void Init()
{
	s_desc_dirty = false;
	for (int i = 0; i < 8; i++)
		s_attr_dirty[i] = false;
}

void Shutdown()
{

}

void RunVertices(int vtx_attr_group, int primitive, int count)
{
	if (!count)
		return;
	// TODO - grab & load the correct vertex loader if anything is dirty.
	g_VertexLoaders[vtx_attr_group].RunVertices(primitive, count);
}

int GetVertexSize(int vtx_attr_group)
{
	// The vertex loaders will soon cache the vertex size.
	return g_VertexLoaders[vtx_attr_group].ComputeVertexSize();
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
		g_VtxDesc.Hex &= ~0x1FFFF;  // keep the Upper bits
		g_VtxDesc.Hex |= value;
		s_desc_dirty = true;
		break;

	case 0x60:
		g_VtxDesc.Hex &= 0x1FFFF;  // keep the lower 17Bits
		g_VtxDesc.Hex |= (u64)value << 17;
		s_desc_dirty = true;
		break;

	case 0x70:
		_assert_((sub_cmd & 0x0F) < 8);
		g_VtxAttr[sub_cmd & 7].g0.Hex = value;
		g_VertexLoaders[sub_cmd & 7].SetVAT_group0(value);
		s_attr_dirty[sub_cmd & 7] = true;
		break;

	case 0x80:
		_assert_((sub_cmd & 0x0F) < 8);
		g_VtxAttr[sub_cmd & 7].g1.Hex = value;
		g_VertexLoaders[sub_cmd & 7].SetVAT_group1(value);
		s_attr_dirty[sub_cmd & 7] = true;
		break;

	case 0x90:
		_assert_((sub_cmd & 0x0F) < 8);
		g_VtxAttr[sub_cmd & 7].g2.Hex = value;
		g_VertexLoaders[sub_cmd & 7].SetVAT_group2(value);
		s_attr_dirty[sub_cmd & 7] = true;
		break;

	// Pointers to vertex arrays in GC RAM
	case 0xA0:
		arraybases[sub_cmd & 0xF] = value & 0xFFFFFFFF;  // huh, why the mask?
		break;

	case 0xB0:
		arraystrides[sub_cmd & 0xF] = value & 0xFF;
		break;
	}
}
