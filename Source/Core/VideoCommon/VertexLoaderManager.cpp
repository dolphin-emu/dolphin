// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "Core/HW/Memmap.h"

#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoCommon.h"

static int s_attr_dirty;  // bitfield

static VertexLoader *g_VertexLoaders[8];

namespace std
{

template <>
struct hash<VertexLoaderUID>
{
	size_t operator()(const VertexLoaderUID& uid) const
	{
		return uid.GetHash();
	}
};

}

typedef std::unordered_map<VertexLoaderUID, VertexLoader*> VertexLoaderMap;

namespace VertexLoaderManager
{

static VertexLoaderMap g_VertexLoaderMap;
// TODO - change into array of pointers. Keep a map of all seen so far.

void Init()
{
	MarkAllDirty();
	for (VertexLoader*& vertexLoader : g_VertexLoaders)
		vertexLoader = nullptr;
	RecomputeCachedArraybases();
}

void Shutdown()
{
	for (auto& p : g_VertexLoaderMap)
	{
		delete p.second;
	}
	g_VertexLoaderMap.clear();
}

namespace
{
struct entry
{
	std::string text;
	u64 num_verts;
	bool operator < (const entry &other) const
	{
		return num_verts > other.num_verts;
	}
};
}

void AppendListToString(std::string *dest)
{
	std::vector<entry> entries;

	size_t total_size = 0;
	for (const auto& map_entry : g_VertexLoaderMap)
	{
		entry e;
		map_entry.second->AppendToString(&e.text);
		e.num_verts = map_entry.second->GetNumLoadedVerts();
		entries.push_back(e);
		total_size += e.text.size() + 1;
	}
	sort(entries.begin(), entries.end());
	dest->reserve(dest->size() + total_size);
	for (const entry& entry : entries)
	{
		dest->append(entry.text);
	}
}

void MarkAllDirty()
{
	s_attr_dirty = 0xff;
}

static VertexLoader* RefreshLoader(int vtx_attr_group)
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
	return g_VertexLoaders[vtx_attr_group];
}

void RunVertices(int vtx_attr_group, int primitive, int count)
{
	if (!count)
		return;
	RefreshLoader(vtx_attr_group)->RunVertices(vtx_attr_group, primitive, count);
}

int GetVertexSize(int vtx_attr_group)
{
	return RefreshLoader(vtx_attr_group)->GetVertexSize();
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
		cached_arraybases[sub_cmd & 0xF] = Memory::GetPointer(value);
		break;

	case 0xB0:
		arraystrides[sub_cmd & 0xF] = value & 0xFF;
		break;
	}
}

void FillCPMemoryArray(u32 *memory)
{
	memory[0x30] = MatrixIndexA.Hex;
	memory[0x40] = MatrixIndexB.Hex;
	memory[0x50] = (u32)g_VtxDesc.Hex;
	memory[0x60] = (u32)(g_VtxDesc.Hex >> 17);

	for (int i = 0; i < 8; ++i)
	{
		memory[0x70 + i] = g_VtxAttr[i].g0.Hex;
		memory[0x80 + i] = g_VtxAttr[i].g1.Hex;
		memory[0x90 + i] = g_VtxAttr[i].g2.Hex;
	}

	for (int i = 0; i < 16; ++i)
	{
		memory[0xA0 + i] = arraybases[i];
		memory[0xB0 + i] = arraystrides[i];
	}
}

void RecomputeCachedArraybases()
{
	for (int i = 0; i < 16; i++)
	{
		cached_arraybases[i] = Memory::GetPointer(arraybases[i]);
	}
}
