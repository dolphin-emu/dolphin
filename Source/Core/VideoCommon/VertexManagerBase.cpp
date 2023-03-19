// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/VertexManagerBase.h"

#include <array>
#include <cmath>
#include <memory>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/EnumMap.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"

#include "Core/ConfigManager.h"
#include "Core/DolphinAnalytics.h"
#include "Core/System.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModActionData.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModManager.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PerfQueryBase.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/TextureInfo.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

std::unique_ptr<VertexManagerBase> g_vertex_manager;

using OpcodeDecoder::Primitive;

// GX primitive -> RenderState primitive, no primitive restart
constexpr Common::EnumMap<PrimitiveType, Primitive::GX_DRAW_POINTS> primitive_from_gx{
    PrimitiveType::Triangles,  // GX_DRAW_QUADS
    PrimitiveType::Triangles,  // GX_DRAW_QUADS_2
    PrimitiveType::Triangles,  // GX_DRAW_TRIANGLES
    PrimitiveType::Triangles,  // GX_DRAW_TRIANGLE_STRIP
    PrimitiveType::Triangles,  // GX_DRAW_TRIANGLE_FAN
    PrimitiveType::Lines,      // GX_DRAW_LINES
    PrimitiveType::Lines,      // GX_DRAW_LINE_STRIP
    PrimitiveType::Points,     // GX_DRAW_POINTS
};

// GX primitive -> RenderState primitive, using primitive restart
constexpr Common::EnumMap<PrimitiveType, Primitive::GX_DRAW_POINTS> primitive_from_gx_pr{
    PrimitiveType::TriangleStrip,  // GX_DRAW_QUADS
    PrimitiveType::TriangleStrip,  // GX_DRAW_QUADS_2
    PrimitiveType::TriangleStrip,  // GX_DRAW_TRIANGLES
    PrimitiveType::TriangleStrip,  // GX_DRAW_TRIANGLE_STRIP
    PrimitiveType::TriangleStrip,  // GX_DRAW_TRIANGLE_FAN
    PrimitiveType::Lines,          // GX_DRAW_LINES
    PrimitiveType::Lines,          // GX_DRAW_LINE_STRIP
    PrimitiveType::Points,         // GX_DRAW_POINTS
};

// Due to the BT.601 standard which the GameCube is based on being a compromise
// between PAL and NTSC, neither standard gets square pixels. They are each off
// by ~9% in opposite directions.
// Just in case any game decides to take this into account, we do both these
// tests with a large amount of slop.
static constexpr float ASPECT_RATIO_SLOP = 0.11f;

static bool IsAnamorphicProjection(const Projection::Raw& projection, const Viewport& viewport)
{
  // If ratio between our projection and viewport aspect ratios is similar to 16:9 / 4:3
  // we have an anamorphic projection.
  static constexpr float IDEAL_RATIO = (16 / 9.f) / (4 / 3.f);

  const float projection_ar = projection[2] / projection[0];
  const float viewport_ar = viewport.wd / viewport.ht;

  return std::abs(std::abs(projection_ar / viewport_ar) - IDEAL_RATIO) <
         IDEAL_RATIO * ASPECT_RATIO_SLOP;
}

static bool IsNormalProjection(const Projection::Raw& projection, const Viewport& viewport)
{
  const float projection_ar = projection[2] / projection[0];
  const float viewport_ar = viewport.wd / viewport.ht;
  return std::abs(std::abs(projection_ar / viewport_ar) - 1) < ASPECT_RATIO_SLOP;
}

VertexManagerBase::VertexManagerBase()
    : m_cpu_vertex_buffer(MAXVBUFFERSIZE), m_cpu_index_buffer(MAXIBUFFERSIZE)
{
}

VertexManagerBase::~VertexManagerBase() = default;

bool VertexManagerBase::Initialize()
{
  m_frame_end_event = AfterFrameEvent::Register([this] { OnEndFrame(); }, "VertexManagerBase");
  m_index_generator.Init();
  m_cpu_cull.Init();
  return true;
}

u32 VertexManagerBase::GetRemainingSize() const
{
  return static_cast<u32>(m_end_buffer_pointer - m_cur_buffer_pointer);
}

void VertexManagerBase::AddIndices(OpcodeDecoder::Primitive primitive, u32 num_vertices)
{
  m_index_generator.AddIndices(primitive, num_vertices);
}

bool VertexManagerBase::AreAllVerticesCulled(VertexLoaderBase* loader,
                                             OpcodeDecoder::Primitive primitive, const u8* src,
                                             u32 count)
{
  return m_cpu_cull.AreAllVerticesCulled(loader, primitive, src, count);
}

