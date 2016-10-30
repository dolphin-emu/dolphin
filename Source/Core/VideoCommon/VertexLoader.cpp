// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Assert.h"
#include "Common/CommonTypes.h"

#include "VideoCommon/DataReader.h"
#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexLoaderUtils.h"
#include "VideoCommon/VertexLoader_Color.h"
#include "VideoCommon/VertexLoader_Normal.h"
#include "VideoCommon/VertexLoader_Position.h"
#include "VideoCommon/VertexLoader_TextCoord.h"
#include "VideoCommon/VideoCommon.h"

// This pointer is used as the source/dst for all fixed function loader calls
u8* g_video_buffer_read_ptr;
u8* g_vertex_manager_write_ptr;

static void PosMtx_ReadDirect_UByte(VertexLoader* loader)
{
  u32 posmtx = DataRead<u8>() & 0x3f;
  if (loader->m_counter < 3)
    VertexLoaderManager::position_matrix_index[loader->m_counter + 1] = posmtx;
  DataWrite<u32>(posmtx);
  PRIM_LOG("posmtx: %d, ", posmtx);
}

static void TexMtx_ReadDirect_UByte(VertexLoader* loader)
{
  loader->m_curtexmtx[loader->m_texmtxread] = DataRead<u8>() & 0x3f;

  PRIM_LOG("texmtx%d: %d, ", loader->m_texmtxread, loader->m_curtexmtx[loader->m_texmtxread]);
  loader->m_texmtxread++;
}

static void TexMtx_Write_Float(VertexLoader* loader)
{
  DataWrite(float(loader->m_curtexmtx[loader->m_texmtxwrite++]));
}

static void TexMtx_Write_Float2(VertexLoader* loader)
{
  DataWrite(0.f);
  DataWrite(float(loader->m_curtexmtx[loader->m_texmtxwrite++]));
}

static void TexMtx_Write_Float3(VertexLoader* loader)
{
  DataWrite(0.f);
  DataWrite(0.f);
  DataWrite(float(loader->m_curtexmtx[loader->m_texmtxwrite++]));
}

static void SkipVertex(VertexLoader* loader)
{
  if (loader->m_vertexSkip)
  {
    // reset the output buffer
    g_vertex_manager_write_ptr -= loader->m_native_vtx_decl.stride;

    loader->m_skippedVertices++;
  }
}

VertexLoader::VertexLoader(const TVtxDesc& vtx_desc, const VAT& vtx_attr)
    : VertexLoaderBase(vtx_desc, vtx_attr)
{
  VertexLoader_Normal::Init();

  CompileVertexTranslator();

  // generate frac factors
  m_posScale = 1.0f / (1U << m_VtxAttr.PosFrac);
  for (int i = 0; i < 8; i++)
    m_tcScale[i] = 1.0f / (1U << m_VtxAttr.texCoord[i].Frac);
}

