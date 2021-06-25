// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <vector>

#include "Common/CommonTypes.h"

#include "VideoBackends/Software/NativeVertexFormat.h"
#include "VideoBackends/Software/SetupUnit.h"

#include "VideoCommon/VertexManagerBase.h"

// There is somewhat odd behavior where uninitialized components use values from 16 vertices ago
// (only counting vertices that had initialized data).
template <typename T>
struct CacheEntry
{
  static constexpr auto CACHE_SIZE = 16;

  std::array<T, CACHE_SIZE> data;
  u8 index;

  T Read() { return data[index]; }
  void Write(const T& value)
  {
    data[index] = value;
    index = (index + 1) % CACHE_SIZE;
  }
};

struct CachedData
{
  CacheEntry<Vec3> position;
  std::array<CacheEntry<Vec3>, 3> normal;
  std::array<CacheEntry<std::array<u8, 4>>, 2> color;
  std::array<CacheEntry<std::array<float, 2>>, 8> texCoords;
};

class SWVertexLoader final : public VertexManagerBase
{
public:
  SWVertexLoader() = default;
  ~SWVertexLoader() = default;

protected:
  void DrawCurrentBatch(u32 base_index, u32 num_indices, u32 base_vertex) override;

  void SetFormat(u8 attributeIndex, u8 primitiveType);
  void ParseVertex(const PortableVertexDeclaration& vdec, int index);

  InputVertexData m_vertex;
  SetupUnit m_setup_unit;
  CachedData m_cache;
};
