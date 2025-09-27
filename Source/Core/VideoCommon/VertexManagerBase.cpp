// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/VertexManagerBase.h"

#include <array>
#include <cmath>
#include <memory>
#include <variant>

#include <xxhash.h>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Contains.h"
#include "Common/EnumMap.h"
#include "Common/EnumUtils.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Common/SmallVector.h"
#include "Common/VariantUtil.h"

#include "Core/DolphinAnalytics.h"
#include "Core/HW/SystemTimers.h"
#include "Core/System.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/GraphicsModSystem/Runtime/CameraManager.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModManager.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PerfQueryBase.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Resources/MeshResource.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/TextureInfo.h"
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
  m_frame_end_event =
      AfterFrameEvent::Register([this](Core::System&) { OnEndFrame(); }, "VertexManagerBase");
  m_after_present_event = AfterPresentEvent::Register(
      [this](const PresentInfo& pi) { m_ticks_elapsed = pi.emulated_timestamp; },
      "VertexManagerBase");

  m_index_generator.Init(false);
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

  if (g_ActiveConfig.bGraphicMods)
  {
    // Tell any graphics mod backend we had more indices
    auto& system = Core::System::GetInstance();
    auto& mod_manager = system.GetGraphicsModManager();
    mod_manager.GetBackend().AddIndices(primitive, num_vertices);
  }
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
    if (g_ActiveConfig.bGraphicMods)
    {
      auto& system = Core::System::GetInstance();
      auto& mod_manager = system.GetGraphicsModManager();
      mod_manager.GetBackend().ResetIndices();
    }

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
  m_last_reset_pointer = m_cur_buffer_pointer;
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
  auto& vertex_shader_manager = system.GetVertexShaderManager();
  auto& xf_state_manager = system.GetXFStateManager();

  const auto used_textures = UsedTextures();

  CalculateNormals(VertexLoaderManager::GetCurrentVertexFormat());
  Common::SmallVector<GraphicsModSystem::TextureView, 8> textures;
  std::array<SamplerState, 8> samplers;
  if (!m_cull_all)
  {
    for (const u32 i : used_textures)
    {
      const auto cache_entry = g_texture_cache->Load(TextureInfo::FromStage(i));
      if (!cache_entry)
        continue;
      const float custom_tex_scale = cache_entry->GetWidth() / float(cache_entry->native_width);
      samplers[i] = TextureCacheBase::GetSamplerState(
          i, custom_tex_scale, cache_entry->is_custom_tex, cache_entry->has_arbitrary_mips);

      if (g_ActiveConfig.bGraphicMods)
      {
        GraphicsModSystem::TextureView texture;
        texture.hash_name = cache_entry->texture_info_name;
        texture.texture_data = cache_entry->texture.get();
        if (cache_entry->is_efb_copy)
        {
          texture.texture_type = GraphicsModSystem::EFB;
        }
        else if (cache_entry->is_xfb_copy)
        {
          texture.texture_type = GraphicsModSystem::XFB;
        }
        else
        {
          texture.texture_type = GraphicsModSystem::Normal;
        }
        texture.unit = i;
        textures.push_back(std::move(texture));
      }
    }
  }

  /*Common::SmallVector<std::pair<GraphicsMods::LightID, int>, 8> lights_this_draw;
    if (g_ActiveConfig.bGraphicMods)
    {
      // Add any lights
      const auto& lights_changed = xf_state_manager.GetLightsChanged();
      if (lights_changed[0] >= 0)
      {
        const int istart = lights_changed[0] / 0x10;
        const int iend = (lights_changed[1] + 15) / 0x10;

        for (int i = istart; i < iend; ++i)
        {
          const Light& light = xfmem.lights[i];

          VertexShaderConstants::Light dstlight;

          // xfmem.light.color is packed as abgr in u8[4], so we have to swap the order
          dstlight.color[0] = light.color[3];
          dstlight.color[1] = light.color[2];
          dstlight.color[2] = light.color[1];
          dstlight.color[3] = light.color[0];

          dstlight.cosatt[0] = light.cosatt[0];
          dstlight.cosatt[1] = light.cosatt[1];
          dstlight.cosatt[2] = light.cosatt[2];

          if (fabs(light.distatt[0]) < 0.00001f && fabs(light.distatt[1]) < 0.00001f &&
              fabs(light.distatt[2]) < 0.00001f)
          {
            // dist attenuation, make sure not equal to 0!!!
            dstlight.distatt[0] = .00001f;
          }
          else
          {
            dstlight.distatt[0] = light.distatt[0];
          }
          dstlight.distatt[1] = light.distatt[1];
          dstlight.distatt[2] = light.distatt[2];

          dstlight.pos[0] = light.dpos[0];
          dstlight.pos[1] = light.dpos[1];
          dstlight.pos[2] = light.dpos[2];

          // TODO: Hardware testing is needed to confirm that this normalization is correct
          auto sanitize = [](float f) {
            if (std::isnan(f))
              return 0.0f;
            else if (std::isinf(f))
              return f > 0.0f ? 1.0f : -1.0f;
            else
              return f;
          };
          double norm = double(light.ddir[0]) * double(light.ddir[0]) +
                        double(light.ddir[1]) * double(light.ddir[1]) +
                        double(light.ddir[2]) * double(light.ddir[2]);
          norm = 1.0 / sqrt(norm);
          dstlight.dir[0] = sanitize(static_cast<float>(light.ddir[0] * norm));
          dstlight.dir[1] = sanitize(static_cast<float>(light.ddir[1] * norm));
          dstlight.dir[2] = sanitize(static_cast<float>(light.ddir[2] * norm));

          XXH3_64bits_reset_withSeed(m_hash_state_impl->m_graphics_mod_light_hash_state,
                                     m_hash_state_impl->m_graphics_mod_hash_seed);
          // XXH3_64bits_update(m_hash_state_impl->m_graphics_mod_light_hash_state, &dstlight.color,
          // sizeof(int4));
          XXH3_64bits_update(m_hash_state_impl->m_graphics_mod_light_hash_state, &dstlight.cosatt,
                             sizeof(float4));
          XXH3_64bits_update(m_hash_state_impl->m_graphics_mod_light_hash_state, &dstlight.distatt,
                             sizeof(float4));
          // XXH3_64bits_update(m_hash_state_impl->m_graphics_mod_light_hash_state, &dstlight.dir,
          // sizeof(float4));
          const auto light_id = GraphicsMods::LightID(
              XXH3_64bits_digest(m_hash_state_impl->m_graphics_mod_light_hash_state));
          lights_this_draw.emplace_back(light_id, i);

          if (editor.IsEnabled())
          {
            GraphicsModEditor::LightData light_data;
            light_data.m_id = light_id;
            light_data.m_create_time = std::chrono::steady_clock::now();
            light_data.m_color = dstlight.color;
            light_data.m_cosatt = dstlight.cosatt;
            light_data.m_dir = dstlight.dir;
            light_data.m_distatt = dstlight.distatt;
            light_data.m_pos = dstlight.pos;
            editor.AddLightData(std::move(light_data));
          }
        }
      }
    }*/
  vertex_shader_manager.SetConstants(xf_state_manager);
  /*if (g_ActiveConfig.bGraphicMods)
  {
    if (editor.IsEnabled())
    {
      for (const auto& light_this_draw : lights_this_draw)
      {
        auto& light = vertex_shader_manager.constants.lights[light_this_draw.second];
        const auto light_id = light_this_draw.first;
        bool skip = false;
        GraphicsModActionData::Light light_action_data{&light.color, &light.cosatt, &light.distatt,
                                                       &light.pos,   &light.dir,    &skip};
        for (const auto& action : editor.GetLightActions(light_id))
        {
          action->OnLight(&light_action_data);
        }
        if (skip)
        {
          light.color = {};
          light.cosatt = {};
          light.distatt = {};
          light.pos = {};
          light.dir = {};
        }
      }
    }
  }*/
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
    // Now the vertices can be flushed to the GPU. Everything following the CommitBuffer() call
    // must be careful to not upload any utility vertices, as the binding will be lost otherwise.
    const u32 num_indices = m_index_generator.GetIndexLen();
    if (num_indices == 0)
      return;

    // Texture loading can cause palettes to be applied (-> uniforms -> draws).
    // Palette application does not use vertices, only a full-screen quad, so this is okay.
    // Same with GPU texture decoding, which uses compute shaders.
    g_texture_cache->BindTextures(used_textures, samplers);

    UpdatePipelineConfig();

    if (g_ActiveConfig.bGraphicMods)
    {
      GraphicsModSystem::DrawDataView draw_data;
      draw_data.index_data = {m_index_generator.GetIndexDataStart(),
                              m_index_generator.GetIndexLen()};
      draw_data.projection_type = xfmem.projection.type;
      draw_data.uid = &m_current_pipeline_config;
      draw_data.vertex_data = {m_last_reset_pointer, m_index_generator.GetNumVerts()};
      draw_data.textures = std::move(textures);
      draw_data.samplers = std::move(samplers);
      draw_data.vertex_format = VertexLoaderManager::GetCurrentVertexFormat();
      draw_data.gpu_skinning_position_transform = vertex_shader_manager.constants.transformmatrices;
      draw_data.gpu_skinning_normal_transform = vertex_shader_manager.constants.normalmatrices;

      auto& mod_manager = system.GetGraphicsModManager();
      mod_manager.GetBackend().OnDraw(draw_data, this);
    }
    else
    {
      static VideoCommon::CameraManager dummy_manager;
      DrawEmulatedMesh(dummy_manager);
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
  m_index_generator.Init(false);
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

void VertexManagerBase::ClearAdditionalCameras(const MathUtil::Rectangle<int>& rc,
                                               bool color_enable, bool alpha_enable, bool z_enable,
                                               u32 color, u32 z)
{
  if (g_ActiveConfig.bGraphicMods)
  {
    auto& system = Core::System::GetInstance();
    auto& mod_manager = system.GetGraphicsModManager();
    mod_manager.GetBackend().ClearAdditionalCameras(rc, color_enable, alpha_enable, z_enable, color,
                                                    z);
  }
}

void VertexManagerBase::DrawEmulatedMesh(VideoCommon::CameraManager& camera_manager,
                                         const Common::Matrix44& custom_transform)
{
  auto& system = Core::System::GetInstance();
  auto& geometry_shader_manager = system.GetGeometryShaderManager();
  auto& pixel_shader_manager = system.GetPixelShaderManager();
  auto& vertex_shader_manager = system.GetVertexShaderManager();

  UpdatePipelineObject();

  memcpy(vertex_shader_manager.constants.custom_transform.data(), custom_transform.data.data(),
         4 * sizeof(float4));

  const auto camera_view = camera_manager.GetCurrentCameraView();

  if (camera_view.transform)
  {
    const u64 camera_id = Common::ToUnderlying<>(camera_view.id);
    if (m_last_camera_id != camera_id)
    {
      vertex_shader_manager.ForceProjectionMatrixUpdate(system.GetXFStateManager(),
                                                        *camera_view.transform);
      m_last_camera_id = camera_id;
    }
  }
  else
  {
    if (m_last_camera_id)
    {
      vertex_shader_manager.ForceProjectionMatrixUpdate(system.GetXFStateManager(),
                                                        Common::Matrix44::Identity());
    }
    m_last_camera_id.reset();
  }

  geometry_shader_manager.SetConstants(m_current_primitive_type);
  pixel_shader_manager.SetConstants();
  UploadUniforms();

  g_gfx->SetPipeline(m_current_pipeline_object);

  u32 base_vertex, base_index;
  CommitBuffer(m_index_generator.GetNumVerts(),
               VertexLoaderManager::GetCurrentVertexFormat()->GetVertexStride(),
               m_index_generator.GetIndexLen(), &base_vertex, &base_index);

  if (g_backend_info.api_type != APIType::D3D && g_ActiveConfig.UseVSForLinePointExpand() &&
      (m_current_primitive_type == PrimitiveType::Points ||
       m_current_primitive_type == PrimitiveType::Lines))
  {
    // VS point/line expansion puts the vertex id at gl_VertexID << 2
    // That means the base vertex has to be adjusted to match
    // (The shader adds this after shifting right on D3D, so no need to do this)
    base_vertex <<= 2;
  }

  DrawCurrentBatch(base_index, m_index_generator.GetIndexLen(), base_vertex);

  AbstractFramebuffer* frame_buffer_to_restore = nullptr;

  // Do we have any other views?  If so we need to redraw with those
  // frame buffers...
  const auto camera_views = camera_manager.GetAdditionalViews();
  for (const auto additional_camera_view : camera_views)
  {
    if (xfmem.projection.type == ProjectionType::Orthographic &&
        additional_camera_view.skip_orthographic)
    {
      continue;
    }
    frame_buffer_to_restore = g_gfx->GetCurrentFramebuffer();
    g_gfx->SetFramebuffer(additional_camera_view.framebuffer);
    if (additional_camera_view.transform)
    {
      const u64 camera_id = Common::ToUnderlying<>(additional_camera_view.id);
      if (m_last_camera_id != camera_id)
      {
        vertex_shader_manager.ForceProjectionMatrixUpdate(system.GetXFStateManager(),
                                                          *additional_camera_view.transform);
        m_last_camera_id = camera_id;
      }
    }
    else
    {
      if (m_last_camera_id)
      {
        vertex_shader_manager.ForceProjectionMatrixUpdate(system.GetXFStateManager(),
                                                          Common::Matrix44::Identity());
      }
      m_last_camera_id.reset();
    }

    UploadUniforms();
    DrawCurrentBatch(base_index, m_index_generator.GetIndexLen(), base_vertex);
  }

  if (frame_buffer_to_restore)
  {
    g_gfx->SetFramebuffer(frame_buffer_to_restore);
  }
}

void VertexManagerBase::DrawEmulatedMesh(const VideoCommon::MaterialResource::Data& material_data,
                                         const GraphicsModSystem::DrawDataView& draw_data,
                                         const Common::Matrix44& custom_transform,
                                         VideoCommon::CameraManager& camera_manager)
{
  auto& system = Core::System::GetInstance();
  auto& vertex_shader_manager = system.GetVertexShaderManager();
  memcpy(vertex_shader_manager.constants.custom_transform.data(), custom_transform.data.data(),
         4 * sizeof(float4));

  u32 base_vertex, base_index;
  CommitBuffer(m_index_generator.GetNumVerts(),
               VertexLoaderManager::GetCurrentVertexFormat()->GetVertexStride(),
               m_index_generator.GetIndexLen(), &base_vertex, &base_index);

  if (g_backend_info.api_type != APIType::D3D && g_ActiveConfig.UseVSForLinePointExpand() &&
      (m_current_primitive_type == PrimitiveType::Points ||
       m_current_primitive_type == PrimitiveType::Lines))
  {
    // VS point/line expansion puts the vertex id at gl_VertexID << 2
    // That means the base vertex has to be adjusted to match
    // (The shader adds this after shifting right on D3D, so no need to do this)
    base_vertex <<= 2;
  }

  DrawViewsWithMaterial(base_vertex, base_index, m_index_generator.GetIndexLen(),
                        m_current_primitive_type, draw_data, material_data, camera_manager);
}

void VertexManagerBase::DrawCustomMesh(GraphicsModSystem::DrawCallID draw_call,
                                       const VideoCommon::MeshResource::Data& mesh_data,
                                       const GraphicsModSystem::DrawDataView& draw_data,
                                       const Common::Matrix44& custom_transform,
                                       bool ignore_mesh_transform,
                                       VideoCommon::CameraManager& camera_manager)
{
  auto& system = Core::System::GetInstance();
  auto& vertex_shader_manager = system.GetVertexShaderManager();

  const auto process_mesh_chunk = [&](const VideoCommon::MeshResource::MeshChunk& mesh_chunk) {
    // TODO: draw with a generic material?
    if (!mesh_chunk.GetMaterial()) [[unlikely]]
      return;

    const auto material_data = mesh_chunk.GetMaterial()->GetData();
    if (!material_data) [[unlikely]]
      return;

    const auto pipeline = material_data->GetPipeline();
    if (!pipeline->m_config.vertex_shader || !pipeline->m_config.pixel_shader) [[unlikely]]
      return;

    const auto vertex_data = mesh_chunk.GetVertexData();
    const auto index_data = mesh_chunk.GetIndexData();
    if (vertex_data.empty() || index_data.empty()) [[unlikely]]
      return;

    vertex_shader_manager.SetVertexFormat(
        mesh_chunk.GetComponentsAvailable(),
        mesh_chunk.GetNativeVertexFormat()->GetVertexDeclaration());

    Common::Matrix44 computed_transform;
    computed_transform = Common::Matrix44::Translate(mesh_chunk.GetPivotPoint()) * custom_transform;
    if (!ignore_mesh_transform)
    {
      computed_transform *= mesh_chunk.GetTransform();
    }
    memcpy(vertex_shader_manager.constants.custom_transform.data(), computed_transform.data.data(),
           4 * sizeof(float4));

    u32 base_vertex, base_index;
    UploadUtilityVertices(vertex_data.data(), mesh_chunk.GetVertexStride(),
                          static_cast<u32>(vertex_data.size()), index_data.data(),
                          static_cast<u32>(index_data.size()), &base_vertex, &base_index);

    DrawViewsWithMaterial(base_vertex, base_index, static_cast<u32>(index_data.size()),
                          mesh_chunk.GetPrimitiveType(), draw_data, *material_data, camera_manager);
  };

  for (const auto& mesh_chunk : mesh_data.GetMeshChunks(draw_call))
  {
    process_mesh_chunk(mesh_chunk);
  }
}

void VertexManagerBase::DrawViewsWithMaterial(
    u32 base_vertex, u32 base_index, u32 index_size, PrimitiveType primitive_type,
    const GraphicsModSystem::DrawDataView& draw_data,
    const VideoCommon::MaterialResource::Data& material_data,
    VideoCommon::CameraManager& camera_manager)
{
  auto& system = Core::System::GetInstance();
  auto& vertex_shader_manager = system.GetVertexShaderManager();

  AbstractFramebuffer* frame_buffer_to_restore = nullptr;

  const auto camera_view = camera_manager.GetCurrentCameraView();
  const auto additional_camera_views = camera_manager.GetAdditionalViews();
  if (camera_view.framebuffer || !additional_camera_views.empty())
  {
    frame_buffer_to_restore = g_gfx->GetCurrentFramebuffer();
  }

  if (camera_view.framebuffer)
  {
    g_gfx->SetFramebuffer(camera_view.framebuffer);
  }

  if (camera_view.transform)
  {
    // Get the current camera id, if it changed
    // we need to reload our projection matrix
    const u64 camera_id = Common::ToUnderlying<>(camera_view.id);
    if (m_last_camera_id != camera_id)
    {
      vertex_shader_manager.ForceProjectionMatrixUpdate(system.GetXFStateManager(),
                                                        *camera_view.transform);
      m_last_camera_id = camera_id;
    }
  }
  else
  {
    // If we had a camera last draw but none this draw we need to reload our projection matrix
    if (m_last_camera_id)
    {
      vertex_shader_manager.ForceProjectionMatrixUpdate(system.GetXFStateManager(),
                                                        Common::Matrix44::Identity());
    }
    m_last_camera_id.reset();
  }
  DrawWithMaterial(base_vertex, base_index, index_size, primitive_type, draw_data, material_data,
                   camera_manager);

  // Do we have any other views?  If so, we need to redraw with the current material to those
  // frame buffers...
  for (const auto additional_camera_view : additional_camera_views)
  {
    if (xfmem.projection.type == ProjectionType::Orthographic &&
        additional_camera_view.skip_orthographic)
    {
      continue;
    }
    g_gfx->SetFramebuffer(additional_camera_view.framebuffer);
    if (additional_camera_view.transform)
    {
      const u64 camera_id = Common::ToUnderlying<>(additional_camera_view.id);
      if (m_last_camera_id != camera_id)
      {
        vertex_shader_manager.ForceProjectionMatrixUpdate(system.GetXFStateManager(),
                                                          *additional_camera_view.transform);
        m_last_camera_id = camera_id;
      }
    }
    else
    {
      if (m_last_camera_id)
      {
        vertex_shader_manager.ForceProjectionMatrixUpdate(system.GetXFStateManager(),
                                                          Common::Matrix44::Identity());
      }
      m_last_camera_id.reset();
    }
    DrawWithMaterial(base_vertex, base_index, index_size, primitive_type, draw_data, material_data,
                     camera_manager);
  }

  if (frame_buffer_to_restore)
  {
    g_gfx->SetFramebuffer(frame_buffer_to_restore);
  }

  if (auto next_material = material_data.GetNextMaterial())
  {
    const auto data = next_material->GetData();
    DrawViewsWithMaterial(base_vertex, base_index, index_size, primitive_type, draw_data, *data,
                          camera_manager);
  }
}

void VertexManagerBase::DrawWithMaterial(u32 base_vertex, u32 base_index, u32 index_size,
                                         PrimitiveType primitive_type,
                                         const GraphicsModSystem::DrawDataView& draw_data,
                                         const VideoCommon::MaterialResource::Data& material_data,
                                         VideoCommon::CameraManager& camera_manager)
{
  auto& system = Core::System::GetInstance();
  auto& geometry_shader_manager = system.GetGeometryShaderManager();
  auto& pixel_shader_manager = system.GetPixelShaderManager();

  // Now we can upload uniforms, as nothing else will override them.
  geometry_shader_manager.SetConstants(primitive_type);
  pixel_shader_manager.SetConstants();
  const auto pixel_uniforms = material_data.GetPixelUniforms();
  if (!pixel_uniforms.empty())
  {
    pixel_shader_manager.custom_constants = pixel_uniforms;
    pixel_shader_manager.custom_constants_dirty = true;
  }
  UploadUniforms();

  g_gfx->SetPipeline(material_data.GetPipeline());

  for (const auto& texture_like : material_data.GetTextures())
  {
    const SamplerState* sampler = nullptr;
    if (texture_like.texture == nullptr) [[unlikely]]
      continue;
    if (texture_like.sampler_origin == VideoCommon::TextureSamplerValue::SamplerOrigin::Asset)
    {
      sampler = &texture_like.sampler;
    }
    else
    {
      if (!texture_like.texture_hash.empty())
      {
        for (const auto& texture_view : draw_data.textures)
        {
          if (texture_view.hash_name == texture_like.texture_hash)
          {
            sampler = &draw_data.samplers[texture_view.unit];
            break;
          }
        }
      }
    }

    if (!sampler)
      continue;

    g_gfx->SetTexture(texture_like.sampler_index, texture_like.texture);
    g_gfx->SetSamplerState(texture_like.sampler_index, *sampler);
  }

  DrawCurrentBatch(base_index, index_size, base_vertex);
}
