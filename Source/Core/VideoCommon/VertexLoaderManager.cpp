// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <mutex>
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

static NativeVertexFormat* s_current_vtx_fmt;

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

typedef std::unordered_map<VertexLoaderUID, std::unique_ptr<VertexLoader>> VertexLoaderMap;

namespace VertexLoaderManager
{

static std::mutex s_vertex_loader_map_lock;
static VertexLoaderMap s_vertex_loader_map;
// TODO - change into array of pointers. Keep a map of all seen so far.

void Init()
{
	MarkAllDirty();
	for (auto& map_entry : g_main_cp_state.vertex_loaders)
		map_entry = nullptr;
	for (auto& map_entry : g_preprocess_cp_state.vertex_loaders)
		map_entry = nullptr;
	RecomputeCachedArraybases();
}

void Shutdown()
{
	std::lock_guard<std::mutex> lk(s_vertex_loader_map_lock);
	s_vertex_loader_map.clear();
	VertexLoader::ClearNativeVertexFormatCache();
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
	std::lock_guard<std::mutex> lk(s_vertex_loader_map_lock);
	std::vector<entry> entries;

	size_t total_size = 0;
	for (const auto& map_entry : s_vertex_loader_map)
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
	g_main_cp_state.attr_dirty = BitSet32::AllTrue(8);
	g_preprocess_cp_state.attr_dirty = BitSet32::AllTrue(8);
}

static VertexLoader* RefreshLoader(int vtx_attr_group, CPState* state)
{
	VertexLoader* loader;
	if (state->attr_dirty[vtx_attr_group])
	{
		VertexLoaderUID uid(state->vtx_desc, state->vtx_attr[vtx_attr_group]);
		std::lock_guard<std::mutex> lk(s_vertex_loader_map_lock);
		VertexLoaderMap::iterator iter = s_vertex_loader_map.find(uid);
		if (iter != s_vertex_loader_map.end())
		{
			loader = iter->second.get();
		}
		else
		{
			loader = new VertexLoader(state->vtx_desc, state->vtx_attr[vtx_attr_group]);
			s_vertex_loader_map[uid] = std::unique_ptr<VertexLoader>(loader);
			INCSTAT(stats.numVertexLoaders);
		}
		state->vertex_loaders[vtx_attr_group] = loader;
		state->attr_dirty[vtx_attr_group] = false;
	} else {
		loader = state->vertex_loaders[vtx_attr_group];
	}
	return loader;
}

bool RunVertices(int vtx_attr_group, int primitive, int count, size_t buf_size, bool skip_drawing)
{
	if (!count)
		return true;

	CPState* state = &g_main_cp_state;

	VertexLoader* loader = RefreshLoader(vtx_attr_group, state);

	size_t size = count * loader->GetVertexSize();
	if (buf_size < size)
		return false;

	if (skip_drawing || (bpmem.genMode.cullmode == GenMode::CULL_ALL && primitive < 5))
	{
		// if cull mode is CULL_ALL, ignore triangles and quads
		DataSkip((u32)size);
		return true;
	}

	NativeVertexFormat* native = loader->GetNativeVertexFormat();

	// If the native vertex format changed, force a flush.
	if (native != s_current_vtx_fmt)
		VertexManager::Flush();
	s_current_vtx_fmt = native;

	VertexManager::PrepareForAdditionalData(primitive, count,
			loader->GetNativeVertexDeclaration().stride);

	loader->RunVertices(state->vtx_attr[vtx_attr_group], primitive, count);

	IndexGenerator::AddIndices(primitive, count);

	ADDSTAT(stats.thisFrame.numPrims, count);
	INCSTAT(stats.thisFrame.numPrimitiveJoins);
	return true;
}

int GetVertexSize(int vtx_attr_group, bool preprocess)
{
	return RefreshLoader(vtx_attr_group, preprocess ? &g_preprocess_cp_state : &g_main_cp_state)->GetVertexSize();
}

NativeVertexFormat* GetCurrentVertexFormat()
{
	return s_current_vtx_fmt;
}

}  // namespace

void LoadCPReg(u32 sub_cmd, u32 value, bool is_preprocess)
{
	bool update_global_state = !is_preprocess;
	CPState* state = is_preprocess ? &g_preprocess_cp_state : &g_main_cp_state;
	switch (sub_cmd & 0xF0)
	{
	case 0x30:
		if (update_global_state)
			VertexShaderManager::SetTexMatrixChangedA(value);
		break;

	case 0x40:
		if (update_global_state)
			VertexShaderManager::SetTexMatrixChangedB(value);
		break;

	case 0x50:
		state->vtx_desc.Hex &= ~0x1FFFF;  // keep the Upper bits
		state->vtx_desc.Hex |= value;
		state->attr_dirty = BitSet32::AllTrue(8);
		break;

	case 0x60:
		state->vtx_desc.Hex &= 0x1FFFF;  // keep the lower 17Bits
		state->vtx_desc.Hex |= (u64)value << 17;
		state->attr_dirty = BitSet32::AllTrue(8);
		break;

	case 0x70:
		_assert_((sub_cmd & 0x0F) < 8);
		state->vtx_attr[sub_cmd & 7].g0.Hex = value;
		state->attr_dirty[sub_cmd & 7] = true;
		break;

	case 0x80:
		_assert_((sub_cmd & 0x0F) < 8);
		state->vtx_attr[sub_cmd & 7].g1.Hex = value;
		state->attr_dirty[sub_cmd & 7] = true;
		break;

	case 0x90:
		_assert_((sub_cmd & 0x0F) < 8);
		state->vtx_attr[sub_cmd & 7].g2.Hex = value;
		state->attr_dirty[sub_cmd & 7] = true;
		break;

	// Pointers to vertex arrays in GC RAM
	case 0xA0:
		state->array_bases[sub_cmd & 0xF] = value;
		if (update_global_state)
			cached_arraybases[sub_cmd & 0xF] = Memory::GetPointer(value);
		break;

	case 0xB0:
		state->array_strides[sub_cmd & 0xF] = value & 0xFF;
		break;
	}
}

void FillCPMemoryArray(u32 *memory)
{
	memory[0x30] = g_main_cp_state.matrix_index_a.Hex;
	memory[0x40] = g_main_cp_state.matrix_index_b.Hex;
	memory[0x50] = (u32)g_main_cp_state.vtx_desc.Hex;
	memory[0x60] = (u32)(g_main_cp_state.vtx_desc.Hex >> 17);

	for (int i = 0; i < 8; ++i)
	{
		memory[0x70 + i] = g_main_cp_state.vtx_attr[i].g0.Hex;
		memory[0x80 + i] = g_main_cp_state.vtx_attr[i].g1.Hex;
		memory[0x90 + i] = g_main_cp_state.vtx_attr[i].g2.Hex;
	}

	for (int i = 0; i < 16; ++i)
	{
		memory[0xA0 + i] = g_main_cp_state.array_bases[i];
		memory[0xB0 + i] = g_main_cp_state.array_strides[i];
	}
}

void RecomputeCachedArraybases()
{
	for (int i = 0; i < 16; i++)
	{
		cached_arraybases[i] = Memory::GetPointer(g_main_cp_state.array_bases[i]);
	}
}
