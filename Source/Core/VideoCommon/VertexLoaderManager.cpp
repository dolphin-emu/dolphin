// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/VertexLoaderManager.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/EnumMap.h"
#include "Common/Logging/Log.h"

#include "Core/HW/Memmap.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderBase.h"
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

Common::EnumMap<u8*, CPArray::TexCoord7> cached_arraybases;

BitSet8 g_main_vat_dirty;
BitSet8 g_preprocess_vat_dirty;
bool g_bases_dirty;  // Main only
u8 g_current_vat;    // Main only
std::array<VertexLoaderBase*, CP_NUM_VAT_REG> g_main_vertex_loaders;
std::array<VertexLoaderBase*, CP_NUM_VAT_REG> g_preprocess_vertex_loaders;

void Init()
{
  MarkAllDirty();
  for (auto& map_entry : g_main_vertex_loaders)
    map_entry = nullptr;
  for (auto& map_entry : g_preprocess_vertex_loaders)
    map_entry = nullptr;
  SETSTAT(g_stats.num_vertex_loaders, 0);
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
  if (!g_bases_dirty)
    return;

  // Some games such as Burnout 2 can put invalid addresses into
  // the array base registers. (see issue 8591)
  // But the vertex arrays with invalid addresses aren't actually enabled.
  // Note: Only array bases 0 through 11 are used by the Vertex loaders.
  //       12 through 15 are used for loading data into xfmem.
  // We also only update the array base if the vertex description states we are going to use it.
  if (IsIndexed(g_main_cp_state.vtx_desc.low.Position))
    cached_arraybases[CPArray::Position] =
        Memory::GetPointer(g_main_cp_state.array_bases[CPArray::Position]);

  if (IsIndexed(g_main_cp_state.vtx_desc.low.Normal))
    cached_arraybases[CPArray::Normal] =
        Memory::GetPointer(g_main_cp_state.array_bases[CPArray::Normal]);

  for (u8 i = 0; i < g_main_cp_state.vtx_desc.low.Color.Size(); i++)
  {
    if (IsIndexed(g_main_cp_state.vtx_desc.low.Color[i]))
      cached_arraybases[CPArray::Color0 + i] =
          Memory::GetPointer(g_main_cp_state.array_bases[CPArray::Color0 + i]);
  }

  for (u8 i = 0; i < g_main_cp_state.vtx_desc.high.TexCoord.Size(); i++)
  {
    if (IsIndexed(g_main_cp_state.vtx_desc.high.TexCoord[i]))
      cached_arraybases[CPArray::TexCoord0 + i] =
          Memory::GetPointer(g_main_cp_state.array_bases[CPArray::TexCoord0 + i]);
  }

  g_bases_dirty = false;
}

namespace
{
struct entry
{
  std::string text;
  u64 num_verts;
  bool operator<(const entry& other) const { return num_verts > other.num_verts; }
};
}  // namespace

void MarkAllDirty()
{
  g_main_vat_dirty = BitSet8::AllTrue(8);
  g_preprocess_vat_dirty = BitSet8::AllTrue(8);
}

NativeVertexFormat* GetOrCreateMatchingFormat(const PortableVertexDeclaration& decl)
{
  auto iter = s_native_vertex_map.find(decl);
  if (iter == s_native_vertex_map.end())
  {
    std::unique_ptr<NativeVertexFormat> fmt = g_renderer->CreateNativeVertexFormat(decl);
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

  auto MakeDummyAttribute = [](AttributeFormat& attr, ComponentFormat type, int components,
                               bool integer) {
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
    MakeDummyAttribute(new_decl.position, ComponentFormat::Float, 1, false);
  for (size_t i = 0; i < std::size(new_decl.normals); i++)
  {
    if (decl.normals[i].enable)
      CopyAttribute(new_decl.normals[i], decl.normals[i]);
    else
      MakeDummyAttribute(new_decl.normals[i], ComponentFormat::Float, 1, false);
  }
  for (size_t i = 0; i < std::size(new_decl.colors); i++)
  {
    if (decl.colors[i].enable)
      CopyAttribute(new_decl.colors[i], decl.colors[i]);
    else
      MakeDummyAttribute(new_decl.colors[i], ComponentFormat::UByte, 4, false);
  }
  for (size_t i = 0; i < std::size(new_decl.texcoords); i++)
  {
    if (decl.texcoords[i].enable)
      CopyAttribute(new_decl.texcoords[i], decl.texcoords[i]);
    else
      MakeDummyAttribute(new_decl.texcoords[i], ComponentFormat::Float, 1, false);
  }
  if (decl.posmtx.enable)
    CopyAttribute(new_decl.posmtx, decl.posmtx);
  else
    MakeDummyAttribute(new_decl.posmtx, ComponentFormat::UByte, 1, true);

  return GetOrCreateMatchingFormat(new_decl);
}

static VertexLoaderBase* RefreshLoader(int vtx_attr_group, bool preprocess = false)
{
  CPState* state = preprocess ? &g_preprocess_cp_state : &g_main_cp_state;
  BitSet8& attr_dirty = preprocess ? g_preprocess_vat_dirty : g_main_vat_dirty;
  auto& vertex_loaders = preprocess ? g_main_vertex_loaders : g_preprocess_vertex_loaders;
  g_current_vat = vtx_attr_group;

  VertexLoaderBase* loader;
  if (attr_dirty[vtx_attr_group])
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
      INCSTAT(g_stats.num_vertex_loaders);
    }
    if (check_for_native_format)
    {
      // search for a cached native vertex format
      const PortableVertexDeclaration& format = loader->m_native_vtx_decl;
      std::unique_ptr<NativeVertexFormat>& native = s_native_vertex_map[format];
      if (!native)
        native = g_renderer->CreateNativeVertexFormat(format);
      loader->m_native_vertex_format = native.get();
    }
    vertex_loaders[vtx_attr_group] = loader;
    attr_dirty[vtx_attr_group] = false;
  }
  else
  {
    loader = vertex_loaders[vtx_attr_group];
  }

  // Lookup pointers for any vertex arrays.
  if (!preprocess)
    UpdateVertexArrayPointers();

  return loader;
}

int RunVertices(int vtx_attr_group, OpcodeDecoder::Primitive primitive, int count, DataReader src,
                bool is_preprocess)
{
  if (!count)
    return 0;

  VertexLoaderBase* loader = RefreshLoader(vtx_attr_group, is_preprocess);

  int size = count * loader->m_vertex_size;
  if ((int)src.size() < size)
    return -1;

  if (is_preprocess)
    return size;

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
  bool cullall = (bpmem.genMode.cullmode == CullMode::All &&
                  primitive < OpcodeDecoder::Primitive::GX_DRAW_LINES);

  DataReader dst = g_vertex_manager->PrepareForAdditionalData(
      primitive, count, loader->m_native_vtx_decl.stride, cullall);

  count = loader->RunVertices(src, dst, count);

  g_vertex_manager->AddIndices(primitive, count);
  g_vertex_manager->FlushData(count, loader->m_native_vtx_decl.stride);

  ADDSTAT(g_stats.this_frame.num_prims, count);
  INCSTAT(g_stats.this_frame.num_primitive_joins);
  return size;
}

NativeVertexFormat* GetCurrentVertexFormat()
{
  return s_current_vtx_fmt;
}

}  // namespace VertexLoaderManager