DataReader VertexManagerBase::PrepareForAdditionalData(OpcodeDecoder::Primitive primitive,
                                                       u32 count, u32 stride, bool cullall)
{
  // Flush all EFB pokes. Since the buffer is shared, we can't draw pokes+primitives concurrently.
  g_framebuffer_manager->FlushEFBPokes();

  // The SSE vertex loader can write up to 4 bytes past the end
  u32 const needed_vertex_bytes = count * stride + 4;

  // We can't merge different kinds of primitives, so we have to flush here
  PrimitiveType new_primitive_type = g_ActiveConfig.backend_info.bSupportsPrimitiveRestart ?
                                         primitive_from_gx_pr[primitive] :
                                         primitive_from_gx[primitive];
  if (m_current_primitive_type != new_primitive_type) [[unlikely]]
  {
    Flush();

    // Have to update the rasterization state for point/line cull modes.
    m_current_primitive_type = new_primitive_type;
    SetRasterizationStateChanged();
  }

  u32 remaining_indices = GetRemainingIndices(primitive);
  u32 remaining_index_generator_indices = m_index_generator.GetRemainingIndices(primitive);

  // Check for size in buffer, if the buffer gets full, call Flush()
  if (!m_is_flushed && (count > remaining_index_generator_indices || count > remaining_indices ||
                        needed_vertex_bytes > GetRemainingSize())) [[unlikely]]
  {
    Flush();
  }

  m_cull_all = cullall;

  // need to alloc new buffer
  if (m_is_flushed) [[unlikely]]
  {
    if (cullall)
    {
      // This buffer isn't getting sent to the GPU. Just allocate it on the cpu.
      m_cur_buffer_pointer = m_base_buffer_pointer = m_cpu_vertex_buffer.data();
      m_end_buffer_pointer = m_base_buffer_pointer + m_cpu_vertex_buffer.size();
      m_index_generator.Start(m_cpu_index_buffer.data());
    }
    else
    {
      ResetBuffer(stride);
    }

    remaining_index_generator_indices = m_index_generator.GetRemainingIndices(primitive);
    remaining_indices = GetRemainingIndices(primitive);
    m_is_flushed = false;
  }

  // Now that we've reset the buffer, there should be enough space. It's possible that we still
  // won't have enough space in a few rare cases, such as vertex shader line/point expansion with a
  // ton of lines in one draw command, in which case we will either need to add support for
  // splitting a single draw command into multiple draws or using bigger indices.
  ASSERT_MSG(VIDEO, count <= remaining_index_generator_indices,
             "VertexManager: Too few remaining index values ({} > {}). "
             "32-bit indices or primitive breaking needed.",
             count, remaining_index_generator_indices);
  ASSERT_MSG(VIDEO, count <= remaining_indices,
             "VertexManager: Buffer not large enough for all indices! ({} > {}) "
             "Increase MAXIBUFFERSIZE or we need primitive breaking after all.",
             count, remaining_indices);
  ASSERT_MSG(VIDEO, needed_vertex_bytes <= GetRemainingSize(),
             "VertexManager: Buffer not large enough for all vertices! ({} > {}) "
             "Increase MAXVBUFFERSIZE or we need primitive breaking after all.",
             needed_vertex_bytes, GetRemainingSize());

  return DataReader(m_cur_buffer_pointer, m_end_buffer_pointer);
}

DataReader VertexManagerBase::DisableCullAll(u32 stride)
{
  if (m_cull_all)
  {
    m_cull_all = false;
    ResetBuffer(stride);
  }
  return DataReader(m_cur_buffer_pointer, m_end_buffer_pointer);
}

void VertexManagerBase::FlushData(u32 count, u32 stride)
{
  m_cur_buffer_pointer += count * stride;
}

u32 VertexManagerBase::GetRemainingIndices(OpcodeDecoder::Primitive primitive) const
{
  const u32 index_len = MAXIBUFFERSIZE - m_index_generator.GetIndexLen();

  if (primitive >= Primitive::GX_DRAW_LINES)
  {
    if (g_Config.UseVSForLinePointExpand())
    {
      if (g_Config.backend_info.bSupportsPrimitiveRestart)
      {
        switch (primitive)
        {
        case Primitive::GX_DRAW_LINES:
          return index_len / 5 * 2;
        case Primitive::GX_DRAW_LINE_STRIP:
          return index_len / 5 + 1;
        case Primitive::GX_DRAW_POINTS:
          return index_len / 5;
        default:
          return 0;
        }
      }
      else
      {
        switch (primitive)
        {
        case Primitive::GX_DRAW_LINES:
          return index_len / 6 * 2;
        case Primitive::GX_DRAW_LINE_STRIP:
          return index_len / 6 + 1;
        case Primitive::GX_DRAW_POINTS:
          return index_len / 6;
        default:
          return 0;
        }
      }
    }
    else
    {
      switch (primitive)
      {
      case Primitive::GX_DRAW_LINES:
        return index_len;
      case Primitive::GX_DRAW_LINE_STRIP:
        return index_len / 2 + 1;
      case Primitive::GX_DRAW_POINTS:
        return index_len;
      default:
        return 0;
      }
    }
  }
  else if (g_Config.backend_info.bSupportsPrimitiveRestart)
  {
    switch (primitive)
    {
    case Primitive::GX_DRAW_QUADS:
    case Primitive::GX_DRAW_QUADS_2:
      return index_len / 5 * 4;
    case Primitive::GX_DRAW_TRIANGLES:
      return index_len / 4 * 3;
    case Primitive::GX_DRAW_TRIANGLE_STRIP:
      return index_len / 1 - 1;
    case Primitive::GX_DRAW_TRIANGLE_FAN:
      return index_len / 6 * 4 + 1;
    default:
      return 0;
    }
  }
  else
  {
    switch (primitive)
    {
    case Primitive::GX_DRAW_QUADS:
    case Primitive::GX_DRAW_QUADS_2:
      return index_len / 6 * 4;
    case Primitive::GX_DRAW_TRIANGLES:
      return index_len;
    case Primitive::GX_DRAW_TRIANGLE_STRIP:
      return index_len / 3 + 2;
    case Primitive::GX_DRAW_TRIANGLE_FAN:
      return index_len / 3 + 2;
    default:
      return 0;
    }
  }
}

