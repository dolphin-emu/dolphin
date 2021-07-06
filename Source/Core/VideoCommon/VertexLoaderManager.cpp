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

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

#include "Core/DolphinAnalytics.h"
#include "Core/HW/Memmap.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/CommandProcessor.h"
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

u8* cached_arraybases[NUM_VERTEX_COMPONENT_ARRAYS];

void Init()
{
  MarkAllDirty();
  for (auto& map_entry : g_main_cp_state.vertex_loaders)
    map_entry = nullptr;
  for (auto& map_entry : g_preprocess_cp_state.vertex_loaders)
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
  if (!g_main_cp_state.bases_dirty)
    return;

  // Some games such as Burnout 2 can put invalid addresses into
  // the array base registers. (see issue 8591)
  // But the vertex arrays with invalid addresses aren't actually enabled.
  // Note: Only array bases 0 through 11 are used by the Vertex loaders.
  //       12 through 15 are used for loading data into xfmem.
  // We also only update the array base if the vertex description states we are going to use it.
  if (IsIndexed(g_main_cp_state.vtx_desc.low.Position))
    cached_arraybases[ARRAY_POSITION] =
        Memory::GetPointer(g_main_cp_state.array_bases[ARRAY_POSITION]);

  if (IsIndexed(g_main_cp_state.vtx_desc.low.Normal))
    cached_arraybases[ARRAY_NORMAL] = Memory::GetPointer(g_main_cp_state.array_bases[ARRAY_NORMAL]);

  for (size_t i = 0; i < g_main_cp_state.vtx_desc.low.Color.Size(); i++)
  {
    if (IsIndexed(g_main_cp_state.vtx_desc.low.Color[i]))
      cached_arraybases[ARRAY_COLOR0 + i] =
          Memory::GetPointer(g_main_cp_state.array_bases[ARRAY_COLOR0 + i]);
  }

  for (size_t i = 0; i < g_main_cp_state.vtx_desc.high.TexCoord.Size(); i++)
  {
    if (IsIndexed(g_main_cp_state.vtx_desc.high.TexCoord[i]))
      cached_arraybases[ARRAY_TEXCOORD0 + i] =
          Memory::GetPointer(g_main_cp_state.array_bases[ARRAY_TEXCOORD0 + i]);
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
}  // namespace

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
  for (size_t i = 0; i < std::size(new_decl.normals); i++)
  {
    if (decl.normals[i].enable)
      CopyAttribute(new_decl.normals[i], decl.normals[i]);
    else
      MakeDummyAttribute(new_decl.normals[i], VAR_FLOAT, 1, false);
  }
  for (size_t i = 0; i < std::size(new_decl.colors); i++)
  {
    if (decl.colors[i].enable)
      CopyAttribute(new_decl.colors[i], decl.colors[i]);
    else
      MakeDummyAttribute(new_decl.colors[i], VAR_UNSIGNED_BYTE, 4, false);
  }
  for (size_t i = 0; i < std::size(new_decl.texcoords); i++)
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

int RunVertices(int vtx_attr_group, int primitive, int count, DataReader src, bool is_preprocess)
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
  bool cullall = (bpmem.genMode.cullmode == CullMode::All && primitive < 5);

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

void LoadCPReg(u32 sub_cmd, u32 value, bool is_preprocess)
{
  bool update_global_state = !is_preprocess;
  CPState* state = is_preprocess ? &g_preprocess_cp_state : &g_main_cp_state;
  switch (sub_cmd & CP_COMMAND_MASK)
  {
  case UNKNOWN_00:
  case UNKNOWN_10:
  case UNKNOWN_20:
    if (!(sub_cmd == UNKNOWN_20 && value == 0))
    {
      // All titles using libogc or the official SDK issue 0x20 with value=0 on startup
      DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_CP_PERF_COMMAND);
      DEBUG_LOG_FMT(VIDEO, "Unknown CP command possibly relating to perf queries used: {:02x}",
                    sub_cmd);
    }
    break;

  case MATINDEX_A:
    if (sub_cmd != MATINDEX_A)
    {
      DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_MAYBE_INVALID_CP_COMMAND);
      WARN_LOG_FMT(VIDEO,
                   "CP MATINDEX_A: an exact value of {:02x} was expected "
                   "but instead a value of {:02x} was seen",
                   MATINDEX_A, sub_cmd);
    }

    if (update_global_state)
      VertexShaderManager::SetTexMatrixChangedA(value);
    break;

  case MATINDEX_B:
    if (sub_cmd != MATINDEX_B)
    {
      DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_MAYBE_INVALID_CP_COMMAND);
      WARN_LOG_FMT(VIDEO,
                   "CP MATINDEX_B: an exact value of {:02x} was expected "
                   "but instead a value of {:02x} was seen",
                   MATINDEX_B, sub_cmd);
    }

    if (update_global_state)
      VertexShaderManager::SetTexMatrixChangedB(value);
    break;

  case VCD_LO:
    if (sub_cmd != VCD_LO)  // Stricter than YAGCD
    {
      DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_MAYBE_INVALID_CP_COMMAND);
      WARN_LOG_FMT(VIDEO,
                   "CP VCD_LO: an exact value of {:02x} was expected "
                   "but instead a value of {:02x} was seen",
                   VCD_LO, sub_cmd);
    }

    state->vtx_desc.low.Hex = value;
    state->attr_dirty = BitSet32::AllTrue(CP_NUM_VAT_REG);
    state->bases_dirty = true;
    break;

  case VCD_HI:
    if (sub_cmd != VCD_HI)  // Stricter than YAGCD
    {
      DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_MAYBE_INVALID_CP_COMMAND);
      WARN_LOG_FMT(VIDEO,
                   "CP VCD_HI: an exact value of {:02x} was expected "
                   "but instead a value of {:02x} was seen",
                   VCD_HI, sub_cmd);
    }

    state->vtx_desc.high.Hex = value;
    state->attr_dirty = BitSet32::AllTrue(CP_NUM_VAT_REG);
    state->bases_dirty = true;
    break;

  case CP_VAT_REG_A:
    if ((sub_cmd - CP_VAT_REG_A) >= CP_NUM_VAT_REG)
    {
      DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_MAYBE_INVALID_CP_COMMAND);
      WARN_LOG_FMT(VIDEO, "CP_VAT_REG_A: Invalid VAT {}", sub_cmd - CP_VAT_REG_A);
    }
    state->vtx_attr[sub_cmd & CP_VAT_MASK].g0.Hex = value;
    state->attr_dirty[sub_cmd & CP_VAT_MASK] = true;
    break;

  case CP_VAT_REG_B:
    if ((sub_cmd - CP_VAT_REG_B) >= CP_NUM_VAT_REG)
    {
      DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_MAYBE_INVALID_CP_COMMAND);
      WARN_LOG_FMT(VIDEO, "CP_VAT_REG_B: Invalid VAT {}", sub_cmd - CP_VAT_REG_B);
    }
    state->vtx_attr[sub_cmd & CP_VAT_MASK].g1.Hex = value;
    state->attr_dirty[sub_cmd & CP_VAT_MASK] = true;
    break;

  case CP_VAT_REG_C:
    if ((sub_cmd - CP_VAT_REG_C) >= CP_NUM_VAT_REG)
    {
      DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_MAYBE_INVALID_CP_COMMAND);
      WARN_LOG_FMT(VIDEO, "CP_VAT_REG_C: Invalid VAT {}", sub_cmd - CP_VAT_REG_C);
    }
    state->vtx_attr[sub_cmd & CP_VAT_MASK].g2.Hex = value;
    state->attr_dirty[sub_cmd & CP_VAT_MASK] = true;
    break;

  // Pointers to vertex arrays in GC RAM
  case ARRAY_BASE:
    state->array_bases[sub_cmd & CP_ARRAY_MASK] =
        value & CommandProcessor::GetPhysicalAddressMask();
    state->bases_dirty = true;
    break;

  case ARRAY_STRIDE:
    state->array_strides[sub_cmd & CP_ARRAY_MASK] = value & 0xFF;
    break;

  default:
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::USES_UNKNOWN_CP_COMMAND);
    WARN_LOG_FMT(VIDEO, "Unknown CP register {:02x} set to {:08x}", sub_cmd, value);
  }
}

void FillCPMemoryArray(u32* memory)
{
  memory[MATINDEX_A] = g_main_cp_state.matrix_index_a.Hex;
  memory[MATINDEX_B] = g_main_cp_state.matrix_index_b.Hex;
  memory[VCD_LO] = g_main_cp_state.vtx_desc.low.Hex;
  memory[VCD_HI] = g_main_cp_state.vtx_desc.high.Hex;

  for (int i = 0; i < CP_NUM_VAT_REG; ++i)
  {
    memory[CP_VAT_REG_A + i] = g_main_cp_state.vtx_attr[i].g0.Hex;
    memory[CP_VAT_REG_B + i] = g_main_cp_state.vtx_attr[i].g1.Hex;
    memory[CP_VAT_REG_C + i] = g_main_cp_state.vtx_attr[i].g2.Hex;
  }

  for (int i = 0; i < CP_NUM_ARRAYS; ++i)
  {
    memory[ARRAY_BASE + i] = g_main_cp_state.array_bases[i];
    memory[ARRAY_STRIDE + i] = g_main_cp_state.array_strides[i];
  }
}
