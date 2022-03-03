// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/VertexLoader_Position.h"

#include <limits>
#include <type_traits>

#include "Common/CommonTypes.h"
#include "Common/EnumMap.h"
#include "Common/Swap.h"

#include "VideoCommon/DataReader.h"
#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexLoaderUtils.h"
#include "VideoCommon/VideoCommon.h"

namespace
{
template <typename T>
constexpr float PosScale(T val, float scale)
{
  return val * scale;
}

template <>
constexpr float PosScale(float val, [[maybe_unused]] float scale)
{
  return val;
}

template <typename T, int N>
void Pos_ReadDirect(VertexLoader* loader)
{
  static_assert(N <= 3, "N > 3 is not sane!");
  const auto scale = loader->m_posScale;
  DataReader dst(g_vertex_manager_write_ptr, nullptr);
  DataReader src(g_video_buffer_read_ptr, nullptr);

  for (int i = 0; i < N; ++i)
  {
    const float value = PosScale(src.Read<T>(), scale);
    if (loader->m_counter < 3)
      VertexLoaderManager::position_cache[loader->m_counter][i] = value;
    dst.Write(value);
  }

  g_vertex_manager_write_ptr = dst.GetPointer();
  g_video_buffer_read_ptr = src.GetPointer();
  LOG_VTX();
}

template <typename I, typename T, int N>
void Pos_ReadIndex(VertexLoader* loader)
{
  static_assert(std::is_unsigned<I>::value, "Only unsigned I is sane!");
  static_assert(N <= 3, "N > 3 is not sane!");

  const auto index = DataRead<I>();
  loader->m_vertexSkip = index == std::numeric_limits<I>::max();
  const auto data =
      reinterpret_cast<const T*>(VertexLoaderManager::cached_arraybases[CPArray::Position] +
                                 (index * g_main_cp_state.array_strides[CPArray::Position]));
  const auto scale = loader->m_posScale;
  DataReader dst(g_vertex_manager_write_ptr, nullptr);

  for (int i = 0; i < N; ++i)
  {
    const float value = PosScale(Common::FromBigEndian(data[i]), scale);
    if (loader->m_counter < 3)
      VertexLoaderManager::position_cache[loader->m_counter][i] = value;
    dst.Write(value);
  }

  g_vertex_manager_write_ptr = dst.GetPointer();
  LOG_VTX();
}

using Common::EnumMap;

// These functions are to work around a "too many initializer values" error with nested brackets
// C++ does not let you write std::array<std::array<u32, 2>, 2> a = {{1, 2}, {3, 4}}
// (although it does allow std::array<std::array<u32, 2>, 2> b = {1, 2, 3, 4})
constexpr EnumMap<TPipelineFunction, CoordComponentCount::XYZ> e(TPipelineFunction xy,
                                                                 TPipelineFunction xyz)
{
  return {xy, xyz};
}
constexpr EnumMap<u32, CoordComponentCount::XYZ> e(u32 xy, u32 xyz)
{
  return {xy, xyz};
}

constexpr EnumMap<EnumMap<TPipelineFunction, CoordComponentCount::XYZ>, ComponentFormat::Float>
f(EnumMap<EnumMap<TPipelineFunction, CoordComponentCount::XYZ>, ComponentFormat::Float> in)
{
  return in;
}

constexpr EnumMap<EnumMap<u32, CoordComponentCount::XYZ>, ComponentFormat::Float>
g(EnumMap<EnumMap<u32, CoordComponentCount::XYZ>, ComponentFormat::Float> in)
{
  return in;
}

template <typename T>
using Table = EnumMap<EnumMap<EnumMap<T, CoordComponentCount::XYZ>, ComponentFormat::Float>,
                      VertexComponentFormat::Index16>;

constexpr Table<TPipelineFunction> s_table_read_position = {
    f({
        e(nullptr, nullptr),
        e(nullptr, nullptr),
        e(nullptr, nullptr),
        e(nullptr, nullptr),
        e(nullptr, nullptr),
    }),
    f({
        e(Pos_ReadDirect<u8, 2>, Pos_ReadDirect<u8, 3>),
        e(Pos_ReadDirect<s8, 2>, Pos_ReadDirect<s8, 3>),
        e(Pos_ReadDirect<u16, 2>, Pos_ReadDirect<u16, 3>),
        e(Pos_ReadDirect<s16, 2>, Pos_ReadDirect<s16, 3>),
        e(Pos_ReadDirect<float, 2>, Pos_ReadDirect<float, 3>),
    }),
    f({
        e(Pos_ReadIndex<u8, u8, 2>, Pos_ReadIndex<u8, u8, 3>),
        e(Pos_ReadIndex<u8, s8, 2>, Pos_ReadIndex<u8, s8, 3>),
        e(Pos_ReadIndex<u8, u16, 2>, Pos_ReadIndex<u8, u16, 3>),
        e(Pos_ReadIndex<u8, s16, 2>, Pos_ReadIndex<u8, s16, 3>),
        e(Pos_ReadIndex<u8, float, 2>, Pos_ReadIndex<u8, float, 3>),
    }),
    f({
        e(Pos_ReadIndex<u16, u8, 2>, Pos_ReadIndex<u16, u8, 3>),
        e(Pos_ReadIndex<u16, s8, 2>, Pos_ReadIndex<u16, s8, 3>),
        e(Pos_ReadIndex<u16, u16, 2>, Pos_ReadIndex<u16, u16, 3>),
        e(Pos_ReadIndex<u16, s16, 2>, Pos_ReadIndex<u16, s16, 3>),
        e(Pos_ReadIndex<u16, float, 2>, Pos_ReadIndex<u16, float, 3>),
    }),
};

constexpr Table<u32> s_table_read_position_vertex_size = {
    g({
        e(0u, 0u),
        e(0u, 0u),
        e(0u, 0u),
        e(0u, 0u),
        e(0u, 0u),
    }),
    g({
        e(2, 3),
        e(2, 3),
        e(4, 6),
        e(4, 6),
        e(8, 12),
    }),
    g({
        e(1, 1),
        e(1, 1),
        e(1, 1),
        e(1, 1),
        e(1, 1),
    }),
    g({
        e(2, 2),
        e(2, 2),
        e(2, 2),
        e(2, 2),
        e(2, 2),
    }),
};
}  // Anonymous namespace

u32 VertexLoader_Position::GetSize(VertexComponentFormat type, ComponentFormat format,
                                   CoordComponentCount elements)
{
  return s_table_read_position_vertex_size[type][format][elements];
}

TPipelineFunction VertexLoader_Position::GetFunction(VertexComponentFormat type,
                                                     ComponentFormat format,
                                                     CoordComponentCount elements)
{
  return s_table_read_position[type][format][elements];
}
