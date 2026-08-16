// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/VertexManagerBase.h"

#include <array>
#include <cmath>
#include <limits>
#include <memory>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Contains.h"
#include "Common/EnumMap.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Common/SmallVector.h"

#include "Core/DolphinAnalytics.h"
#include "Core/HW/SystemTimers.h"
#include "Core/System.h"

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/GraphicsModSystem/Runtime/CustomShaderCache.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModActionData.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModManager.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PerfQueryBase.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"
#include "VideoCommon/XFStateManager.h"

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

static float CalculateProjectionViewportRatio(const Projection::Raw& projection,
                                              const Viewport& viewport)
{
  const float projection_ar = projection[2] / projection[0];
  const float viewport_ar = viewport.wd / viewport.ht;

  return std::abs(projection_ar / viewport_ar);
}

static bool IsAnamorphicProjection(const Projection::Raw& projection, const Viewport& viewport,
                                   const VideoConfig& config)
{
  // If ratio between our projection and viewport aspect ratios is similar to 16:9 / 4:3
  // we have an anamorphic projection. This value can be overridden by a GameINI.
  // Game cheats that change the aspect ratio to natively unsupported ones
  // won't be automatically recognized here.

  return std::abs(CalculateProjectionViewportRatio(projection, viewport) -
                  config.widescreen_heuristic_widescreen_ratio) <
         config.widescreen_heuristic_aspect_ratio_slop;
}

static bool IsNormalProjection(const Projection::Raw& projection, const Viewport& viewport,
                               const VideoConfig& config)
{
  return std::abs(CalculateProjectionViewportRatio(projection, viewport) -
                  config.widescreen_heuristic_standard_ratio) <
         config.widescreen_heuristic_aspect_ratio_slop;
}

VertexManagerBase::VertexManagerBase()
    : m_cpu_vertex_buffer(MAXVBUFFERSIZE), m_cpu_index_buffer(MAXIBUFFERSIZE)
{
}

VertexManagerBase::~VertexManagerBase() = default;

bool VertexManagerBase::Initialize()
{
  auto& video_events = GetVideoEvents();

  m_frame_end_event =
      video_events.after_frame_event.Register([this](Core::System&) { OnEndFrame(); });
  m_after_present_event = video_events.after_present_event.Register(
      [this](const PresentInfo& pi) { m_ticks_elapsed = pi.emulated_timestamp; });
  m_index_generator.Init();
  m_custom_shader_cache = std::make_unique<CustomShaderCache>();
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
  PrimitiveType new_primitive_type = g_backend_info.bSupportsPrimitiveRestart ?
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
      if (g_backend_info.bSupportsPrimitiveRestart)
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
  else if (g_backend_info.bSupportsPrimitiveRestart)
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
  if (g_bounding_box->IsEnabled() && g_ActiveConfig.bBBoxEnable && g_backend_info.bSupportsBBox)
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
    GetVideoEvents().before_frame_event.Trigger();

    // Gated on the same answer the replay is, so that a configuration which can never replay does
    // not pay for a recording nothing will ever read. Recording is not free: every draw's vertices
    // have to be read back out of the streaming buffer, which is uncached memory.
    //
    // Safe to run even from the flush a replay does before it submits anything: the recorder
    // opens the new recording in a slot the replay is not reading from.
    if (IsFrameGenerationUsable())
    {
      m_frame_recorder.BeginFrame();
    }
    else
    {
      // Nothing usable can come of a recording made now, so drop what is being held.
      m_frame_recorder.Invalidate();

      // The render targets the replay copies live in are only given back when the feature itself
      // is off. Something that merely stops a replay for a while, such as bounding box emulation
      // coming and going mid-scene, should not cost a round of render target allocation each time.
      if (!g_ActiveConfig.frame_generation.bEnabled)
        m_replay_targets.Release();
    }
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
      DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::MismatchedGPUTexGensBetweenXFAndBP);
    }
    if (xfmem.numChan.numColorChans != bpmem.genMode.numcolchans)
    {
      DolphinAnalytics::Instance().ReportGameQuirk(GameQuirk::MismatchedGPUColorsBetweenXFAndBP);
    }

    return;
  }

