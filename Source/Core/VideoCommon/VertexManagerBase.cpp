// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/VertexManagerBase.h"

#include <array>
#include <cmath>
#include <memory>

#include "Common/BitSet.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"

#include "Core/ConfigManager.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PerfQueryBase.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/SamplerCommon.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"
#include "VideoCommon/OnScreenDisplay.h"

std::unique_ptr<VertexManagerBase> g_vertex_manager;

// GX primitive -> RenderState primitive, no primitive restart
constexpr std::array<PrimitiveType, 8> primitive_from_gx{{
    PrimitiveType::Triangles,  // GX_DRAW_QUADS
    PrimitiveType::Triangles,  // GX_DRAW_QUADS_2
    PrimitiveType::Triangles,  // GX_DRAW_TRIANGLES
    PrimitiveType::Triangles,  // GX_DRAW_TRIANGLE_STRIP
    PrimitiveType::Triangles,  // GX_DRAW_TRIANGLE_FAN
    PrimitiveType::Lines,      // GX_DRAW_LINES
    PrimitiveType::Lines,      // GX_DRAW_LINE_STRIP
    PrimitiveType::Points,     // GX_DRAW_POINTS
}};

// Due to the BT.601 standard which the GameCube is based on being a compromise
// between PAL and NTSC, neither standard gets square pixels. They are each off
// by ~9% in opposite directions.
// Just in case any game decides to take this into account, we do both these
// tests with a large amount of slop.
static bool AspectIs4_3(float width, float height)
{
  float aspect = fabsf(width / height);
  return fabsf(aspect - 4.0f / 3.0f) < 4.0f / 3.0f * 0.11;  // within 11% of 4:3
}

static bool AspectIs16_9(float width, float height)
{
  float aspect = fabsf(width / height);
  return fabsf(aspect - 16.0f / 9.0f) < 16.0f / 9.0f * 0.11;  // within 11% of 16:9
}

VertexManagerBase::VertexManagerBase()
    : m_cpu_vertex_buffer(MAXVBUFFERSIZE), m_cpu_index_buffer(MAXIBUFFERSIZE)
{
}

VertexManagerBase::~VertexManagerBase() = default;

bool VertexManagerBase::Initialize()
{
  return true;
}

u32 VertexManagerBase::GetRemainingSize() const
{
  return static_cast<u32>(m_end_buffer_pointer - m_cur_buffer_pointer);
}

DataReader VertexManagerBase::PrepareForAdditionalData(int primitive, u32 count, u32 stride,
                                                       bool cullall)
{
  // Flush all EFB pokes. Since the buffer is shared, we can't draw pokes+primitives concurrently.
  g_framebuffer_manager->FlushEFBPokes();

  // The SSE vertex loader can write up to 4 bytes past the end
  u32 const needed_vertex_bytes = count * stride + 4;

  // We can't merge different kinds of primitives, so we have to flush here
  PrimitiveType new_primitive_type = primitive_from_gx[primitive];
  if (m_current_primitive_type != new_primitive_type)
  {
    Flush();

    // Have to update the rasterization state for point/line cull modes.
    m_current_primitive_type = new_primitive_type;
    SetRasterizationStateChanged();
  }

  // Check for size in buffer, if the buffer gets full, call Flush()
  if (!m_is_flushed &&
      (count > IndexGenerator::GetRemainingIndices() || count > GetRemainingIndices(primitive) ||
       needed_vertex_bytes > GetRemainingSize()))
  {
    Flush();

    if (count > IndexGenerator::GetRemainingIndices())
      ERROR_LOG(VIDEO, "Too little remaining index values. Use 32-bit or reset them on flush.");
    if (count > GetRemainingIndices(primitive))
      ERROR_LOG(VIDEO, "VertexManager: Buffer not large enough for all indices! "
                       "Increase MAXIBUFFERSIZE or we need primitive breaking after all.");
    if (needed_vertex_bytes > GetRemainingSize())
      ERROR_LOG(VIDEO, "VertexManager: Buffer not large enough for all vertices! "
                       "Increase MAXVBUFFERSIZE or we need primitive breaking after all.");
  }

  m_cull_all = cullall;

  // need to alloc new buffer
  if (m_is_flushed)
  {
    if (cullall)
    {
      // This buffer isn't getting sent to the GPU. Just allocate it on the cpu.
      m_cur_buffer_pointer = m_base_buffer_pointer = m_cpu_vertex_buffer.data();
      m_end_buffer_pointer = m_base_buffer_pointer + m_cpu_vertex_buffer.size();
      IndexGenerator::Start(m_cpu_index_buffer.data());
    }
    else
    {
      ResetBuffer(stride);
    }

    m_is_flushed = false;
  }

  return DataReader(m_cur_buffer_pointer, m_end_buffer_pointer);
}

