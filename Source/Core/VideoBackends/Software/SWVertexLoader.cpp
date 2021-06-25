// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Software/SWVertexLoader.h"

#include <algorithm>
#include <cstddef>
#include <limits>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

#include "VideoBackends/Software/DebugUtil.h"
#include "VideoBackends/Software/NativeVertexFormat.h"
#include "VideoBackends/Software/Rasterizer.h"
#include "VideoBackends/Software/SWRenderer.h"
#include "VideoBackends/Software/Tev.h"
#include "VideoBackends/Software/TransformUnit.h"

#include "VideoCommon/CPMemory.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderBase.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

void SWVertexLoader::DrawCurrentBatch(u32 base_index, u32 num_indices, u32 base_vertex)
{
  DebugUtil::OnObjectBegin();

  u8 primitiveType = 0;
  switch (m_current_primitive_type)
  {
  case PrimitiveType::Points:
    primitiveType = OpcodeDecoder::GX_DRAW_POINTS;
    break;
  case PrimitiveType::Lines:
    primitiveType = OpcodeDecoder::GX_DRAW_LINES;
    break;
  case PrimitiveType::Triangles:
    primitiveType = OpcodeDecoder::GX_DRAW_TRIANGLES;
    break;
  case PrimitiveType::TriangleStrip:
    primitiveType = OpcodeDecoder::GX_DRAW_TRIANGLE_STRIP;
    break;
  }

  m_setup_unit.Init(primitiveType);

  // set all states with are stored within video sw
  for (int i = 0; i < 4; i++)
  {
    Rasterizer::SetTevReg(i, Tev::RED_C, PixelShaderManager::constants.kcolors[i][0]);
    Rasterizer::SetTevReg(i, Tev::GRN_C, PixelShaderManager::constants.kcolors[i][1]);
    Rasterizer::SetTevReg(i, Tev::BLU_C, PixelShaderManager::constants.kcolors[i][2]);
    Rasterizer::SetTevReg(i, Tev::ALP_C, PixelShaderManager::constants.kcolors[i][3]);
  }

  for (u32 i = 0; i < m_index_generator.GetIndexLen(); i++)
  {
    const u16 index = m_cpu_index_buffer[i];
    memset(static_cast<void*>(&m_vertex), 0, sizeof(m_vertex));

    // parse the videocommon format to our own struct format (m_vertex)
    SetFormat(g_main_cp_state.last_id, primitiveType);
    ParseVertex(VertexLoaderManager::GetCurrentVertexFormat()->GetVertexDeclaration(), index);

    // transform this vertex so that it can be used for rasterization (outVertex)
    OutputVertexData* outVertex = m_setup_unit.GetVertex();
    TransformUnit::TransformPosition(&m_vertex, outVertex);
    outVertex->normal = {};
    if (VertexLoaderManager::g_current_components & VB_HAS_NRM0)
    {
      TransformUnit::TransformNormal(
          &m_vertex, (VertexLoaderManager::g_current_components & VB_HAS_NRM2) != 0, outVertex);
    }
    TransformUnit::TransformColor(&m_vertex, outVertex);
    TransformUnit::TransformTexCoord(&m_vertex, outVertex);

    // assemble and rasterize the primitive
    m_setup_unit.SetupVertex();

    INCSTAT(g_stats.this_frame.num_vertices_loaded)
  }

  DebugUtil::OnObjectEnd();
}

void SWVertexLoader::SetFormat(u8 attributeIndex, u8 primitiveType)
{
  // matrix index from xf regs or cp memory?
  if (xfmem.MatrixIndexA.PosNormalMtxIdx != g_main_cp_state.matrix_index_a.PosNormalMtxIdx ||
      xfmem.MatrixIndexA.Tex0MtxIdx != g_main_cp_state.matrix_index_a.Tex0MtxIdx ||
      xfmem.MatrixIndexA.Tex1MtxIdx != g_main_cp_state.matrix_index_a.Tex1MtxIdx ||
      xfmem.MatrixIndexA.Tex2MtxIdx != g_main_cp_state.matrix_index_a.Tex2MtxIdx ||
      xfmem.MatrixIndexA.Tex3MtxIdx != g_main_cp_state.matrix_index_a.Tex3MtxIdx ||
      xfmem.MatrixIndexB.Tex4MtxIdx != g_main_cp_state.matrix_index_b.Tex4MtxIdx ||
      xfmem.MatrixIndexB.Tex5MtxIdx != g_main_cp_state.matrix_index_b.Tex5MtxIdx ||
      xfmem.MatrixIndexB.Tex6MtxIdx != g_main_cp_state.matrix_index_b.Tex6MtxIdx ||
      xfmem.MatrixIndexB.Tex7MtxIdx != g_main_cp_state.matrix_index_b.Tex7MtxIdx)
  {
    ERROR_LOG_FMT(VIDEO, "Matrix indices don't match");
  }

  m_vertex.posMtx = xfmem.MatrixIndexA.PosNormalMtxIdx;
  m_vertex.texMtx[0] = xfmem.MatrixIndexA.Tex0MtxIdx;
  m_vertex.texMtx[1] = xfmem.MatrixIndexA.Tex1MtxIdx;
  m_vertex.texMtx[2] = xfmem.MatrixIndexA.Tex2MtxIdx;
  m_vertex.texMtx[3] = xfmem.MatrixIndexA.Tex3MtxIdx;
  m_vertex.texMtx[4] = xfmem.MatrixIndexB.Tex4MtxIdx;
  m_vertex.texMtx[5] = xfmem.MatrixIndexB.Tex5MtxIdx;
  m_vertex.texMtx[6] = xfmem.MatrixIndexB.Tex6MtxIdx;
  m_vertex.texMtx[7] = xfmem.MatrixIndexB.Tex7MtxIdx;
}