auto VertexManagerBase::ResetFlushAspectRatioCount() -> FlushStatistics
{
  const auto result = m_flush_statistics;
  m_flush_statistics = {};
  return result;
}

void VertexManagerBase::ResetBuffer(u32 vertex_stride)
{
  m_base_buffer_pointer = m_cpu_vertex_buffer.data();
  m_cur_buffer_pointer = m_cpu_vertex_buffer.data();
  m_end_buffer_pointer = m_base_buffer_pointer + m_cpu_vertex_buffer.size();
  m_index_generator.Start(m_cpu_index_buffer.data());
}

void VertexManagerBase::CommitBuffer(u32 num_vertices, u32 vertex_stride, u32 num_indices,
                                     u32* out_base_vertex, u32* out_base_index)
{
  *out_base_vertex = 0;
  *out_base_index = 0;
}

void VertexManagerBase::DrawCurrentBatch(u32 base_index, u32 num_indices, u32 base_vertex)
{
  // If bounding box is enabled, we need to flush any changes first, then invalidate what we have.
  if (g_bounding_box->IsEnabled() && g_ActiveConfig.bBBoxEnable &&
      g_ActiveConfig.backend_info.bSupportsBBox)
  {
    g_bounding_box->Flush();
  }

  g_gfx->DrawIndexed(base_index, num_indices, base_vertex);
}

void VertexManagerBase::UploadUniforms()
{
}

void VertexManagerBase::InvalidateConstants()
{
  auto& system = Core::System::GetInstance();
  auto& vertex_shader_manager = system.GetVertexShaderManager();
  auto& geometry_shader_manager = system.GetGeometryShaderManager();
  auto& pixel_shader_manager = system.GetPixelShaderManager();
  vertex_shader_manager.dirty = true;
  geometry_shader_manager.dirty = true;
  pixel_shader_manager.dirty = true;
}

void VertexManagerBase::UploadUtilityUniforms(const void* uniforms, u32 uniforms_size)
{
}

void VertexManagerBase::UploadUtilityVertices(const void* vertices, u32 vertex_stride,
                                              u32 num_vertices, const u16* indices, u32 num_indices,
                                              u32* out_base_vertex, u32* out_base_index)
{
  // The GX vertex list should be flushed before any utility draws occur.
  ASSERT(m_is_flushed);

  // Copy into the buffers usually used for GX drawing.
  ResetBuffer(std::max(vertex_stride, 1u));
  if (vertices)
  {
    const u32 copy_size = vertex_stride * num_vertices;
    ASSERT((m_cur_buffer_pointer + copy_size) <= m_end_buffer_pointer);
    std::memcpy(m_cur_buffer_pointer, vertices, copy_size);
    m_cur_buffer_pointer += copy_size;
  }
  if (indices)
    m_index_generator.AddExternalIndices(indices, num_indices, num_vertices);

  CommitBuffer(num_vertices, vertex_stride, num_indices, out_base_vertex, out_base_index);
}

u32 VertexManagerBase::GetTexelBufferElementSize(TexelBufferFormat buffer_format)
{
  // R8 - 1, R16 - 2, RGBA8 - 4, R32G32 - 8
  return 1u << static_cast<u32>(buffer_format);
}

bool VertexManagerBase::UploadTexelBuffer(const void* data, u32 data_size, TexelBufferFormat format,
                                          u32* out_offset)
{
  return false;
}

bool VertexManagerBase::UploadTexelBuffer(const void* data, u32 data_size, TexelBufferFormat format,
                                          u32* out_offset, const void* palette_data,
                                          u32 palette_size, TexelBufferFormat palette_format,
                                          u32* palette_offset)
{
  return false;
}

