// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/OpcodeDecoding.h"

class CPUCull
{
public:
  ~CPUCull();
  void Init();
  bool AreAllVerticesCulled(VertexLoaderBase* loader, OpcodeDecoder::Primitive primitive,
                            const u8* src, u32 count);

  struct alignas(16) TransformedVertex
  {
    float x, y, z, w;
  };

  using TransformFunction = void (*)(void*, const void*, u32, int);
  using CullFunction = bool (*)(const CPUCull::TransformedVertex*, int);

private:
  template <typename T>
  struct BufferDeleter
  {
    void operator()(T* ptr);
  };
  std::unique_ptr<TransformedVertex[], BufferDeleter<TransformedVertex>> m_transform_buffer{};
  u32 m_transform_buffer_size = 0;
  std::array<std::array<TransformFunction, 2>, 2> m_transform_table{};
  Common::EnumMap<Common::EnumMap<CullFunction, CullMode::All>,
                  OpcodeDecoder::Primitive::GX_DRAW_TRIANGLE_FAN>
      m_cull_table{};
};