#if defined(_DEBUG) || defined(DEBUGFAST)
  PRIM_LOG("frame{}:\n texgen={}, numchan={}, dualtex={}, ztex={}, cole={}, alpe={}, ze={}",
           g_ActiveConfig.iSaveTargetId, xfmem.numTexGen.numTexGens, xfmem.numChan.numColorChans,
           xfmem.dualTexTrans.enabled, bpmem.ztex2.op.Value(), bpmem.blendmode.color_update.Value(),
           bpmem.blendmode.alpha_update.Value(), bpmem.zmode.update_enable.Value());

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
  auto& system = Core::System::GetInstance();
  if (!system.IsWii())
  {
    const bool is_perspective = xfmem.projection.type == ProjectionType::Perspective;

    auto& counts =
        is_perspective ? m_flush_statistics.perspective : m_flush_statistics.orthographic;

    const auto& projection = xfmem.projection.rawProjection;
    // TODO: Potentially the viewport size could be used as weight for the flush count average.
    // This way a small minimap would have less effect than a fullscreen projection.
    const auto& viewport = xfmem.viewport;

    // FYI: This average is based on flushes.
    // It doesn't look at vertex counts like the heuristic does.
    counts.average_ratio.Push(CalculateProjectionViewportRatio(projection, viewport));

    if (IsAnamorphicProjection(projection, viewport, g_ActiveConfig))
    {
      ++counts.anamorphic_flush_count;
      counts.anamorphic_vertex_count += m_index_generator.GetIndexLen();
    }
    else if (IsNormalProjection(projection, viewport, g_ActiveConfig))
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

  auto& pixel_shader_manager = system.GetPixelShaderManager();
  auto& geometry_shader_manager = system.GetGeometryShaderManager();
  auto& vertex_shader_manager = system.GetVertexShaderManager();
  auto& xf_state_manager = system.GetXFStateManager();

  if (g_ActiveConfig.bGraphicMods)
  {
    const double seconds_elapsed =
        static_cast<double>(m_ticks_elapsed) / system.GetSystemTimers().GetTicksPerSecond();
    pixel_shader_manager.constants.time_ms = seconds_elapsed * 1000;
  }

  CalculateNormals(VertexLoaderManager::GetCurrentVertexFormat());
  // Calculate ZSlope for zfreeze
  const auto used_textures = UsedTextures();
  std::vector<std::string> texture_names;
  Common::SmallVector<u32, 8> texture_units;
  std::array<SamplerState, 8> samplers;
  if (!m_cull_all)
  {
    if (!g_ActiveConfig.bGraphicMods)
    {
      for (const u32 i : used_textures)
      {
        const auto cache_entry = g_texture_cache->Load(i);
        if (!cache_entry)
          continue;
        const float custom_tex_scale = cache_entry->GetWidth() / float(cache_entry->native_width);
        samplers[i] = TextureCacheBase::GetSamplerState(
            i, custom_tex_scale, cache_entry->is_custom_tex, cache_entry->has_arbitrary_mips);
      }
    }
    else
    {
      for (const u32 i : used_textures)
      {
        const auto cache_entry = g_texture_cache->Load(i);
        if (cache_entry)
        {
          if (!Common::Contains(texture_names, cache_entry->texture_info_name))
          {
            texture_names.push_back(cache_entry->texture_info_name);
            texture_units.push_back(i);
          }

          const float custom_tex_scale = cache_entry->GetWidth() / float(cache_entry->native_width);
          samplers[i] = TextureCacheBase::GetSamplerState(
              i, custom_tex_scale, cache_entry->is_custom_tex, cache_entry->has_arbitrary_mips);
        }
      }
    }
  }
  vertex_shader_manager.SetConstants(texture_names, xf_state_manager);
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
    CustomPixelShaderContents custom_pixel_shader_contents;
    std::optional<CustomPixelShader> custom_pixel_shader;
    std::vector<std::string> custom_pixel_texture_names;
    std::span<u8> custom_pixel_shader_uniforms;
    bool skip = false;
    for (size_t i = 0; i < texture_names.size(); i++)
    {
      GraphicsModActionData::DrawStarted draw_started{texture_units, &skip, &custom_pixel_shader,
                                                      &custom_pixel_shader_uniforms};
      for (const auto& action : g_graphics_mod_manager->GetDrawStartedActions(texture_names[i]))
      {
        action->OnDrawStarted(&draw_started);
        if (custom_pixel_shader)
        {
          custom_pixel_shader_contents.shaders.push_back(*custom_pixel_shader);
          custom_pixel_texture_names.push_back(texture_names[i]);
        }
        custom_pixel_shader = std::nullopt;
      }
    }

    // Now the vertices can be flushed to the GPU. Everything following the CommitBuffer() call
    // must be careful to not upload any utility vertices, as the binding will be lost otherwise.
    const u32 num_indices = m_index_generator.GetIndexLen();
    if (num_indices == 0)
      return;

    // Texture loading can cause palettes to be applied (-> uniforms -> draws).
    // Palette application does not use vertices, only a full-screen quad, so this is okay.
    // Same with GPU texture decoding, which uses compute shaders.
    g_texture_cache->BindTextures(used_textures, samplers);

    if (!skip)
    {
      UpdatePipelineConfig();
      UpdatePipelineObject();
      if (m_current_pipeline_object)
      {
        const AbstractPipeline* pipeline_object = m_current_pipeline_object;
        if (!custom_pixel_shader_contents.shaders.empty())
        {
          if (const auto custom_pipeline =
                  GetCustomPipeline(custom_pixel_shader_contents, m_current_pipeline_config,
                                    m_current_uber_pipeline_config, m_current_pipeline_object))
          {
            pipeline_object = custom_pipeline;
          }
        }
        RenderDrawCall(pixel_shader_manager, geometry_shader_manager, custom_pixel_shader_contents,
                       custom_pixel_shader_uniforms, m_current_primitive_type, pipeline_object,
                       used_textures, samplers);
      }
    }

    // Even if we skip the draw, emulated state should still be impacted
    OnDraw();

    // The EFB cache is now potentially stale.
    g_framebuffer_manager->FlagPeekCacheAsOutOfDate();
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

    // The scene is about to become an unrelated one, and interpolating from where it was to where
    // the loaded state puts it would sweep everything across the screen for a frame.
    m_frame_recorder.Invalidate();
  }

  p.Do(m_zslope);
  p.Do(VertexLoaderManager::normal_cache);
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

