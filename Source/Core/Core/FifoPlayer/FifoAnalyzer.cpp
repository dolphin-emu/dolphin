// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/FifoPlayer/FifoAnalyzer.h"

#include <numeric>

#include "Common/Assert.h"
#include "Common/MsgHandler.h"
#include "Common/Swap.h"

#include "Core/FifoPlayer/FifoRecordAnalyzer.h"

#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/VertexLoader_Normal.h"
#include "VideoCommon/VertexLoader_Position.h"
#include "VideoCommon/VertexLoader_TextCoord.h"

namespace FifoAnalyzer
{
namespace
{
u8 ReadFifo8(const u8*& data)
{
  const u8 value = data[0];
  data += 1;
  return value;
}

u16 ReadFifo16(const u8*& data)
{
  const u16 value = Common::swap16(data);
  data += 2;
  return value;
}

u32 ReadFifo32(const u8*& data)
{
  const u32 value = Common::swap32(data);
  data += 4;
  return value;
}

std::array<int, 21> CalculateVertexElementSizes(int vatIndex, const CPMemory& cpMem)
{
  const TVtxDesc& vtxDesc = cpMem.vtxDesc;
  const VAT& vtxAttr = cpMem.vtxAttr[vatIndex];

  // Colors
  const std::array<ColorFormat, 2> colComp{
      vtxAttr.g0.Color0Comp,
      vtxAttr.g0.Color1Comp,
  };

  const std::array<TexComponentCount, 8> tcElements{
      vtxAttr.g0.Tex0CoordElements, vtxAttr.g1.Tex1CoordElements, vtxAttr.g1.Tex2CoordElements,
      vtxAttr.g1.Tex3CoordElements, vtxAttr.g1.Tex4CoordElements, vtxAttr.g2.Tex5CoordElements,
      vtxAttr.g2.Tex6CoordElements, vtxAttr.g2.Tex7CoordElements,
  };
  const std::array<ComponentFormat, 8> tcFormat{
      vtxAttr.g0.Tex0CoordFormat, vtxAttr.g1.Tex1CoordFormat, vtxAttr.g1.Tex2CoordFormat,
      vtxAttr.g1.Tex3CoordFormat, vtxAttr.g1.Tex4CoordFormat, vtxAttr.g2.Tex5CoordFormat,
      vtxAttr.g2.Tex6CoordFormat, vtxAttr.g2.Tex7CoordFormat,
  };

  std::array<int, 21> sizes{};

  // Add position and texture matrix indices
  sizes[0] = vtxDesc.low.PosMatIdx;
  for (size_t i = 0; i < vtxDesc.low.TexMatIdx.Size(); ++i)
  {
    sizes[i + 1] = vtxDesc.low.TexMatIdx[i];
  }

  // Position
  sizes[9] = VertexLoader_Position::GetSize(vtxDesc.low.Position, vtxAttr.g0.PosFormat,
                                            vtxAttr.g0.PosElements);

  // Normals
  if (vtxDesc.low.Normal != VertexComponentFormat::NotPresent)
  {
    sizes[10] = VertexLoader_Normal::GetSize(vtxDesc.low.Normal, vtxAttr.g0.NormalFormat,
                                             vtxAttr.g0.NormalElements, vtxAttr.g0.NormalIndex3);
  }
  else
  {
    sizes[10] = 0;
  }

  // Colors
  for (size_t i = 0; i < vtxDesc.low.Color.Size(); i++)
  {
    int size = 0;

    switch (vtxDesc.low.Color[i])
    {
    case VertexComponentFormat::NotPresent:
      break;
    case VertexComponentFormat::Direct:
      switch (colComp[i])
      {
      case ColorFormat::RGB565:
        size = 2;
        break;
      case ColorFormat::RGB888:
        size = 3;
        break;
      case ColorFormat::RGB888x:
        size = 4;
        break;
      case ColorFormat::RGBA4444:
        size = 2;
        break;
      case ColorFormat::RGBA6666:
        size = 3;
        break;
      case ColorFormat::RGBA8888:
        size = 4;
        break;
      default:
        ASSERT(0);
        break;
      }
      break;
    case VertexComponentFormat::Index8:
      size = 1;
      break;
    case VertexComponentFormat::Index16:
      size = 2;
      break;
    }

    sizes[11 + i] = size;
  }

  // Texture coordinates
  for (size_t i = 0; i < tcFormat.size(); i++)
  {
    sizes[13 + i] =
        VertexLoader_TextCoord::GetSize(vtxDesc.high.TexCoord[i], tcFormat[i], tcElements[i]);
  }

  return sizes;
}
}  // Anonymous namespace

bool s_DrawingObject;
FifoAnalyzer::CPMemory s_CpMem;

u32 AnalyzeCommand(const u8* data, DecodeMode mode)
{
  const u8* dataStart = data;

  int cmd = ReadFifo8(data);

  switch (cmd)
  {
  case OpcodeDecoder::GX_NOP:
  case OpcodeDecoder::GX_CMD_UNKNOWN_METRICS:
  case OpcodeDecoder::GX_CMD_INVL_VC:
    break;

  case OpcodeDecoder::GX_LOAD_CP_REG:
  {
    s_DrawingObject = false;

    u32 cmd2 = ReadFifo8(data);
    u32 value = ReadFifo32(data);
    LoadCPReg(cmd2, value, s_CpMem);
    break;
  }

  case OpcodeDecoder::GX_LOAD_XF_REG:
  {
    s_DrawingObject = false;

    u32 cmd2 = ReadFifo32(data);
    u8 streamSize = ((cmd2 >> 16) & 15) + 1;

    data += streamSize * 4;
    break;
  }

  case OpcodeDecoder::GX_LOAD_INDX_A:
  case OpcodeDecoder::GX_LOAD_INDX_B:
  case OpcodeDecoder::GX_LOAD_INDX_C:
  case OpcodeDecoder::GX_LOAD_INDX_D:
  {
    s_DrawingObject = false;

    int array = 0xc + (cmd - OpcodeDecoder::GX_LOAD_INDX_A) / 8;
    u32 value = ReadFifo32(data);

    if (mode == DecodeMode::Record)
      FifoRecordAnalyzer::ProcessLoadIndexedXf(value, array);
    break;
  }

  case OpcodeDecoder::GX_CMD_CALL_DL:
    // The recorder should have expanded display lists into the fifo stream and skipped the call to
    // start them
    // That is done to make it easier to track where memory is updated
    ASSERT(false);
    data += 8;
    break;

  case OpcodeDecoder::GX_LOAD_BP_REG:
  {
    s_DrawingObject = false;
    ReadFifo32(data);
    break;
  }

  default:
    if (cmd & 0x80)
    {
      s_DrawingObject = true;

      const std::array<int, 21> sizes =
          CalculateVertexElementSizes(cmd & OpcodeDecoder::GX_VAT_MASK, s_CpMem);

      // Determine offset of each element that might be a vertex array
      // The first 9 elements are never vertex arrays so we just accumulate their sizes.
      int offset = std::accumulate(sizes.begin(), sizes.begin() + 9, 0u);
      std::array<int, NUM_VERTEX_COMPONENT_ARRAYS> offsets;
      for (size_t i = 0; i < offsets.size(); ++i)
      {
        offsets[i] = offset;
        offset += sizes[i + 9];
      }

      const int vertexSize = offset;
      const int numVertices = ReadFifo16(data);

      if (mode == DecodeMode::Record && numVertices > 0)
      {
        for (size_t i = 0; i < offsets.size(); ++i)
        {
          FifoRecordAnalyzer::WriteVertexArray(static_cast<int>(i), data + offsets[i], vertexSize,
                                               numVertices);
        }
      }

      data += numVertices * vertexSize;
    }
    else
    {
      PanicAlertFmt("FifoPlayer: Unknown Opcode ({:#x}).\n", cmd);
      return 0;
    }
    break;
  }

  return (u32)(data - dataStart);
}

void LoadCPReg(u32 subCmd, u32 value, CPMemory& cpMem)
{
  switch (subCmd & CP_COMMAND_MASK)
  {
  case VCD_LO:
    cpMem.vtxDesc.low.Hex = value;
    break;

  case VCD_HI:
    cpMem.vtxDesc.high.Hex = value;
    break;

  case CP_VAT_REG_A:
    ASSERT(subCmd - CP_VAT_REG_A < CP_NUM_VAT_REG);
    cpMem.vtxAttr[subCmd & CP_VAT_MASK].g0.Hex = value;
    break;

  case CP_VAT_REG_B:
    ASSERT(subCmd - CP_VAT_REG_B < CP_NUM_VAT_REG);
    cpMem.vtxAttr[subCmd & CP_VAT_MASK].g1.Hex = value;
    break;

  case CP_VAT_REG_C:
    ASSERT(subCmd - CP_VAT_REG_C < CP_NUM_VAT_REG);
    cpMem.vtxAttr[subCmd & CP_VAT_MASK].g2.Hex = value;
    break;

  case ARRAY_BASE:
    cpMem.arrayBases[subCmd & CP_ARRAY_MASK] = value;
    break;

  case ARRAY_STRIDE:
    cpMem.arrayStrides[subCmd & CP_ARRAY_MASK] = value & 0xFF;
    break;
  }
}
}  // namespace FifoAnalyzer