void VertexManagerBase::FlushData(u32 count, u32 stride)
{
  m_cur_buffer_pointer += count * stride;
}

u32 VertexManagerBase::GetRemainingIndices(int primitive)
{
  u32 index_len = MAXIBUFFERSIZE - IndexGenerator::GetIndexLen();

  switch (primitive)
  {
  case OpcodeDecoder::GX_DRAW_QUADS:
  case OpcodeDecoder::GX_DRAW_QUADS_2:
    return index_len / 6 * 4;
  case OpcodeDecoder::GX_DRAW_TRIANGLES:
    return index_len;
  case OpcodeDecoder::GX_DRAW_TRIANGLE_STRIP:
    return index_len / 3 + 2;
  case OpcodeDecoder::GX_DRAW_TRIANGLE_FAN:
    return index_len / 3 + 2;

  case OpcodeDecoder::GX_DRAW_LINES:
    return index_len;
  case OpcodeDecoder::GX_DRAW_LINE_STRIP:
    return index_len / 2 + 1;

  case OpcodeDecoder::GX_DRAW_POINTS:
    return index_len;

  default:
    return 0;
  }
}

std::pair<size_t, size_t> VertexManagerBase::ResetFlushAspectRatioCount()
{
  std::pair<size_t, size_t> val = std::make_pair(m_flush_count_4_3, m_flush_count_anamorphic);
  m_flush_count_4_3 = 0;
  m_flush_count_anamorphic = 0;
  return val;
}

void VertexManagerBase::ResetBuffer(u32 vertex_stride)
{
  m_base_buffer_pointer = m_cpu_vertex_buffer.data();
  m_cur_buffer_pointer = m_cpu_vertex_buffer.data();
  m_end_buffer_pointer = m_base_buffer_pointer + m_cpu_vertex_buffer.size();
  IndexGenerator::Start(m_cpu_index_buffer.data());
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
  if (::BoundingBox::active && g_ActiveConfig.bBBoxEnable &&
      g_ActiveConfig.backend_info.bSupportsBBox)
  {
    g_renderer->BBoxFlush();
  }

  g_renderer->DrawIndexed(base_index, num_indices, base_vertex);
}

void VertexManagerBase::UploadUniforms()
{
}

