// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/VertexManagerBase.h"

#include <cmath>
#include <memory>

#include "Common/BitSet.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
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
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

std::unique_ptr<VertexManagerBase> g_vertex_manager;

static const PrimitiveType primitive_from_gx[8] = {
    PRIMITIVE_TRIANGLES,  // GX_DRAW_QUADS
    PRIMITIVE_TRIANGLES,  // GX_DRAW_QUADS_2
    PRIMITIVE_TRIANGLES,  // GX_DRAW_TRIANGLES
    PRIMITIVE_TRIANGLES,  // GX_DRAW_TRIANGLE_STRIP
    PRIMITIVE_TRIANGLES,  // GX_DRAW_TRIANGLE_FAN
    PRIMITIVE_LINES,      // GX_DRAW_LINES
    PRIMITIVE_LINES,      // GX_DRAW_LINE_STRIP
    PRIMITIVE_POINTS,     // GX_DRAW_POINTS
};

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
  if (m_current_primitive_type != primitive_from_gx[primitive])
    Flush();
  m_current_primitive_type = primitive_from_gx[primitive];

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
    g_vertex_manager->ResetBuffer(stride);
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
    {
      const auto* tentry = g_texture_cache->Load(i);

      if (tentry)
      {
        g_renderer->SetSamplerState(i & 3, i >> 2, tentry->is_custom_tex);
        PixelShaderManager::SetTexDims(i, tentry->native_width, tentry->native_height);
      }
      else
      {
        ERROR_LOG(VIDEO, "error loading texture");
      }
    }
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
    // set the rest of the global constants
    GeometryShaderManager::SetConstants();
    PixelShaderManager::SetConstants();

    if (PerfQueryBase::ShouldEmulate())
      g_perf_query->EnableQuery(bpmem.zcontrol.early_ztest ? PQG_ZCOMP_ZCOMPLOC : PQG_ZCOMP);
    g_vertex_manager->vFlush();
    if (PerfQueryBase::ShouldEmulate())
      g_perf_query->DisableQuery(bpmem.zcontrol.early_ztest ? PQG_ZCOMP_ZCOMPLOC : PQG_ZCOMP);
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
  g_vertex_manager->vDoState(p);
}

void VertexManagerBase::CalculateZSlope(NativeVertexFormat* format)
{
  float out[12];
  float viewOffset[2] = {xfmem.viewport.xOrig - bpmem.scissorOffset.x * 2,
                         xfmem.viewport.yOrig - bpmem.scissorOffset.y * 2};

  if (m_current_primitive_type != PRIMITIVE_TRIANGLES)
    return;

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