template <typename T, typename I>
static T ReadNormalized(I value)
{
  T casted = (T)value;
  if (!std::numeric_limits<T>::is_integer && std::numeric_limits<I>::is_integer)
  {
    // normalize if non-float is converted to a float
    casted *= (T)(1.0 / std::numeric_limits<I>::max());
  }
  return casted;
}

template <typename T>
static void ReadVertexAttribute0(T* dst, DataReader src, const AttributeFormat& format,
                                 std::size_t base_component, std::size_t components, bool reverse)
{
  ASSERT(format.components >= base_component);

  src.Skip(format.offset);
  const std::size_t component_size = (1ull << (format.type >> 1));
  src.Skip(base_component * component_size);

  for (std::size_t i = 0; i < std::min(format.components - base_component, components); i++)
  {
    const std::size_t i_dst = reverse ? components - i - 1 : i;
    switch (format.type)
    {
    case VAR_UNSIGNED_BYTE:
      dst[i_dst] = ReadNormalized<T, u8>(src.Read<u8, false>());
      break;
    case VAR_BYTE:
      dst[i_dst] = ReadNormalized<T, s8>(src.Read<s8, false>());
      break;
    case VAR_UNSIGNED_SHORT:
      dst[i_dst] = ReadNormalized<T, u16>(src.Read<u16, false>());
      break;
    case VAR_SHORT:
      dst[i_dst] = ReadNormalized<T, s16>(src.Read<s16, false>());
      break;
    case VAR_FLOAT:
      dst[i_dst] = ReadNormalized<T, float>(src.Read<float, false>());
      break;
    }

    ASSERT_MSG(VIDEO, !format.integer || format.type != VAR_FLOAT,
               "only non-float values are allowed to be streamed as integer");
  }
}

static void ReadVertexAttribute(Vec3* dst, DataReader src, CacheEntry<Vec3>* cache,
                                const AttributeFormat& format)
{
  if (format.enable)
  {
    // Depending on the format, not all fields may be filled (e.g. only x and y for a position); the
    // remainder should be zero-initialized.
    dst->SetZero();
    ReadVertexAttribute0(&dst[0][0], src, format, 0, 3, false);
    cache->Write(*dst);
  }
  else
  {
    *dst = cache->Read();
  }
}

template <typename T, std::size_t count>
static void ReadVertexAttribute(std::array<T, count>* dst, DataReader src,
                                CacheEntry<std::array<T, count>>* cache,
                                const AttributeFormat& format, bool reverse)
{
  if (format.enable)
  {
    // Depending on the format, not all fields may be filled (e.g. only x and y for a position); the
    // remainder should be zero-initialized.
    std::fill(dst->begin(), dst->end(), 0);

    ReadVertexAttribute0(dst->data(), src, format, 0, count, reverse);
    cache->Write(*dst);
  }
  else
  {
    *dst = cache->Read();
  }
}

template <typename T>
static void ReadVertexAttributeNoCache(T* dst, DataReader src, const AttributeFormat& format,
                                       std::size_t base_component)
{
  // The position and texture matrix indices don't seem to use the cache, and instead fall back to
  // values in xfmem.  We prepare these values once in SetFormat, so there's no need to do anything
  // else here.
  if (format.enable)
  {
    ReadVertexAttribute0(dst, src, format, base_component, 1, false);
  }
}

void SWVertexLoader::ParseVertex(const PortableVertexDeclaration& vdec, int index)
{
  DataReader src(m_cpu_vertex_buffer.data(),
                 m_cpu_vertex_buffer.data() + m_cpu_vertex_buffer.size());
  src.Skip(index * vdec.stride);

  ReadVertexAttribute(&m_vertex.position, src, &m_cache.position, vdec.position);

  for (std::size_t i = 0; i < m_vertex.normal.size(); i++)
  {
    ReadVertexAttribute(&m_vertex.normal[i], src, &m_cache.normal[i], vdec.normals[i]);
  }

  for (std::size_t i = 0; i < m_vertex.color.size(); i++)
  {
    ReadVertexAttribute(&m_vertex.color[i], src, &m_cache.color[i], vdec.colors[i], true);
  }

  for (std::size_t i = 0; i < m_vertex.texCoords.size(); i++)
  {
    ReadVertexAttribute(&m_vertex.texCoords[i], src, &m_cache.texCoords[i], vdec.texcoords[i],
                        false);

    // if texmtx is included, texcoord will always be 3 floats, z will be the texmtx index
    if (vdec.texcoords[i].components == 3)
    {
      ReadVertexAttributeNoCache(&m_vertex.texMtx[i], src, vdec.texcoords[i], 2);
    }
  }

  ReadVertexAttributeNoCache(&m_vertex.posMtx, src, vdec.posmtx, 0);
}
