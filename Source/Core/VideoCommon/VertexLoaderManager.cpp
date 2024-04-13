// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/VertexLoaderManager.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/EnumMap.h"
#include "Common/Logging/Log.h"

#include "Core/DolphinAnalytics.h"
#include "Core/HW/Memmap.h"
#include "Core/System.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderBase.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

namespace VertexLoaderManager
{
// Used by zfreeze
std::array<u32, 3> position_matrix_index_cache;
// 3 vertices, 4 floats each to allow SIMD overwrite
alignas(sizeof(std::array<float, 4>)) std::array<std::array<float, 4>, 3> position_cache;
alignas(sizeof(std::array<float, 4>)) std::array<float, 4> tangent_cache;
alignas(sizeof(std::array<float, 4>)) std::array<float, 4> binormal_cache;

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
std::array<VertexLoaderBase*, CP_NUM_VAT_REG> g_main_vertex_loaders;
std::array<VertexLoaderBase*, CP_NUM_VAT_REG> g_preprocess_vertex_loaders;
bool g_needs_cp_xf_consistency_check;

void Init()
{
  MarkAllDirty();
  g_main_vertex_loaders.fill(nullptr);
  g_preprocess_vertex_loaders.fill(nullptr);
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
  if (!g_bases_dirty) [[likely]]
    return;

  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  // Some games such as Burnout 2 can put invalid addresses into
  // the array base registers. (see issue 8591)
  // But the vertex arrays with invalid addresses aren't actually enabled.
  // Note: Only array bases 0 through 11 are used by the Vertex loaders.
  //       12 through 15 are used for loading data into xfmem.
  // We also only update the array base if the vertex description states we are going to use it.
  // TODO: For memory safety, we need to check the sizes returned by GetSpanForAddress
  if (IsIndexed(g_main_cp_state.vtx_desc.low.Position))
  {
    cached_arraybases[CPArray::Position] =
        memory.GetSpanForAddress(g_main_cp_state.array_bases[CPArray::Position]).data();
  }

  if (IsIndexed(g_main_cp_state.vtx_desc.low.Normal))
  {
    cached_arraybases[CPArray::Normal] =
        memory.GetSpanForAddress(g_main_cp_state.array_bases[CPArray::Normal]).data();
  }

  for (u8 i = 0; i < g_main_cp_state.vtx_desc.low.Color.Size(); i++)
  {
    if (IsIndexed(g_main_cp_state.vtx_desc.low.Color[i]))
    {
      cached_arraybases[CPArray::Color0 + i] =
          memory.GetSpanForAddress(g_main_cp_state.array_bases[CPArray::Color0 + i]).data();
    }
  }

  for (u8 i = 0; i < g_main_cp_state.vtx_desc.high.TexCoord.Size(); i++)
  {
    if (IsIndexed(g_main_cp_state.vtx_desc.high.TexCoord[i]))
    {
      cached_arraybases[CPArray::TexCoord0 + i] =
          memory.GetSpanForAddress(g_main_cp_state.array_bases[CPArray::TexCoord0 + i]).data();
    }
  }

  g_bases_dirty = false;
}

void MarkAllDirty()
{
  g_bases_dirty = true;
  g_main_vat_dirty = BitSet8::AllTrue(8);
  g_preprocess_vat_dirty = BitSet8::AllTrue(8);
  g_needs_cp_xf_consistency_check = true;
}

NativeVertexFormat* GetOrCreateMatchingFormat(const PortableVertexDeclaration& decl)
{
  auto iter = s_native_vertex_map.find(decl);
  if (iter == s_native_vertex_map.end())
  {
    std::unique_ptr<NativeVertexFormat> fmt = g_gfx->CreateNativeVertexFormat(decl);
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
  static_assert(std::is_trivially_copyable_v<PortableVertexDeclaration>);
  std::memset(static_cast<void*>(&new_decl), 0, sizeof(new_decl));
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

namespace detail
{
template <bool IsPreprocess>
VertexLoaderBase* GetOrCreateLoader(int vtx_attr_group)
{
  constexpr CPState* state = IsPreprocess ? &g_preprocess_cp_state : &g_main_cp_state;
  constexpr BitSet8& attr_dirty = IsPreprocess ? g_preprocess_vat_dirty : g_main_vat_dirty;
  constexpr auto& vertex_loaders =
      IsPreprocess ? g_preprocess_vertex_loaders : g_main_vertex_loaders;

  VertexLoaderBase* loader;

  // We are not allowed to create a native vertex format on preprocessing as this is on the wrong
  // thread
  bool check_for_native_format = !IsPreprocess;

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
    auto [it, added] = s_vertex_loader_map.try_emplace(
        uid,
        VertexLoaderBase::CreateVertexLoader(state->vtx_desc, state->vtx_attr[vtx_attr_group]));
    loader = it->second.get();
    INCSTAT(g_stats.num_vertex_loaders);
  }
  if (check_for_native_format)
  {
    // search for a cached native vertex format
    loader->m_native_vertex_format = GetOrCreateMatchingFormat(loader->m_native_vtx_decl);
  }
  vertex_loaders[vtx_attr_group] = loader;
  attr_dirty[vtx_attr_group] = false;
  return loader;
}

}  // namespace detail

static void CheckCPConfiguration(int vtx_attr_group)
{
  // Validate that the XF input configuration matches the CP configuration
  u32 num_cp_colors = std::count_if(
      g_main_cp_state.vtx_desc.low.Color.begin(), g_main_cp_state.vtx_desc.low.Color.end(),
      [](auto format) { return format != VertexComponentFormat::NotPresent; });
  u32 num_cp_tex_coords = std::count_if(
      g_main_cp_state.vtx_desc.high.TexCoord.begin(), g_main_cp_state.vtx_desc.high.TexCoord.end(),
      [](auto format) { return format != VertexComponentFormat::NotPresent; });

  u32 num_cp_normals;
  if (g_main_cp_state.vtx_desc.low.Normal == VertexComponentFormat::NotPresent)
    num_cp_normals = 0;
  else if (g_main_cp_state.vtx_attr[vtx_attr_group].g0.NormalElements == NormalComponentCount::NTB)
    num_cp_normals = 3;
  else
    num_cp_normals = 1;

  std::optional<u32> num_xf_normals;
  switch (xfmem.invtxspec.numnormals)
  {
  case NormalCount::None:
    num_xf_normals = 0;
    break;
  case NormalCount::Normal:
    num_xf_normals = 1;
    break;
  case NormalCount::NormalTangentBinormal:
  case NormalCount::Invalid:  // see https://bugs.dolphin-emu.org/issues/13070
    num_xf_normals = 3;
    break;
  }

  if (num_cp_colors != xfmem.invtxspec.numcolors || num_cp_normals != num_xf_normals ||
      num_cp_tex_coords != xfmem.invtxspec.numtextures) [[unlikely]]
  {
    PanicAlertFmt("Mismatched configuration between CP and XF stages - {}/{} colors, {}/{} "
                  "normals, {}/{} texture coordinates. Please report on the issue tracker.\n\n"
                  "VCD: {:08x} {:08x}\nVAT {}: {:08x} {:08x} {:08x}\nXF vertex spec: {:08x}",
                  num_cp_colors, xfmem.invtxspec.numcolors, num_cp_normals,
                  num_xf_normals.has_value() ? fmt::to_string(num_xf_normals.value()) : "invalid",
                  num_cp_tex_coords, xfmem.invtxspec.numtextures, g_main_cp_state.vtx_desc.low.Hex,
                  g_main_cp_state.vtx_desc.high.Hex, vtx_attr_group,
                  g_main_cp_state.vtx_attr[vtx_attr_group].g0.Hex,
                  g_main_cp_state.vtx_attr[vtx_attr_group].g1.Hex,
                  g_main_cp_state.vtx_attr[vtx_attr_group].g2.Hex, xfmem.invtxspec.hex);

    // Analytics reporting so we can discover which games have this problem, that way when we
    // eventually simulate the behavior we have test cases for it.
    if (num_cp_colors != xfmem.invtxspec.numcolors) [[unlikely]]
    {
      DolphinAnalytics::Instance().ReportGameQuirk(
          GameQuirk::MISMATCHED_GPU_COLORS_BETWEEN_CP_AND_XF);
    }
    if (num_cp_normals != num_xf_normals) [[unlikely]]
    {
      DolphinAnalytics::Instance().ReportGameQuirk(
          GameQuirk::MISMATCHED_GPU_NORMALS_BETWEEN_CP_AND_XF);
    }
    if (num_cp_tex_coords != xfmem.invtxspec.numtextures) [[unlikely]]
    {
      DolphinAnalytics::Instance().ReportGameQuirk(
          GameQuirk::MISMATCHED_GPU_TEX_COORDS_BETWEEN_CP_AND_XF);
    }

    // Don't bail out, though; we can still render something successfully
    // (real hardware seems to hang in this case, though)
  }

  if (g_main_cp_state.matrix_index_a.Hex != xfmem.MatrixIndexA.Hex ||
      g_main_cp_state.matrix_index_b.Hex != xfmem.MatrixIndexB.Hex) [[unlikely]]
  {
    WARN_LOG_FMT(VIDEO,
                 "Mismatched matrix index configuration between CP and XF stages - "
                 "index A: {:08x}/{:08x}, index B {:08x}/{:08x}.",
                 g_main_cp_state.matrix_index_a.Hex, xfmem.MatrixIndexA.Hex,
                 g_main_cp_state.matrix_index_b.Hex, xfmem.MatrixIndexB.Hex);
    DolphinAnalytics::Instance().ReportGameQuirk(
        GameQuirk::MISMATCHED_GPU_MATRIX_INDICES_BETWEEN_CP_AND_XF);
  }

  if (g_main_cp_state.vtx_attr[vtx_attr_group].g0.PosFormat >= ComponentFormat::InvalidFloat5)
  {
    WARN_LOG_FMT(VIDEO, "Invalid position format {} for VAT {} - {:08x} {:08x} {:08x}",
                 g_main_cp_state.vtx_attr[vtx_attr_group].g0.PosFormat, vtx_attr_group,
                 g_main_cp_state.vtx_attr[vtx_attr_group].g0.Hex,
                 g_main_cp_state.vtx_attr[vtx_attr_group].g1.Hex,
                 g_main_cp_state.vtx_attr[vtx_attr_group].g2.Hex);
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::INVALID_POSITION_COMPONENT_FORMAT);
  }
  if (g_main_cp_state.vtx_attr[vtx_attr_group].g0.NormalFormat >= ComponentFormat::InvalidFloat5)
  {
    WARN_LOG_FMT(VIDEO, "Invalid normal format {} for VAT {} - {:08x} {:08x} {:08x}",
                 g_main_cp_state.vtx_attr[vtx_attr_group].g0.NormalFormat, vtx_attr_group,
                 g_main_cp_state.vtx_attr[vtx_attr_group].g0.Hex,
                 g_main_cp_state.vtx_attr[vtx_attr_group].g1.Hex,
                 g_main_cp_state.vtx_attr[vtx_attr_group].g2.Hex);
    DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::INVALID_NORMAL_COMPONENT_FORMAT);
  }
  for (size_t i = 0; i < 8; i++)
  {
    if (g_main_cp_state.vtx_attr[vtx_attr_group].GetTexFormat(i) >= ComponentFormat::InvalidFloat5)
    {
      WARN_LOG_FMT(VIDEO,
                   "Invalid texture coordinate {} format {} for VAT {} - {:08x} {:08x} {:08x}", i,
                   g_main_cp_state.vtx_attr[vtx_attr_group].GetTexFormat(i), vtx_attr_group,
                   g_main_cp_state.vtx_attr[vtx_attr_group].g0.Hex,
                   g_main_cp_state.vtx_attr[vtx_attr_group].g1.Hex,
                   g_main_cp_state.vtx_attr[vtx_attr_group].g2.Hex);
      DolphinAnalytics::Instance().ReportGameQuirk(
          GameQuirk::INVALID_TEXTURE_COORDINATE_COMPONENT_FORMAT);
    }
  }
  for (size_t i = 0; i < 2; i++)
  {
    if (g_main_cp_state.vtx_attr[vtx_attr_group].GetColorFormat(i) > ColorFormat::RGBA8888)
    {
      WARN_LOG_FMT(VIDEO, "Invalid color {} format {} for VAT {} - {:08x} {:08x} {:08x}", i,
                   g_main_cp_state.vtx_attr[vtx_attr_group].GetColorFormat(i), vtx_attr_group,
                   g_main_cp_state.vtx_attr[vtx_attr_group].g0.Hex,
                   g_main_cp_state.vtx_attr[vtx_attr_group].g1.Hex,
                   g_main_cp_state.vtx_attr[vtx_attr_group].g2.Hex);
      DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::INVALID_COLOR_COMPONENT_FORMAT);
    }
  }
}