void VertexManagerBase::CalculateNormals(NativeVertexFormat* format)
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

  if (vert_decl.normals[0].enable)
    return;

  VertexLoaderManager::normal_cache[3] = 0;
  if (vertex_shader_manager.constants.cached_normal != VertexLoaderManager::normal_cache)
  {
    vertex_shader_manager.constants.cached_normal = VertexLoaderManager::normal_cache;
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

  // Note: deliberately no recording invalidation here. This runs once per frame whether or not
  // anything actually changed, so dropping the recordings would leave nothing to interpolate
  // against, ever. The case that matters is the shader cache throwing away the pipelines a
  // recording points at, and ShaderCache::ClearCaches handles that itself.
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
  if (std::ranges::binary_search(m_scheduled_command_buffer_kicks, m_draw_counter))
  {
    // Kick a command buffer on the background thread.
    g_gfx->Flush();
    m_unflushed_efb_copy = false;
    m_last_efb_copy_draw_counter = m_draw_counter;
  }
}

void VertexManagerBase::OnCPUEFBAccess()
{
  // Check this isn't another access without any draws in between.
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
  m_frame_recorder.EndFrame();

  m_draw_counter = 0;
  m_last_efb_copy_draw_counter = 0;
  m_scheduled_command_buffer_kicks.clear();

  // If we have no CPU access at all, leave everything in the one command buffer for maximum
  // parallelism between CPU/GPU, at the cost of slightly higher latency.
  if (m_cpu_accesses_this_frame.empty())
    return;

  // In order to reduce CPU readback latency, we want to kick a command buffer roughly halfway
  // between the draw counters that invoked the readback, or every 250 draws, whichever is
  // smaller.
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

  m_cpu_accesses_this_frame.clear();

  // We invalidate the pipeline object at the start of the frame.
  // This is for the rare case where only a single pipeline configuration is used,
  // and hybrid ubershaders have compiled the specialized shader, but without any
  // state changes the specialized shader will not take over.
  InvalidatePipelineObject();
}

void VertexManagerBase::NotifyCustomShaderCacheOfHostChange(const ShaderHostConfig& host_config)
{
  m_custom_shader_cache->SetHostConfig(host_config);
  m_custom_shader_cache->Reload();
}

