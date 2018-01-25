// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Common/Assert.h"
#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Core/ARBruteForcer.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VR.h"
#include "VideoCommon/VertexLoaderBase.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VertexShaderManager.h"

namespace VertexLoaderManager
{
float position_cache[3][4];

// The counter added to the address of the array is 1, 2, or 3, but never zero.
// So only index 1 - 3 are used.
u32 position_matrix_index[4];

static NativeVertexFormatMap s_native_vertex_map;
static NativeVertexFormat* s_current_vtx_fmt;
u32 g_current_components;

typedef std::unordered_map<VertexLoaderUID, std::unique_ptr<VertexLoaderBase>> VertexLoaderMap;
static std::mutex s_vertex_loader_map_lock;
static VertexLoaderMap s_vertex_loader_map;
// TODO - change into array of pointers. Keep a map of all seen so far.

u8* cached_arraybases[12];

void Init()
{
  MarkAllDirty();
  for (auto& map_entry : g_main_cp_state.vertex_loaders)
    map_entry = nullptr;
  for (auto& map_entry : g_preprocess_cp_state.vertex_loaders)
    map_entry = nullptr;
  SETSTAT(stats.numVertexLoaders, 0);
}

void Clear()
{
  std::lock_guard<std::mutex> lk(s_vertex_loader_map_lock);
  s_vertex_loader_map.clear();
  s_native_vertex_map.clear();
}

void UpdateVertexArrayPointers()
{
  // Anything to update?
  if (!g_main_cp_state.bases_dirty)
    return;

  // Some games such as Burnout 2 can put invalid addresses into
  // the array base registers. (see issue 8591)
  // But the vertex arrays with invalid addresses aren't actually enabled.
  // Note: Only array bases 0 through 11 are used by the Vertex loaders.
  //       12 through 15 are used for loading data into xfmem.
  for (int i = 0; i < 12; i++)
  {
    // Only update the array base if the vertex description states we are going to use it.
    if (g_main_cp_state.vtx_desc.GetVertexArrayStatus(i) & MASK_INDEXED)
      cached_arraybases[i] = Memory::GetPointer(g_main_cp_state.array_bases[i]);
  }

  g_main_cp_state.bases_dirty = false;
}

namespace
{
struct entry
{
  std::string text;
  u64 num_verts;
  bool operator<(const entry& other) const { return num_verts > other.num_verts; }
};
}

std::string VertexLoadersToString()
{
  std::lock_guard<std::mutex> lk(s_vertex_loader_map_lock);
  std::vector<entry> entries;

  size_t total_size = 0;
  for (const auto& map_entry : s_vertex_loader_map)
  {
    entry e = {map_entry.second->ToString(),
               static_cast<u64>(map_entry.second->m_numLoadedVertices)};

    total_size += e.text.size() + 1;
    entries.push_back(std::move(e));
  }

  sort(entries.begin(), entries.end());

  std::string dest;
  dest.reserve(total_size);
  for (const entry& entry : entries)
  {
    dest += entry.text;
    dest += '\n';
  }

  return dest;
}

void MarkAllDirty()
{
  g_main_cp_state.attr_dirty = BitSet32::AllTrue(8);
  g_preprocess_cp_state.attr_dirty = BitSet32::AllTrue(8);
}

NativeVertexFormat* GetOrCreateMatchingFormat(const PortableVertexDeclaration& decl)
{
  auto iter = s_native_vertex_map.find(decl);
  if (iter == s_native_vertex_map.end())
  {
    std::unique_ptr<NativeVertexFormat> fmt = g_vertex_manager->CreateNativeVertexFormat(decl);
    auto ipair = s_native_vertex_map.emplace(decl, std::move(fmt));
    iter = ipair.first;
  }

  return iter->second.get();
}

NativeVertexFormat* GetUberVertexFormat(const PortableVertexDeclaration& decl)
{
  // The padding in the structs can cause the memcmp() in the map to create duplicates.
  // Avoid this by initializing the padding to zero.
  PortableVertexDeclaration new_decl;
  std::memset(&new_decl, 0, sizeof(new_decl));
  new_decl.stride = decl.stride;

  auto MakeDummyAttribute = [](AttributeFormat& attr, VarType type, int components, bool integer) {
    attr.type = type;
    attr.components = components;
    attr.offset = 0;
    attr.enable = true;
    attr.integer = integer;
  };
  auto CopyAttribute = [](AttributeFormat& attr, const AttributeFormat& src) {
    attr.type = src.type;
    attr.components = src.components;
    attr.offset = src.offset;
    attr.enable = src.enable;
    attr.integer = src.integer;
  };

  if (decl.position.enable)
    CopyAttribute(new_decl.position, decl.position);
  else
    MakeDummyAttribute(new_decl.position, VAR_FLOAT, 1, false);
  for (size_t i = 0; i < ArraySize(new_decl.normals); i++)
  {
    if (decl.normals[i].enable)
      CopyAttribute(new_decl.normals[i], decl.normals[i]);
    else
      MakeDummyAttribute(new_decl.normals[i], VAR_FLOAT, 1, false);
  }
  for (size_t i = 0; i < ArraySize(new_decl.colors); i++)
  {
    if (decl.colors[i].enable)
      CopyAttribute(new_decl.colors[i], decl.colors[i]);
    else
      MakeDummyAttribute(new_decl.colors[i], VAR_UNSIGNED_BYTE, 4, false);
  }
  for (size_t i = 0; i < ArraySize(new_decl.texcoords); i++)
  {
    if (decl.texcoords[i].enable)
      CopyAttribute(new_decl.texcoords[i], decl.texcoords[i]);
    else
      MakeDummyAttribute(new_decl.texcoords[i], VAR_FLOAT, 1, false);
  }
  if (decl.posmtx.enable)
    CopyAttribute(new_decl.posmtx, decl.posmtx);
  else
    MakeDummyAttribute(new_decl.posmtx, VAR_UNSIGNED_BYTE, 1, true);

  return GetOrCreateMatchingFormat(new_decl);
}

static VertexLoaderBase* RefreshLoader(int vtx_attr_group, bool preprocess = false)
{
  CPState* state = preprocess ? &g_preprocess_cp_state : &g_main_cp_state;
  state->last_id = vtx_attr_group;

  VertexLoaderBase* loader;
  if (state->attr_dirty[vtx_attr_group])
  {
    // We are not allowed to create a native vertex format on preprocessing as this is on the wrong
    // thread
    bool check_for_native_format = !preprocess;

    VertexLoaderUID uid(state->vtx_desc, state->vtx_attr[vtx_attr_group]);
    std::lock_guard<std::mutex> lk(s_vertex_loader_map_lock);
    VertexLoaderMap::iterator iter = s_vertex_loader_map.find(uid);
    if (iter != s_vertex_loader_map.end())
    {
      loader = iter->second.get();
      check_for_native_format &= !loader->m_native_vertex_format;
    }
    else
    {
      s_vertex_loader_map[uid] =
          VertexLoaderBase::CreateVertexLoader(state->vtx_desc, state->vtx_attr[vtx_attr_group]);
      loader = s_vertex_loader_map[uid].get();
      INCSTAT(stats.numVertexLoaders);
    }
    if (check_for_native_format)
    {
      // search for a cached native vertex format
      const PortableVertexDeclaration& format = loader->m_native_vtx_decl;
      std::unique_ptr<NativeVertexFormat>& native = s_native_vertex_map[format];
      if (!native)
      {
        native = g_vertex_manager->CreateNativeVertexFormat(format);
      }
      loader->m_native_vertex_format = native.get();
    }
    state->vertex_loaders[vtx_attr_group] = loader;
    state->attr_dirty[vtx_attr_group] = false;
  }
  else
  {
    loader = state->vertex_loaders[vtx_attr_group];
  }

  // Lookup pointers for any vertex arrays.
  if (!preprocess)
    UpdateVertexArrayPointers();

  return loader;
}

int RunVertices(int vtx_attr_group, int primitive, int count, DataReader src, bool skip_drawing,
                bool is_preprocess)
{
  if (!count)
    return 0;

  SConfig& m_LocalCoreStartupParameter = SConfig::GetInstance();

  VertexLoaderBase* loader = RefreshLoader(vtx_attr_group, is_preprocess);
  int size = count * loader->m_VertexSize;
  if ((int)src.size() < size)
    return -1;

#ifdef DEBUG_OBJECTS
  if (skip_objects_count < m_LocalCoreStartupParameter.skip_objects_end_two)
  {
    if (skip_objects_count >= m_LocalCoreStartupParameter.skip_objects_start_two)
    {
      // Skip Object
      skip_objects_count++;
      return size;
    }
  }

  if (skip_objects_count < m_LocalCoreStartupParameter.skip_objects_end)
  {
    if (skip_objects_count >= m_LocalCoreStartupParameter.skip_objects_start)
    {
      // Skip Object
      skip_objects_count++;
      return size;
    }
  }
  skip_objects_count++;
#else
  if (skip_objects_count < m_LocalCoreStartupParameter.skip_objects_end)
  {
    if (++skip_objects_count >= m_LocalCoreStartupParameter.skip_objects_start)
    {
      // Skip Object
      return size;
    }
  }
#endif

  if (skip_drawing || is_preprocess)
    return size;

  // Hide Objects Code code
  if (!m_LocalCoreStartupParameter.hide_objects_updating)
  {
    m_LocalCoreStartupParameter.hide_objects_done = false;
    for (const SkipEntry& entry : m_LocalCoreStartupParameter.object_removal_codes)
    {
      // Set lock so codes can be enabled/disabled in game without crashes.
      if (!memcmp(src.GetPointer(), entry.data(), entry.size()))
      {
        // Data didn't match, try next object_removal_code
        return size;
      }
    }
  }
  m_LocalCoreStartupParameter.hide_objects_done = true;

  // If the native vertex format changed, force a flush.
  if (loader->m_native_vertex_format != s_current_vtx_fmt ||
      loader->m_native_components != g_current_components)
  {
    g_vertex_manager->Flush();
  }
  s_current_vtx_fmt = loader->m_native_vertex_format;
  g_current_components = loader->m_native_components;
  VertexShaderManager::SetVertexFormat(loader->m_native_components);

  // if cull mode is CULL_ALL, tell VertexManager to skip triangles and quads.
  // They still need to go through vertex loading, because we need to calculate a zfreeze refrence
  // slope.
  bool cullall = (bpmem.genMode.cullmode == GenMode::CULL_ALL && primitive < 5);

  DataReader dst = g_vertex_manager->PrepareForAdditionalData(
      primitive, count, loader->m_native_vtx_decl.stride, cullall);

  count = loader->RunVertices(src, dst, count);

  IndexGenerator::AddIndices(primitive, count);

  g_vertex_manager->FlushData(count, loader->m_native_vtx_decl.stride);

  ADDSTAT(stats.thisFrame.numPrims, count);
  INCSTAT(stats.thisFrame.numPrimitiveJoins);
  return size;
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
    state->bases_dirty = true;
    break;

  case 0x60:
    state->vtx_desc.Hex &= 0x1FFFF;  // keep the lower 17Bits
    state->vtx_desc.Hex |= (u64)value << 17;
    state->attr_dirty = BitSet32::AllTrue(8);
    state->bases_dirty = true;
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
    if (ARBruteForcer::ch_bruteforce && (sub_cmd & 0x0F) >= 8)
      Core::KillDolphinAndRestart();
    else
      _assert_((sub_cmd & 0x0F) < 8);
    state->vtx_attr[sub_cmd & 7].g2.Hex = value;
    state->attr_dirty[sub_cmd & 7] = true;
    break;

  // Pointers to vertex arrays in GC RAM
  case 0xA0:
    state->array_bases[sub_cmd & 0xF] = value;
    state->bases_dirty = true;
    break;

  case 0xB0:
    state->array_strides[sub_cmd & 0xF] = value & 0xFF;
    break;
  }
}

void FillCPMemoryArray(u32* memory)
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
