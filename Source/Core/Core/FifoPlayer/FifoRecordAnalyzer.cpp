// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/FifoPlayer/FifoRecordAnalyzer.h"

#include <algorithm>

#include "Common/MsgHandler.h"
#include "Core/FifoPlayer/FifoAnalyzer.h"
#include "Core/FifoPlayer/FifoRecorder.h"
#include "Core/HW/Memmap.h"

using namespace FifoAnalyzer;

void FifoRecordAnalyzer::Initialize(const u32* cpMem)
{
  s_DrawingObject = false;

  FifoAnalyzer::LoadCPReg(VCD_LO, cpMem[VCD_LO], s_CpMem);
  FifoAnalyzer::LoadCPReg(VCD_HI, cpMem[VCD_HI], s_CpMem);
  for (u32 i = 0; i < CP_NUM_VAT_REG; ++i)
    FifoAnalyzer::LoadCPReg(CP_VAT_REG_A + i, cpMem[CP_VAT_REG_A + i], s_CpMem);

  const u32* const bases_start = cpMem + ARRAY_BASE;
  const u32* const bases_end = bases_start + s_CpMem.arrayBases.size();
  std::copy(bases_start, bases_end, s_CpMem.arrayBases.begin());

  const u32* const strides_start = cpMem + ARRAY_STRIDE;
  const u32* const strides_end = strides_start + s_CpMem.arrayStrides.size();
  std::copy(strides_start, strides_end, s_CpMem.arrayStrides.begin());
}

void FifoRecordAnalyzer::ProcessLoadIndexedXf(u32 val, int array)
{
  int index = val >> 16;
  int size = ((val >> 12) & 0xF) + 1;

  u32 address = s_CpMem.arrayBases[array] + s_CpMem.arrayStrides[array] * index;

  FifoRecorder::GetInstance().UseMemory(address, size * 4, MemoryUpdate::XF_DATA);
}

void FifoRecordAnalyzer::WriteVertexArray(int arrayIndex, const u8* vertexData, int vertexSize,
                                          int numVertices)
{
  // Skip if not indexed array
  VertexComponentFormat arrayType;
  if (arrayIndex == ARRAY_POSITION)
    arrayType = s_CpMem.vtxDesc.low.Position;
  else if (arrayIndex == ARRAY_NORMAL)
    arrayType = s_CpMem.vtxDesc.low.Normal;
  else if (arrayIndex >= ARRAY_COLOR0 && arrayIndex < ARRAY_COLOR0 + NUM_COLOR_ARRAYS)
    arrayType = s_CpMem.vtxDesc.low.Color[arrayIndex - ARRAY_COLOR0];
  else if (arrayIndex >= ARRAY_TEXCOORD0 && arrayIndex < ARRAY_TEXCOORD0 + NUM_TEXCOORD_ARRAYS)
    arrayType = s_CpMem.vtxDesc.high.TexCoord[arrayIndex - ARRAY_TEXCOORD0];
  else
  {
    PanicAlertFmt("Invalid arrayIndex {}", arrayIndex);
    return;
  }

  if (!IsIndexed(arrayType))
    return;

  int maxIndex = 0;

  // Determine min and max indices
  if (arrayType == VertexComponentFormat::Index8)
  {
    for (int i = 0; i < numVertices; ++i)
    {
      int index = *vertexData;
      vertexData += vertexSize;

      // 0xff skips the vertex
      if (index != 0xff)
      {
        if (index > maxIndex)
          maxIndex = index;
      }
    }
  }
  else
  {
    for (int i = 0; i < numVertices; ++i)
    {
      int index = Common::swap16(vertexData);
      vertexData += vertexSize;

      // 0xffff skips the vertex
      if (index != 0xffff)
      {
        if (index > maxIndex)
          maxIndex = index;
      }
    }
  }

  u32 arrayStart = s_CpMem.arrayBases[arrayIndex];
  u32 arraySize = s_CpMem.arrayStrides[arrayIndex] * (maxIndex + 1);

  FifoRecorder::GetInstance().UseMemory(arrayStart, arraySize, MemoryUpdate::VERTEX_STREAM);
}
