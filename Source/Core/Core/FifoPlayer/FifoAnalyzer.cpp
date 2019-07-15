// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
  const std::array<u64, 2> colDesc{
      vtxDesc.Color0,
      vtxDesc.Color1,
  };
  const std::array<u32, 2> colComp{
      vtxAttr.g0.Color0Comp,
      vtxAttr.g0.Color1Comp,
  };

  const std::array<u32, 8> tcElements{
      vtxAttr.g0.Tex0CoordElements, vtxAttr.g1.Tex1CoordElements, vtxAttr.g1.Tex2CoordElements,
      vtxAttr.g1.Tex3CoordElements, vtxAttr.g1.Tex4CoordElements, vtxAttr.g2.Tex5CoordElements,
      vtxAttr.g2.Tex6CoordElements, vtxAttr.g2.Tex7CoordElements,
  };
  const std::array<u32, 8> tcFormat{
      vtxAttr.g0.Tex0CoordFormat, vtxAttr.g1.Tex1CoordFormat, vtxAttr.g1.Tex2CoordFormat,
      vtxAttr.g1.Tex3CoordFormat, vtxAttr.g1.Tex4CoordFormat, vtxAttr.g2.Tex5CoordFormat,
      vtxAttr.g2.Tex6CoordFormat, vtxAttr.g2.Tex7CoordFormat,
  };

  std::array<int, 21> sizes{};

  // Add position and texture matrix indices
  u64 vtxDescHex = cpMem.vtxDesc.Hex;
  for (int i = 0; i < 9; ++i)
  {
    sizes[i] = vtxDescHex & 1;
    vtxDescHex >>= 1;
  }

  // Position
  sizes[9] = VertexLoader_Position::GetSize(vtxDesc.Position, vtxAttr.g0.PosFormat,
                                            vtxAttr.g0.PosElements);

  // Normals
  if (vtxDesc.Normal != NOT_PRESENT)
  {
    sizes[10] = VertexLoader_Normal::GetSize(vtxDesc.Normal, vtxAttr.g0.NormalFormat,
                                             vtxAttr.g0.NormalElements, vtxAttr.g0.NormalIndex3);
  }
  else
  {
    sizes[10] = 0;
  }

  // Colors
  for (size_t i = 0; i < colDesc.size(); i++)
  {
    int size = 0;

    switch (colDesc[i])
    {
    case NOT_PRESENT:
      break;
    case DIRECT:
      switch (colComp[i])
      {
      case FORMAT_16B_565:
        size = 2;
        break;
      case FORMAT_24B_888:
        size = 3;
        break;
      case FORMAT_32B_888x:
        size = 4;
        break;
      case FORMAT_16B_4444:
        size = 2;
        break;
      case FORMAT_24B_6666:
        size = 3;
        break;
      case FORMAT_32B_8888:
        size = 4;
        break;
      default:
        ASSERT(0);
        break;
      }
      break;
    case INDEX8:
      size = 1;
      break;
    case INDEX16:
      size = 2;
      break;
    }

    sizes[11 + i] = size;
  }

  // Texture coordinates
  vtxDescHex = vtxDesc.Hex >> 17;
  for (size_t i = 0; i < tcFormat.size(); i++)
  {
    sizes[13 + i] = VertexLoader_TextCoord::GetSize(vtxDescHex & 3, tcFormat[i], tcElements[i]);
    vtxDescHex >>= 2;
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
  case 0x44:
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
      std::array<int, 12> offsets;
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
      PanicAlert("FifoPlayer: Unknown Opcode (0x%x).\n", cmd);
      return 0;
    }
    break;
  }

  return (u32)(data - dataStart);
}

void LoadCPReg(u32 subCmd, u32 value, CPMemory& cpMem)
{
  switch (subCmd & 0xF0)
  {
  case 0x50:
    cpMem.vtxDesc.Hex &= ~0x1FFFF;  // keep the Upper bits
    cpMem.vtxDesc.Hex |= value;
    break;

  case 0x60:
    cpMem.vtxDesc.Hex &= 0x1FFFF;  // keep the lower 17Bits
    cpMem.vtxDesc.Hex |= (u64)value << 17;
    break;

  case 0x70:
    ASSERT((subCmd & 0x0F) < 8);
    cpMem.vtxAttr[subCmd & 7].g0.Hex = value;
    break;

  case 0x80:
    ASSERT((subCmd & 0x0F) < 8);
    cpMem.vtxAttr[subCmd & 7].g1.Hex = value;
    break;

  case 0x90:
    ASSERT((subCmd & 0x0F) < 8);
    cpMem.vtxAttr[subCmd & 7].g2.Hex = value;
    break;

  case 0xA0:
    cpMem.arrayBases[subCmd & 0xF] = value;
    break;

  case 0xB0:
    cpMem.arrayStrides[subCmd & 0xF] = value & 0xFF;
    break;
  }
}
}  // namespace FifoAnalyzer
