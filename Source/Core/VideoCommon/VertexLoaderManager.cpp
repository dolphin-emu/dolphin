// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Common/CommonFuncs.h"
#include "Core/HW/Memmap.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoCommon.h"

static int s_attr_dirty;  // bitfield

static NativeVertexFormat* s_current_vtx_fmt;

typedef std::pair<VertexLoader*, NativeVertexFormat*> VertexLoaderCacheItem;
static VertexLoaderCacheItem s_VertexLoaders[8];

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

typedef std::unordered_map<VertexLoaderUID, VertexLoaderCacheItem> VertexLoaderMap;
typedef std::map<PortableVertexDeclaration, std::unique_ptr<NativeVertexFormat>> NativeVertexLoaderMap;

namespace VertexLoaderManager
{

static VertexLoaderMap s_VertexLoaderMap;
static NativeVertexLoaderMap s_native_vertex_map;
// TODO - change into array of pointers. Keep a map of all seen so far.

void Init()
{
	MarkAllDirty();
	for (auto& map_entry : s_VertexLoaders)
	{
		map_entry.first = nullptr;
		map_entry.second = nullptr;
	}
	RecomputeCachedArraybases();
}

void Shutdown()
{
	for (auto& map_entry : s_VertexLoaderMap)
	{
		delete map_entry.second.first;
	}
	s_VertexLoaderMap.clear();
	s_native_vertex_map.clear();
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
	for (const auto& map_entry : s_VertexLoaderMap)
	{
		entry e;
		map_entry.second.first->AppendToString(&e.text);
		e.num_verts = map_entry.second.first->GetNumLoadedVerts();
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

static NativeVertexFormat* GetNativeVertexFormat(const PortableVertexDeclaration& format,
                                                 u32 components)
{
	auto& native = s_native_vertex_map[format];
	if (!native)
	{
		auto raw_pointer = g_vertex_manager->CreateNativeVertexFormat();
		native = std::unique_ptr<NativeVertexFormat>(raw_pointer);
		native->Initialize(format);
		native->m_components = components;
	}
	return native.get();
}

static VertexLoaderCacheItem RefreshLoader(int vtx_attr_group)
{
	if ((s_attr_dirty >> vtx_attr_group) & 1)
	{
		VertexLoaderUID uid(g_VtxDesc, g_VtxAttr[vtx_attr_group]);
		VertexLoaderMap::iterator iter = s_VertexLoaderMap.find(uid);
		if (iter != s_VertexLoaderMap.end())
		{
			s_VertexLoaders[vtx_attr_group] = iter->second;
		}
		else
		{
			VertexLoader* loader = new VertexLoader(g_VtxDesc, g_VtxAttr[vtx_attr_group]);

			NativeVertexFormat* vtx_fmt = GetNativeVertexFormat(
				loader->GetNativeVertexDeclaration(),
				loader->GetNativeComponents());

			s_VertexLoaderMap[uid] = std::make_pair(loader, vtx_fmt);
			s_VertexLoaders[vtx_attr_group] = std::make_pair(loader, vtx_fmt);
			INCSTAT(stats.numVertexLoaders);
		}
	}
	s_attr_dirty &= ~(1 << vtx_attr_group);
	return s_VertexLoaders[vtx_attr_group];
}

bool RunVertices(int vtx_attr_group, int primitive, int count, size_t buf_size, bool skip_drawing)
{
	if (!count)
		return true;
	auto loader = RefreshLoader(vtx_attr_group);

	size_t size = count * loader.first->GetVertexSize();
	if (buf_size < size)
		return false;

	if (skip_drawing || (bpmem.genMode.cullmode == GenMode::CULL_ALL && primitive < 5))
	{
		// if cull mode is CULL_ALL, ignore triangles and quads
		DataSkip((u32)size);
		return true;
	}

	// If the native vertex format changed, force a flush.
	if (loader.second != s_current_vtx_fmt)
		VertexManager::Flush();
	s_current_vtx_fmt = loader.second;

	VertexManager::PrepareForAdditionalData(primitive, count,
			loader.first->GetNativeVertexDeclaration().stride);

	loader.first->RunVertices(g_VtxAttr[vtx_attr_group], primitive, count);

	IndexGenerator::AddIndices(primitive, count);

	ADDSTAT(stats.thisFrame.numPrims, count);
	INCSTAT(stats.thisFrame.numPrimitiveJoins);
	return true;
}

int GetVertexSize(int vtx_attr_group)
{
	return RefreshLoader(vtx_attr_group).first->GetVertexSize();
}

NativeVertexFormat* GetCurrentVertexFormat()
{
	return s_current_vtx_fmt;
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