BitSet32 VertexManagerBase::UsedTextures() const
{
  BitSet32 usedtextures;
  for (u32 i = 0; i < bpmem.genMode.numtevstages + 1u; ++i)
    if (bpmem.tevorders[i / 2].getEnable(i & 1))
      usedtextures[bpmem.tevorders[i / 2].getTexMap(i & 1)] = true;

  if (bpmem.genMode.numindstages > 0)
    for (unsigned int i = 0; i < bpmem.genMode.numtevstages + 1u; ++i)
      if (bpmem.tevind[i].IsActive() && bpmem.tevind[i].bt < bpmem.genMode.numindstages)
        usedtextures[bpmem.tevindref.getTexMap(bpmem.tevind[i].bt)] = true;

  return usedtextures;
}

void VertexManagerBase::Flush()
{
  if (m_is_flushed)
    return;

  m_is_flushed = true;

  if (m_draw_counter == 0)
  {
    // This is more or less the start of the Frame
    BeforeFrameEvent::Trigger();
  }

  if (xfmem.numTexGen.numTexGens != bpmem.genMode.numtexgens ||
      xfmem.numChan.numColorChans != bpmem.genMode.numcolchans)
  {
    ERROR_LOG_FMT(
        VIDEO,
        "Mismatched configuration between XF and BP stages - {}/{} texgens, {}/{} colors. "
        "Skipping draw. Please report on the issue tracker.",
        xfmem.numTexGen.numTexGens, bpmem.genMode.numtexgens.Value(), xfmem.numChan.numColorChans,
        bpmem.genMode.numcolchans.Value());

    // Analytics reporting so we can discover which games have this problem, that way when we
    // eventually simulate the behavior we have test cases for it.
    if (xfmem.numTexGen.numTexGens != bpmem.genMode.numtexgens)
    {
      DolphinAnalytics::Instance().ReportGameQuirk(
          GameQuirk::MISMATCHED_GPU_TEXGENS_BETWEEN_XF_AND_BP);
    }
    if (xfmem.numChan.numColorChans != bpmem.genMode.numcolchans)
    {
      DolphinAnalytics::Instance().ReportGameQuirk(
          GameQuirk::MISMATCHED_GPU_COLORS_BETWEEN_XF_AND_BP);
    }

    return;
  }

#if defined(_DEBUG) || defined(DEBUGFAST)
  PRIM_LOG("frame{}:\n texgen={}, numchan={}, dualtex={}, ztex={}, cole={}, alpe={}, ze={}",
           g_ActiveConfig.iSaveTargetId, xfmem.numTexGen.numTexGens, xfmem.numChan.numColorChans,
           xfmem.dualTexTrans.enabled, bpmem.ztex2.op.Value(), bpmem.blendmode.colorupdate.Value(),
           bpmem.blendmode.alphaupdate.Value(), bpmem.zmode.updateenable.Value());

  for (u32 i = 0; i < xfmem.numChan.numColorChans; ++i)
  {
    LitChannel* ch = &xfmem.color[i];
    PRIM_LOG("colchan{}: matsrc={}, light={:#x}, ambsrc={}, diffunc={}, attfunc={}", i,
             ch->matsource.Value(), ch->GetFullLightMask(), ch->ambsource.Value(),
             ch->diffusefunc.Value(), ch->attnfunc.Value());
    ch = &xfmem.alpha[i];
    PRIM_LOG("alpchan{}: matsrc={}, light={:#x}, ambsrc={}, diffunc={}, attfunc={}", i,
             ch->matsource.Value(), ch->GetFullLightMask(), ch->ambsource.Value(),
             ch->diffusefunc.Value(), ch->attnfunc.Value());
  }

  for (u32 i = 0; i < xfmem.numTexGen.numTexGens; ++i)
  {
    TexMtxInfo tinfo = xfmem.texMtxInfo[i];
    if (tinfo.texgentype != TexGenType::EmbossMap)
      tinfo.hex &= 0x7ff;
    if (tinfo.texgentype != TexGenType::Regular)
      tinfo.projection = TexSize::ST;

    PRIM_LOG("txgen{}: proj={}, input={}, gentype={}, srcrow={}, embsrc={}, emblght={}, "
             "postmtx={}, postnorm={}",
             i, tinfo.projection.Value(), tinfo.inputform.Value(), tinfo.texgentype.Value(),
             tinfo.sourcerow.Value(), tinfo.embosssourceshift.Value(),
             tinfo.embosslightshift.Value(), xfmem.postMtxInfo[i].index.Value(),
             xfmem.postMtxInfo[i].normalize.Value());
  }

  PRIM_LOG("pixel: tev={}, ind={}, texgen={}, dstalpha={}, alphatest={:#x}",
           bpmem.genMode.numtevstages.Value() + 1, bpmem.genMode.numindstages.Value(),
           bpmem.genMode.numtexgens.Value(), bpmem.dstalpha.enable.Value(),
           (bpmem.alpha_test.hex >> 16) & 0xff);
#endif

  // Track some stats used elsewhere by the anamorphic widescreen heuristic.
  if (!SConfig::GetInstance().bWii)
  {
    const bool is_perspective = xfmem.projection.type == ProjectionType::Perspective;

    auto& counts =
        is_perspective ? m_flush_statistics.perspective : m_flush_statistics.orthographic;

    if (IsAnamorphicProjection(xfmem.projection.rawProjection, xfmem.viewport))
    {
      ++counts.anamorphic_flush_count;
      counts.anamorphic_vertex_count += m_index_generator.GetIndexLen();
    }
    else if (IsNormalProjection(xfmem.projection.rawProjection, xfmem.viewport))
    {
      ++counts.normal_flush_count;
      counts.normal_vertex_count += m_index_generator.GetIndexLen();
    }
    else
    {
      ++counts.other_flush_count;
      counts.other_vertex_count += m_index_generator.GetIndexLen();
    }
  }

  auto& system = Core::System::GetInstance();
  auto& pixel_shader_manager = system.GetPixelShaderManager();
  auto& geometry_shader_manager = system.GetGeometryShaderManager();
  auto& vertex_shader_manager = system.GetVertexShaderManager();

  CalculateBinormals(VertexLoaderManager::GetCurrentVertexFormat());
  // Calculate ZSlope for zfreeze
  const auto used_textures = UsedTextures();
  std::vector<std::string> texture_names;
  if (!m_cull_all)
  {
    if (!g_ActiveConfig.bGraphicMods)
    {
      for (const u32 i : used_textures)
      {
        g_texture_cache->Load(TextureInfo::FromStage(i));
      }
    }
    else
    {
      for (const u32 i : used_textures)
      {
        const auto cache_entry = g_texture_cache->Load(TextureInfo::FromStage(i));
        if (cache_entry)
        {
          texture_names.push_back(cache_entry->texture_info_name);
        }
      }
    }
  }
  vertex_shader_manager.SetConstants(texture_names);
  if (!bpmem.genMode.zfreeze)
  {
    // Must be done after VertexShaderManager::SetConstants()
    CalculateZSlope(VertexLoaderManager::GetCurrentVertexFormat());
  }
  else if (m_zslope.dirty && !m_cull_all)  // or apply any dirty ZSlopes
  {
    pixel_shader_manager.SetZSlope(m_zslope.dfdx, m_zslope.dfdy, m_zslope.f0);
    m_zslope.dirty = false;
  }

  if (!m_cull_all)
  {
    for (const auto& texture_name : texture_names)
    {
      bool skip = false;
      GraphicsModActionData::DrawStarted draw_started{&skip};
      for (const auto action : g_graphics_mod_manager->GetDrawStartedActions(texture_name))
      {
        action->OnDrawStarted(&draw_started);
      }
      if (skip == true)
        return;
    }

    // Now the vertices can be flushed to the GPU. Everything following the CommitBuffer() call
    // must be careful to not upload any utility vertices, as the binding will be lost otherwise.
    const u32 num_indices = m_index_generator.GetIndexLen();
    if (num_indices == 0)
      return;
    u32 base_vertex, base_index;
    CommitBuffer(m_index_generator.GetNumVerts(),
                 VertexLoaderManager::GetCurrentVertexFormat()->GetVertexStride(), num_indices,
                 &base_vertex, &base_index);

    if (g_ActiveConfig.backend_info.api_type != APIType::D3D &&
        g_ActiveConfig.UseVSForLinePointExpand() &&
        (m_current_primitive_type == PrimitiveType::Points ||
         m_current_primitive_type == PrimitiveType::Lines))
    {
      // VS point/line expansion puts the vertex id at gl_VertexID << 2
      // That means the base vertex has to be adjusted to match
      // (The shader adds this after shifting right on D3D, so no need to do this)
      base_vertex <<= 2;
    }

    // Texture loading can cause palettes to be applied (-> uniforms -> draws).
    // Palette application does not use vertices, only a full-screen quad, so this is okay.
    // Same with GPU texture decoding, which uses compute shaders.
    g_texture_cache->BindTextures(used_textures);

    // Now we can upload uniforms, as nothing else will override them.
    geometry_shader_manager.SetConstants(m_current_primitive_type);
    pixel_shader_manager.SetConstants();
    UploadUniforms();

    // Update the pipeline, or compile one if needed.
    UpdatePipelineConfig();
    UpdatePipelineObject();
    if (m_current_pipeline_object)
    {
      g_gfx->SetPipeline(m_current_pipeline_object);
      if (PerfQueryBase::ShouldEmulate())
        g_perf_query->EnableQuery(bpmem.zcontrol.early_ztest ? PQG_ZCOMP_ZCOMPLOC : PQG_ZCOMP);

      DrawCurrentBatch(base_index, num_indices, base_vertex);
      INCSTAT(g_stats.this_frame.num_draw_calls);

      if (PerfQueryBase::ShouldEmulate())
        g_perf_query->DisableQuery(bpmem.zcontrol.early_ztest ? PQG_ZCOMP_ZCOMPLOC : PQG_ZCOMP);

      OnDraw();

      // The EFB cache is now potentially stale.
      g_framebuffer_manager->FlagPeekCacheAsOutOfDate();
    }
  }

  if (xfmem.numTexGen.numTexGens != bpmem.genMode.numtexgens)
  {
    ERROR_LOG_FMT(VIDEO,
                  "xf.numtexgens ({}) does not match bp.numtexgens ({}). Error in command stream.",
                  xfmem.numTexGen.numTexGens, bpmem.genMode.numtexgens.Value());
  }
}