void VertexLoader::CompileVertexTranslator()
{
  m_VertexSize = 0;
  const TVtxAttr& vtx_attr = m_VtxAttr;

  // Reset pipeline
  m_numPipelineStages = 0;

  // Colors
  const u64 col[2] = {m_VtxDesc.Color0, m_VtxDesc.Color1};
  // TextureCoord
  const u64 tc[8] = {m_VtxDesc.Tex0Coord, m_VtxDesc.Tex1Coord, m_VtxDesc.Tex2Coord,
                     m_VtxDesc.Tex3Coord, m_VtxDesc.Tex4Coord, m_VtxDesc.Tex5Coord,
                     m_VtxDesc.Tex6Coord, m_VtxDesc.Tex7Coord};

  u32 components = 0;

  // Position in pc vertex format.
  int nat_offset = 0;

  // Position Matrix Index
  if (m_VtxDesc.PosMatIdx)
  {
    WriteCall(PosMtx_ReadDirect_UByte);
    components |= VB_HAS_POSMTXIDX;
    m_native_vtx_decl.posmtx.components = 4;
    m_native_vtx_decl.posmtx.enable = true;
    m_native_vtx_decl.posmtx.offset = nat_offset;
    m_native_vtx_decl.posmtx.type = VAR_UNSIGNED_BYTE;
    m_native_vtx_decl.posmtx.integer = true;
    nat_offset += 4;
    m_VertexSize += 1;
  }

  if (m_VtxDesc.Tex0MatIdx)
  {
    m_VertexSize += 1;
    components |= VB_HAS_TEXMTXIDX0;
    WriteCall(TexMtx_ReadDirect_UByte);
  }
  if (m_VtxDesc.Tex1MatIdx)
  {
    m_VertexSize += 1;
    components |= VB_HAS_TEXMTXIDX1;
    WriteCall(TexMtx_ReadDirect_UByte);
  }
  if (m_VtxDesc.Tex2MatIdx)
  {
    m_VertexSize += 1;
    components |= VB_HAS_TEXMTXIDX2;
    WriteCall(TexMtx_ReadDirect_UByte);
  }
  if (m_VtxDesc.Tex3MatIdx)
  {
    m_VertexSize += 1;
    components |= VB_HAS_TEXMTXIDX3;
    WriteCall(TexMtx_ReadDirect_UByte);
  }
  if (m_VtxDesc.Tex4MatIdx)
  {
    m_VertexSize += 1;
    components |= VB_HAS_TEXMTXIDX4;
    WriteCall(TexMtx_ReadDirect_UByte);
  }
  if (m_VtxDesc.Tex5MatIdx)
  {
    m_VertexSize += 1;
    components |= VB_HAS_TEXMTXIDX5;
    WriteCall(TexMtx_ReadDirect_UByte);
  }
  if (m_VtxDesc.Tex6MatIdx)
  {
    m_VertexSize += 1;
    components |= VB_HAS_TEXMTXIDX6;
    WriteCall(TexMtx_ReadDirect_UByte);
  }
  if (m_VtxDesc.Tex7MatIdx)
  {
    m_VertexSize += 1;
    components |= VB_HAS_TEXMTXIDX7;
    WriteCall(TexMtx_ReadDirect_UByte);
  }

  // Write vertex position loader
  WriteCall(VertexLoader_Position::GetFunction(m_VtxDesc.Position, m_VtxAttr.PosFormat,
                                               m_VtxAttr.PosElements));

  m_VertexSize += VertexLoader_Position::GetSize(m_VtxDesc.Position, m_VtxAttr.PosFormat,
                                                 m_VtxAttr.PosElements);
  int pos_elements = m_VtxAttr.PosElements + 2;
  m_native_vtx_decl.position.components = pos_elements;
  m_native_vtx_decl.position.enable = true;
  m_native_vtx_decl.position.offset = nat_offset;
  m_native_vtx_decl.position.type = VAR_FLOAT;
  m_native_vtx_decl.position.integer = false;
  nat_offset += pos_elements * sizeof(float);

  // Normals
  if (m_VtxDesc.Normal != NOT_PRESENT)
  {
    m_VertexSize += VertexLoader_Normal::GetSize(m_VtxDesc.Normal, m_VtxAttr.NormalFormat,
                                                 m_VtxAttr.NormalElements, m_VtxAttr.NormalIndex3);

    TPipelineFunction pFunc = VertexLoader_Normal::GetFunction(
        m_VtxDesc.Normal, m_VtxAttr.NormalFormat, m_VtxAttr.NormalElements, m_VtxAttr.NormalIndex3);

    if (pFunc == nullptr)
    {
      PanicAlert("VertexLoader_Normal::GetFunction(%i %i %i %i) returned zero!",
                 (u32)m_VtxDesc.Normal, m_VtxAttr.NormalFormat, m_VtxAttr.NormalElements,
                 m_VtxAttr.NormalIndex3);
    }
    WriteCall(pFunc);

    for (int i = 0; i < (vtx_attr.NormalElements ? 3 : 1); i++)
    {
      m_native_vtx_decl.normals[i].components = 3;
      m_native_vtx_decl.normals[i].enable = true;
      m_native_vtx_decl.normals[i].offset = nat_offset;
      m_native_vtx_decl.normals[i].type = VAR_FLOAT;
      m_native_vtx_decl.normals[i].integer = false;
      nat_offset += 12;
    }

    components |= VB_HAS_NRM0;
    if (m_VtxAttr.NormalElements == 1)
      components |= VB_HAS_NRM1 | VB_HAS_NRM2;
  }

  for (int i = 0; i < 2; i++)
  {
    m_native_vtx_decl.colors[i].components = 4;
    m_native_vtx_decl.colors[i].type = VAR_UNSIGNED_BYTE;
    m_native_vtx_decl.colors[i].integer = false;
    switch (col[i])
    {
    case NOT_PRESENT:
      break;
    case DIRECT:
      switch (m_VtxAttr.color[i].Comp)
      {
      case FORMAT_16B_565:
        m_VertexSize += 2;
        WriteCall(Color_ReadDirect_16b_565);
        break;
      case FORMAT_24B_888:
        m_VertexSize += 3;
        WriteCall(Color_ReadDirect_24b_888);
        break;
      case FORMAT_32B_888x:
        m_VertexSize += 4;
        WriteCall(Color_ReadDirect_32b_888x);
        break;
      case FORMAT_16B_4444:
        m_VertexSize += 2;
        WriteCall(Color_ReadDirect_16b_4444);
        break;
      case FORMAT_24B_6666:
        m_VertexSize += 3;
        WriteCall(Color_ReadDirect_24b_6666);
        break;
      case FORMAT_32B_8888:
        m_VertexSize += 4;
        WriteCall(Color_ReadDirect_32b_8888);
        break;
      default:
        _assert_(0);
        break;
      }
      break;
    case INDEX8:
      m_VertexSize += 1;
      switch (m_VtxAttr.color[i].Comp)
      {
      case FORMAT_16B_565:
        WriteCall(Color_ReadIndex8_16b_565);
        break;
      case FORMAT_24B_888:
        WriteCall(Color_ReadIndex8_24b_888);
        break;
      case FORMAT_32B_888x:
        WriteCall(Color_ReadIndex8_32b_888x);
        break;
      case FORMAT_16B_4444:
        WriteCall(Color_ReadIndex8_16b_4444);
        break;
      case FORMAT_24B_6666:
        WriteCall(Color_ReadIndex8_24b_6666);
        break;
      case FORMAT_32B_8888:
        WriteCall(Color_ReadIndex8_32b_8888);
        break;
      default:
        _assert_(0);
        break;
      }
      break;
    case INDEX16:
      m_VertexSize += 2;
      switch (m_VtxAttr.color[i].Comp)
      {
      case FORMAT_16B_565:
        WriteCall(Color_ReadIndex16_16b_565);
        break;
      case FORMAT_24B_888:
        WriteCall(Color_ReadIndex16_24b_888);
        break;
      case FORMAT_32B_888x:
        WriteCall(Color_ReadIndex16_32b_888x);
        break;
      case FORMAT_16B_4444:
        WriteCall(Color_ReadIndex16_16b_4444);
        break;
      case FORMAT_24B_6666:
        WriteCall(Color_ReadIndex16_24b_6666);
        break;
      case FORMAT_32B_8888:
        WriteCall(Color_ReadIndex16_32b_8888);
        break;
      default:
        _assert_(0);
        break;
      }
      break;
    }
    // Common for the three bottom cases
    if (col[i] != NOT_PRESENT)
    {
      components |= VB_HAS_COL0 << i;
      m_native_vtx_decl.colors[i].offset = nat_offset;
      m_native_vtx_decl.colors[i].enable = true;
      nat_offset += 4;
    }
  }

  // Texture matrix indices (remove if corresponding texture coordinate isn't enabled)
  for (int i = 0; i < 8; i++)
  {
    m_native_vtx_decl.texcoords[i].offset = nat_offset;
    m_native_vtx_decl.texcoords[i].type = VAR_FLOAT;
    m_native_vtx_decl.texcoords[i].integer = false;

    const int format = m_VtxAttr.texCoord[i].Format;
    const int elements = m_VtxAttr.texCoord[i].Elements;

    if (tc[i] != NOT_PRESENT)
    {
      _assert_msg_(VIDEO, DIRECT <= tc[i] && tc[i] <= INDEX16,
                   "Invalid texture coordinates!\n(tc[i] = %d)", (u32)tc[i]);
      _assert_msg_(VIDEO, FORMAT_UBYTE <= format && format <= FORMAT_FLOAT,
                   "Invalid texture coordinates format!\n(format = %d)", format);
      _assert_msg_(VIDEO, 0 <= elements && elements <= 1,
                   "Invalid number of texture coordinates elements!\n(elements = %d)", elements);

      components |= VB_HAS_UV0 << i;
      WriteCall(VertexLoader_TextCoord::GetFunction(tc[i], format, elements));
      m_VertexSize += VertexLoader_TextCoord::GetSize(tc[i], format, elements);
    }

    if (components & (VB_HAS_TEXMTXIDX0 << i))
    {
      m_native_vtx_decl.texcoords[i].enable = true;
      if (tc[i] != NOT_PRESENT)
      {
        // if texmtx is included, texcoord will always be 3 floats, z will be the texmtx index
        m_native_vtx_decl.texcoords[i].components = 3;
        nat_offset += 12;
        WriteCall(m_VtxAttr.texCoord[i].Elements ? TexMtx_Write_Float : TexMtx_Write_Float2);
      }
      else
      {
        m_native_vtx_decl.texcoords[i].components = 3;
        nat_offset += 12;
        WriteCall(TexMtx_Write_Float3);
      }
    }
    else
    {
      if (tc[i] != NOT_PRESENT)
      {
        m_native_vtx_decl.texcoords[i].enable = true;
        m_native_vtx_decl.texcoords[i].components = vtx_attr.texCoord[i].Elements ? 2 : 1;
        nat_offset += 4 * (vtx_attr.texCoord[i].Elements ? 2 : 1);
      }
    }

    if (tc[i] == NOT_PRESENT)
    {
      // if there's more tex coords later, have to write a dummy call
      int j = i + 1;
      for (; j < 8; ++j)
      {
        if (tc[j] != NOT_PRESENT)
        {
          WriteCall(VertexLoader_TextCoord::GetDummyFunction());  // important to get indices right!
          break;
        }
      }
      // tricky!
      if (j == 8 && !((components & VB_HAS_TEXMTXIDXALL) & (VB_HAS_TEXMTXIDXALL << (i + 1))))
      {
        // no more tex coords and tex matrices, so exit loop
        break;
      }
    }
  }

  // indexed position formats may skip a the vertex
  if (m_VtxDesc.Position & 2)
  {
    WriteCall(SkipVertex);
  }

  m_native_components = components;
  m_native_vtx_decl.stride = nat_offset;
}

void VertexLoader::WriteCall(TPipelineFunction func)
{
  m_PipelineStages[m_numPipelineStages++] = func;
}

int VertexLoader::RunVertices(DataReader src, DataReader dst, int count)
{
  g_vertex_manager_write_ptr = dst.GetPointer();
  g_video_buffer_read_ptr = src.GetPointer();

  m_numLoadedVertices += count;
  m_skippedVertices = 0;

  for (m_counter = count - 1; m_counter >= 0; m_counter--)
  {
    m_tcIndex = 0;
    m_colIndex = 0;
    m_texmtxwrite = m_texmtxread = 0;
    for (int i = 0; i < m_numPipelineStages; i++)
      m_PipelineStages[i](this);
    PRIM_LOG("\n");
  }

  return count - m_skippedVertices;
}
