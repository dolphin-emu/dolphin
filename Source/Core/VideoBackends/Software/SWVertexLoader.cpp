// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Software/SWVertexLoader.h"

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

#include "VideoCommon/DataReader.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderBase.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

SWVertexLoader::SWVertexLoader() = default;

SWVertexLoader::~SWVertexLoader() = default;

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

  for (u32 i = 0; i < IndexGenerator::GetIndexLen(); i++)
  {
    const u16 index = m_cpu_index_buffer[i];
    memset(static_cast<void*>(&m_vertex), 0, sizeof(m_vertex));

    // Super Mario Sunshine requires those to be zero for those debug boxes.
    m_vertex.color = {};

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
    TransformUnit::TransformTexCoord(&m_vertex, outVertex, m_tex_gen_special_case);

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
    ERROR_LOG(VIDEO, "Matrix indices don't match");
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

  // special case if only pos and tex coord 0 and tex coord input is AB11
  // http://libogc.devkitpro.org/gx_8h.html#a55a426a3ff796db584302bddd829f002
  m_tex_gen_special_case = VertexLoaderManager::g_current_components == VB_HAS_UV0 &&
                           xfmem.texMtxInfo[0].projection == XF_TEXPROJ_ST;
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

template <typename T, bool swap = false>
static void ReadVertexAttribute(T* dst, DataReader src, const AttributeFormat& format,
                                int base_component, int components, bool reverse)
{
  if (format.enable)
  {
    src.Skip(format.offset);
    src.Skip(base_component * (1 << (format.type >> 1)));

    int i;
    for (i = 0; i < std::min(format.components - base_component, components); i++)
    {
      int i_dst = reverse ? components - i - 1 : i;
      switch (format.type)
      {
      case VAR_UNSIGNED_BYTE:
        dst[i_dst] = ReadNormalized<T, u8>(src.Read<u8, swap>());
        break;
      case VAR_BYTE:
        dst[i_dst] = ReadNormalized<T, s8>(src.Read<s8, swap>());
        break;
      case VAR_UNSIGNED_SHORT:
        dst[i_dst] = ReadNormalized<T, u16>(src.Read<u16, swap>());
        break;
      case VAR_SHORT:
        dst[i_dst] = ReadNormalized<T, s16>(src.Read<s16, swap>());
        break;
      case VAR_FLOAT:
        dst[i_dst] = ReadNormalized<T, float>(src.Read<float, swap>());
        break;
      }

      ASSERT_MSG(VIDEO, !format.integer || format.type != VAR_FLOAT,
                 "only non-float values are allowed to be streamed as integer");
    }
    for (; i < components; i++)
    {
      int i_dst = reverse ? components - i - 1 : i;
      dst[i_dst] = i == 3;
    }
  }
}

void SWVertexLoader::ParseVertex(const PortableVertexDeclaration& vdec, int index)
{
  DataReader src(m_cpu_vertex_buffer.data(),
                 m_cpu_vertex_buffer.data() + m_cpu_vertex_buffer.size());
  src.Skip(index * vdec.stride);

  ReadVertexAttribute<float>(&m_vertex.position[0], src, vdec.position, 0, 3, false);

  for (std::size_t i = 0; i < m_vertex.normal.size(); i++)
  {
    ReadVertexAttribute<float>(&m_vertex.normal[i][0], src, vdec.normals[i], 0, 3, false);
  }

  for (std::size_t i = 0; i < m_vertex.color.size(); i++)
  {
    ReadVertexAttribute<u8>(m_vertex.color[i].data(), src, vdec.colors[i], 0, 4, true);
  }

  for (std::size_t i = 0; i < m_vertex.texCoords.size(); i++)
  {
    ReadVertexAttribute<float>(m_vertex.texCoords[i].data(), src, vdec.texcoords[i], 0, 2, false);

    // the texmtr is stored as third component of the texCoord
    if (vdec.texcoords[i].components >= 3)
    {
      ReadVertexAttribute<u8>(&m_vertex.texMtx[i], src, vdec.texcoords[i], 2, 1, false);
    }
  }

  ReadVertexAttribute<u8>(&m_vertex.posMtx, src, vdec.posmtx, 0, 1, false);
}