void VertexManagerBase::InvalidateConstants()
{
  VertexShaderManager::dirty = true;
  GeometryShaderManager::dirty = true;
  PixelShaderManager::dirty = true;
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
    IndexGenerator::AddExternalIndices(indices, num_indices, num_vertices);

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

void VertexManagerBase::LoadTextures()
{
  BitSet32 usedtextures;
  for (u32 i = 0; i < bpmem.genMode.numtevstages + 1u; ++i)
    if (bpmem.tevorders[i / 2].getEnable(i & 1))
      usedtextures[bpmem.tevorders[i / 2].getTexMap(i & 1)] = true;

  if (bpmem.genMode.numindstages > 0)
    for (unsigned int i = 0; i < bpmem.genMode.numtevstages + 1u; ++i)
      if (bpmem.tevind[i].IsActive() && bpmem.tevind[i].bt < bpmem.genMode.numindstages)
        usedtextures[bpmem.tevindref.getTexMap(bpmem.tevind[i].bt)] = true;

  for (unsigned int i : usedtextures)
    g_texture_cache->Load(i);

  g_texture_cache->BindTextures();
}

void VertexManagerBase::Flush()
{
  if (m_is_flushed)
    return;

  m_is_flushed = true;

  // loading a state will invalidate BP, so check for it
  g_video_backend->CheckInvalidState();

#if defined(_DEBUG) || defined(DEBUGFAST)
  PRIM_LOG("frame%d:\n texgen=%u, numchan=%u, dualtex=%u, ztex=%u, cole=%u, alpe=%u, ze=%u",
           g_ActiveConfig.iSaveTargetId, xfmem.numTexGen.numTexGens, xfmem.numChan.numColorChans,
           xfmem.dualTexTrans.enabled, bpmem.ztex2.op.Value(), bpmem.blendmode.colorupdate.Value(),
           bpmem.blendmode.alphaupdate.Value(), bpmem.zmode.updateenable.Value());

  for (u32 i = 0; i < xfmem.numChan.numColorChans; ++i)
  {
    LitChannel* ch = &xfmem.color[i];
    PRIM_LOG("colchan%u: matsrc=%u, light=0x%x, ambsrc=%u, diffunc=%u, attfunc=%u", i,
             ch->matsource.Value(), ch->GetFullLightMask(), ch->ambsource.Value(),
             ch->diffusefunc.Value(), ch->attnfunc.Value());
    ch = &xfmem.alpha[i];
    PRIM_LOG("alpchan%u: matsrc=%u, light=0x%x, ambsrc=%u, diffunc=%u, attfunc=%u", i,
             ch->matsource.Value(), ch->GetFullLightMask(), ch->ambsource.Value(),
             ch->diffusefunc.Value(), ch->attnfunc.Value());
  }

  for (u32 i = 0; i < xfmem.numTexGen.numTexGens; ++i)
  {
    TexMtxInfo tinfo = xfmem.texMtxInfo[i];
    if (tinfo.texgentype != XF_TEXGEN_EMBOSS_MAP)
      tinfo.hex &= 0x7ff;
    if (tinfo.texgentype != XF_TEXGEN_REGULAR)
      tinfo.projection = 0;

    PRIM_LOG("txgen%u: proj=%u, input=%u, gentype=%u, srcrow=%u, embsrc=%u, emblght=%u, "
             "postmtx=%u, postnorm=%u",
             i, tinfo.projection.Value(), tinfo.inputform.Value(), tinfo.texgentype.Value(),
             tinfo.sourcerow.Value(), tinfo.embosssourceshift.Value(),
             tinfo.embosslightshift.Value(), xfmem.postMtxInfo[i].index.Value(),
             xfmem.postMtxInfo[i].normalize.Value());
  }

  PRIM_LOG("pixel: tev=%u, ind=%u, texgen=%u, dstalpha=%u, alphatest=0x%x",
           bpmem.genMode.numtevstages.Value() + 1, bpmem.genMode.numindstages.Value(),
           bpmem.genMode.numtexgens.Value(), bpmem.dstalpha.enable.Value(),
           (bpmem.alpha_test.hex >> 16) & 0xff);
#endif

  // Track some stats used elsewhere by the anamorphic widescreen heuristic.
  if (!SConfig::GetInstance().bWii)
  {
    const auto& raw_projection = xfmem.projection.rawProjection;
    const bool viewport_is_4_3 = AspectIs4_3(xfmem.viewport.wd, xfmem.viewport.ht);
    if (AspectIs16_9(raw_projection[2], raw_projection[0]) && viewport_is_4_3)
    {
      // Projection is 16:9 and viewport is 4:3, we are rendering an anamorphic
      // widescreen picture.
      m_flush_count_anamorphic++;
    }
    else if (AspectIs4_3(raw_projection[2], raw_projection[0]) && viewport_is_4_3)
    {
      // Projection and viewports are both 4:3, we are rendering a normal image.
      m_flush_count_4_3++;
    }
  }

  // Calculate ZSlope for zfreeze
  VertexShaderManager::SetConstants();
  if (!bpmem.genMode.zfreeze)
  {
    // Must be done after VertexShaderManager::SetConstants()
    CalculateZSlope(VertexLoaderManager::GetCurrentVertexFormat());
  }
  else if (m_zslope.dirty && !m_cull_all)  // or apply any dirty ZSlopes
  {
    PixelShaderManager::SetZSlope(m_zslope.dfdx, m_zslope.dfdy, m_zslope.f0);
    m_zslope.dirty = false;
  }

  if (!m_cull_all)
  {
    // Now the vertices can be flushed to the GPU. Everything following the CommitBuffer() call
    // must be careful to not upload any utility vertices, as the binding will be lost otherwise.
    const u32 num_indices = IndexGenerator::GetIndexLen();
    u32 base_vertex, base_index;
    CommitBuffer(IndexGenerator::GetNumVerts(),
                 VertexLoaderManager::GetCurrentVertexFormat()->GetVertexStride(), num_indices,
                 &base_vertex, &base_index);

    // Texture loading can cause palettes to be applied (-> uniforms -> draws).
    // Palette application does not use vertices, only a full-screen quad, so this is okay.
    // Same with GPU texture decoding, which uses compute shaders.
    LoadTextures();

    // Now we can upload uniforms, as nothing else will override them.
    GeometryShaderManager::SetConstants();
    PixelShaderManager::SetConstants();
    UploadUniforms();

    // Update the pipeline, or compile one if needed.
    UpdatePipelineConfig();
    UpdatePipelineObject();

    // logic ops draw hack
    if (m_current_pipeline_config.blending_state.logicopenable && num_indices == 6 &&
        g_ActiveConfig.bLogicOpsDrawHack)
    {
      BlendMode::LogicOp logicmode = m_current_pipeline_config.blending_state.logicmode;
      if (!(logicmode == BlendMode::LogicOp::CLEAR || logicmode == BlendMode::LogicOp::COPY ||
            logicmode == BlendMode::LogicOp::COPY_INVERTED || logicmode == BlendMode::LogicOp::SET))
      {
        OnDraw();
        return;
      }
    }

    if (m_current_pipeline_object)
    {
      g_renderer->SetPipeline(m_current_pipeline_object);
      if (PerfQueryBase::ShouldEmulate())
        g_perf_query->EnableQuery(bpmem.zcontrol.early_ztest ? PQG_ZCOMP_ZCOMPLOC : PQG_ZCOMP);

      DrawCurrentBatch(base_index, num_indices, base_vertex);
      INCSTAT(g_stats.this_frame.num_draw_calls);

      if (!g_ActiveConfig.backend_info.bSupportsDualSourceBlend)
      {
        const AbstractPipeline* pipeline = GetPipelineForAlphaPass();
        if (pipeline)
        {
          // Execute the draw, again
          g_renderer->SetPipeline(pipeline);
          DrawCurrentBatch(base_index, num_indices, base_vertex);
        }
      }

      if (PerfQueryBase::ShouldEmulate())
        g_perf_query->DisableQuery(bpmem.zcontrol.early_ztest ? PQG_ZCOMP_ZCOMPLOC : PQG_ZCOMP);

      OnDraw();

      // The EFB cache is now potentially stale.
      g_framebuffer_manager->FlagPeekCacheAsOutOfDate();
    }
  }

  if (xfmem.numTexGen.numTexGens != bpmem.genMode.numtexgens)
  {
    OSD::AddMessage(StringFromFormat("xf.numtexgens(%d) does not match bp.numtexgens(%d).",
                                     xfmem.numTexGen.numTexGens, bpmem.genMode.numtexgens.Value()));
  }
}

void VertexManagerBase::DoState(PointerWrap& p)
{
  p.Do(m_zslope);
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
  for (unsigned int i = 0; i < 3; ++i)
  {
    // If this vertex format has per-vertex position matrix IDs, look it up.
    if (vert_decl.posmtx.enable)
      mtxIdx = VertexLoaderManager::position_matrix_index[3 - i];

    if (vert_decl.position.components == 2)
      VertexLoaderManager::position_cache[2 - i][2] = 0;

    VertexShaderManager::TransformToClipSpace(&VertexLoaderManager::position_cache[2 - i][0],
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

void VertexManagerBase::UpdatePipelineConfig()
{
  NativeVertexFormat* vertex_format = VertexLoaderManager::GetCurrentVertexFormat();
  if (vertex_format != m_current_pipeline_config.vertex_format)
  {
    m_current_pipeline_config.vertex_format = vertex_format;
    m_pipeline_config_changed = true;
  }

  VertexShaderUid vs_uid = GetVertexShaderUid();
  if (vs_uid != m_current_pipeline_config.vs_uid)
  {
    m_current_pipeline_config.vs_uid = vs_uid;
    m_pipeline_config_changed = true;
  }

  PixelShaderUid ps_uid = GetPixelShaderUid();
  if (ps_uid != m_current_pipeline_config.ps_uid)
  {
    m_current_pipeline_config.ps_uid = ps_uid;
    m_pipeline_config_changed = true;
  }

  GeometryShaderUid gs_uid = GetGeometryShaderUid(GetCurrentPrimitiveType());
  if (gs_uid != m_current_pipeline_config.gs_uid)
  {
    m_current_pipeline_config.gs_uid = gs_uid;
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

    // Ensure we try again next draw. Otherwise, if no registers change between frames, the
    // object will never be drawn, even when the shader is ready.
    m_pipeline_config_changed = true;
  }
  break;
  }
}

const AbstractPipeline* VertexManagerBase::GetPipelineForAlphaPass()
{
  const AbstractPipeline* pipeline_object = nullptr;
  if (m_current_pipeline_config.blending_state.IsDualSourceBlend())
  {
    VideoCommon::GXPipelineUid pipeline_config(m_current_pipeline_config);
    // Skip depth writes for this pass. The results will be the same, so no
    // point in overwriting depth values with the same value.
    pipeline_config.depth_state.hex = 0;
    // Only allow alpha writes, and disable blending.
    pipeline_config.blending_state.hex = 0;
    pipeline_config.blending_state.alphaupdate = true;
    // diable fog
    pixel_shader_uid_data* uid_data = pipeline_config.ps_uid.GetUidData();
    uid_data->fog_fsel = 0;
    uid_data->fog_proj = 0;
    uid_data->fog_RangeBaseEnabled = 0;
    // alpha pass
    uid_data->useDstAlpha = 1;

    switch (g_ActiveConfig.iShaderCompilationMode)
    {
    case ShaderCompilationMode::Synchronous:
    {
      // Block and compile the specialized shader.
      pipeline_object = g_shader_cache->GetPipelineForUid(pipeline_config);
    }
    break;

    case ShaderCompilationMode::AsynchronousSkipRendering:
    {
      // Can we background compile shaders? If so, get the pipeline asynchronously.
      auto res = g_shader_cache->GetPipelineForUidAsync(pipeline_config);
      if (res)
      {
        // Specialized shaders are ready, prefer these.
        pipeline_object = *res;
      }
    }
    break;
    }
  }
  return pipeline_object;
}

void VertexManagerBase::OnDraw()
{
  m_draw_counter++;

  // If we didn't have any CPU access last frame, do nothing.
  if (m_scheduled_command_buffer_kicks.empty() || !m_allow_background_execution)
    return;

  // Check if this draw is scheduled to kick a command buffer.
  // The draw counters will always be sorted so a binary search is possible here.
  if (std::binary_search(m_scheduled_command_buffer_kicks.begin(),
                         m_scheduled_command_buffer_kicks.end(), m_draw_counter))
  {
    // Kick a command buffer on the background thread.
    g_renderer->Flush();
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
    return;

  g_renderer->Flush();
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
    std::stringstream ss;
    std::for_each(m_cpu_accesses_this_frame.begin(), m_cpu_accesses_this_frame.end(), [&ss](u32 idx) { ss << idx << ","; });
    WARN_LOG(VIDEO, "CPU EFB accesses in last frame: %s", ss.str().c_str());
  }
  {
    std::stringstream ss;
    std::for_each(m_scheduled_command_buffer_kicks.begin(), m_scheduled_command_buffer_kicks.end(), [&ss](u32 idx) { ss << idx << ","; });
    WARN_LOG(VIDEO, "Scheduled command buffer kicks: %s", ss.str().c_str());
  }
#endif

  m_cpu_accesses_this_frame.clear();
}