void VertexManagerBase::RenderDrawCall(
    PixelShaderManager& pixel_shader_manager, GeometryShaderManager& geometry_shader_manager,
    const CustomPixelShaderContents& custom_pixel_shader_contents,
    std::span<u8> custom_pixel_shader_uniforms, PrimitiveType primitive_type,
    const AbstractPipeline* current_pipeline, BitSet32 used_textures,
    const std::array<SamplerState, 8>& samplers)
{
  // Now we can upload uniforms, as nothing else will override them.
  geometry_shader_manager.SetConstants(primitive_type);
  pixel_shader_manager.SetConstants();
  if (!custom_pixel_shader_uniforms.empty() &&
      pixel_shader_manager.custom_constants.data() != custom_pixel_shader_uniforms.data())
  {
    pixel_shader_manager.custom_constants_dirty = true;
  }
  pixel_shader_manager.custom_constants = custom_pixel_shader_uniforms;
  UploadUniforms();

  // Capture the draw before CommitBuffer(), which is free to move the vertex data out from under
  // us, and after the constants above have been finalised for this draw.
  if (m_frame_recorder.IsRecording())
  {
    NativeVertexFormat* const vertex_format = VertexLoaderManager::GetCurrentVertexFormat();
    auto& vertex_shader_manager = Core::System::GetInstance().GetVertexShaderManager();

    // Where this batch's vertices start, worked out backwards from the write cursor rather than
    // taken from m_base_buffer_pointer. The two are only the same on the backends that hand out a
    // fresh allocation per batch: Vulkan and D3D12 set the base to the start of the whole streaming
    // buffer and the cursor to this batch's offset within it, so the base there addresses whatever
    // was written at offset zero instead of this draw. Everything of the batch is contiguous and
    // ends at the cursor, which makes this exact on every backend.
    const u32 vertex_count = m_index_generator.GetNumVerts();
    const u32 vertex_stride = vertex_format->GetVertexStride();
    const u8* const vertex_data = m_cur_buffer_pointer - vertex_count * vertex_stride;

    m_frame_recorder.RecordDraw(current_pipeline, vertex_format, primitive_type, vertex_data,
                                vertex_count, vertex_stride, m_index_generator.GetIndexDataStart(),
                                m_index_generator.GetIndexLen(), vertex_shader_manager.constants,
                                pixel_shader_manager.constants, geometry_shader_manager.constants,
                                g_texture_cache->GetBoundTextures(), samplers, used_textures);
  }

  g_gfx->SetPipeline(current_pipeline);

  u32 base_vertex, base_index;
  CommitBuffer(m_index_generator.GetNumVerts(),
               VertexLoaderManager::GetCurrentVertexFormat()->GetVertexStride(),
               m_index_generator.GetIndexLen(), &base_vertex, &base_index);

  if (g_backend_info.api_type != APIType::D3D && g_ActiveConfig.UseVSForLinePointExpand() &&
      (primitive_type == PrimitiveType::Points || primitive_type == PrimitiveType::Lines))
  {
    // VS point/line expansion puts the vertex id at gl_VertexID << 2
    // That means the base vertex has to be adjusted to match
    // (The shader adds this after shifting right on D3D, so no need to do this)
    base_vertex <<= 2;
  }

  if (PerfQueryBase::ShouldEmulate())
    g_perf_query->EnableQuery(bpmem.zcontrol.early_ztest ? PQG_ZCOMP_ZCOMPLOC : PQG_ZCOMP);

  DrawCurrentBatch(base_index, m_index_generator.GetIndexLen(), base_vertex);

  // Track the total emulated state draws
  INCSTAT(g_stats.this_frame.num_draw_calls);

  if (PerfQueryBase::ShouldEmulate())
    g_perf_query->DisableQuery(bpmem.zcontrol.early_ztest ? PQG_ZCOMP_ZCOMPLOC : PQG_ZCOMP);
}

bool VertexManagerBase::IsFrameGenerationUsable() const
{
  if (!g_ActiveConfig.frame_generation.bEnabled)
    return false;

  // OpenGL is not supported. Its streaming buffers are mapped through whatever is bound to
  // GL_ARRAY_BUFFER at the time, and ResetBuffer() only rebinds the vertex format when the one it
  // finds bound is invalid. A replay sets the recorded draw's pipeline immediately beforehand,
  // which leaves a valid but unrelated format bound, so the rebind is skipped and glMapBufferRange
  // returns nothing to write into. Making the replay hold to that invariant is what this needs
  // before it can be turned back on here.
  if (g_backend_info.api_type == APIType::OpenGL)
    return false;

  // Anti-aliasing gives the EFB more than one sample per pixel. The replay's own targets would
  // have to resolve rather than sample, and the recorded pipelines are compiled for the sample
  // count the EFB has, so there is no target that satisfies both.
  if (g_ActiveConfig.MultisamplingEnabled())
    return false;

  // Pixel shaders have the bounding box atomics baked into them whenever it is active, so
  // replaying them would push interpolated geometry into the emulated bounding box registers that
  // games read back. There is no way to mask that off per draw, so leave the feature alone here.
  if (g_ActiveConfig.bBBoxEnable && g_bounding_box && g_bounding_box->IsEnabled())
    return false;

  return true;
}

bool VertexManagerBase::CanReplayRecordedFrame() const
{
  return IsFrameGenerationUsable() && m_frame_recorder.CanInterpolate();
}