template <bool IsPreprocess>
int RunVertices(int vtx_attr_group, OpcodeDecoder::Primitive primitive, int count, const u8* src)
{
  if (count == 0) [[unlikely]]
    return 0;
  ASSERT(count > 0);

  VertexLoaderBase* loader = RefreshLoader<IsPreprocess>(vtx_attr_group);

  int size = count * loader->m_vertex_size;

  if constexpr (!IsPreprocess)
  {
    // Doing early return for the opposite case would be cleaner
    // but triggers a false unreachable code warning in MSVC debug builds.

    if (g_needs_cp_xf_consistency_check) [[unlikely]]
    {
      CheckCPConfiguration(vtx_attr_group);
      g_needs_cp_xf_consistency_check = false;
    }

    // If the native vertex format changed, force a flush.
    if (loader->m_native_vertex_format != s_current_vtx_fmt ||
        loader->m_native_components != g_current_components) [[unlikely]]
    {
      g_vertex_manager->Flush();

      s_current_vtx_fmt = loader->m_native_vertex_format;
      g_current_components = loader->m_native_components;
      auto& system = Core::System::GetInstance();
      auto& vertex_shader_manager = system.GetVertexShaderManager();
      vertex_shader_manager.SetVertexFormat(loader->m_native_components,
                                            loader->m_native_vertex_format->GetVertexDeclaration());
    }

    // CPUCull's performance increase comes from encoding fewer GPU commands, not sending less data
    // Therefore it's only useful to check if culling could remove a flush
    const bool can_cpu_cull = g_ActiveConfig.bCPUCull &&
                              primitive < OpcodeDecoder::Primitive::GX_DRAW_LINES &&
                              !g_vertex_manager->HasSendableVertices();

    // if cull mode is CULL_ALL, tell VertexManager to skip triangles and quads.
    // They still need to go through vertex loading, because we need to calculate a zfreeze
    // reference slope.
    const bool cullall = (bpmem.genMode.cullmode == CullMode::All &&
                          primitive < OpcodeDecoder::Primitive::GX_DRAW_LINES);

    const int stride = loader->m_native_vtx_decl.stride;
    DataReader dst = g_vertex_manager->PrepareForAdditionalData(primitive, count, stride,
                                                                cullall || can_cpu_cull);

    count = loader->RunVertices(src, dst.GetPointer(), count);

    if (can_cpu_cull && !cullall)
    {
      if (!g_vertex_manager->AreAllVerticesCulled(loader, primitive, dst.GetPointer(), count))
      {
        DataReader new_dst = g_vertex_manager->DisableCullAll(stride);
        memmove(new_dst.GetPointer(), dst.GetPointer(), count * stride);
      }
    }

    g_vertex_manager->AddIndices(primitive, count);
    g_vertex_manager->FlushData(count, loader->m_native_vtx_decl.stride);

    ADDSTAT(g_stats.this_frame.num_prims, count);
    INCSTAT(g_stats.this_frame.num_primitive_joins);
  }
  return size;
}

template int RunVertices<false>(int vtx_attr_group, OpcodeDecoder::Primitive primitive, int count,
                                const u8* src);
template int RunVertices<true>(int vtx_attr_group, OpcodeDecoder::Primitive primitive, int count,
                               const u8* src);

NativeVertexFormat* GetCurrentVertexFormat()
{
  return s_current_vtx_fmt;
}

}  // namespace VertexLoaderManager
