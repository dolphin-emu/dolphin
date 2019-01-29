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
#include "VideoCommon/DataReader.h"
#include "VideoCommon/Debugger.h"
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

// GX primitive -> RenderState primitive, using primitive restart
constexpr std::array<PrimitiveType, 8> primitive_from_gx_pr{{
    PrimitiveType::TriangleStrip,  // GX_DRAW_QUADS
    PrimitiveType::TriangleStrip,  // GX_DRAW_QUADS_2
    PrimitiveType::TriangleStrip,  // GX_DRAW_TRIANGLES
    PrimitiveType::TriangleStrip,  // GX_DRAW_TRIANGLE_STRIP
    PrimitiveType::TriangleStrip,  // GX_DRAW_TRIANGLE_FAN
    PrimitiveType::Lines,          // GX_DRAW_LINES
    PrimitiveType::Lines,          // GX_DRAW_LINE_STRIP
    PrimitiveType::Points,         // GX_DRAW_POINTS
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
{
}

VertexManagerBase::~VertexManagerBase()
{
}

u32 VertexManagerBase::GetRemainingSize() const
{
  return static_cast<u32>(m_end_buffer_pointer - m_cur_buffer_pointer);
}

DataReader VertexManagerBase::PrepareForAdditionalData(int primitive, u32 count, u32 stride,
                                                       bool cullall)
{
  // The SSE vertex loader can write up to 4 bytes past the end
  u32 const needed_vertex_bytes = count * stride + 4;

  // We can't merge different kinds of primitives, so we have to flush here
  PrimitiveType new_primitive_type = g_ActiveConfig.backend_info.bSupportsPrimitiveRestart ?
                                         primitive_from_gx_pr[primitive] :
                                         primitive_from_gx[primitive];
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
    g_vertex_manager->ResetBuffer(stride, cullall);
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

  if (g_Config.backend_info.bSupportsPrimitiveRestart)
  {
    switch (primitive)
    {
    case OpcodeDecoder::GX_DRAW_QUADS:
    case OpcodeDecoder::GX_DRAW_QUADS_2:
      return index_len / 5 * 4;
    case OpcodeDecoder::GX_DRAW_TRIANGLES:
      return index_len / 4 * 3;
    case OpcodeDecoder::GX_DRAW_TRIANGLE_STRIP:
      return index_len / 1 - 1;
    case OpcodeDecoder::GX_DRAW_TRIANGLE_FAN:
      return index_len / 6 * 4 + 1;

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
  else
  {
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
}

std::pair<size_t, size_t> VertexManagerBase::ResetFlushAspectRatioCount()
{
  std::pair<size_t, size_t> val = std::make_pair(m_flush_count_4_3, m_flush_count_anamorphic);
  m_flush_count_4_3 = 0;
  m_flush_count_anamorphic = 0;
  return val;
}

void VertexManagerBase::UploadUtilityVertices(const void* vertices, u32 vertex_stride,
                                              u32 num_vertices, const u16* indices, u32 num_indices,
                                              u32* out_base_vertex, u32* out_base_index)
{
  // The GX vertex list should be flushed before any utility draws occur.
  ASSERT(m_is_flushed);

  // Copy into the buffers usually used for GX drawing.
  ResetBuffer(std::max(vertex_stride, 1u), false);
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

void VertexManagerBase::Flush()
{
  if (m_is_flushed)
    return;

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

  // If the primitave is marked CullAll. All we need to do is update the vertex constants and
  // calculate the zfreeze refrence slope
  if (!m_cull_all)
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

  // set global vertex constants
  VertexShaderManager::SetConstants();

  // Track some stats used elsewhere by the anamorphic widescreen heuristic.
  if (!SConfig::GetInstance().bWii)
  {
    float* rawProjection = xfmem.projection.rawProjection;
    bool viewport_is_4_3 = AspectIs4_3(xfmem.viewport.wd, xfmem.viewport.ht);
    if (AspectIs16_9(rawProjection[2], rawProjection[0]) && viewport_is_4_3)
    {
      // Projection is 16:9 and viewport is 4:3, we are rendering an anamorphic
      // widescreen picture.
      m_flush_count_anamorphic++;
    }
    else if (AspectIs4_3(rawProjection[2], rawProjection[0]) && viewport_is_4_3)
    {
      // Projection and viewports are both 4:3, we are rendering a normal image.
      m_flush_count_4_3++;
    }
  }

  // Calculate ZSlope for zfreeze
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
    // Update and upload constants. Note for the Vulkan backend, this must occur before the
    // vertex/index buffer is committed, otherwise the data will be associated with the
    // previous command buffer, instead of the one with the draw if there is an overflow.
    GeometryShaderManager::SetConstants();
    PixelShaderManager::SetConstants();
    UploadConstants();

    // Now the vertices can be flushed to the GPU.
    const u32 num_indices = IndexGenerator::GetIndexLen();
    u32 base_vertex, base_index;
    CommitBuffer(IndexGenerator::GetNumVerts(),
                 VertexLoaderManager::GetCurrentVertexFormat()->GetVertexStride(), num_indices,
                 &base_vertex, &base_index);

    // Update the pipeline, or compile one if needed.
    UpdatePipelineConfig();
    UpdatePipelineObject();
    if (m_current_pipeline_object)
    {
      g_renderer->SetPipeline(m_current_pipeline_object);
      if (PerfQueryBase::ShouldEmulate())
        g_perf_query->EnableQuery(bpmem.zcontrol.early_ztest ? PQG_ZCOMP_ZCOMPLOC : PQG_ZCOMP);

      DrawCurrentBatch(base_index, num_indices, base_vertex);
      INCSTAT(stats.thisFrame.numDrawCalls);

      if (PerfQueryBase::ShouldEmulate())
        g_perf_query->DisableQuery(bpmem.zcontrol.early_ztest ? PQG_ZCOMP_ZCOMPLOC : PQG_ZCOMP);
    }
  }

  GFX_DEBUGGER_PAUSE_AT(NEXT_FLUSH, true);

  if (xfmem.numTexGen.numTexGens != bpmem.genMode.numtexgens)
    ERROR_LOG(VIDEO,
              "xf.numtexgens (%d) does not match bp.numtexgens (%d). Error in command stream.",
              xfmem.numTexGen.numTexGens, bpmem.genMode.numtexgens.Value());

  m_is_flushed = true;
  m_cull_all = false;
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