void VertexManagerBase::DoState(PointerWrap& p)
{
  if (p.IsReadMode())
  {
    // Flush old vertex data before loading state.
    Flush();
  }

  p.Do(m_zslope);
  p.Do(VertexLoaderManager::tangent_cache);
  p.Do(VertexLoaderManager::binormal_cache);
}

void VertexManagerBase::CalculateZSlope(NativeVertexFormat* format)
{
  float out[12];
  float viewOffset[2] = {xfmem.viewport.xOrig - bpmem.scissorOffset.x * 2,
                         xfmem.viewport.yOrig - bpmem.scissorOffset.y * 2};

  if (m_current_primitive_type != PrimitiveType::Triangles &&
      m_current_primitive_type != PrimitiveType::TriangleStrip)
  {
    return;
  }

  // Global matrix ID.
  u32 mtxIdx = g_main_cp_state.matrix_index_a.PosNormalMtxIdx;
  const PortableVertexDeclaration vert_decl = format->GetVertexDeclaration();

  // Make sure the buffer contains at least 3 vertices.
  if ((m_cur_buffer_pointer - m_base_buffer_pointer) < (vert_decl.stride * 3))
    return;

  // Lookup vertices of the last rendered triangle and software-transform them
  // This allows us to determine the depth slope, which will be used if z-freeze
  // is enabled in the following flush.
  auto& system = Core::System::GetInstance();
  auto& vertex_shader_manager = system.GetVertexShaderManager();
  for (unsigned int i = 0; i < 3; ++i)
  {
    // If this vertex format has per-vertex position matrix IDs, look it up.
    if (vert_decl.posmtx.enable)
      mtxIdx = VertexLoaderManager::position_matrix_index_cache[2 - i];

    if (vert_decl.position.components == 2)
      VertexLoaderManager::position_cache[2 - i][2] = 0;

    vertex_shader_manager.TransformToClipSpace(&VertexLoaderManager::position_cache[2 - i][0],
                                               &out[i * 4], mtxIdx);

    // Transform to Screenspace
    float inv_w = 1.0f / out[3 + i * 4];

    out[0 + i * 4] = out[0 + i * 4] * inv_w * xfmem.viewport.wd + viewOffset[0];
    out[1 + i * 4] = out[1 + i * 4] * inv_w * xfmem.viewport.ht + viewOffset[1];
    out[2 + i * 4] = out[2 + i * 4] * inv_w * xfmem.viewport.zRange + xfmem.viewport.farZ;
  }

  float dx31 = out[8] - out[0];
  float dx12 = out[0] - out[4];
  float dy12 = out[1] - out[5];
  float dy31 = out[9] - out[1];

  float DF31 = out[10] - out[2];
  float DF21 = out[6] - out[2];
  float a = DF31 * -dy12 - DF21 * dy31;
  float b = dx31 * DF21 + dx12 * DF31;
  float c = -dx12 * dy31 - dx31 * -dy12;

  // Sometimes we process de-generate triangles. Stop any divide by zeros
  if (c == 0)
    return;

  m_zslope.dfdx = -a / c;
  m_zslope.dfdy = -b / c;
  m_zslope.f0 = out[2] - (out[0] * m_zslope.dfdx + out[1] * m_zslope.dfdy);
  m_zslope.dirty = true;
}

