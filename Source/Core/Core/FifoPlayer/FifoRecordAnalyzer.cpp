// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/FifoPlayer/FifoRecordAnalyzer.h"

#include <algorithm>

#include "Core/FifoPlayer/FifoAnalyzer.h"
#include "Core/FifoPlayer/FifoRecorder.h"
#include "Core/HW/Memmap.h"

using namespace FifoAnalyzer;

void FifoRecordAnalyzer::Initialize(const u32* cpMem)
{
  s_DrawingObject = false;

  FifoAnalyzer::LoadCPReg(0x50, *(cpMem + 0x50), s_CpMem);
  FifoAnalyzer::LoadCPReg(0x60, *(cpMem + 0x60), s_CpMem);
  for (int i = 0; i < 8; ++i)
    FifoAnalyzer::LoadCPReg(0x70 + i, *(cpMem + 0x70 + i), s_CpMem);

  const u32* const bases_start = cpMem + 0xA0;
  const u32* const bases_end = bases_start + s_CpMem.arrayBases.size();
  std::copy(bases_start, bases_end, s_CpMem.arrayBases.begin());

  const u32* const strides_start = cpMem + 0xB0;
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
  int arrayType = (s_CpMem.vtxDesc.Hex >> (9 + (arrayIndex * 2))) & 3;
  if (arrayType < 2)
    return;

  int maxIndex = 0;

  // Determine min and max indices
  if (arrayType == INDEX8)
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
