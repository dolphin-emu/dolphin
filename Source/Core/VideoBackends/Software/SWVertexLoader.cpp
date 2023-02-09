// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Software/SWVertexLoader.h"

#include <cstddef>
#include <limits>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

#include "Core/System.h"

#include "VideoBackends/Software/NativeVertexFormat.h"
#include "VideoBackends/Software/Rasterizer.h"
#include "VideoBackends/Software/SWRenderer.h"
#include "VideoBackends/Software/Tev.h"
#include "VideoBackends/Software/TransformUnit.h"

#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderBase.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

SWVertexLoader::SWVertexLoader() = default;

SWVertexLoader::~SWVertexLoader() = default;

DataReader SWVertexLoader::PrepareForAdditionalData(OpcodeDecoder::Primitive primitive, u32 count,
                                                    u32 stride, bool cullall)
{
  // The software renderer needs cullall to be false for zfreeze to work
  return VertexManagerBase::PrepareForAdditionalData(primitive, count, stride, false);
}

void SWVertexLoader::DrawCurrentBatch(u32 base_index, u32 num_indices, u32 base_vertex)
{
  using OpcodeDecoder::Primitive;
  Primitive primitive_type = Primitive::GX_DRAW_QUADS;
  switch (m_current_primitive_type)
  {
  case PrimitiveType::Points:
    primitive_type = Primitive::GX_DRAW_POINTS;
    break;
  case PrimitiveType::Lines:
    primitive_type = Primitive::GX_DRAW_LINES;
    break;
  case PrimitiveType::Triangles:
    primitive_type = Primitive::GX_DRAW_TRIANGLES;
    break;
  case PrimitiveType::TriangleStrip:
    primitive_type = Primitive::GX_DRAW_TRIANGLE_STRIP;
    break;
  }

  // Flush bounding box here because software overrides the base function
  if (g_bounding_box->IsEnabled())
    g_bounding_box->Flush();

  m_setup_unit.Init(primitive_type);
  Rasterizer::SetTevKonstColors();

  for (u32 i = 0; i < m_index_generator.GetIndexLen(); i++)
  {
    const u16 index = m_cpu_index_buffer[i];
    memset(static_cast<void*>(&m_vertex), 0, sizeof(m_vertex));

    // parse the videocommon format to our own struct format (m_vertex)
    SetFormat();
    ParseVertex(VertexLoaderManager::GetCurrentVertexFormat()->GetVertexDeclaration(), index);

    // transform this vertex so that it can be used for rasterization (outVertex)
    OutputVertexData* outVertex = m_setup_unit.GetVertex();
    TransformUnit::TransformPosition(&m_vertex, outVertex);
    outVertex->normal = {};
    if (VertexLoaderManager::g_current_components & VB_HAS_NORMAL)
      TransformUnit::TransformNormal(&m_vertex, outVertex);
    TransformUnit::TransformColor(&m_vertex, outVertex);
    TransformUnit::TransformTexCoord(&m_vertex, outVertex);

    // assemble and rasterize the primitive
    m_setup_unit.SetupVertex();

    INCSTAT(g_stats.this_frame.num_vertices_loaded);
  }

  INCSTAT(g_stats.this_frame.num_drawn_objects);
}

void SWVertexLoader::SetFormat()
{
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

template <typename T, bool swap = false>
static void ReadVertexAttribute(T* dst, DataReader src, const AttributeFormat& format,
                                int base_component, int components, bool reverse)
{
  if (format.enable)
  {
    src.Skip(format.offset);
    src.Skip(base_component * GetElementSize(format.type));

    int i;
    for (i = 0; i < std::min(format.components - base_component, components); i++)
    {
      int i_dst = reverse ? components - i - 1 : i;
      switch (format.type)
      {
      case ComponentFormat::UByte:
        dst[i_dst] = ReadNormalized<T, u8>(src.Read<u8, swap>());
        break;
      case ComponentFormat::Byte:
        dst[i_dst] = ReadNormalized<T, s8>(src.Read<s8, swap>());
        break;
      case ComponentFormat::UShort:
        dst[i_dst] = ReadNormalized<T, u16>(src.Read<u16, swap>());
        break;
      case ComponentFormat::Short:
        dst[i_dst] = ReadNormalized<T, s16>(src.Read<s16, swap>());
        break;
      case ComponentFormat::Float:
        dst[i_dst] = ReadNormalized<T, float>(src.Read<float, swap>());
        break;
      }

      ASSERT_MSG(VIDEO, !format.integer || format.type != ComponentFormat::Float,
                 "only non-float values are allowed to be streamed as integer");
    }
    for (; i < components; i++)
    {
      int i_dst = reverse ? components - i - 1 : i;
      dst[i_dst] = i == 3;
    }
  }
}

static void ParseColorAttributes(InputVertexData* dst, DataReader& src,
                                 const PortableVertexDeclaration& vdec)
{
  const auto set_default_color = [](std::array<u8, 4>& color) {
    color[Tev::ALP_C] = g_ActiveConfig.iMissingColorValue & 0xFF;
    color[Tev::BLU_C] = (g_ActiveConfig.iMissingColorValue >> 8) & 0xFF;
    color[Tev::GRN_C] = (g_ActiveConfig.iMissingColorValue >> 16) & 0xFF;
    color[Tev::RED_C] = (g_ActiveConfig.iMissingColorValue >> 24) & 0xFF;
  };

  if (vdec.colors[0].enable)
  {
    // Use color0 for channel 0, and color1 for channel 1 if both colors 0 and 1 are present.
    ReadVertexAttribute<u8>(dst->color[0].data(), src, vdec.colors[0], 0, 4, true);
    if (vdec.colors[1].enable)
      ReadVertexAttribute<u8>(dst->color[1].data(), src, vdec.colors[1], 0, 4, true);
    else
      set_default_color(dst->color[1]);
  }
  else
  {
    // If only one of the color attributes is enabled, it is directed to color 0.
    if (vdec.colors[1].enable)
      ReadVertexAttribute<u8>(dst->color[0].data(), src, vdec.colors[1], 0, 4, true);
    else
      set_default_color(dst->color[0]);
    set_default_color(dst->color[1]);
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
  if (!vdec.normals[1].enable)
  {
    auto& system = Core::System::GetInstance();
    auto& vertex_shader_manager = system.GetVertexShaderManager();
    m_vertex.normal[1][0] = vertex_shader_manager.constants.cached_tangent[0];
    m_vertex.normal[1][1] = vertex_shader_manager.constants.cached_tangent[1];
    m_vertex.normal[1][2] = vertex_shader_manager.constants.cached_tangent[2];
  }
  if (!vdec.normals[2].enable)
  {
    auto& system = Core::System::GetInstance();
    auto& vertex_shader_manager = system.GetVertexShaderManager();
    m_vertex.normal[2][0] = vertex_shader_manager.constants.cached_binormal[0];
    m_vertex.normal[2][1] = vertex_shader_manager.constants.cached_binormal[1];
    m_vertex.normal[2][2] = vertex_shader_manager.constants.cached_binormal[2];
  }

  ParseColorAttributes(&m_vertex, src, vdec);

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