void VertexManagerBase::CalculateBinormals(NativeVertexFormat* format)
{
  const PortableVertexDeclaration vert_decl = format->GetVertexDeclaration();

  // Only update the binormal/tangent vertex shader constants if the vertex format lacks binormals
  // (VertexLoaderManager::binormal_cache gets updated by the vertex loader when binormals are
  // present, though)
  if (vert_decl.normals[1].enable)
    return;

  VertexLoaderManager::tangent_cache[3] = 0;
  VertexLoaderManager::binormal_cache[3] = 0;

  auto& system = Core::System::GetInstance();
  auto& vertex_shader_manager = system.GetVertexShaderManager();
  if (vertex_shader_manager.constants.cached_tangent != VertexLoaderManager::tangent_cache)
  {
    vertex_shader_manager.constants.cached_tangent = VertexLoaderManager::tangent_cache;
    vertex_shader_manager.dirty = true;
  }
  if (vertex_shader_manager.constants.cached_binormal != VertexLoaderManager::binormal_cache)
  {
    vertex_shader_manager.constants.cached_binormal = VertexLoaderManager::binormal_cache;
    vertex_shader_manager.dirty = true;
  }
}

void VertexManagerBase::UpdatePipelineConfig()
{
  NativeVertexFormat* vertex_format = VertexLoaderManager::GetCurrentVertexFormat();
  if (vertex_format != m_current_pipeline_config.vertex_format)
  {
    m_current_pipeline_config.vertex_format = vertex_format;
    m_current_uber_pipeline_config.vertex_format =
        VertexLoaderManager::GetUberVertexFormat(vertex_format->GetVertexDeclaration());
    m_pipeline_config_changed = true;
  }

  VertexShaderUid vs_uid = GetVertexShaderUid();
  if (vs_uid != m_current_pipeline_config.vs_uid)
  {
    m_current_pipeline_config.vs_uid = vs_uid;
    m_current_uber_pipeline_config.vs_uid = UberShader::GetVertexShaderUid();
    m_pipeline_config_changed = true;
  }

  PixelShaderUid ps_uid = GetPixelShaderUid();
  if (ps_uid != m_current_pipeline_config.ps_uid)
  {
    m_current_pipeline_config.ps_uid = ps_uid;
    m_current_uber_pipeline_config.ps_uid = UberShader::GetPixelShaderUid();
    m_pipeline_config_changed = true;
  }

  GeometryShaderUid gs_uid = GetGeometryShaderUid(GetCurrentPrimitiveType());
  if (gs_uid != m_current_pipeline_config.gs_uid)
  {
    m_current_pipeline_config.gs_uid = gs_uid;
    m_current_uber_pipeline_config.gs_uid = gs_uid;
    m_pipeline_config_changed = true;
  }

  if (m_rasterization_state_changed)
  {
    m_rasterization_state_changed = false;

    RasterizationState new_rs = {};
    new_rs.Generate(bpmem, m_current_primitive_type);
    if (new_rs != m_current_pipeline_config.rasterization_state)
    {
      m_current_pipeline_config.rasterization_state = new_rs;
      m_current_uber_pipeline_config.rasterization_state = new_rs;
      m_pipeline_config_changed = true;
    }
  }

  if (m_depth_state_changed)
  {
    m_depth_state_changed = false;

    DepthState new_ds = {};
    new_ds.Generate(bpmem);
    if (new_ds != m_current_pipeline_config.depth_state)
    {
      m_current_pipeline_config.depth_state = new_ds;
      m_current_uber_pipeline_config.depth_state = new_ds;
      m_pipeline_config_changed = true;
    }
  }

  if (m_blending_state_changed)
  {
    m_blending_state_changed = false;

    BlendingState new_bs = {};
    new_bs.Generate(bpmem);
    if (new_bs != m_current_pipeline_config.blending_state)
    {
      m_current_pipeline_config.blending_state = new_bs;
      m_current_uber_pipeline_config.blending_state = new_bs;
      m_pipeline_config_changed = true;
    }
  }
}

