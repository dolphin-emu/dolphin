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

#include <map>
#include <vector>
#include <algorithm>

#include "VideoCommon.h"
#include "Statistics.h"

#include "VertexShaderManager.h"
#include "VertexLoader.h"
#include "VertexLoaderManager.h"

static int s_attr_dirty;  // bitfield

static VertexLoader *g_VertexLoaders[8];

namespace VertexLoaderManager
{

typedef std::map<VertexLoaderUID, VertexLoader *> VertexLoaderMap;
static VertexLoaderMap g_VertexLoaderMap;
// TODO - change into array of pointers. Keep a map of all seen so far.

void Init()
{
	MarkAllDirty();
	for (int i = 0; i < 8; i++)
		g_VertexLoaders[i] = NULL;
	RecomputeCachedArraybases();
}

void Shutdown()
{
	for (VertexLoaderMap::iterator iter = g_VertexLoaderMap.begin(); iter != g_VertexLoaderMap.end(); ++iter)
	{
		delete iter->second;
	}
	g_VertexLoaderMap.clear();
}

namespace {
struct entry {
	std::string text;
	u64 num_verts;
	bool operator < (const entry &other) const {
		return num_verts > other.num_verts;
	}
};
}

void AppendListToString(std::string *dest)
{
	std::vector<entry> entries;

	size_t total_size = 0;
	for (VertexLoaderMap::const_iterator iter = g_VertexLoaderMap.begin(); iter != g_VertexLoaderMap.end(); ++iter)
	{
		entry e;
		iter->second->AppendToString(&e.text);
		e.num_verts = iter->second->GetNumLoadedVerts();
		entries.push_back(e);
		total_size += e.text.size() + 1;
	}
	sort(entries.begin(), entries.end());
	dest->reserve(dest->size() + total_size);
	for (std::vector<entry>::const_iterator iter = entries.begin(); iter != entries.end(); ++iter) {
		dest->append(iter->text);
	}
}

void MarkAllDirty()
{
	s_attr_dirty = 0xff;
}

static void RefreshLoader(int vtx_attr_group)
{
	if ((s_attr_dirty >> vtx_attr_group) & 1)
	{
		VertexLoaderUID uid;
		uid.InitFromCurrentState(vtx_attr_group);
		VertexLoaderMap::iterator iter = g_VertexLoaderMap.find(uid);
		if (iter != g_VertexLoaderMap.end())
		{
			g_VertexLoaders[vtx_attr_group] = iter->second;
		}
		else
		{
			VertexLoader *loader = new VertexLoader(g_VtxDesc, g_VtxAttr[vtx_attr_group]);
			g_VertexLoaderMap[uid] = loader;
			g_VertexLoaders[vtx_attr_group] = loader;
			INCSTAT(stats.numVertexLoaders);
		}
	}
	s_attr_dirty &= ~(1 << vtx_attr_group);
}

void RunVertices(int vtx_attr_group, int primitive, int count)
{
	if (!count)
		return;
	RefreshLoader(vtx_attr_group);
	g_VertexLoaders[vtx_attr_group]->RunVertices(vtx_attr_group, primitive, count);
}

int GetVertexSize(int vtx_attr_group)
{
	RefreshLoader(vtx_attr_group);
	return g_VertexLoaders[vtx_attr_group]->GetVertexSize();
}

}  // namespace

void LoadCPReg(u32 sub_cmd, u32 value)
{
	switch (sub_cmd & 0xF0)
	{
	case 0x30:
		VertexShaderManager::SetTexMatrixChangedA(value);
		break;

	case 0x40:
		VertexShaderManager::SetTexMatrixChangedB(value);
		break;

	case 0x50:
		g_VtxDesc.Hex &= ~0x1FFFF;  // keep the Upper bits
		g_VtxDesc.Hex |= value;
		s_attr_dirty = 0xFF;
		break;

	case 0x60:
		g_VtxDesc.Hex &= 0x1FFFF;  // keep the lower 17Bits
		g_VtxDesc.Hex |= (u64)value << 17;
		s_attr_dirty = 0xFF;
		break;

	case 0x70:
		_assert_((sub_cmd & 0x0F) < 8);
		g_VtxAttr[sub_cmd & 7].g0.Hex = value;
		s_attr_dirty |= 1 << (sub_cmd & 7);
		break;

	case 0x80:
		_assert_((sub_cmd & 0x0F) < 8);
		g_VtxAttr[sub_cmd & 7].g1.Hex = value;
		s_attr_dirty |= 1 << (sub_cmd & 7);
		break;

	case 0x90:
		_assert_((sub_cmd & 0x0F) < 8);
		g_VtxAttr[sub_cmd & 7].g2.Hex = value;
		s_attr_dirty |= 1 << (sub_cmd & 7);
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

void RecomputeCachedArraybases()
{
	for (int i = 0; i < 16; i++)
	{
		cached_arraybases[i] = Memory_GetPtr(arraybases[i]);
	}
}
