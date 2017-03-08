// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/FifoPlayer/FifoAnalyzer.h"

#include <algorithm>
#include <numeric>

#include "Common/Assert.h"
#include "Common/Swap.h"

#include "Core/FifoPlayer/FifoRecordAnalyzer.h"

#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/VertexLoader_Normal.h"
#include "VideoCommon/VertexLoader_Position.h"
#include "VideoCommon/VertexLoader_TextCoord.h"

namespace FifoAnalyzer
{
bool s_DrawingObject;
FifoAnalyzer::CPMemory s_CpMem;

void Init()
{
  VertexLoader_Normal::Init();
}

u8 ReadFifo8(const u8*& data)
{
  u8 value = data[0];
  data += 1;
  return value;
}

u16 ReadFifo16(const u8*& data)
{
  u16 value = Common::swap16(data);
  data += 2;
  return value;
}

u32 ReadFifo32(const u8*& data)
{
  u32 value = Common::swap32(data);
  data += 4;
  return value;
}

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

    if (mode == DECODE_RECORD)
      FifoRecordAnalyzer::ProcessLoadIndexedXf(value, array);
    break;
  }

  case OpcodeDecoder::GX_CMD_CALL_DL:
    // The recorder should have expanded display lists into the fifo stream and skipped the call to
    // start them
    // That is done to make it easier to track where memory is updated
    _assert_(false);
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

      int sizes[21];
      CalculateVertexElementSizes(sizes, cmd & OpcodeDecoder::GX_VAT_MASK, s_CpMem);

      // Determine offset of each element that might be a vertex array
      // The first 9 elements are never vertex arrays so we just accumulate their sizes.
      int offsets[12];
      int offset = std::accumulate(&sizes[0], &sizes[9], 0u);
      for (int i = 0; i < 12; ++i)
      {
        offsets[i] = offset;
        offset += sizes[i + 9];
      }

      int vertexSize = offset;
      int numVertices = ReadFifo16(data);

      if (mode == DECODE_RECORD && numVertices > 0)
      {
        for (int i = 0; i < 12; ++i)
        {
          FifoRecordAnalyzer::WriteVertexArray(i, data + offsets[i], vertexSize, numVertices);
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
    _assert_((subCmd & 0x0F) < 8);
    cpMem.vtxAttr[subCmd & 7].g0.Hex = value;
    break;

  case 0x80:
    _assert_((subCmd & 0x0F) < 8);
    cpMem.vtxAttr[subCmd & 7].g1.Hex = value;
    break;

  case 0x90:
    _assert_((subCmd & 0x0F) < 8);
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

void CalculateVertexElementSizes(int sizes[], int vatIndex, const CPMemory& cpMem)
{
  const TVtxDesc& vtxDesc = cpMem.vtxDesc;
  const VAT& vtxAttr = cpMem.vtxAttr[vatIndex];

  // Colors
  const u64 colDesc[2] = {vtxDesc.Color0, vtxDesc.Color1};
  const u32 colComp[2] = {vtxAttr.g0.Color0Comp, vtxAttr.g0.Color1Comp};

  const u32 tcElements[8] = {vtxAttr.g0.Tex0CoordElements, vtxAttr.g1.Tex1CoordElements,
                             vtxAttr.g1.Tex2CoordElements, vtxAttr.g1.Tex3CoordElements,
                             vtxAttr.g1.Tex4CoordElements, vtxAttr.g2.Tex5CoordElements,
                             vtxAttr.g2.Tex6CoordElements, vtxAttr.g2.Tex7CoordElements};

  const u32 tcFormat[8] = {vtxAttr.g0.Tex0CoordFormat, vtxAttr.g1.Tex1CoordFormat,
                           vtxAttr.g1.Tex2CoordFormat, vtxAttr.g1.Tex3CoordFormat,
                           vtxAttr.g1.Tex4CoordFormat, vtxAttr.g2.Tex5CoordFormat,
                           vtxAttr.g2.Tex6CoordFormat, vtxAttr.g2.Tex7CoordFormat};

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
  for (int i = 0; i < 2; i++)
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
        _assert_(0);
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
  for (int i = 0; i < 8; i++)
  {
    sizes[13 + i] = VertexLoader_TextCoord::GetSize(vtxDescHex & 3, tcFormat[i], tcElements[i]);
    vtxDescHex >>= 2;
  }
}
}