void VertexManagerBase::UpdatePipelineObject()
{
  if (!m_pipeline_config_changed)
    return;

  m_current_pipeline_object = nullptr;
  m_pipeline_config_changed = false;

  switch (g_ActiveConfig.iShaderCompilationMode)
  {
  case ShaderCompilationMode::Synchronous:
  {
    // Ubershaders disabled? Block and compile the specialized shader.
    m_current_pipeline_object = g_shader_cache->GetPipelineForUid(m_current_pipeline_config);
  }
  break;

  case ShaderCompilationMode::SynchronousUberShaders:
  {
    // Exclusive ubershader mode, always use ubershaders.
    m_current_pipeline_object =
        g_shader_cache->GetUberPipelineForUid(m_current_uber_pipeline_config);
  }
  break;

  case ShaderCompilationMode::AsynchronousUberShaders:
  case ShaderCompilationMode::AsynchronousSkipRendering:
  {
    // Can we background compile shaders? If so, get the pipeline asynchronously.
    auto res = g_shader_cache->GetPipelineForUidAsync(m_current_pipeline_config);
    if (res)
    {
      // Specialized shaders are ready, prefer these.
      m_current_pipeline_object = *res;
      return;
    }

    if (g_ActiveConfig.iShaderCompilationMode == ShaderCompilationMode::AsynchronousUberShaders)
    {
      // Specialized shaders not ready, use the ubershaders.
      m_current_pipeline_object =
          g_shader_cache->GetUberPipelineForUid(m_current_uber_pipeline_config);
    }
    else
    {
      // Ensure we try again next draw. Otherwise, if no registers change between frames, the
      // object will never be drawn, even when the shader is ready.
      m_pipeline_config_changed = true;
    }
  }
  break;
  }
}

void VertexManagerBase::OnConfigChange()
{
  // Reload index generator function tables in case VS expand config changed
  m_index_generator.Init();
}