void VertexManagerBase::ReplayRecordedFrame(AbstractFramebuffer* target, float phase)
{
  if (target == nullptr || !CanReplayRecordedFrame())
    return;

  const VideoCommon::RecordedFrame& current = m_frame_recorder.GetCurrentFrame();
  const VideoCommon::RecordedFrame& previous = m_frame_recorder.GetPreviousFrame();

  // Which draw in the previous recording each of this frame's draws is, and the motion the scene
  // shares between the two. Both are worked out once per pair of recordings, since the same pair
  // is replayed several times over.
  EnsureReplayCorrespondence(current, previous, m_frame_recorder.GetFrameCounter());

  // Make sure the emulated batch is on its way before the stream buffers get used for the replay,
  // as UploadUtilityVertices() would otherwise trample the vertex loader's mapped pointer.
  //
  // Note that this deliberately does not use AbstractGfx::BeginUtilityDrawing(): on OpenGL that
  // turns off the two clip distances the emulated vertex shaders write for depth clamping, and
  // the recorded pipelines are emulated ones.
  Flush();

  auto& system = Core::System::GetInstance();
  auto& vertex_shader_manager = system.GetVertexShaderManager();
  auto& pixel_shader_manager = system.GetPixelShaderManager();
  auto& geometry_shader_manager = system.GetGeometryShaderManager();

  // The shader managers only rewrite the parts of their constants that the emulated GPU reported
  // as changed, so anything overwritten here that the game does not happen to upload again next
  // frame would keep the interpolated value forever. Put the originals back afterwards.
  const VertexShaderConstants saved_vertex_constants = vertex_shader_manager.constants;
  const PixelShaderConstants saved_pixel_constants = pixel_shader_manager.constants;
  const GeometryShaderConstants saved_geometry_constants = geometry_shader_manager.constants;

  g_gfx->SetFramebuffer(target);

  // Every generated frame starts from a blank target, whether or not the emulated GPU cleared.
  //
  // This is not optional the way it is for the real frame. Several are drawn one after another into
  // the same target, so without it each one composites onto the last: the depth buffer would still
  // hold the previous one's geometry and reject most of this one's, while anything drawn without
  // depth testing, such as the 2D overlays, would pass every time and pile up. The whole target is
  // cleared rather than any recorded rectangle, since anything outside it is equally stale.
  //
  // A depth of all ones is the far plane, which is what an untouched depth buffer must read as for
  // the first geometry drawn against it to pass.
  g_gfx->ClearRegion(target->GetRect(), true, true, true, 0, 0xFFFFFF);
  g_gfx->SetFramebuffer(target);

  // The whole frame is replayed, in order, passes and all. The emulated GPU builds the visible
  // image out of several of them, and a pass that renders into a texture is as much a part of what
  // the scene looks like at an instant as the pass that draws it.
  for (const VideoCommon::RecordedCommand& command : current.commands)
  {
    switch (command.type)
    {
    case VideoCommon::RecordedCommandType::Clear:
      ReplayClear(target, current.clears[command.index]);
      break;

    case VideoCommon::RecordedCommandType::Draw:
      ReplayDraw(current, previous, command.index, phase);
      break;

    case VideoCommon::RecordedCommandType::Copy:
      ReplayCopy(target, current, command.index);
      break;
    }
  }

  // Only now that every draw has been submitted is it safe to put the emulated constants back:
  // Metal reads them when the draw is actually encoded rather than when UploadUniforms() runs.
  vertex_shader_manager.constants = saved_vertex_constants;
  pixel_shader_manager.constants = saved_pixel_constants;
  geometry_shader_manager.constants = saved_geometry_constants;
  InvalidateConstants();

  // The replay has left its own pipeline bound, so make the next emulated draw set that up again.
  // The textures look after themselves, as every flush rebinds the units it uses.
  //
  // InvalidatePipelineObject() rather than just clearing the pointer: the pointer alone leaves
  // m_pipeline_config_changed false, so UpdatePipelineObject() returns without doing anything and
  // the next flush finds a null pipeline and drops the draw.
  InvalidatePipelineObject();
  SetRasterizationStateChanged();
  SetDepthStateChanged();
  SetBlendingStateChanged();
}

void VertexManagerBase::ReplayClear(AbstractFramebuffer* target,
                                    const VideoCommon::RecordedClear& clear)
{
  // The coordinate conversion and the alpha rules for formats without an alpha channel belong to
  // the framebuffer manager, so that a replayed clear lands where the emulated one did on every
  // backend rather than tracking a copy of the same rules.
  g_framebuffer_manager->ClearFramebuffer(target, clear.rect, clear.color_enable,
                                          clear.alpha_enable, clear.z_enable, clear.color, clear.z,
                                          clear.pixel_format);

  // ClearRegion() finishes by putting the EFB back, which is not what is being drawn into here.
  g_gfx->SetFramebuffer(target);
}

void VertexManagerBase::ReplayCopy(AbstractFramebuffer* target,
                                   const VideoCommon::RecordedFrame& current, u32 copy_index)
{
  const VideoCommon::RecordedCopy& copy = current.copies[copy_index];

  AbstractFramebuffer* const destination =
      m_replay_targets.GetFramebuffer(copy_index, copy.destination_config);
  if (destination == nullptr)
    return;

  // The copy shader reads the EFB in normalised coordinates, and a replay renders into a target
  // the same size as the EFB, so the recorded uniforms address this one exactly as they addressed
  // the real one.
  AbstractTexture* const source =
      copy.from_depth ? target->GetDepthAttachment() : target->GetColorAttachment();
  if (source == nullptr)
    return;

  g_gfx->BeginUtilityDrawing();
  source->FinishedRendering();

  UploadUtilityUniforms(current.copy_uniform_data.data() + copy.uniform_offset, copy.uniform_size);

  // The same draw the texture cache makes for the real copy, so the two cannot drift apart.
  TextureCacheBase::DrawEFBCopy(destination, source, copy.pipeline, copy.linear_filter);

  // EndUtilityDrawing() puts the EFB back, and the rest of the frame goes on being drawn here.
  g_gfx->SetFramebuffer(target);
}