void VertexManagerBase::OnDraw()
{
  m_draw_counter++;

  // If the last efb copy was too close to the one before it, don't forget about it until the next
  // efb copy happens (which might not be for a long time)
  u32 diff = m_draw_counter - m_last_efb_copy_draw_counter;
  if (m_unflushed_efb_copy && diff > MINIMUM_DRAW_CALLS_PER_COMMAND_BUFFER_FOR_READBACK)
  {
    g_gfx->Flush();
    m_unflushed_efb_copy = false;
    m_last_efb_copy_draw_counter = m_draw_counter;
  }

  // If we didn't have any CPU access last frame, do nothing.
  if (m_scheduled_command_buffer_kicks.empty() || !m_allow_background_execution)
    return;

  // Check if this draw is scheduled to kick a command buffer.
  // The draw counters will always be sorted so a binary search is possible here.
  if (std::binary_search(m_scheduled_command_buffer_kicks.begin(),
                         m_scheduled_command_buffer_kicks.end(), m_draw_counter))
  {
    // Kick a command buffer on the background thread.
    g_gfx->Flush();
    m_unflushed_efb_copy = false;
    m_last_efb_copy_draw_counter = m_draw_counter;
  }
}

void VertexManagerBase::OnCPUEFBAccess()
{
  // Check this isn't another access without any draws inbetween.
  if (!m_cpu_accesses_this_frame.empty() && m_cpu_accesses_this_frame.back() == m_draw_counter)
    return;

  // Store the current draw counter for scheduling in OnEndFrame.
  m_cpu_accesses_this_frame.emplace_back(m_draw_counter);
}

void VertexManagerBase::OnEFBCopyToRAM()
{
  // If we're not deferring, try to preempt it next frame.
  if (!g_ActiveConfig.bDeferEFBCopies)
  {
    OnCPUEFBAccess();
    return;
  }

  // Otherwise, only execute if we have at least 10 objects between us and the last copy.
  const u32 diff = m_draw_counter - m_last_efb_copy_draw_counter;
  m_last_efb_copy_draw_counter = m_draw_counter;
  if (diff < MINIMUM_DRAW_CALLS_PER_COMMAND_BUFFER_FOR_READBACK)
  {
    m_unflushed_efb_copy = true;
    return;
  }

  m_unflushed_efb_copy = false;
  g_gfx->Flush();
}

void VertexManagerBase::OnEndFrame()
{
  m_draw_counter = 0;
  m_last_efb_copy_draw_counter = 0;
  m_scheduled_command_buffer_kicks.clear();

  // If we have no CPU access at all, leave everything in the one command buffer for maximum
  // parallelism between CPU/GPU, at the cost of slightly higher latency.
  if (m_cpu_accesses_this_frame.empty())
    return;

  // In order to reduce CPU readback latency, we want to kick a command buffer roughly halfway
  // between the draw counters that invoked the readback, or every 250 draws, whichever is smaller.
  if (g_ActiveConfig.iCommandBufferExecuteInterval > 0)
  {
    u32 last_draw_counter = 0;
    u32 interval = static_cast<u32>(g_ActiveConfig.iCommandBufferExecuteInterval);
    for (u32 draw_counter : m_cpu_accesses_this_frame)
    {
      // We don't want to waste executing command buffers for only a few draws, so set a minimum.
      // Leave last_draw_counter as-is, so we get the correct number of draws between submissions.
      u32 draw_count = draw_counter - last_draw_counter;
      if (draw_count < MINIMUM_DRAW_CALLS_PER_COMMAND_BUFFER_FOR_READBACK)
        continue;

      if (draw_count <= interval)
      {
        u32 mid_point = draw_count / 2;
        m_scheduled_command_buffer_kicks.emplace_back(last_draw_counter + mid_point);
      }
      else
      {
        u32 counter = interval;
        while (counter < draw_count)
        {
          m_scheduled_command_buffer_kicks.emplace_back(last_draw_counter + counter);
          counter += interval;
        }
      }

      last_draw_counter = draw_counter;
    }
  }

#if 0
  {
    std::ostringstream ss;
    std::for_each(m_cpu_accesses_this_frame.begin(), m_cpu_accesses_this_frame.end(), [&ss](u32 idx) { ss << idx << ","; });
    WARN_LOG_FMT(VIDEO, "CPU EFB accesses in last frame: {}", ss.str());
  }
  {
    std::ostringstream ss;
    std::for_each(m_scheduled_command_buffer_kicks.begin(), m_scheduled_command_buffer_kicks.end(), [&ss](u32 idx) { ss << idx << ","; });
    WARN_LOG_FMT(VIDEO, "Scheduled command buffer kicks: {}", ss.str());
  }
#endif

  m_cpu_accesses_this_frame.clear();

  // We invalidate the pipeline object at the start of the frame.
  // This is for the rare case where only a single pipeline configuration is used,
  // and hybrid ubershaders have compiled the specialized shader, but without any
  // state changes the specialized shader will not take over.
  InvalidatePipelineObject();
}