void VertexManagerBase::BindReplayTextures(const VideoCommon::RecordedDraw& draw)
{
  for (u32 unit = 0; unit < VideoCommon::NUM_RECORDED_TEXTURES; ++unit)
  {
    // Anything the frame rendered for itself is sampled from the replay's own copy of it, which
    // holds the scene at this sub-frame rather than at the last real one.
    if (const u32 slot = draw.texture_copy_slots[unit]; slot != 0)
    {
      if (AbstractTexture* const replayed = m_replay_targets.GetTexture(slot - 1))
      {
        g_gfx->SetTexture(unit, replayed);
        g_gfx->SetSamplerState(unit, draw.samplers[unit]);
        continue;
      }

      // No replay copy, because the target could not be made or the copy came in over the limit.
      // The emulated GPU's own copy is a frame stale but is a picture of the right thing.
    }

    // The cache can move an entry's texture into its reuse pool while the entry itself is still
    // alive, so this has to be checked rather than assumed.
    if (!draw.textures[unit] || !draw.textures[unit]->texture)
      continue;

    g_gfx->SetTexture(unit, draw.textures[unit]->texture.get());
    g_gfx->SetSamplerState(unit, draw.samplers[unit]);
  }
}

void VertexManagerBase::ResolveCorrespondence(const VideoCommon::RecordedFrame& current,
                                              const VideoCommon::RecordedFrame& previous)
{
  m_replay_matches.assign(current.draws.size(), nullptr);

  // Which of the previous frame's draws have already been spoken for. A draw is one object at one
  // instant, so it can be the counterpart of at most one draw in this frame: letting several claim
  // it leaves whichever of them the search happened to reach first with the right partner and the
  // rest interpolating against something that is somewhere else entirely. With N instances of a
  // mesh against N-1 of them last frame, at least one pairing is wrong however the search is
  // ordered, and which one changes from frame to frame.
  m_replay_claimed.assign(previous.draws.size(), false);

  // How far the previous frame's draw list is offset from this one's, carried across the whole
  // list and nudged whenever a match is found off it. A game that adds or drops a single draw
  // shifts everything after it by one, and pairing on the index alone would then fail for the
  // whole rest of the scene at once.
  s64 alignment = 0;

  for (u32 draw_index = 0; draw_index < current.draws.size(); ++draw_index)
  {
    const VideoCommon::RecordedDraw& draw = current.draws[draw_index];
    const VertexShaderConstants& draw_constants = current.vertex_constants[draw.vertex_constants];

    // How far this draw's transform is allowed to be from a candidate's before the two are taken
    // to be different objects rather than one object that moved.
    //
    // Relative to the size of the transform itself, never an absolute number: a position matrix
    // carries the object's distance from the camera in its translation column, and games work in
    // world scales that differ by orders of magnitude, so a threshold that suits one scene rejects
    // everything or nothing in the next. Against the magnitude it asks whether the object moved a
    // lot *for what it is*, which means the same thing everywhere.
    const float limit = CORRESPONDENCE_MAX_RELATIVE_DISTANCE *
                        std::max(VideoCommon::TransformMagnitude(draw_constants), 1.0f);

    const VideoCommon::RecordedDraw* match = nullptr;
    s64 match_candidate = 0;
    float match_distance = std::numeric_limits<float>::max();
    bool match_same_geometry = false;
    s64 match_step = 0;

    // The furthest any candidate got before being turned away, so that a draw which fails can say
    // which test failed it rather than only that it failed.
    bool saw_candidate = false;
    bool saw_same_shape = false;
    bool saw_same_texture = false;

    // Walked outwards from the running alignment, nearest in the list first, and a candidate only
    // wins on a strictly smaller transform distance. That way position decides between instances
    // of the same mesh, and where it cannot decide, the list does.
    //
    // It cannot decide for the 2D overlays: a game draws those through one orthographic transform
    // with their screen position baked into the vertices, so every one of them sits at the same
    // distance. Without the list breaking the tie they pair up arbitrarily and smear across the
    // screen.
    for (s64 offset = 0; offset <= CORRESPONDENCE_SEARCH_DISTANCE; ++offset)
    {
      for (s64 direction = 0; direction < (offset == 0 ? 1 : 2); ++direction)
      {
        const s64 step = (direction == 0) ? -offset : offset;
        const s64 candidate = static_cast<s64>(draw_index) + alignment + step;
        if (candidate < 0 || candidate >= static_cast<s64>(previous.draws.size()))
          continue;

        if (m_replay_claimed[candidate])
          continue;

        const VideoCommon::RecordedDraw& previous_draw = previous.draws[candidate];
        saw_candidate = true;

        const VideoCommon::DrawCorrespondence correspondence =
            VideoCommon::CompareDraws(previous_draw, draw);
        if (correspondence != VideoCommon::DrawCorrespondence::Yes)
        {
          if (correspondence == VideoCommon::DrawCorrespondence::DifferentTexture)
            saw_same_shape = true;
          continue;
        }
        saw_same_shape = true;
        saw_same_texture = true;

        const float distance = VideoCommon::TransformDistance(
            previous.vertex_constants[previous_draw.vertex_constants], draw_constants);

        // Too far apart to be the same object. Without this the search is an unconditional argmin
        // over the window: DrawsCorrespond only compares the pipeline, the vertex format, the
        // counts and the textures, all of which any run of instanced content satisfies, so the
        // nearest tree, particle or glyph quad is always accepted however far away it is. A wrong
        // pairing does not merely slide the object, it blends the projection and texture matrices
        // of two unrelated draws, so it streaks and distorts as well.
        if (distance > limit)
          continue;

        // The same vertices settle it, ahead of what the transforms have to say, but only among
        // candidates that already passed the test above.
        //
        // A skinned character is the case that needs this. Its position comes from per-vertex
        // indices into the bone matrices rather than from the shared position matrix, so the
        // shared one holds whatever it last happened to hold and is the same for every skinned
        // draw in the frame: the transform distance is blind to them and scores every candidate
        // alike. Their meshes are not alike, and do not change from frame to frame, so this tells
        // them apart exactly.
        //
        // Never a key in its own right. Any mesh a game does not rewrite in RAM hashes the same
        // every frame, so a shared quad buffer or a glyph atlas makes this true for long runs of
        // unrelated draws; as a tie-break inside the distance limit that is harmless, as an
        // override of it, it would let a draw eight slots away beat the one sitting at the
        // alignment.
        const bool same_geometry = previous_draw.vertex_hash == draw.vertex_hash;

        if (match != nullptr)
        {
          if (match_same_geometry && !same_geometry)
            continue;

          if (match_same_geometry == same_geometry && distance >= match_distance)
            continue;
        }

        match = &previous_draw;
        match_candidate = candidate;
        match_distance = distance;
        match_same_geometry = same_geometry;
        match_step = step;
      }
    }

    if (match != nullptr)
    {
      m_replay_claimed[match_candidate] = true;
      alignment += match_step;
      INCSTAT(g_stats.this_frame.num_framegen_matched);
    }
    else if (saw_same_texture)
    {
      // Something in the window was the same draw in every respect except where it stands.
      INCSTAT(g_stats.this_frame.num_framegen_too_far);
    }
    else if (saw_same_shape)
    {
      INCSTAT(g_stats.this_frame.num_framegen_no_texture);
    }
    else if (saw_candidate)
    {
      INCSTAT(g_stats.this_frame.num_framegen_no_pipeline);
    }
    else
    {
      INCSTAT(g_stats.this_frame.num_framegen_no_candidate);
    }

    m_replay_matches[draw_index] = match;
  }
}

void VertexManagerBase::EnsureReplayCorrespondence(const VideoCommon::RecordedFrame& current,
                                                   const VideoCommon::RecordedFrame& previous,
                                                   u64 frame_counter)
{
  if (m_replay_counter == frame_counter && m_replay_matches.size() == current.draws.size())
    return;

  m_replay_counter = frame_counter;
  ResolveCorrespondence(current, previous);
}

void VertexManagerBase::SubmitRecordedDraw(const VideoCommon::RecordedFrame& frame,
                                           const VideoCommon::RecordedDraw& draw,
                                           const VertexShaderConstants& vertex_constants)
{
  auto& system = Core::System::GetInstance();
  auto& vertex_shader_manager = system.GetVertexShaderManager();
  auto& pixel_shader_manager = system.GetPixelShaderManager();
  auto& geometry_shader_manager = system.GetGeometryShaderManager();

  vertex_shader_manager.constants = vertex_constants;
  vertex_shader_manager.dirty = true;

  pixel_shader_manager.constants = frame.pixel_constants[draw.pixel_constants];
  pixel_shader_manager.dirty = true;
  geometry_shader_manager.constants = frame.geometry_constants[draw.geometry_constants];
  geometry_shader_manager.dirty = true;
  UploadUniforms();

  // Bind the textures the draw was recorded with directly. Going through the texture cache
  // would bind whatever is bound now and would write to the emulated pixel shader constants.
  BindReplayTextures(draw);

  const auto& viewport = draw.viewport_and_scissor;
  g_gfx->SetViewport(viewport.viewport_x, viewport.viewport_y, viewport.viewport_width,
                     viewport.viewport_height, viewport.viewport_near_depth,
                     viewport.viewport_far_depth);
  g_gfx->SetScissorRect(viewport.scissor_rect);

  // The pipeline has to be set before the vertices are uploaded, as on OpenGL it is what binds
  // the vertex array object that owns the index buffer binding.
  g_gfx->SetPipeline(draw.pipeline);

  // Nothing may touch the stream buffers between this upload and the draw that consumes it, or
  // the base offsets stop addressing the data that was just written.
  u32 base_vertex = 0;
  u32 base_index = 0;
  UploadUtilityVertices(frame.vertex_data.data() + draw.vertex_data_offset, draw.vertex_stride,
                        draw.vertex_count, frame.index_data.data() + draw.index_data_offset,
                        draw.index_count, &base_vertex, &base_index);

  if (g_backend_info.api_type != APIType::D3D && g_ActiveConfig.UseVSForLinePointExpand() &&
      (draw.primitive_type == PrimitiveType::Points || draw.primitive_type == PrimitiveType::Lines))
  {
    base_vertex <<= 2;
  }

  // Deliberately not DrawCurrentBatch(), which would flush the bounding box, and deliberately
  // not wrapped in a performance query, which would count these pixels as emulated ones.
  g_gfx->DrawIndexed(base_index, draw.index_count, base_vertex);
}

void VertexManagerBase::ReplayDraw(const VideoCommon::RecordedFrame& current,
                                   const VideoCommon::RecordedFrame& previous, u32 draw_index,
                                   float phase)
{
  const VideoCommon::RecordedDraw& draw = current.draws[draw_index];
  const VertexShaderConstants& draw_constants = current.vertex_constants[draw.vertex_constants];
  const VideoCommon::RecordedDraw* const match = m_replay_matches[draw_index];

  if (match == nullptr)
  {
    // Nothing corresponded, so this draw is submitted exactly as this frame submitted it. That
    // leaves it a whole frame ahead of the interpolated scene around it, which is a real cost, but
    // it is a bounded and steady one: the draw is where the game actually put it, and it stays
    // there for every sub-frame rather than moving or changing strength between them.
    //
    // Attempts at doing better than this are recorded in FRAMEGEN_AUDIT.md. Every one of them
    // needed either a per-draw opacity, which the game's own pipelines cannot express and no
    // per-pixel mask is available to fake, or the previous recording's draws replayed alongside
    // this one's, which puts two copies of anything translucent into the same picture. Both traded
    // a steady error for an unsteady one. The way to make this better is to leave fewer draws
    // unmatched, not to dress up the ones that are.
    SubmitRecordedDraw(current, draw, draw_constants);
    return;
  }

  VertexShaderConstants interpolated;
  VideoCommon::InterpolateTransforms(previous.vertex_constants[match->vertex_constants],
                                     draw_constants, phase, &interpolated);
  SubmitRecordedDraw(current, draw, interpolated);
}

const AbstractPipeline* VertexManagerBase::GetCustomPipeline(
    const CustomPixelShaderContents& custom_pixel_shader_contents,
    const VideoCommon::GXPipelineUid& current_pipeline_config,
    const VideoCommon::GXUberPipelineUid& current_uber_pipeline_config,
    const AbstractPipeline* current_pipeline) const
{
  if (current_pipeline)
  {
    if (!custom_pixel_shader_contents.shaders.empty())
    {
      CustomShaderInstance custom_shaders;
      custom_shaders.pixel_contents = custom_pixel_shader_contents;
      switch (g_ActiveConfig.iShaderCompilationMode)
      {
      case ShaderCompilationMode::Synchronous:
      case ShaderCompilationMode::AsynchronousSkipRendering:
      {
        if (auto pipeline = m_custom_shader_cache->GetPipelineAsync(
                current_pipeline_config, custom_shaders, current_pipeline->m_config))
        {
          return *pipeline;
        }
      }
      break;
      case ShaderCompilationMode::SynchronousUberShaders:
      {
        // D3D has issues compiling large custom ubershaders
        // use specialized shaders instead
        if (g_backend_info.api_type == APIType::D3D)
        {
          if (auto pipeline = m_custom_shader_cache->GetPipelineAsync(
                  current_pipeline_config, custom_shaders, current_pipeline->m_config))
          {
            return *pipeline;
          }
        }
        else
        {
          if (auto pipeline = m_custom_shader_cache->GetPipelineAsync(
                  current_uber_pipeline_config, custom_shaders, current_pipeline->m_config))
          {
            return *pipeline;
          }
        }
      }
      break;
      case ShaderCompilationMode::AsynchronousUberShaders:
      {
        if (auto pipeline = m_custom_shader_cache->GetPipelineAsync(
                current_pipeline_config, custom_shaders, current_pipeline->m_config))
        {
          return *pipeline;
        }
        else if (auto uber_pipeline = m_custom_shader_cache->GetPipelineAsync(
                     current_uber_pipeline_config, custom_shaders, current_pipeline->m_config))
        {
          return *uber_pipeline;
        }
      }
      break;
      };
    }
  }

  return nullptr;
}
